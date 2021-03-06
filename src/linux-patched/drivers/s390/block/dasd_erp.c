/*
 * File...........: linux/drivers/s390/block/dasd.c
 * Author(s)......: Holger Smolinski <Holger.Smolinski@de.ibm.com>
 *		    Horst Hummel <Horst.Hummel@de.ibm.com>
 *		    Carsten Otte <Cotte@de.ibm.com>
 *		    Martin Schwidefsky <schwidefsky@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * (C) IBM Corporation, IBM Deutschland Entwicklung GmbH, 1999-2001
 *
 * 05/04/02 split from dasd.c, code restructuring.
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/init.h>

#include <asm/debug.h>
#include <asm/ebcdic.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

/* This is ugly... */
#define PRINTK_HEADER "dasd_erp:"

#include "dasd_int.h"

dasd_ccw_req_t *
dasd_alloc_erp_request(char *magic, int cplength, int datasize,
		       dasd_device_t * device)
{
	unsigned long flags;
	dasd_ccw_req_t *cqr;
	char *data;
	int size;

	/* Sanity checks */
	if ( magic == NULL || datasize > PAGE_SIZE ||
	     (cplength*sizeof(ccw1_t)) > PAGE_SIZE)
		BUG();
	debug_text_event ( dasd_debug_area, 1, "ALLC");
	debug_text_event ( dasd_debug_area, 1, magic);
	debug_int_event ( dasd_debug_area, 1, cplength);
	debug_int_event ( dasd_debug_area, 1, datasize);

	size = (sizeof(dasd_ccw_req_t) + 7L) & -8L;
	if (cplength > 0)
		size += cplength * sizeof(ccw1_t);
	if (datasize > 0)
		size += datasize;
	spin_lock_irqsave(&device->mem_lock, flags);
	cqr = (dasd_ccw_req_t *) dasd_alloc_chunk(&device->erp_chunks, size);
	spin_unlock_irqrestore(&device->mem_lock, flags);
	if (cqr == NULL)
		return ERR_PTR(-ENOMEM);
	memset(cqr, 0, sizeof(dasd_ccw_req_t));
	data = (char *) cqr + ((sizeof(dasd_ccw_req_t) + 7L) & -8L);
	cqr->cpaddr = NULL;
	if (cplength > 0) {
		cqr->cpaddr = (ccw1_t *) data;
		data += cplength*sizeof(ccw1_t);
		memset(cqr->cpaddr, 0, cplength*sizeof(ccw1_t));
	}
	cqr->data = NULL;
	if (datasize > 0) {
		cqr->data = data;
 		memset(cqr->data, 0, datasize);
	}
	strncpy((char *) &cqr->magic, magic, 4);
	ASCEBC((char *) &cqr->magic, 4);
	atomic_inc(&device->ref_count);
	return cqr;
}

void
dasd_free_erp_request(dasd_ccw_req_t * cqr, dasd_device_t * device)
{
	unsigned long flags;

	if (cqr->dstat != NULL)
		kfree(cqr->dstat);
	debug_text_event(dasd_debug_area, 1, "FREE");
	debug_int_event(dasd_debug_area, 1, (long) cqr);
	spin_lock_irqsave(&device->mem_lock, flags);
	dasd_free_chunk(&device->erp_chunks, cqr);
	spin_unlock_irqrestore(&device->mem_lock, flags);
	atomic_dec(&device->ref_count);
}

/*
 * DESCRIPTION
 *   sets up the default-ERP dasd_ccw_req_t, namely one, which performs a TIC
 *   to the original channel program with a retry counter of 16
 *
 * PARAMETER
 *   cqr		failed CQR
 *
 * RETURN VALUES
 *   erp		CQR performing the ERP
 */
dasd_ccw_req_t *
dasd_default_erp_action(dasd_ccw_req_t * cqr)
{
	dasd_device_t *device;
	dasd_ccw_req_t *erp;

	MESSAGE(KERN_DEBUG, "%s", "Default ERP called... ");
	device = cqr->device;
	erp = dasd_alloc_erp_request((char *) &cqr->magic, 1, 0, device);
	if (IS_ERR(erp)) {
		DEV_MESSAGE(KERN_ERR, device, "%s",
			    "Unable to allocate request for default ERP");
		cqr->status = DASD_CQR_FAILED;
		cqr->stopclk = get_clock();
		return cqr;
	}

	erp->cpaddr->cmd_code = CCW_CMD_TIC;
	erp->cpaddr->cda = (__u32) (addr_t) cqr->cpaddr;
	erp->function = dasd_default_erp_action;
	erp->refers = cqr;
	erp->device = device;
	erp->magic = cqr->magic;
	erp->retries = 16;
	erp->status = DASD_CQR_FILLED;

	list_add(&erp->list, &device->ccw_queue);
	erp->status = DASD_CQR_QUEUED;

	return erp;

}				/* end dasd_default_erp_action */

