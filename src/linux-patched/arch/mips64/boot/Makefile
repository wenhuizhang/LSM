#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995, 1998, 1999 by Ralf Baechle
#

#
# Some DECstations need all possible sections of an ECOFF executable
#
ifdef CONFIG_DECSTATION
  E2EFLAGS = -a
else
  E2EFLAGS =
endif

all: vmlinux.ecoff addinitrd

vmlinux.ecoff:	elf2ecoff $(TOPDIR)/vmlinux
	./elf2ecoff $(TOPDIR)/vmlinux vmlinux.ecoff $(E2EFLAGS)

elf2ecoff: elf2ecoff.c
	$(HOSTCC) -o $@ $^

addinitrd: addinitrd.c
	$(HOSTCC) -o $@ $^

clean:
	rm -f vmlinux.ecoff

mrproper:
	rm -f vmlinux.ecoff addinitrd elf2ecoff

include $(TOPDIR)/Rules.make
