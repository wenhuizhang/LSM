/*
 *	Low-Level PCI Support for PC
 *
 *	(c) 1999--2000 Martin Mares <mj@ucw.cz>

 Note: on x86-64 there is no PCI BIOS so there is no way to sort in the 
 same order as 32bit Linux. This could cause grief for dualbooting because
 devices may wander.  May want to use ACPI for sorting eventually. 

 */

#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ioport.h>

#include <asm/segment.h>
#include <asm/io.h>
#include <asm/smp.h>

#include "pci.h"

unsigned int pci_probe = PCI_PROBE_CONF1 | PCI_PROBE_CONF2;

int pcibios_last_bus = 0xfe; /* XXX */
struct pci_bus *pci_root_bus = NULL;
struct pci_ops *pci_root_ops = NULL;

int (*pci_config_read)(int seg, int bus, int dev, int fn, int reg, int len, u32 *value) = NULL;
int (*pci_config_write)(int seg, int bus, int dev, int fn, int reg, int len, u32 value) = NULL;

/*
 * legacy, numa, and acpi all want to call pcibios_scan_root
 * from their initcalls. This flag prevents that.
 */
int pcibios_scanned;

/*
 * This interrupt-safe spinlock protects all accesses to PCI
 * configuration space.
 */
spinlock_t pci_config_lock = SPIN_LOCK_UNLOCKED;

/*
 * Several buggy motherboards address only 16 devices and mirror
 * them to next 16 IDs. We try to detect this `feature' on all
 * primary buses (those containing host bridges as they are
 * expected to be unique) and remove the ghost devices.
 */

static void __devinit pcibios_fixup_ghosts(struct pci_bus *b)
{
	struct list_head *ln, *mn;
	struct pci_dev *d, *e;
	int mirror = PCI_DEVFN(16,0);
	int seen_host_bridge = 0;
	int i;

	DBG("PCI: Scanning for ghost devices on bus %d\n", b->number);
	for (ln=b->devices.next; ln != &b->devices; ln=ln->next) {
		d = pci_dev_b(ln);
		if ((d->class >> 8) == PCI_CLASS_BRIDGE_HOST)
			seen_host_bridge++;
		for (mn=ln->next; mn != &b->devices; mn=mn->next) {
			e = pci_dev_b(mn);
			if (e->devfn != d->devfn + mirror ||
			    e->vendor != d->vendor ||
			    e->device != d->device ||
			    e->class != d->class)
				continue;
			for(i=0; i<PCI_NUM_RESOURCES; i++)
				if (e->resource[i].start != d->resource[i].start ||
				    e->resource[i].end != d->resource[i].end ||
				    e->resource[i].flags != d->resource[i].flags)
					continue;
			break;
		}
		if (mn == &b->devices)
			return;
	}
	if (!seen_host_bridge)
		return;
	printk(KERN_WARNING "PCI: Ignoring ghost devices on bus %02x\n", b->number);

	ln = &b->devices;
	while (ln->next != &b->devices) {
		d = pci_dev_b(ln->next);
		if (d->devfn >= mirror) {
			list_del(&d->global_list);
			list_del(&d->bus_list);
			kfree(d);
		} else
			ln = ln->next;
	}
}

/*
 *  Called after each bus is probed, but before its children
 *  are examined.
 */

void __devinit  pcibios_fixup_bus(struct pci_bus *b)
{
	pcibios_fixup_ghosts(b);
	pci_read_bridge_bases(b);
}


struct pci_bus * __devinit pcibios_scan_root(int busnum)
{
	struct list_head *list;
	struct pci_bus *bus;

	list_for_each(list, &pci_root_buses) {
		bus = pci_bus_b(list);
		if (bus->number == busnum) {
			/* Already scanned */
			return bus;
		}
	}

	printk("PCI: Probing PCI hardware (bus %02x)\n", busnum);

	return pci_scan_bus(busnum, pci_root_ops, NULL);
}

static int __init pcibios_init(void)
{
	if (!pci_root_ops) {
		printk("PCI: System does not support PCI\n");
		return 0;
	}

	pcibios_resource_survey();

	/* may eventually need to do ACPI sort here. */
	return 0;
}

subsys_initcall(pcibios_init);

char * __devinit  pcibios_setup(char *str)
{
	if (!strcmp(str, "off")) {
		pci_probe = 0;
		return NULL;
	}
#ifdef CONFIG_PCI_DIRECT
	else if (!strcmp(str, "conf1")) {
		pci_probe = PCI_PROBE_CONF1 | PCI_NO_CHECKS;
		return NULL;
	}
	else if (!strcmp(str, "conf2")) {
		pci_probe = PCI_PROBE_CONF2 | PCI_NO_CHECKS;
		return NULL;
	}
#endif
#ifdef CONFIG_ACPI_PCI
	else if (!strcmp(str, "noacpi")) {
		pci_probe |= PCI_NO_ACPI_ROUTING;
		return NULL;
	}
#endif
	else if (!strcmp(str, "rom")) {
		pci_probe |= PCI_ASSIGN_ROMS;
		return NULL;
	} else if (!strcmp(str, "assign-busses")) {
		pci_probe |= PCI_ASSIGN_ALL_BUSSES;
		return NULL;
	} else if (!strcmp(str, "usepirqmask")) {
		pci_probe |= PCI_USE_PIRQ_MASK;
		return NULL;
	} else if (!strncmp(str, "irqmask=", 8)) {
		pcibios_irq_mask = simple_strtol(str+8, NULL, 0);
		return NULL;
	} else if (!strncmp(str, "lastbus=", 8)) {
		pcibios_last_bus = simple_strtol(str+8, NULL, 0);
		return NULL;
	}
	return str;
}

unsigned int pcibios_assign_all_busses(void)
{
	return (pci_probe & PCI_ASSIGN_ALL_BUSSES) ? 1 : 0;
}

int pcibios_enable_device(struct pci_dev *dev)
{
	int err;

	if ((err = pcibios_enable_resources(dev)) < 0)
		return err;

	return pcibios_enable_irq(dev);
}
