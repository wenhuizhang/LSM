#
# Makefile for the input core drivers.
#

# Objects that export symbols.

export-objs	:= input.o

# Each configuration option enables a list of files.

obj-$(CONFIG_INPUT)		+= input.o
obj-$(CONFIG_INPUT_KEYBDEV)	+= keybdev.o
obj-$(CONFIG_INPUT_MOUSEDEV)	+= mousedev.o
obj-$(CONFIG_INPUT_JOYDEV)	+= joydev.o
obj-$(CONFIG_INPUT_EVDEV)	+= evdev.o
obj-$(CONFIG_INPUT_TSDEV)	+= tsdev.o
obj-$(CONFIG_INPUT_POWER)	+= power.o
obj-$(CONFIG_INPUT_EVBUG)	+= evbug.o
obj-$(CONFIG_INPUT_UINPUT)	+= uinput.o

obj-$(CONFIG_INPUT_KEYBOARD)	+= keyboard/
obj-$(CONFIG_INPUT_MOUSE)	+= mouse/
obj-$(CONFIG_INPUT_JOYSTICK)	+= joystick/
obj-$(CONFIG_INPUT_TOUCHSCREEN)	+= touchscreen/

# The global Rules.make.

include $(TOPDIR)/Rules.make
