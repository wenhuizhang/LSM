/*
 * linux/drivers/video/skeletonfb.c -- Skeleton for a frame buffer device
 *
 *  Modified to new api Jan 2001 by James Simmons (jsimmons@transvirtual.com)
 *
 *  Created 28 Dec 1997 by Geert Uytterhoeven
 *
 *
 *  I have started rewriting this driver as a example of the upcoming new API
 *  The primary goal is to remove the console code from fbdev and place it
 *  into fbcon.c. This reduces the code and makes writing a new fbdev driver
 *  easy since the author doesn't need to worry about console internals. It
 *  also allows the ability to run fbdev without a console/tty system on top 
 *  of it. 
 *
 *  First the roles of struct fb_info and struct display have changed. Struct
 *  display will go away. The way the the new framebuffer console code will
 *  work is that it will act to translate data about the tty/console in 
 *  struct vc_data to data in a device independent way in struct fb_info. Then
 *  various functions in struct fb_ops will be called to store the device 
 *  dependent state in the par field in struct fb_info and to change the 
 *  hardware to that state. This allows a very clean seperation of the fbdev
 *  layer from the console layer. It also allows one to use fbdev on its own
 *  which is a bounus for embedded devices. The reason this approach works is  
 *  for each framebuffer device when used as a tty/console device is allocated
 *  a set of virtual terminals to it. Only one virtual terminal can be active 
 *  per framebuffer device. We already have all the data we need in struct 
 *  vc_data so why store a bunch of colormaps and other fbdev specific data
 *  per virtual terminal. 
 *
 *  As you can see doing this makes the con parameter pretty much useless
 *  for struct fb_ops functions, as it should be. Also having struct  
 *  fb_var_screeninfo and other data in fb_info pretty much eliminates the 
 *  need for get_fix and get_var. Once all drivers use the fix, var, and cmap
 *  fbcon can be written around these fields. This will also eliminate the
 *  need to regenerate struct fb_var_screeninfo, struct fb_fix_screeninfo
 *  struct fb_cmap every time get_var, get_fix, get_cmap functions are called
 *  as many drivers do now. 
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>

    /*
     *  This is just simple sample code.
     *
     *  No warranty that it actually compiles.
     *  Even less warranty that it actually works :-)
     */

/*
 *  If your driver supports multiple boards, you should make the  
 *  below data types arrays, or allocate them dynamically (using kmalloc()). 
 */ 

/* 
 * This structure defines the hardware state of the graphics card. Normally
 * you place this in a header file in linux/include/video. This file usually
 * also includes register information. That allows other driver subsystems
 * and userland applications the ability to use the same header file to 
 * avoid duplicate work and easy porting of software. 
 */
struct xxx_par;

/*
 * Here we define the default structs fb_fix_screeninfo and fb_var_screeninfo
 * if we don't use modedb. If we do use modedb see xxxfb_init how to use it
 * to get a fb_var_screeninfo. Otherwise define a default var as well. 
 */
static struct fb_fix_screeninfo xxxfb_fix __initdata = {
	id:		"FB's name", 
	type:		FB_TYPE_PACKED_PIXELS,
	visual:		FB_VISUAL_PSEUDOCOLOR,
	xpanstep:	1,
	ypanstep:	1,
	ywrapstep:	1, 
	accel:		FB_ACCEL_NONE,
};

    /*
     * 	Modern graphical hardware not only supports pipelines but some 
     *  also support multiple monitors where each display can have its  
     *  its own unique data. In this case each display could be  
     *  represented by a seperate framebuffer device thus a seperate 
     *  struct fb_info. Now the struct xxx_par represents the graphics
     *  hardware state thus only one exist per card. In this case the 
     *  struct xxx_par for each graphics card would be shared between 
     *  every struct fb_info that represents a framebuffer on that card. 
     *  This allows when one display changes it video resolution (info->var) 
     *  the other displays know instantly. Each display can always be
     *  aware of the entire hardware state that affects it because they share
     *  the same xxx_par struct. The other side of the coin is multiple
     *  graphics cards that pass data around until it is finally displayed
     *  on one monitor. Such examples are the voodoo 1 cards and high end
     *  NUMA graphics servers. For this case we have a bunch of pars, each
     *  one that represents a graphics state, that belong to one struct 
     *  fb_info. Their you would want to have *par point to a array of device
     *  states and have each struct fb_ops function deal with all those 
     *  states. I hope this covers every possible hardware design. If not
     *  feel free to send your ideas at jsimmons@users.sf.net 
     */

    /*
     *  If your driver supports multiple boards or it supports multiple 
     *  framebuffers, you should make these arrays, or allocate them 
     *  dynamically (using kmalloc()). 
     */ 
