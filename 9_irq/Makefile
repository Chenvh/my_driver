obj-m += imx6uirq.o
myapp := app_irq

KERNELDIR :=/home/chen/imx6ull/kernel
CURRENT_PATH := $(shell pwd)

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean

cleanapp:
	rm $(myapp)
app:
	arm-linux-gnueabihf-gcc $(myapp).c -o $(myapp)


