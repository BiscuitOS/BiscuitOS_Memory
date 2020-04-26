#
# Module Project
#
# (C) 2020.02.02 BuddyZhang1 <buddy.zhang@aliyun.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
ifneq ($(KERNELRELEASE),)

## Target
ifeq ("$(origin MODULE_NAME)", "command line")
MODULE_NAME		:= $(MODULE_NAME)
else
MODULE_NAME		:= BiscuitOS_mod
endif
obj-m			:= $(MODULE_NAME).o

# Architecture Memory
ARCH_MM			:= arm

## Source Code
$(MODULE_NAME)-m	:= main.o
$(MODULE_NAME)-m	+= $(patsubst $(PWD)/%.c,%.o, $(wildcard $(PWD)/mm/*.c))
$(MODULE_NAME)-m	+= $(patsubst $(PWD)/%.c,%.o, $(wildcard $(PWD)/fs/*.c))
$(MODULE_NAME)-m	+= $(patsubst $(PWD)/%.c,%.o, $(wildcard $(PWD)/init/*.c))
$(MODULE_NAME)-m	+= $(patsubst $(PWD)/%.c,%.o, $(wildcard $(PWD)/arch/$(ARCH_MM)/*.c)) 
$(MODULE_NAME)-m	+= $(patsubst $(PWD)/%.S,%.o, $(wildcard $(PWD)/arch/$(ARCH_MM)/*.S)) 

## Memory Allocator Test Code
#  0) bootmem
$(MODULE_NAME)-m	+= modules/bootmem/main.o
#  1) Buddy
# obj-m			+= $(MODULE_NAME)-buddy.o
# $(MODULE_NAME)-buddy-m	:= modules/buddy/module.o
$(MODULE_NAME)-m	+= modules/buddy/main.o
#  2) Slab
# obj-m			+= $(MODULE_NAME)-slab.o
# $(MODULE_NAME)-slab-m	:= modules/slab/module.o
$(MODULE_NAME)-m	+= modules/slab/main.o
#  3) VMALLOC
# obj-m			+= $(MODULE_NAME)-vmalloc.o
# $(MODULE_NAME)-vmalloc-m	:= modules/vmalloc/module.o
$(MODULE_NAME)-m	+= modules/vmalloc/main.o


# LD-scripts
ldflags-y		+= -r -T $(PWD)/BiscuitOS.lds
## CFlags
ccflags-y		+= -DCONFIG_NODES_SHIFT=0
ccflags-y		+= -DCONFIG_NR_CPUS_BS=8
# Support HighMem
ccflags-y		+= -DCONFIG_HIGHMEM_BS
# Support SLAB Debug
# ccflags-y		+= -DCONFIG_DEBUG_SLAB_BS
## ASFlags
asflags-y		:= -I$(PWD)/arch/$(ARCH_MM)/include
asflags-y		+= -I$(PWD)/include

## Header
ccflags-y		+= -I$(PWD)/arch/$(ARCH_MM)/include
ccflags-y		+= -I$(PWD)/include

else

## Parse argument
## Default support ARM32
ifeq ("$(origin BSROOT)", "command line")
BSROOT=$(BSROOT)
else
BSROOT=/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32
endif

ifeq ("$(origin ARCH)", "command line")
ARCH=$(ARCH)
else
ARCH=arm
endif

ifeq ("$(origin CROSS_TOOLS)", "command line")
CROSS_TOOLS=$(CROSS_TOOLS)
else
CROSS_TOOLS=arm-linux-gnueabi
endif

## Don't Edit
KERNELDIR=$(BSROOT)/linux/linux
CROSS_COMPILE_PATH=$(BSROOT)/$(CROSS_TOOLS)/$(CROSS_TOOLS)/bin
CROSS_COMPILE=$(CROSS_COMPILE_PATH)/$(CROSS_TOOLS)-
INCLUDEDIR=$(KERNELDIR)/include
ARCHINCLUDEDIR=$(KERNELDIR)/arch/$(ARCH)/include
INSTALLDIR=$(BSROOT)/rootfs/rootfs/

## X86/X64 Architecture
ifeq ($(ARCH), i386)
CROSS_COMPILE	:=
else ifeq ($(ARCH), x86_64)
CROSS_COMPILE	:=
endif

## Compile
AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

# FLAGS
CFLAGS += -I$(INCLUDEDIR) -I$(ARCHINCLUDEDIR)

all: $(OBJS)
	make -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) modules

install:
	make -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) \
		INSTALL_MOD_PATH=$(INSTALLDIR) modules_install
	@chmod 755 RunBiscuitOS_mm.sh
	@cp -rfa RunBiscuitOS_mm.sh $(BSROOT)/rootfs/rootfs/usr/bin

clean:
	@rm -rf *.ko *.o *.mod.o *.mod.c *.symvers *.order \
               .*.o.cmd .tmp_versions *.ko.cmd .*.ko.cmd \
		mm/*.o arch/arm/*.o mm/.*.o.*  \
		fs/*.o fs/.*.o.*  \
		arch/arm/.*.o.* init/*.o modules/buddy/*.o \
		modules/buddy/.*.cmd modules/bootmem/*.o \
		modules/bootmem/.*.cmd \
		modules/slab/.*.cmd modules/slab/*.o \
		modules/vmalloc/.*.cmd modules/vmalloc/*.o

endif
