#
# arch/sh/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1999 Stuart Menefy
#

SYSTEM  =$(TOPDIR)/vmlinux

Image:	$(SYSTEM)
	$(OBJCOPY) $(OBJCOPYFLAGS) $< $@

zImage: compressed/vmlinux
	$(OBJCOPY) $(OBJCOPYFLAGS) $< $@

compressed/vmlinux: $(TOPDIR)/vmlinux
	$(MAKE) -C compressed vmlinux

install: Image
	sh -x ./install.sh $(KERNELRELEASE) Image $(TOPDIR)/System.map "$(INSTALL_PATH)"

zinstall: zImage
	sh -x ./install.sh $(KERNELRELEASE) zImage $(TOPDIR)/System.map "$(INSTALL_PATH)"

clean:
	rm -f tools/build
	rm -f setup bootsect zImage compressed/vmlinux.out
	rm -f bsetup bbootsect bzImage compressed/bvmlinux.out
	@$(MAKE) -C compressed clean
