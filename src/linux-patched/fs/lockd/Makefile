#
# Makefile for the linux lock manager stuff
#

export-objs := lockd_syms.o

obj-$(CONFIG_LOCKD) += lockd.o

lockd-objs-y := clntlock.o clntproc.o host.o svc.o svclock.o svcshare.o \
	        svcproc.o svcsubs.o mon.o xdr.o lockd_syms.o
lockd-objs-$(CONFIG_LOCKD_V4) += xdr4.o svc4proc.o
lockd-objs		      := $(lockd-objs-y)

include $(TOPDIR)/Rules.make
