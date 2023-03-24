obj-m += led_sem.o

KERNELDIR=/home/chen/imx6ull/kernel
CURRENT_PATH := $(shell pwd)

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean

cleanapp:
	rm app_sem
app:
	arm-linux-gnueabihf-gcc app_sem.c -o app_sem


