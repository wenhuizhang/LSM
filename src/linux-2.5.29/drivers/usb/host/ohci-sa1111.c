/*
 * OHCI HCD (Host Controller Driver) for USB.
 *
 * (C) Copyright 1999 Roman Weissgaerber <weissg@vienna.at>
 * (C) Copyright 2000-2002 David Brownell <dbrownell@users.sourceforge.net>
 * (C) Hewlett-Packard Company
 * 
 * SA1111 Bus Glue
 *
 * Written by Christopher Hoover <ch@hpl.hp.com>
 * Based on fragments of previous driver by Rusell King et al.
 *
 * This file is licenced under the GPL.
 */
 
#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <asm/arch/assabet.h>
#include <asm/arch/badge4.h>
#include <asm/hardware/sa1111.h>

#ifndef CONFIG_SA1111
#error "This file is SA-1111 bus glue.  CONFIG_SA1111 must be defined."
#endif

/*-------------------------------------------------------------------------*/

static void sa1111_start_hc(void)
{
	unsigned int usb_rst = 0;

	printk(KERN_DEBUG __FILE__ 
	       ": starting SA-1111 OHCI USB Controller\n");

#ifdef CONFIG_SA1100_BADGE4
	if (machine_is_badge4()) {
		badge4_set_5V(BADGE4_5V_USB, 1);
	}
#endif

	if (machine_is_xp860() ||
	    machine_has_neponset() ||
	    machine_is_pfs168() ||
	    machine_is_badge4())
		usb_rst = USB_RESET_PWRSENSELOW | USB_RESET_PWRCTRLLOW;

	/*
	 * Configure the power sense and control lines.  Place the USB
	 * host controller in reset.
	 */
	USB_RESET = usb_rst | USB_RESET_FORCEIFRESET | USB_RESET_FORCEHCRESET;

	/*
	 * Now, carefully enable the USB clock, and take
	 * the USB host controller out of reset.
	 */
	SKPCR |= SKPCR_UCLKEN;
	udelay(11);
	USB_RESET = usb_rst;
}

static void sa1111_stop_hc(void)
{
	printk(KERN_DEBUG __FILE__ 
	       ": stopping SA-1111 OHCI USB Controller\n");

	/*
	 * Put the USB host controller into reset.
	 */
	USB_RESET |= USB_RESET_FORCEIFRESET | USB_RESET_FORCEHCRESET;

	/*
	 * Stop the USB clock.
	 */
	SKPCR &= ~SKPCR_UCLKEN;

#ifdef CONFIG_SA1100_BADGE4
	if (machine_is_badge4()) {
		/* Disable power to the USB bus */
		badge4_set_5V(BADGE4_5V_USB, 0);
	}
#endif
}


/*-------------------------------------------------------------------------*/

#if 0
static void dump_hci_status(const char *label)
{
	unsigned long status = USB_STATUS; 

	dbg ("%s USB_STATUS = { %s%s%s%s%s}", label,
	     ((status & USB_STATUS_IRQHCIRMTWKUP) ? "IRQHCIRMTWKUP " : ""),
	     ((status & USB_STATUS_IRQHCIBUFFACC) ? "IRQHCIBUFFACC " : ""),
	     ((status & USB_STATUS_NIRQHCIM) ? "" : "IRQHCIM "),
	     ((status & USB_STATUS_NHCIMFCLR) ? "" : "HCIMFCLR "),
	     ((status & USB_STATUS_USBPWRSENSE) ? "USBPWRSENSE " : ""));
}
#endif

static void usb_hcd_sa1111_hcim_irq (int irq, void *__hcd, struct pt_regs * r)
{
	//dump_hci_status("irq");

#if 0
	/* may work better this way -- need to investigate further */
	if (USB_STATUS & USB_STATUS_NIRQHCIM) {
		//dbg ("not normal HC interrupt; ignoring");
		return;
	}
#endif

	usb_hcd_irq(irq, __hcd, r);
}

/*-------------------------------------------------------------------------*/

void usb_hcd_sa1111_remove (struct usb_hcd *);

/* configure so an HC device and id are always provided */
/* always called with process context; sleeping is OK */


/**
 * usb_hcd_sa1111_probe - initialize SA-1111-based HCDs
 * Context: !in_interrupt()
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 *
 * Store this function in the HCD's struct pci_driver as probe().
 */