static struct fb_info info;

    /* 
     * Each one represents the a state of the hardware. Most hardware have 
     * just one hardware state. These here represent the default state(s). 
     */
static struct xxx_par __initdata current_par;

    /* To go away in the near future */ 
static struct display disp;

int xxxfb_init(void);
int xxxfb_setup(char*);

/**
 *      xxxfb_check_var - Optional function. Validates a var passed in. 
 *      @var: frame buffer variable screen structure
 *      @info: frame buffer structure that represents a single frame buffer 
 *
 *	Checks to see if the hardware supports the state requested by
 *	var passed in. This function does not alter the hardware state!!! 
 *	This means the data stored in struct fb_info and struct xxx_par do 
 *      not change. This includes the var inside of struct fb_info. 
 *	Do NOT change these. This function can be called on its own if we
 *	intent to only test a mode and not actually set it. The stuff in 
 *	modedb.c is a example of this. If the var passed in is slightly 
 *	off by what the hardware can support then we alter the var PASSED in
 *	to what we can do. If the hardware doesn't support mode change 
 * 	a -EINVAL will be returned by the upper layers. You don't need to 
 *	implement this function then.
 *
 *	Returns negative errno on error, or zero on success.
 */
static int xxxfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
    const struct xxx_par *par = (const struct xxx_par *) info->par;
    /* ... */
    return 0;	   	
}

/**
 *      xxxfb_set_par - Optional function. Alters the hardware state.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *	Using the fb_var_screeninfo in fb_info we set the resolution of the
 *	this particular framebuffer. This function alters the par AND the
 *	fb_fix_screeninfo stored in fb_info. It doesn't not alter var in 
 *	fb_info since we are using that data. This means we depend on the
 *	data in var inside fb_info to be supported by the hardware. 
 *	xxxfb_check_var is always called before xxxfb_set_par to ensure this.
 *
 */
static int xxxfb_set_par(struct fb_info *info)
{
    struct xxx_par *par = (struct xxx_par *) info->par;
    /* ... */
    return 0;	
}

/**
 *  	xxxfb_setcolreg - Optional function. Sets a color register.
 *      @regno: boolean, 0 copy local, 1 get_user() function
 *      @red: frame buffer colormap structure
 *	@green: The green value which can be up to 16 bits wide 
 *	@blue:  The blue value which can be up to 16 bits wide.
 *	@transp: If supported the alpha value which can be up to 16 bits wide.	
 *      @info: frame buffer info structure
 * 
 *  	Set a single color register. The values supplied have a 16 bit
 *  	magnitude which needs to be scaled in this function for the hardware. 
 *	Things to take into consideration are how many color registers, if
 *	any, are supported with the current color visual. With truecolor mode
 *	no color palettes are supported. Here a psuedo palette is created 
 *	which we store the value in pseudo_palette in struct fb_info. For
 *	pseudocolor mode we have a limited color palette. To deal with this
 *	we can program what color is displayed for a particular pixel value.
 *	DirectColor is similar in that we can program each color field. If
 *	we have a static colormap we don't need to implement this function. 
 * 
 *	Returns negative errno on error, or zero on success.
 */
static int xxxfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   const struct fb_info *info)
{
    if (regno >= 256)  /* no. of hw registers */
       return 1;
    /*
     * Program hardware... do anything you want with transp
     */

    /* grayscale works only partially under directcolor */
    if (info->var.grayscale) {
       /* grayscale = 0.30*R + 0.59*G + 0.11*B */
       red = green = blue = (red * 77 + green * 151 + blue * 28) >> 8;
    }

    /* Directcolor:
     *   var->{color}.offset contains start of bitfield
     *   var->{color}.length contains length of bitfield
     *   {hardwarespecific} contains width of DAC
     *   cmap[X] is programmed to (X << red.offset) | (X << green.offset) | (X << blue.offset)
     *   RAMDAC[X] is programmed to (red, green, blue)
     *
     * Pseudocolor:
     *    uses offset = 0 && length = DAC register width.
     *    var->{color}.offset is 0
     *    var->{color}.length contains widht of DAC
     *    cmap is not used
     *    DAC[X] is programmed to (red, green, blue)
     * Truecolor:
     *    does not use RAMDAC (usually has 3 of them).
     *    var->{color}.offset contains start of bitfield
     *    var->{color}.length contains length of bitfield
     *    cmap is programmed to (red << red.offset) | (green << green.offset) |
     *                      (blue << blue.offset) | (transp << transp.offset)
     *    RAMDAC does not exist
     */
#define CNVT_TOHW(val,width) ((((val)<<(width))+0x7FFF-(val))>>16)
    switch (info->fix.visual) {
       case FB_VISUAL_TRUECOLOR:
       case FB_VISUAL_PSEUDOCOLOR:
               red = CNVT_TOHW(red, info->var.red.length);
               green = CNVT_TOHW(green, info->var.green.length);
               blue = CNVT_TOHW(blue, info->var.blue.length);
               transp = CNVT_TOHW(transp, info->var.transp.length);
               break;
       case FB_VISUAL_DIRECTCOLOR:
	       /* example here assumes 8 bit DAC. Might be different 
		* for your hardware */	
               red = CNVT_TOHW(red, 8);       
               green = CNVT_TOHW(green, 8);
               blue = CNVT_TOHW(blue, 8);
               /* hey, there is bug in transp handling... */
               transp = CNVT_TOHW(transp, 8);
               break;
    }
#undef CNVT_TOHW
    /* Truecolor has hardware independent palette */
    if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
       u32 v;

       if (regno >= 16)
           return 1;

       v = (red << info->var.red.offset) |
           (green << info->var.green.offset) |
           (blue << info->var.blue.offset) |
           (transp << info->var.transp.offset);

       switch (info->var.bits_per_pixel) {
		case 8:
			/* Yes some hand held devices have this. */ 
           		((u8*)(info->pseudo_palette))[regno] = v;
			break;	
   		case 16:
           		((u16*)(info->pseudo_palette))[regno] = v;
			break;
		case 24:
		case 32:	
           		((u32*)(info->pseudo_palette))[regno] = v;
			break;
       }
       return 0;
    }
    /* ... */
    return 0;
}

