obj-m += blockio.o
myapp := block


KERNELDIR := /home/chen/imx6ull/kernel
CURRENT_PATH := $(shell pwd)

build: kernel_modules

kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean

cleanapp:
	rm App$(myapp)
app:
	arm-linux-gnueabihf-gcc app_$(myapp).c -o App$(myapp)

cleanall:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean
	rm App$(myapp)


.PHONY:cleanapp cleanall