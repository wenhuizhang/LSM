/*
 *  linux/drivers/video/anakinfb.c
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   23-Apr-2001 TTC	Created
 */

#include <linux/types.h>
#include <linux/fb.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>

#include <asm/io.h>

#include <video/fbcon.h>

static u32 colreg[16];
static struct fb_info fb_info;
static struct display display;

static struct fb_var_screeninfo anakinfb_var = {
	xres:		400,
	yres:		234,
	xres_virtual:	400,
	yres_virtual:	234,
	bits_per_pixel:	16,
	red:		{ 11, 5, 0 },
	green:		{  5, 6, 0 }, 
	blue:		{  0, 5, 0 },
        activate:       FB_ACTIVATE_NOW,
        height:         -1,
        width:          -1,
        vmode:          FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo anakinfb_fix = {
	id:		"AnakinFB",
	smem_start:	VGA_START,
	smem_len:	VGA_SIZE,
	type:		FB_TYPE_PACKED_PIXELS,
	visual:		FB_VISUAL_TRUECOLOR,
	line_length:	400*2,
	accel:		FB_ACCEL_NONE,
};

static int
anakinfb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			u_int transp, struct fb_info *info)
{
	if (regno > 15)
		return 1;

	colreg[regno] = (red & 0xf800) | (green & 0xfc00 >> 5) |
			(blue & 0xf800 >> 11);
	return 0;
}

static struct fb_ops anakinfb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	gen_get_fix,
	fb_get_var:	gen_get_var,
	fb_set_var:	gen_set_var,
	fb_get_cmap:	gen_get_cmap,
	fb_set_cmap:	gen_set_cmap,
	fb_setcolreg:	anakinfb_setcolreg,
	fb_fillrect:	cfb_fillrect,
	fb_copyarea:	cfb_copyarea,
	fb_imageblit:	cfb_imageblit,
};

int __init
anakinfb_init(void)
{
	memset(&fb_info, 0, sizeof(struct fb_info));
	memset(&display, 0, sizeof(struct display));

	strcpy(fb_info.modename, anakinfb_fix.id);
	fb_info.node = NODEV;
	fb_info.currcon = -1;
	fb_info.flags = FBINFO_FLAG_DEFAULT;
	fb_info.fbops = &anakinfb_ops;
	fb_info.var = anakinfb_var;
	fb_info.fix = anakinfb_fix;
	fb_info.disp = &display;
	strcpy(fb_info.fontname, "VGA8x16");
	fb_info.changevar = NULL;
	fb_info.switch_con = gen_switch_con;
	fb_info.updatevar = gen_update_var;
	if (!(request_mem_region(VGA_START, VGA_SIZE, "vga")))
		return -ENOMEM;
	if (fb_info.screen_base = ioremap(VGA_START, VGA_SIZE)) {
		release_mem_region(VGA_START, VGA_SIZE);
		return -EIO;
	}

	fb_alloc_cmap(&fb_info.cmap, 16, 0);
	gen_set_disp(-1, &fb_info);

	if (register_framebuffer(&fb_info) < 0) {
		iounmap(display.screen_base);
		release_mem_region(VGA_START, VGA_SIZE);
		return -EINVAL;
	}

	MOD_INC_USE_COUNT;
	return 0;
}

MODULE_AUTHOR("Tak-Shing Chan <chan@aleph1.co.uk>");
MODULE_DESCRIPTION("Anakin framebuffer driver");
MODULE_SUPPORTED_DEVICE("fb");