/**
 *      xxxfb_pan_display - NOT a required function. Pans the display.
 *      @var: frame buffer variable screen structure
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *	Pan (or wrap, depending on the `vmode' field) the display using the
 *  	`xoffset' and `yoffset' fields of the `var' structure.
 *  	If the values don't fit, return -EINVAL.
 *
 *      Returns negative errno on error, or zero on success.
 *
 */
static int xxxfb_pan_display(struct fb_var_screeninfo *var, int con,
			     const struct fb_info *info)
{
    /* ... */
    return 0;
}

/**
 *      xxxfb_blank - NOT a required function. Blanks the display.
 *      @blank_mode: the blank mode we want. 
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *      Blank the screen if blank_mode != 0, else unblank. Return 0 if
 *      blanking succeeded, != 0 if un-/blanking failed due to e.g. a 
 *      video mode which doesn't support it. Implements VESA suspend
 *      and powerdown modes on hardware that supports disabling hsync/vsync:
 *      blank_mode == 2: suspend vsync
 *      blank_mode == 3: suspend hsync
 *      blank_mode == 4: powerdown
 *
 *      Returns negative errno on error, or zero on success.
 *
 */
static int xxxfb_blank(int blank_mode, const struct fb_info *info)
{
    /* ... */
    return 0;
}

/* ------------ Accelerated Functions --------------------- */

/*
 * We provide our own functions if we have hardware acceleration
 * or non packed pixel format layouts. If we have no hardware 
 * acceleration, we use a generic unaccelerated function. If using
 * a pack pixel format just use the functions in cfb*.c. Each file 
 * has one of the three different accel functions we support. You   
 * can use these functions as fallbacks if hardware unsupported
 * action is requested. Also if you have non pack pixel modes and
 * non accelerated cards you have to provide your own functions.
 */

/**
 *      xxxfb_fillrect - REQUIRED function. Can use generic routines if 
 *		 	 non acclerated hardware and packed pixel based.
 *			 Draws a rectangle on the screen.		
 *
 *      @info: frame buffer structure that represents a single frame buffer
 *	@x1: The x and y corrdinates of the upper left hand corner of the 
 *	@y1: area we want to draw to. 
 *	@width: How wide the rectangle is we want to draw.
 *	@height: How tall the rectangle is we want to draw.
 *	@color:	The color to fill in the rectangle with. 
 *	@rop: The rater operation. We can draw the rectangle with a COPY
 *	      of XOR which provides erasing effect. 
 *
 *	This drawing operation places/removes a retangle on the screen 
 *	depending on the rastering operation with the value of color which
 *	is in the current color depth format.
 */
void xxxfb_fillrect(struct fb_info *p, struct fb_fillrect *region)
{
}

/**
 *      xxxfb_copyarea - REQUIRED function. Can use generic routines if
 *                       non acclerated hardware and packed pixel based.
 *                       Copies on area of the screen to another area.
 *
 *      @info: frame buffer structure that represents a single frame buffer
 *      @sx: The x and y corrdinates of the upper left hand corner of the
 *      @sy: source area on the screen.
 *      @width: How wide the rectangle is we want to copy.
 *      @height: How tall the rectangle is we want to copy.
 *      @dx: The x and y coordinates of the destination area on the screen.
 *
 *      This drawing operation copies a rectangular area from one area of the
 *	screen to another area.
 */
