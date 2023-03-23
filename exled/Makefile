#obj-m += led_drv.o
#obj-y += led.o
#obj-m += led_dts_drv.o
obj-m += exled.o

KERNELDIR=/home/cwh/imx6ull/kernel
CURRENT_PATH := $(shell pwd)

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean

cleanapp:
	rm led
app:
	arm-linux-gnueabihf-gcc led_test.c -o led

