#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995, 1998, 2001 by Ralf Baechle
#

OBJS  = milo.o a.out.o

#
# Some DECstations need all possible sections of an ECOFF executable
#
ifdef CONFIG_DECSTATION
  E2EFLAGS = -a
else
  E2EFLAGS =
endif

#
# Drop some uninteresting sections in the kernel.
# This is only relevant for ELF kernels but doesn't hurt a.out
#
drop-sections	= .reginfo .mdebug
strip-flags	= $(addprefix --remove-section=,$(drop-sections))

all: vmlinux.ecoff addinitrd

vmlinux.ecoff:	elf2ecoff $(TOPDIR)/vmlinux
	./elf2ecoff $(TOPDIR)/vmlinux vmlinux.ecoff $(E2EFLAGS)

elf2ecoff: elf2ecoff.c
	$(HOSTCC) -o $@ $^

addinitrd: addinitrd.c
	$(HOSTCC) -o $@ $^

clean:
	rm -f vmlinux.ecoff
	rm -f zImage zImage.tmp

mrproper:
	rm -f vmlinux.ecoff
	rm -f addinitrd
	rm -f elf2ecoff

include $(TOPDIR)/Rules.make
