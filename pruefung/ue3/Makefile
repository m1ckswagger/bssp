DRIVER = cdrv

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m := $(DRIVER).o

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else

	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

endif

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

clean-hardcoded:
	rm -rf \
	.$(DRIVER).o.cmd \
	.$(DRIVER).ko.cmd \
	.$(DRIVER).mod.o.cmd \
	$(DRIVER).mod.o \
	$(DRIVER).ko \
	$(DRIVER).mod.c \
	$(DRIVER).o \
	modules.order \
	Module.symvers \
	.tmp_versions

