# $Id: Makefile,v 1.35 2000/12/15 00:41:18 davem Exp $
# Makefile for Sparc library files..
#

L_TARGET = lib.a

EXTRA_AFLAGS := -ansi -DST_DIV0=0x02

obj-y := mul.o rem.o sdiv.o udiv.o umul.o urem.o ashrdi3.o memcpy.o memset.o \
         strlen.o checksum.o blockops.o memscan.o memcmp.o strncmp.o \
	 strncpy_from_user.o divdi3.o udivdi3.o strlen_user.o \
	 copy_user.o locks.o atomic.o bitops.o debuglocks.o lshrdi3.o \
	 ashldi3.o rwsem.o muldi3.o

include $(TOPDIR)/Rules.make
