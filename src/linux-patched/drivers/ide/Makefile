#
# Makefile for the kernel ata, atapi, and ide block device drivers.
#
# 12 September 2000, Bartlomiej Zolnierkiewicz <bkz@linux-ide.org>
# Rewritten to use lists instead of if-statements.
#
# Note : at this point, these files are compiled on all systems.
# In the future, some of these should be built conditionally.
#

export-objs	:= device.o ide-taskfile.o main.o ide.o probe.o quirks.o pcidma.o tcq.o \
		   atapi.o ataraid.o

obj-$(CONFIG_BLK_DEV_HD)	+= hd.o
obj-$(CONFIG_BLK_DEV_IDE)       += ide-mod.o
obj-$(CONFIG_BLK_DEV_IDECS)     += ide-cs.o
obj-$(CONFIG_BLK_DEV_IDEDISK)   += ide-disk.o
obj-$(CONFIG_ATAPI)		+= atapi.o
obj-$(CONFIG_BLK_DEV_IDECD)     += ide-cd.o
obj-$(CONFIG_BLK_DEV_IDETAPE)   += ide-tape.o
obj-$(CONFIG_BLK_DEV_IDEFLOPPY) += ide-floppy.o

obj-$(CONFIG_BLK_DEV_IT8172)    += it8172.o
obj-$(CONFIG_BLK_DEV_QD65XX)    += qd65xx.o

ide-obj-$(CONFIG_BLK_DEV_AEC62XX)	+= aec62xx.o
ide-obj-$(CONFIG_BLK_DEV_ALI14XX)	+= ali14xx.o
ide-obj-$(CONFIG_BLK_DEV_ALI15X3)	+= alim15x3.o
ide-obj-$(CONFIG_BLK_DEV_AMD74XX)	+= amd74xx.o
ide-obj-$(CONFIG_BLK_DEV_BUDDHA)	+= buddha.o
ide-obj-$(CONFIG_BLK_DEV_CMD640)	+= cmd640.o
ide-obj-$(CONFIG_BLK_DEV_CMD64X)	+= cmd64x.o
ide-obj-$(CONFIG_BLK_DEV_CS5530)	+= cs5530.o
ide-obj-$(CONFIG_BLK_DEV_CY82C693)	+= cy82c693.o
ide-obj-$(CONFIG_BLK_DEV_DTC2278)	+= dtc2278.o
ide-obj-$(CONFIG_BLK_DEV_FALCON_IDE)	+= falconide.o
ide-obj-$(CONFIG_BLK_DEV_GAYLE)		+= gayle.o
ide-obj-$(CONFIG_BLK_DEV_Q40IDE)	+= q40ide.o
ide-obj-$(CONFIG_BLK_DEV_HPT34X)	+= hpt34x.o
ide-obj-$(CONFIG_BLK_DEV_HPT366)	+= hpt366.o
ide-obj-$(CONFIG_BLK_DEV_HT6560B)	+= ht6560b.o
ide-obj-$(CONFIG_BLK_DEV_IDE_ICSIDE)	+= icside.o
ide-obj-$(CONFIG_BLK_DEV_IDEDMA)	+= quirks.o
ide-obj-$(CONFIG_BLK_DEV_IDEDMA_PCI)	+= pcidma.o
ide-obj-$(CONFIG_BLK_DEV_IDE_TCQ)	+= tcq.o
ide-obj-$(CONFIG_PCI)			+= ide-pci.o
ide-obj-$(CONFIG_BLK_DEV_ISAPNP)	+= ide-pnp.o
ide-obj-$(CONFIG_BLK_DEV_IDE_PMAC)	+= ide-pmac.o
ide-obj-$(CONFIG_BLK_DEV_MAC_IDE)	+= macide.o
ide-obj-$(CONFIG_BLK_DEV_NS87415)	+= ns87415.o
ide-obj-$(CONFIG_BLK_DEV_OPTI621)	+= opti621.o
ide-obj-$(CONFIG_BLK_DEV_SVWKS)		+= serverworks.o
ide-obj-$(CONFIG_BLK_DEV_PDC202XX)	+= pdc202xx.o
ide-obj-$(CONFIG_BLK_DEV_PDC4030)	+= pdc4030.o
ide-obj-$(CONFIG_BLK_DEV_PIIX)		+= piix.o
ide-obj-$(CONFIG_BLK_DEV_IDE_RAPIDE)	+= rapide.o
ide-obj-$(CONFIG_BLK_DEV_RZ1000)	+= rz1000.o
ide-obj-$(CONFIG_BLK_DEV_SIS5513)	+= sis5513.o
ide-obj-$(CONFIG_BLK_DEV_SL82C105)	+= sl82c105.o
ide-obj-$(CONFIG_BLK_DEV_TRM290)	+= trm290.o
ide-obj-$(CONFIG_BLK_DEV_UMC8672)	+= umc8672.o
ide-obj-$(CONFIG_BLK_DEV_VIA82CXXX)	+= via82cxxx.o
ide-obj-$(CONFIG_BLK_DEV_MPC8xx_IDE)	+= ide-m8xx.o

# The virtualised raid layers MUST come after the ide itself or bad stuff
# will happen.
obj-$(CONFIG_BLK_DEV_ATARAID)		+= ataraid.o
obj-$(CONFIG_BLK_DEV_ATARAID_PDC)	+= pdcraid.o
obj-$(CONFIG_BLK_DEV_ATARAID_HPT)	+= hptraid.o

ide-mod-objs	:= device.o ide-taskfile.o main.o ide.o probe.o \
		   ioctl.o ata-timing.o $(ide-obj-y)

include $(TOPDIR)/Rules.make