/*
 * DESCRIPTION
 *   Frees all ERPs of the current ERP Chain and set the status
 *   of the original CQR either to DASD_CQR_DONE if ERP was successful
 *   or to DASD_CQR_FAILED if ERP was NOT successful.
 *   NOTE: This function is only called if no discipline postaction
 *	   is available
 *
 * PARAMETER
 *   erp		current erp_head
 *
 * RETURN VALUES
 *   cqr		pointer to the original CQR
 */
dasd_ccw_req_t *
dasd_default_erp_postaction(dasd_ccw_req_t * cqr)
{
	dasd_device_t *device;
	int success;

	if (cqr->refers == NULL || cqr->function == NULL)
		BUG();

	device = cqr->device;
	success = cqr->status == DASD_CQR_DONE;

	/* free all ERPs - but NOT the original cqr */
	while (cqr->refers != NULL) {
		dasd_ccw_req_t *refers;

		refers = cqr->refers;
		/* remove the request from the device queue */
		list_del(&cqr->list);
		/* free the finished erp request */
		dasd_free_erp_request(cqr, device);
		cqr = refers;
	}

	/* set corresponding status to original cqr */
	if (success)
		cqr->status = DASD_CQR_DONE;
	else {
		cqr->status = DASD_CQR_FAILED;
		cqr->stopclk = get_clock();
	}

	return cqr;

}				/* end default_erp_postaction */

/*
 * Print the hex dump of the memory used by a request. This includes
 * all error recovery ccws that have been chained in from of the 
 * real request.
 */
static inline void
hex_dump_memory(dasd_device_t *device, void *data, int len)
{
	int *pint;

	pint = (int *) data;
	while (len > 0) {
		DEV_MESSAGE(KERN_ERR, device, "%p: %08x %08x %08x %08x",
			    pint, pint[0], pint[1], pint[2], pint[3]);
		pint += 4;
		len -= 16;
	}
}

void
dasd_log_ccw(dasd_ccw_req_t * cqr, int caller, __u32 cpa)
{
	dasd_device_t *device;
	dasd_ccw_req_t *lcqr;
	ccw1_t *ccw;
	int cplength;

	device = cqr->device;
	/* dump sense data */
	if (device->discipline && device->discipline->dump_sense)
		device->discipline->dump_sense(device, cqr);

	/* log the channel program */
	for (lcqr = cqr; lcqr != NULL; lcqr = lcqr->refers) {
		DEV_MESSAGE(KERN_ERR, device,
			    "(%s) ERP chain report for req: %p",
			    caller == 0 ? "EXAMINE" : "ACTION", lcqr);
		hex_dump_memory(device, lcqr, sizeof(dasd_ccw_req_t));

		cplength = 1;
		ccw = lcqr->cpaddr;
		while (ccw++->flags & (CCW_FLAG_DC | CCW_FLAG_CC))
			cplength++;

		if (cplength > 40) {	/* log only parts of the CP */
			DEV_MESSAGE(KERN_ERR, device, "%s",
				    "Start of channel program:");
			hex_dump_memory(device, lcqr->cpaddr,
					40*sizeof(ccw1_t));

			DEV_MESSAGE(KERN_ERR, device, "%s",
				    "End of channel program:");
			hex_dump_memory(device, lcqr->cpaddr + cplength - 10,
					10*sizeof(ccw1_t));
		} else {	/* log the whole CP */
			DEV_MESSAGE(KERN_ERR, device, "%s",
				    "Channel program (complete):");
			hex_dump_memory(device, lcqr->cpaddr,
					cplength*sizeof(ccw1_t));
		}

		if (lcqr != cqr)
			continue;

		/*
		 * Log bytes arround failed CCW but only if we did
		 * not log the whole CP of the CCW is outside the
		 * logged CP. 
		 */
		if (cplength > 40 ||
		    ((addr_t) cpa < (addr_t) lcqr->cpaddr &&
		     (addr_t) cpa > (addr_t) (lcqr->cpaddr + cplength + 4))) {
			
			DEV_MESSAGE(KERN_ERR, device,
				    "Failed CCW (%p) (area):",
				    (void *) (long) cpa);
			hex_dump_memory(device, cqr->cpaddr - 10,
					20*sizeof(ccw1_t));
		}
	}

}				/* end log_erp_chain */

EXPORT_SYMBOL(dasd_default_erp_action);
EXPORT_SYMBOL(dasd_default_erp_postaction);
EXPORT_SYMBOL(dasd_alloc_erp_request);
EXPORT_SYMBOL(dasd_free_erp_request);
EXPORT_SYMBOL(dasd_log_ccw);
