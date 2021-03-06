#
# arch/i386/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
#

# ROOT_DEV specifies the default root-device when making the image.
# This can be either FLOPPY, CURRENT, /dev/xxxx or empty, in which case
# the default of FLOPPY is used by 'build'.

ROOT_DEV := CURRENT

# If you want to preset the SVGA mode, uncomment the next line and
# set SVGA_MODE to whatever number you want.
# Set it to -DSVGA_MODE=NORMAL_VGA if you just want the EGA/VGA mode.
# The number is the same as you would ordinarily press at bootup.

SVGA_MODE := -DSVGA_MODE=NORMAL_VGA

# If you want the RAM disk device, define this to be the size in blocks.

#RAMDISK := -DRAMDISK=512

EXTRA_TARGETS	:= vmlinux.bin bvmlinux.bin bootsect bootsect.o \
		   setup setup.o zImage bzImage

host-progs	:= tools/build

# 	Default

boot: bzImage

include $(TOPDIR)/Rules.make

# ---------------------------------------------------------------------------

zImage:  IMAGE_OFFSET := 0x1000
zImage:  EXTRA_AFLAGS := -traditional $(SVGA_MODE) $(RAMDISK)
bzImage: IMAGE_OFFSET := 0x100000
bzImage: EXTRA_AFLAGS := -traditional $(SVGA_MODE) $(RAMDISK) -D__BIG_KERNEL__
bzImage: BUILDFLAGS   := -b

quiet_cmd_image = BUILD  $(RELDIR)/$@
cmd_image = tools/build $(BUILDFLAGS) bootsect setup vmlinux.bin \
			$(ROOT_DEV) > $@

zImage bzImage: bootsect setup vmlinux.bin tools/build FORCE
	$(call if_changed,image)

vmlinux.bin: compressed/vmlinux FORCE
	$(call if_changed,objcopy)

LDFLAGS_bootsect := -Ttext 0x0 -s --oformat binary
LDFLAGS_setup	 := -Ttext 0x0 -s --oformat binary -e begtext

setup bootsect: %: %.o FORCE
	$(call if_changed,ld)

compressed/vmlinux: FORCE
	@$(MAKE) IMAGE_OFFSET=$(IMAGE_OFFSET) -C compressed vmlinux


zdisk: $(BOOTIMAGE)
	dd bs=8192 if=$(BOOTIMAGE) of=/dev/fd0

zlilo: $(BOOTIMAGE)
	if [ -f $(INSTALL_PATH)/vmlinuz ]; then mv $(INSTALL_PATH)/vmlinuz $(INSTALL_PATH)/vmlinuz.old; fi
	if [ -f $(INSTALL_PATH)/System.map ]; then mv $(INSTALL_PATH)/System.map $(INSTALL_PATH)/System.old; fi
	cat $(BOOTIMAGE) > $(INSTALL_PATH)/vmlinuz
	cp $(TOPDIR)/System.map $(INSTALL_PATH)/
	if [ -x /sbin/lilo ]; then /sbin/lilo; else /etc/lilo/install; fi

install: $(BOOTIMAGE)
	sh -x ./install.sh $(KERNELRELEASE) $(BOOTIMAGE) $(TOPDIR)/System.map "$(INSTALL_PATH)"

clean:
	@echo 'Cleaning up (boot)'
	@rm -f tools/build vmlinux.bin bvmlinux.bin
	@rm -f setup bootsect zImage compressed/vmlinux.out
	@rm -f bsetup bbootsect bzImage compressed/bvmlinux.out
	@$(MAKE) -C compressed clean

archhelp:
	@echo  '* bzImage	- Compressed kernel image (arch/$(ARCH)/boot/bzImage)'
	@echo  '  install	- Install kernel using'
	@echo  '                  (your) ~/bin/installkernel or'
	@echo  '                  (distribution) /sbin/installkernel or'
	@echo  '        	  install to $$(INSTALL_PATH) and run lilo'

