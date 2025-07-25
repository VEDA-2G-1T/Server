// led_pwm_dev.c - LED PWM fade in/out driver (user triggers via ioctl)

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/types.h>

// Define IOCTL magic number
#define LEDPWM_IOC_MAGIC 'L'

// User-space fade request structure passed via IOCTL
struct ledpwm_fade_req {
    __u64 period_ns;      // PWM period in nanoseconds
    __u64 duration_ms;    // Total fade in/out duration in milliseconds
    __u8  steps;          // Number of brightness steps per direction
    __u8  polarity;       // PWM polarity (0: normal, 1: inversed)
    __u8  reserved[6];    // Padding for future use or alignment
};

// IOCTL command to trigger fade operation
#define LEDPWM_IOC_FADE _IOW(LEDPWM_IOC_MAGIC, 0x02, struct ledpwm_fade_req)

// Per-device driver data
struct ledpwm_dev {
    struct pwm_device *pwm;         // Associated PWM device
    struct device     *dev;         // Parent device
    struct pwm_state   state;       // Current PWM state

    struct miscdevice miscdev;      // Misc device for user interaction
    struct work_struct fade_work;   // Workqueue for async fade operation
    struct ledpwm_fade_req fade_req;// Cached user-requested fade parameters
};

// Workqueue handler that executes LED fading
static void ledpwm_fade_worker(struct work_struct *w)
{
    struct ledpwm_dev *ldev = container_of(w, struct ledpwm_dev, fade_work);
    const struct ledpwm_fade_req *req = &ldev->fade_req;
    struct pwm_state st;
    u64 max_duty = req->period_ns;
    u64 delay_us = div64_u64(req->duration_ms * 1000, req->steps * 2);
    int i;

    dev_info(ldev->dev, "Fade start: period=%llu ns, duration=%llu ms, steps=%u\n",
             req->period_ns, req->duration_ms, req->steps);

    // Initialize PWM state for fade operation
    pwm_get_state(ldev->pwm, &st);
    st.period = req->period_ns;
    st.polarity = req->polarity ? PWM_POLARITY_INVERSED : PWM_POLARITY_NORMAL;
    st.enabled = true;

    // Fade in (duty cycle from 0% to 100%)
    for (i = 0; i <= req->steps; ++i) {
        st.duty_cycle = div64_u64(max_duty * i, req->steps);
        pwm_apply_state(ldev->pwm, &st);
        usleep_range(delay_us, delay_us + 500);
    }

    // Fade out (duty cycle from 100% to 0%)
    for (i = req->steps - 1; i >= 0; --i) {
        st.duty_cycle = div64_u64(max_duty * i, req->steps);
        pwm_apply_state(ldev->pwm, &st);
        usleep_range(delay_us, delay_us + 500);
    }

    // Turn off PWM after fade completes
    st.enabled = false;
    pwm_apply_state(ldev->pwm, &st);

    dev_info(ldev->dev, "Fade complete.\n");
}

// IOCTL handler: receives fade request from user-space
static long ledpwm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct ledpwm_dev *ldev = filp->private_data;

    if (cmd == LEDPWM_IOC_FADE) {
        struct ledpwm_fade_req req;

        // Copy fade request from user-space
        if (copy_from_user(&req, (void __user *)arg, sizeof(req)))
            return -EFAULT;

        // Validate parameters
        if (req.steps == 0 || req.period_ns == 0 || req.duration_ms == 0)
            return -EINVAL;

        // Store request and schedule fade work
        memcpy(&ldev->fade_req, &req, sizeof(req));
        schedule_work(&ldev->fade_work);
        return 0;
    }

    return -ENOTTY; // Unsupported IOCTL command
}

// File open: bind file handle to ledpwm_dev
static int ledpwm_open(struct inode *inode, struct file *file)
{
    struct ledpwm_dev *ldev = container_of(file->private_data, struct ledpwm_dev, miscdev);
    file->private_data = ldev;
    return 0;
}

// File operations for misc device
static const struct file_operations ledpwm_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = ledpwm_ioctl,
    .open = ledpwm_open,
    .llseek = no_llseek,
};

// Platform probe: allocate resources and register misc device
static int ledpwm_probe(struct platform_device *pdev)
{
    struct ledpwm_dev *ldev;
    int ret;

    // Allocate per-device struct
    ldev = devm_kzalloc(&pdev->dev, sizeof(*ldev), GFP_KERNEL);
    if (!ldev)
        return -ENOMEM;

    ldev->dev = &pdev->dev;

    // Acquire PWM from device tree
    ldev->pwm = devm_pwm_get(&pdev->dev, NULL);
    if (IS_ERR(ldev->pwm))
        return dev_err_probe(&pdev->dev, PTR_ERR(ldev->pwm), "Failed to get PWM\n");

    // Initialize and apply default PWM state
    pwm_init_state(ldev->pwm, &ldev->state);
    pwm_apply_state(ldev->pwm, &ldev->state);

    // Prepare fade worker
    INIT_WORK(&ldev->fade_work, ledpwm_fade_worker);

    // Register misc device node (e.g., /dev/ledpwm0)
    ldev->miscdev.minor = MISC_DYNAMIC_MINOR;
    ldev->miscdev.name = "ledpwm0";
    ldev->miscdev.fops = &ledpwm_fops;
    ldev->miscdev.mode = 0660;

    ret = misc_register(&ldev->miscdev);
    if (ret)
        return dev_err_probe(&pdev->dev, ret, "Failed to register misc device\n");

    platform_set_drvdata(pdev, ldev);
    dev_info(&pdev->dev, "LED PWM driver registered successfully\n");
    return 0;
}

// Platform remove: clean up resources
static int ledpwm_remove(struct platform_device *pdev)
{
    struct ledpwm_dev *ldev = platform_get_drvdata(pdev);
    cancel_work_sync(&ldev->fade_work);
    misc_deregister(&ldev->miscdev);
    return 0;
}

// Device Tree match table
static const struct of_device_id ledpwm_of_match[] = {
    { .compatible = "veda_1,ledpwm" },
    {}
};
MODULE_DEVICE_TABLE(of, ledpwm_of_match);

// Platform driver registration
static struct platform_driver ledpwm_driver = {
    .driver = {
        .name = "ledpwm",
        .of_match_table = ledpwm_of_match,
    },
    .probe = ledpwm_probe,
    .remove = ledpwm_remove,
};
module_platform_driver(ledpwm_driver);

// Module metadata
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kim hyeon seok");
MODULE_DESCRIPTION("LED PWM Fade Driver via ioctl");
