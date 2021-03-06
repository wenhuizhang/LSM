#
# Makefile for the linux kernel.
#

EXTRA_TARGETS	:= head.o init_task.o
EXTRA_AFLAGS	:= -traditional

O_TARGET	:= kernel.o

export-objs	:= debug.o ebcdic.o irq.o s390_ext.o smp.o s390_ksyms.o
obj-y	:= entry.o bitmap.o traps.o time.o process.o irq.o \
            setup.o sys_s390.o ptrace.o signal.o cpcmd.o ebcdic.o \
            semaphore.o s390fpu.o reipl.o s390_ext.o debug.o

obj-$(CONFIG_MODULES)		+= s390_ksyms.o
obj-$(CONFIG_SMP)		+= smp.o

#
# Kernel debugging
#
obj-$(CONFIG_REMOTE_DEBUG)	+= gdb-stub.o #gdb-low.o 

include $(TOPDIR)/Rules.make

.PHONY: asm-offsets.h

entry.S: asm-offsets.h

asm-offsets.h: asm-offsets.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -S $< -o - | grep -- "->" | \
	(echo "#ifndef __ASM_OFFSETS_H__"; \
	 echo "#define __ASM_OFFSETS_H__"; \
	 echo "/*"; \
	 echo " * DO NOT MODIFY"; \
	 echo " *"; \
	 echo " * This file was generated by arch/s390/kernel/Makefile"; \
	 echo " */"; \
	 sed -e "s:^->\([^ ]*\) \([^ ]*\) \(.*\):#define \\1 \\2 /* \\3*/:" \
	     -e "s:->::"; \
	 echo "#endif" \
	) > asm-offsets.h

clean:
	rm -f asm-offsets.h


