ifneq ($(KERNELRELEASE),)

obj-m += dmadrv.o

else

KVER ?= $(shell uname -r)
MODULES_DIR := /lib/modules/$(KVER)
KDIR := $(MODULES_DIR)/build
MODULE_DESTDIR := $(MODULES_DIR)/extra/
DEPMOD := /sbin/depmod

REL := $(subst ., , $(subst -, , $(shell uname -r)))
REL_MAJOR  := $(word 1,$(REL))
REL_MEDIUM := $(word 2,$(REL))
REL_MINOR  := $(word 3,$(REL))

all: module

module:
	@ $(MAKE) -C $(KDIR) $(MAKE_PARAMS) M=$(PWD) modules

install:
	@ $(MAKE) -C $(KDIR) $(MAKE_PARAMS) M=$(PWD) modules_install

help:
	$(MAKE) -C $(KDIR) M=$$PWD help

clean:
	rm -rf *.o *.ko* *.mod.* .*.cmd Module.symvers modules.order .tmp_versions/ *~ core .depend TAGS 


.PHONY: clean all help install module

endif
