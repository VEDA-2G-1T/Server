# Makefile

obj-m += ads1115_driver.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod ads1115_driver.ko

remove:
	sudo rmmod ads1115_driver

reinstall: remove all install
