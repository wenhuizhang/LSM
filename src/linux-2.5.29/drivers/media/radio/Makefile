#
# Makefile for the kernel character device drivers.
#

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs     := miropcm20-rds-core.o

miropcm20-objs	:= miropcm20-rds-core.o miropcm20-radio.o

obj-$(CONFIG_RADIO_AZTECH) += radio-aztech.o
obj-$(CONFIG_RADIO_RTRACK2) += radio-rtrack2.o
obj-$(CONFIG_RADIO_SF16FMI) += radio-sf16fmi.o
obj-$(CONFIG_RADIO_CADET) += radio-cadet.o
obj-$(CONFIG_RADIO_TYPHOON) += radio-typhoon.o
obj-$(CONFIG_RADIO_TERRATEC) += radio-terratec.o
obj-$(CONFIG_RADIO_MAXIRADIO) += radio-maxiradio.o
obj-$(CONFIG_RADIO_RTRACK) += radio-aimslab.o
obj-$(CONFIG_RADIO_ZOLTRIX) += radio-zoltrix.o
obj-$(CONFIG_RADIO_MIROPCM20) += miropcm20.o
obj-$(CONFIG_RADIO_MIROPCM20_RDS) += miropcm20-rds.o
obj-$(CONFIG_RADIO_GEMTEK) += radio-gemtek.o
obj-$(CONFIG_RADIO_GEMTEK_PCI) += radio-gemtek-pci.o
obj-$(CONFIG_RADIO_TRUST) += radio-trust.o
obj-$(CONFIG_RADIO_MAESTRO) += radio-maestro.o

include $(TOPDIR)/Rules.make
