KERNELDIR ?= ..
PWD := $(shell pwd)

obj-m += kqueue.o

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
