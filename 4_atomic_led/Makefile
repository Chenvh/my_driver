#obj-m += led_drv.o
#obj-y += led.o
#obj-m += led_dts_drv.o
obj-m += led_atomic.o

KERNELDIR=/home/cwh/imx6ull/kernel
CURRENT_PATH := $(shell pwd)

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean

cleanapp:
	rm app_atomic
app:
	arm-linux-gnueabihf-gcc app_atomic.c -o app_atomic


