# Makefile for the eicon ISDN device driver

# Objects that export symbols.

export-objs	:= Divas_mod.o eicon_mod.o

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN_DRV_EICON_OLD)    	+= eicon.o
obj-$(CONFIG_ISDN_DRV_EICON_DIVAS)	+= divas.o

# Multipart objects.

eicon-objs	:= eicon_mod.o eicon_isa.o eicon_pci.o eicon_idi.o \
		   eicon_io.o
divas-objs	:= common.o idi.o bri.o pri.o log.o xlog.o kprintf.o fpga.o \
		   fourbri.o lincfg.o linchr.o linsys.o linio.o Divas_mod.o

# Optional parts of multipart objects.

eicon-objs-$(CONFIG_ISDN_DRV_EICON_PCI) += common.o idi.o bri.o pri.o log.o \
		   xlog.o kprintf.o fpga.o fourbri.o lincfg.o linchr.o \
		   linsys.o linio.o

eicon-objs 	+= $(eicon-objs-y)

include $(TOPDIR)/Rules.make