void xxxfb_copyarea(struct fb_info *p, struct fb_copyarea *area) 
{
}

/**
 *      xxxfb_imageblit - REQUIRED function. Can use generic routines if
 *                        non acclerated hardware and packed pixel based.
 *                        Copies a image from system memory to the screen. 
 *
 *      @info: frame buffer structure that represents a single frame buffer
 *	@image:	structure defining the image.
 *
 *      This drawing operation draws a image on the screen. It can be a 
 *	mono image (needed for font handling) or a color image (needed for
 *	tux). 
 */
void xxxfb_imageblit(struct fb_info *p, struct fb_image *image) 
{
}

/* ------------ Hardware Independent Functions ------------ */

    /*
     *  Initialization
     */

int __init xxxfb_init(void)
{
    int cmap_len, retval;	
   
    /* 
     * Here we set the screen_base to the vitrual memory address
     * for the framebuffer. Usually we obtain the resource address
     * from the bus layer and then translate it to virtual memory
     * space via ioremap. Consult ioport.h. 
     */
    info.screen_base = framebuffer_virtual_memory;	
    info.node = NODEV;
    info.fbops = &xxxfb_ops;
    info.fix = xxxfb_fix;
    info.pseudo_palette = pseudo_palette;
    info.flags = FBINFO_FLAG_DEFAULT;
    info.par = current_par;

    /* The following has to be set but in th efuture will go away */
    strcpy(info.modename, xxxfb_fix.id);
    info.changevar = NULL;
    info.currcon = -1;
    info.disp = &disp; 			
    info.switch_con = gen_switch;
    info.updatevar = gen_update_var;
    

    /*
     * This should give a reasonable default video mode. The following is
     * done when we can set a video mode. 
     */
    if (!mode_option)
	mode_option = "640x480@60";	 	

    retval = fb_find_mode(&info.var, &info, mode_option, NULL, 0, NULL, 8);
  
    if (!retval || retval == 4)
	return -EINVAL;			

    /* This has to been done !!! */	
    fb_alloc_cmap(&info.cmap, cmap_len, 0);
	
    /* 
     * The following is done in the case of having hardware with a static 
     * mode. If we are setting the mode ourselves we don't call this. 
     */	
    info.var = xxxfb_var;
    gen_set_disp(-1, &fb_info);
	
    if (register_framebuffer(&info) < 0)
	return -EINVAL;
    printk(KERN_INFO "fb%d: %s frame buffer device\n", GET_FB_IDX(info.node),
	   info.fix.id);
    return 0;
}

    /*
     *  Cleanup
     */

static void __exit xxxfb_cleanup(void)
{
    /*
     *  If your driver supports multiple boards, you should unregister and
     *  clean up all instances.
     */

    unregister_framebuffer(info);
    /* ... */
}

    /*
     *  Setup
     */

/* 
 * Only necessary if your driver takes special options,
 * otherwise we fall back on the generic fb_setup().
 */
int __init xxxfb_setup(char *options)
{
    /* Parse user speficied options (`video=xxxfb:') */
}


/* ------------------------------------------------------------------------- */

    /*
     *  Frame buffer operations
     */

/* If all you need is that - just don't define ->fb_open */
static int xxxfb_open(const struct fb_info *info, int user)
{
    return 0;
}

/* If all you need is that - just don't define ->fb_release */
static int xxxfb_release(const struct fb_info *info, int user)
{
    return 0;
}

static struct fb_ops xxxfb_ops = {
	owner:		THIS_MODULE,
	fb_open:	xxxfb_open,    /* only if you need it to do something */
	fb_release:	xxxfb_release, /* only if you need it to do something */
	/* Stuff to go away. Use generic functions for now */
	fb_get_fix:	gen_get_fix,
	fb_get_var:	gen_get_var,
	fb_set_var:	gen_set_var,	
	fb_get_cmap:	gen_get_cmap,
	fb_set_cmap:	gen_set_cmap,

	fb_check_var:	xxxfb_check_var,
	fb_set_par:	xxxfb_set_par,	   /* optional */	
	fb_setcolreg:	xxxfb_setcolreg,
	fb_blank:	xxxfb_blank,	   /* optional */
	fb_pan_display:	xxxfb_pan_display, /* optional */	
	fb_fillrect:	xxxfb_fillrect,  
	fb_copyarea:	xxxfb_copyarea,  
	fb_imageblit:	xxxfb_imageblit, 
	fb_ioctl:       xxxfb_ioctl,     /* optional */
	fb_mmap:	xxxfb_mmap,      /* optional */	
};

/* ------------------------------------------------------------------------- */


    /*
     *  Modularization
     */

#ifdef MODULE
module_init(xxxfb_init);
#endif 
module_exit(xxxfb_cleanup);

MODULE_LICENSE("GPL");