int usb_hcd_sa1111_probe (const struct hc_driver *driver, struct usb_hcd **hcd_out)
{
	int retval;
	struct usb_hcd *hcd = 0;

	if (!sa1111)
		return -ENODEV;

	if (!request_mem_region(_USB_OHCI_OP_BASE, 
				_USB_EXTENT, hcd_name)) {
		dbg("request_mem_region failed");
		return -EBUSY;
	}

	sa1111_start_hc();

	hcd = driver->hcd_alloc ();
	if (hcd == NULL){
		dbg ("hcd_alloc failed");
		retval = -ENOMEM;
		goto err1;
	}

	hcd->driver = (struct hc_driver *) driver;
	hcd->description = driver->description;
	hcd->irq = NIRQHCIM;
	hcd->regs = (void *) &USB_OHCI_OP_BASE;
	hcd->pdev = SA1111_FAKE_PCIDEV;

	set_irq_type(NIRQHCIM, IRQT_RISING);
	retval = request_irq (NIRQHCIM, usb_hcd_sa1111_hcim_irq, SA_INTERRUPT,
			      hcd->description, hcd);
	if (retval != 0) {
		dbg("request_irq failed");
		retval = -EBUSY;
		goto err2;
	}

	info ("%s (SA-1111) at 0x%p, irq %d\n",
	      hcd->description, hcd->regs, hcd->irq);

	usb_bus_init (&hcd->self);
	hcd->self.op = &usb_hcd_operations;
	hcd->self.hcpriv = (void *) hcd;
	hcd->self.bus_name = "sa1111";
	hcd->product_desc = "SA-1111 OHCI";

	INIT_LIST_HEAD (&hcd->dev_list);

	usb_register_bus (&hcd->self);

	if ((retval = driver->start (hcd)) < 0) 
	{
		usb_hcd_sa1111_remove(hcd);
		return retval;
	}

	*hcd_out = hcd;
	return 0;

 err2:
	if (hcd) driver->hcd_free(hcd);
 err1:
	sa1111_stop_hc();
	release_mem_region(_USB_OHCI_OP_BASE, _USB_EXTENT);
	return retval;
}


/* may be called without controller electrically present */
/* may be called with controller, bus, and devices active */

/**
 * usb_hcd_sa1111_remove - shutdown processing for SA-1111-based HCDs
 * @dev: USB Host Controller being removed
 * Context: !in_interrupt()
 *
 * Reverses the effect of usb_hcd_sa1111_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 *
 */
void usb_hcd_sa1111_remove (struct usb_hcd *hcd)
{
	struct usb_device	*hub;
	void *base;

	info ("remove: %s, state %x", hcd->self.bus_name, hcd->state);

	if (in_interrupt ()) BUG ();

	hub = hcd->self.root_hub;
	hcd->state = USB_STATE_QUIESCING;

	dbg ("%s: roothub graceful disconnect", hcd->self.bus_name);
	usb_disconnect (&hub);

	hcd->driver->stop (hcd);
	hcd->state = USB_STATE_HALT;

	free_irq (hcd->irq, hcd);

	usb_deregister_bus (&hcd->self);
	if (atomic_read (&hcd->self.refcnt) != 1)
		err (__FUNCTION__ ": %s, count != 1", hcd->self.bus_name);

	base = hcd->regs;
	hcd->driver->hcd_free (hcd);

	sa1111_stop_hc();
	release_mem_region(_USB_OHCI_OP_BASE, _USB_EXTENT);
}

/*-------------------------------------------------------------------------*/

static int __devinit
ohci_sa1111_start (struct usb_hcd *hcd)
{
	struct ohci_hcd	*ohci = hcd_to_ohci (hcd);
	int		ret;

	if (hcd->pdev) {
		ohci->hcca = pci_alloc_consistent (hcd->pdev,
				sizeof *ohci->hcca, &ohci->hcca_dma);
		if (!ohci->hcca)
			return -ENOMEM;
	}

        memset (ohci->hcca, 0, sizeof (struct ohci_hcca));
	if ((ret = ohci_mem_init (ohci)) < 0) {
		ohci_stop (hcd);
		return ret;
	}
	ohci->regs = hcd->regs;

	ohci->parent_dev = &sa1111->dev;

	if (hc_reset (ohci) < 0) {
		ohci_stop (hcd);
		return -ENODEV;
	}

	if (hc_start (ohci) < 0) {
		err ("can't start %s", ohci->hcd.self.bus_name);
		ohci_stop (hcd);
		return -EBUSY;
	}

#ifdef	DEBUG
	ohci_dump (ohci, 1);
#endif
	return 0;
}

/*-------------------------------------------------------------------------*/

static const struct hc_driver ohci_sa1111_hc_driver = {
	.description =		hcd_name,

	/*
	 * generic hardware linkage
	 */
	.irq =			ohci_irq,
	.flags =		HCD_USB11,

	/*
	 * basic lifecycle operations
	 */
	.start =		ohci_sa1111_start,
#ifdef	CONFIG_PM
	/* suspend:		ohci_sa1111_suspend,  -- tbd */
	/* resume:		ohci_sa1111_resume,   -- tbd */
#endif
	.stop =			ohci_stop,

	/*
	 * memory lifecycle (except per-request)
	 */
	.hcd_alloc =		ohci_hcd_alloc,
	.hcd_free =		ohci_hcd_free,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.free_config =		ohci_free_config,

	/*
	 * scheduling support
	 */
	.get_frame_number =	ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,
};

/*-------------------------------------------------------------------------*/

/* Only one SA-1111 ever exists. */
static struct usb_hcd *the_sa1111_hcd;

static int __init ohci_hcd_sa1111_init (void) 
{
	dbg (DRIVER_INFO " (SA-1111)");
	dbg ("block sizes: ed %d td %d",
		sizeof (struct ed), sizeof (struct td));

	the_sa1111_hcd = 0;
	return usb_hcd_sa1111_probe(&ohci_sa1111_hc_driver, &the_sa1111_hcd);
}
module_init (ohci_hcd_sa1111_init);


static void __exit ohci_hcd_sa1111_cleanup (void) 
{	
	if (the_sa1111_hcd) {
		usb_hcd_sa1111_remove(the_sa1111_hcd);
		the_sa1111_hcd = 0;
	}
}
module_exit (ohci_hcd_sa1111_cleanup);
