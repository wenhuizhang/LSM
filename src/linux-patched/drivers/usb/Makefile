#
# Makefile for the kernel USB device drivers.
#

# Object files in subdirectories

obj-$(CONFIG_USB)		+= core/

obj-$(CONFIG_USB_EHCI_HCD)	+= host/
obj-$(CONFIG_USB_OHCI_HCD)	+= host/
obj-$(CONFIG_USB_OHCI)		+= host/
obj-$(CONFIG_USB_OHCI_SA1111)	+= host/
obj-$(CONFIG_USB_SL811HS)	+= host/
obj-$(CONFIG_USB_UHCI_ALT)	+= host/
obj-$(CONFIG_USB_UHCI_HCD_ALT)	+= host/
obj-$(CONFIG_USB_UHCI_HCD)	+= host/
obj-$(CONFIG_USB_UHCI)		+= host/

obj-$(CONFIG_USB_ACM)		+= class/
obj-$(CONFIG_USB_AUDIO)		+= class/
obj-$(CONFIG_USB_BLUETOOTH_TTY)	+= class/
obj-$(CONFIG_USB_PRINTER)	+= class/

obj-$(CONFIG_USB_STORAGE)	+= storage/

obj-$(CONFIG_USB_AIPTEK)	+= input/
obj-$(CONFIG_USB_HID)		+= input/
obj-$(CONFIG_USB_KBD)		+= input/
obj-$(CONFIG_USB_MOUSE)		+= input/
obj-$(CONFIG_USB_WACOM)		+= input/

obj-$(CONFIG_USB_DABUSB)	+= media/
obj-$(CONFIG_USB_DSBR)		+= media/
obj-$(CONFIG_USB_IBMCAM)	+= media/
obj-$(CONFIG_USB_KONICAWC)	+= media/
obj-$(CONFIG_USB_OV511)		+= media/
obj-$(CONFIG_USB_PWC)		+= media/
obj-$(CONFIG_USB_SE401)		+= media/
obj-$(CONFIG_USB_STV680)	+= media/
obj-$(CONFIG_USB_VICAM)		+= media/

obj-$(CONFIG_USB_CATC)		+= net/
obj-$(CONFIG_USB_CDCETHER)	+= net/
obj-$(CONFIG_USB_KAWETH)	+= net/
obj-$(CONFIG_USB_PEGASUS)	+= net/
obj-$(CONFIG_USB_RTL8150)	+= net/
obj-$(CONFIG_USB_USBNET)	+= net/

obj-$(CONFIG_USB_DC2XX)		+= image/
obj-$(CONFIG_USB_HPUSBSCSI)	+= image/
obj-$(CONFIG_USB_MDC800)	+= image/
obj-$(CONFIG_USB_MICROTEK)	+= image/
obj-$(CONFIG_USB_SCANNER)	+= image/

obj-$(CONFIG_USB_SERIAL)	+= serial/

obj-$(CONFIG_USB_AUERSWALD)	+= misc/
obj-$(CONFIG_USB_BRLVGER)	+= misc/
obj-$(CONFIG_USB_EMI26)		+= misc/
obj-$(CONFIG_USB_RIO500)	+= misc/
obj-$(CONFIG_USB_TIGL)		+= misc/
obj-$(CONFIG_USB_USS720)	+= misc/

include $(TOPDIR)/Rules.make
