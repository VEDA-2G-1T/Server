// ads1115_driver.c

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h> // class_create, device_create 등을 위해 필요

#define DEVICE_NAME "adc_device"
#define CLASS_NAME  "adc"

static dev_t dev_number;
static struct class* adc_class  = NULL;
static struct cdev    adc_cdev;
static struct i2c_client* adc_client = NULL;

// ===== I2C communication with ADS1115 =====
#define ADS1115_CONVERSION_REG 0x00
#define ADS1115_CONFIG_REG     0x01

static int ads1115_read_adc(struct i2c_client *client, int16_t *out)
{
    // AIN0 vs GND, +/-4.096V, single-shot, 860 SPS
    uint8_t config[3] = {
        ADS1115_CONFIG_REG,
        0b11000010, // MSB: OS=1(start), MUX=100(AIN0/GND), PGA=001(4.096V), MODE=1(single-shot)
        0b11100011  // LSB: DR=111(860SPS), COMP_MODE=0, COMP_POL=0, COMP_LAT=0, COMP_QUE=11
    };

    uint8_t reg = ADS1115_CONVERSION_REG;
    uint8_t buf[2];
    int ret;

    // Send config to start a new conversion
    ret = i2c_master_send(client, config, 3);
    if (ret != 3) {
        pr_err("ADS1115: Failed to send config. ret=%d\n", ret);
        return ret < 0 ? ret : -EIO;
    }

    // ADS1115의 860SPS 설정 시 변환 시간은 약 1.17ms 입니다.
    // 안정성을 위해 조금 더 기다립니다.
    msleep(10);

    // Request to read from the conversion register
    ret = i2c_master_send(client, &reg, 1);
     if (ret != 1) {
        pr_err("ADS1115: Failed to set pointer to conversion reg. ret=%d\n", ret);
        return ret < 0 ? ret : -EIO;
    }

    // Read the 2-byte result
    ret = i2c_master_recv(client, buf, 2);
    if (ret != 2) {
        pr_err("ADS1115: Failed to read conversion data. ret=%d\n", ret);
        return ret < 0 ? ret : -EIO;
    }

    *out = (buf[0] << 8) | buf[1];
    return 0;
}

// ===== File operations =====
static ssize_t adc_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    int16_t raw;
    int ret;

    if (len < sizeof(raw)) {
        return -EINVAL;
    }

    ret = ads1115_read_adc(adc_client, &raw);
    if (ret < 0) {
        pr_err("Failed to read from ads1115\n");
        return ret;
    }

    if (copy_to_user(buffer, &raw, sizeof(raw))) {
        return -EFAULT;
    }

    return sizeof(raw);
}

static int adc_open(struct inode *inodep, struct file *filep)
{
    // i2c_client가 유효한지 확인
    if (!adc_client) {
        pr_err("adc_client is NULL\n");
        return -ENODEV;
    }
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open  = adc_open,
    .read  = adc_read,
};

static int ads1115_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    struct device *dev;

    pr_info("ADS1115 probe function called!\n");

    adc_client = client;

    // 1. Character device 번호 할당
    ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate char dev region\n");
        return ret;
    }

    // 2. Class 생성
    adc_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(adc_class)) {
        pr_err("Failed to create class\n");
        ret = PTR_ERR(adc_class);
        goto err_unregister_chrdev;
    }

    // 3. Device 파일 생성 (/dev/adc_device)
    dev = device_create(adc_class, NULL, dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(dev)) {
        pr_err("Failed to create device\n");
        ret = PTR_ERR(dev);
        goto err_destroy_class;
    }

    // 4. cdev 초기화 및 등록
    cdev_init(&adc_cdev, &fops);
    adc_cdev.owner = THIS_MODULE;
    ret = cdev_add(&adc_cdev, dev_number, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        goto err_destroy_device;
    }

    pr_info("ADS1115 driver loaded successfully\n");
    return 0;

// 에러 발생 시 자원 해제 경로
err_destroy_device:
    device_destroy(adc_class, dev_number);
err_destroy_class:
    class_destroy(adc_class);
err_unregister_chrdev:
    unregister_chrdev_region(dev_number, 1);
    return ret;
}

static void ads1115_remove(struct i2c_client *client)
{
    pr_info("ADS1115 driver removed\n");
    device_destroy(adc_class, dev_number);
    class_destroy(adc_class);
    cdev_del(&adc_cdev);
    unregister_chrdev_region(dev_number, 1);
}

static const struct i2c_device_id ads1115_id[] = {
   { "veda_mic", 0 },
   { }
};
MODULE_DEVICE_TABLE(i2c, ads1115_id);

static const struct of_device_id ads1115_of_match[] = {
    { .compatible = "veda_1,veda_mic" },
    { },
};
MODULE_DEVICE_TABLE(of, ads1115_of_match);

// 이 별칭이 커널이 DT의 장치와 이 모듈을 연결하는 데 도움을 줍니다.
MODULE_ALIAS("i2c:veda_1,veda_mic");

static struct i2c_driver ads1115_driver = {
    .driver = {
        .name = "ads1115_driver",
        .of_match_table = ads1115_of_match,
    },
    .probe = ads1115_probe,
    .remove = ads1115_remove,
    .id_table = ads1115_id,
};
module_i2c_driver(ads1115_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kim hyeon seok");
MODULE_AUTHOR("miky 17");
