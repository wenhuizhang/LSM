#
# Makefile for USB Core files and filesystem
#

export-objs	:= usb.o hcd.o hcd-pci.o urb.o message.o config.o file.o

usbcore-objs	:= usb.o usb-debug.o hub.o hcd.o urb.o message.o \
			config.o file.o

ifeq ($(CONFIG_PCI),y)
	usbcore-objs	+= hcd-pci.o
endif

ifeq ($(CONFIG_USB_DEVICEFS),y)
	usbcore-objs	+= devio.o inode.o devices.o
endif

obj-$(CONFIG_USB)	+= usbcore.o

include $(TOPDIR)/Rules.make
