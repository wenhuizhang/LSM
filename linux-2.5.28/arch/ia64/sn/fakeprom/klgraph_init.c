/* $Id: klgraph_init.c,v 1.2 2001/12/05 16:58:41 jh Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */


/*
 * This is a temporary file that statically initializes the expected 
 * initial klgraph information that is normally provided by prom.
 */

#include <linux/types.h>
#include <linux/config.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/sn/sgi.h>
#include <asm/sn/io.h>
#include <asm/sn/driver.h>
#include <asm/sn/iograph.h>
#include <asm/param.h>
#include <asm/sn/pio.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/addrs.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/hcl_util.h>
#include <asm/sn/intr.h>
#include <asm/sn/xtalk/xtalkaddrs.h>
#include <asm/sn/klconfig.h>

#define SYNERGY_WIDGET          ((char *)0xc0000e0000000000)
#define SYNERGY_SWIZZLE         ((char *)0xc0000e0000000400)
#define HUBREG                  ((char *)0xc0000a0001e00000)
#define WIDGET0                 ((char *)0xc0000a0000000000)
#define WIDGET4                 ((char *)0xc0000a0000000004)

#define SYNERGY_WIDGET          ((char *)0xc0000e0000000000)
#define SYNERGY_SWIZZLE         ((char *)0xc0000e0000000400)
#define HUBREG                  ((char *)0xc0000a0001e00000)
#define WIDGET0                 ((char *)0xc0000a0000000000)

#define convert(a,b,c) temp = (u64 *)a; *temp = b; temp++; *temp = c
void
klgraph_init(void)
{

	u64	*temp;

	/*
	 * Initialize some hub/xbow registers that allows access to 
	 * Xbridge etc.  These are normally done in PROM.
	 */

        /* Write IOERR clear to clear the CRAZY bit in the status */
#ifdef CONFIG_IA64_SGI_SN1
        *(volatile uint64_t *)0xc0000a0001c001f8 = (uint64_t)0xffffffff;

        /* set widget control register...setting bedrock widget id to b */
        *(volatile uint64_t *)0xc0000a0001c00020 = (uint64_t)0x801b;

        /* set io outbound widget access...allow all */
        *(volatile uint64_t *)0xc0000a0001c00110 = (uint64_t)0xff01;

        /* set io inbound widget access...allow all */
        *(volatile uint64_t *)0xc0000a0001c00118 = (uint64_t)0xff01;

        /* set io crb timeout to max */
        *(volatile uint64_t *)0xc0000a0001c003c0 = (uint64_t)0xffffff;
        *(volatile uint64_t *)0xc0000a0001c003c0 = (uint64_t)0xffffff;

        /* set local block io permission...allow all */
        *(volatile uint64_t *)0xc0000a0001e04010 = (uint64_t)0xfffffffffffffff;

        /* clear any errors */
        /* clear_ii_error(); medusa should have cleared these */

        /* set default read response buffers in bridge */
        *(volatile u32 *)0xc0000a000f000280L = 0xba98;
        *(volatile u32 *)0xc0000a000f000288L = 0xba98;
#elif CONFIG_IA64_SGI_SN2
        *(volatile uint64_t *)0xc000000801c001f8 = (uint64_t)0xffffffff;

        /* set widget control register...setting bedrock widget id to a */
        *(volatile uint64_t *)0xc000000801c00020 = (uint64_t)0x801a;

        /* set io outbound widget access...allow all */
        *(volatile uint64_t *)0xc000000801c00110 = (uint64_t)0xff01;

        /* set io inbound widget access...allow all */
        *(volatile uint64_t *)0xc000000801c00118 = (uint64_t)0xff01;

        /* set io crb timeout to max */
        *(volatile uint64_t *)0xc000000801c003c0 = (uint64_t)0xffffff;
        *(volatile uint64_t *)0xc000000801c003c0 = (uint64_t)0xffffff;

        /* set local block io permission...allow all */
// [LB]        *(volatile uint64_t *)0xc000000801e04010 = (uint64_t)0xfffffffffffffff;

        /* clear any errors */
        /* clear_ii_error(); medusa should have cleared these */

        /* set default read response buffers in bridge */
// [PI]       *(volatile u32 *)0xc00000080f000280L = 0xba98;
// [PI]       *(volatile u32 *)0xc00000080f000288L = 0xba98;
#endif	/* CONFIG_IA64_SGI_SN1 */

	/*
	 * kldir entries initialization - mankato
	 */
	convert(0x8000000000002000, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002010, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002020, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002030, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002040, 0x434d5f53505f5357, 0x0000000000030000);
	convert(0x8000000000002050, 0x0000000000000000, 0x0000000000010000);
	convert(0x8000000000002060, 0x0000000000000001, 0x0000000000000000);
	convert(0x8000000000002070, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002080, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002090, 0x0000000000000000, 0x0000000000000000);
	convert(0x80000000000020a0, 0x0000000000000000, 0x0000000000000000);
	convert(0x80000000000020b0, 0x0000000000000000, 0x0000000000000000);
	convert(0x80000000000020c0, 0x434d5f53505f5357, 0x0000000000000000);
	convert(0x80000000000020d0, 0x0000000000002400, 0x0000000000000400);
	convert(0x80000000000020e0, 0x0000000000000001, 0x0000000000000000);
	convert(0x80000000000020f0, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002100, 0x434d5f53505f5357, 0x0000000000040000);
	convert(0x8000000000002110, 0x0000000000000000, 0xffffffffffffffff);
	convert(0x8000000000002120, 0x0000000000000001, 0x0000000000000000);
	convert(0x8000000000002130, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002140, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002150, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002160, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002170, 0x0000000000000000, 0x0000000000000000);
	convert(0x8000000000002180, 0x434d5f53505f5357, 0x0000000000020000);
	convert(0x8000000000002190, 0x0000000000000000, 0x0000000000010000);
	convert(0x80000000000021a0, 0x0000000000000001, 0x0000000000000000); 

	/*
	 * klconfig entries initialization - mankato
	 */
	convert(0x0000000000030000, 0x00000000beedbabe, 0x0000004800000000);
	convert(0x0000000000030010, 0x0003007000000018, 0x800002000f820178);
	convert(0x0000000000030020, 0x80000a000f024000, 0x800002000f800000);
	convert(0x0000000000030030, 0x0300fafa00012580, 0x00000000040f0000);
	convert(0x0000000000030040, 0x0000000000000000, 0x0003097000030070);
	convert(0x0000000000030050, 0x00030970000303b0, 0x0003181000033f70);
	convert(0x0000000000030060, 0x0003d51000037570, 0x0000000000038330);
	convert(0x0000000000030070, 0x0203110100030140, 0x0001000000000101);
	convert(0x0000000000030080, 0x0900000000000000, 0x000000004e465e67);
	convert(0x0000000000030090, 0x0003097000000000, 0x00030b1000030a40);
	convert(0x00000000000300a0, 0x00030cb000030be0, 0x000315a0000314d0);
	convert(0x00000000000300b0, 0x0003174000031670, 0x0000000000000000);
	convert(0x0000000000030100, 0x000000000000001a, 0x3350490000000000);
	convert(0x0000000000030110, 0x0000000000000037, 0x0000000000000000);
	convert(0x0000000000030140, 0x0002420100030210, 0x0001000000000101);
	convert(0x0000000000030150, 0x0100000000000000, 0xffffffffffffffff);
	convert(0x0000000000030160, 0x00030d8000000000, 0x0000000000030e50);
	convert(0x00000000000301c0, 0x0000000000000000, 0x0000000000030070);
	convert(0x00000000000301d0, 0x0000000000000025, 0x424f490000000000);
	convert(0x00000000000301e0, 0x000000004b434952, 0x0000000000000000);
	convert(0x0000000000030210, 0x00027101000302e0, 0x00010000000e4101);
	convert(0x0000000000030220, 0x0200000000000000, 0xffffffffffffffff);
	convert(0x0000000000030230, 0x00030f2000000000, 0x0000000000030ff0);
	convert(0x0000000000030290, 0x0000000000000000, 0x0000000000030140);
	convert(0x00000000000302a0, 0x0000000000000026, 0x7262490000000000);
	convert(0x00000000000302b0, 0x00000000006b6369, 0x0000000000000000);
	convert(0x00000000000302e0, 0x0002710100000000, 0x00010000000f3101);
	convert(0x00000000000302f0, 0x0500000000000000, 0xffffffffffffffff);
	convert(0x0000000000030300, 0x000310c000000000, 0x0003126000031190);
	convert(0x0000000000030310, 0x0003140000031330, 0x0000000000000000);
	convert(0x0000000000030360, 0x0000000000000000, 0x0000000000030140);
	convert(0x0000000000030370, 0x0000000000000029, 0x7262490000000000);
	convert(0x0000000000030380, 0x00000000006b6369, 0x0000000000000000);
	convert(0x0000000000030970, 0x0000000002010102, 0x0000000000000000);
	convert(0x0000000000030980, 0x000000004e465e67, 0xffffffff00000000);
	/* convert(0x00000000000309a0, 0x0000000000037570, 0x0000000100000000); */
	convert(0x00000000000309a0, 0x0000000000037570, 0xffffffff00000000);
	convert(0x00000000000309b0, 0x0000000000030070, 0x0000000000000000);
	convert(0x00000000000309c0, 0x000000000003f420, 0x0000000000000000);
	convert(0x0000000000030a40, 0x0000000002010125, 0x0000000000000000);
	convert(0x0000000000030a50, 0xffffffffffffffff, 0xffffffff00000000);
	convert(0x0000000000030a70, 0x0000000000037b78, 0x0000000000000000);
	convert(0x0000000000030b10, 0x0000000002010125, 0x0000000000000000);
	convert(0x0000000000030b20, 0xffffffffffffffff, 0xffffffff00000000);
	convert(0x0000000000030b40, 0x0000000000037d30, 0x0000000000000001);
	convert(0x0000000000030be0, 0x00000000ff010203, 0x0000000000000000);
	convert(0x0000000000030bf0, 0xffffffffffffffff, 0xffffffff000000ff);
	convert(0x0000000000030c10, 0x0000000000037ee8, 0x0100010000000200);
	convert(0x0000000000030cb0, 0x00000000ff310111, 0x0000000000000000);
	convert(0x0000000000030cc0, 0xffffffffffffffff, 0x0000000000000000);
	convert(0x0000000000030d80, 0x0000000002010104, 0x0000000000000000);
	convert(0x0000000000030d90, 0xffffffffffffffff, 0x00000000000000ff);
	convert(0x0000000000030db0, 0x0000000000037f18, 0x0000000000000000);
	convert(0x0000000000030dc0, 0x0000000000000000, 0x0003007000060000);
	convert(0x0000000000030de0, 0x0000000000000000, 0x0003021000050000);
	convert(0x0000000000030df0, 0x000302e000050000, 0x0000000000000000);
	convert(0x0000000000030e30, 0x0000000000000000, 0x000000000000000a);
	convert(0x0000000000030e50, 0x00000000ff00011a, 0x0000000000000000);
	convert(0x0000000000030e60, 0xffffffffffffffff, 0x0000000000000000);
	convert(0x0000000000030e80, 0x0000000000037fe0, 0x9e6e9e9e9e9e9e9e);
	convert(0x0000000000030e90, 0x000000000000bc6e, 0x0000000000000000);
	convert(0x0000000000030f20, 0x0000000002010205, 0x00000000d0020000);
	convert(0x0000000000030f30, 0xffffffffffffffff, 0x0000000e0000000e);
	convert(0x0000000000030f40, 0x000000000000000e, 0x0000000000000000);
	convert(0x0000000000030f50, 0x0000000000038010, 0x00000000000007ff);
	convert(0x0000000000030f70, 0x0000000000000000, 0x0000000022001077);
	convert(0x0000000000030fa0, 0x0000000000000000, 0x000000000003f4a8);
	convert(0x0000000000030ff0, 0x0000000000310120, 0x0000000000000000);
	convert(0x0000000000031000, 0xffffffffffffffff, 0xffffffff00000002);
	convert(0x0000000000031010, 0x000000000000000e, 0x0000000000000000);
	convert(0x0000000000031020, 0x0000000000038088, 0x0000000000000000);
	convert(0x00000000000310c0, 0x0000000002010205, 0x00000000d0020000);
	convert(0x00000000000310d0, 0xffffffffffffffff, 0x0000000f0000000f);
	convert(0x00000000000310e0, 0x000000000000000f, 0x0000000000000000);
	convert(0x00000000000310f0, 0x00000000000380b8, 0x00000000000007ff);
	convert(0x0000000000031120, 0x0000000022001077, 0x00000000000310a9);
	convert(0x0000000000031130, 0x00000000580211c1, 0x000000008009104c);
	convert(0x0000000000031140, 0x0000000000000000, 0x000000000003f4c0);
	convert(0x0000000000031190, 0x0000000000310120, 0x0000000000000000);
	convert(0x00000000000311a0, 0xffffffffffffffff, 0xffffffff00000003);
	convert(0x00000000000311b0, 0x000000000000000f, 0x0000000000000000);
	convert(0x00000000000311c0, 0x0000000000038130, 0x0000000000000000);
	convert(0x0000000000031260, 0x0000000000110106, 0x0000000000000000);
	convert(0x0000000000031270, 0xffffffffffffffff, 0xffffffff00000004);
	convert(0x0000000000031280, 0x000000000000000f, 0x0000000000000000);
	convert(0x00000000000312a0, 0x00000000ff110013, 0x0000000000000000);
	convert(0x00000000000312b0, 0xffffffffffffffff, 0xffffffff00000000);
	convert(0x00000000000312c0, 0x000000000000000f, 0x0000000000000000);
	convert(0x00000000000312e0, 0x0000000000110012, 0x0000000000000000);
	convert(0x00000000000312f0, 0xffffffffffffffff, 0xffffffff00000000);
	convert(0x0000000000031300, 0x000000000000000f, 0x0000000000000000);
	convert(0x0000000000031310, 0x0000000000038160, 0x0000000000000000);
	convert(0x0000000000031330, 0x00000000ff310122, 0x0000000000000000);
	convert(0x0000000000031340, 0xffffffffffffffff, 0xffffffff00000005);
	convert(0x0000000000031350, 0x000000000000000f, 0x0000000000000000);
	convert(0x0000000000031360, 0x0000000000038190, 0x0000000000000000);
	convert(0x0000000000031400, 0x0000000000310121, 0x0000000000000000);
	convert(0x0000000000031400, 0x0000000000310121, 0x0000000000000000);
	convert(0x0000000000031410, 0xffffffffffffffff, 0xffffffff00000006);
	convert(0x0000000000031420, 0x000000000000000f, 0x0000000000000000);
	convert(0x0000000000031430, 0x00000000000381c0, 0x0000000000000000);
	convert(0x00000000000314d0, 0x00000000ff010201, 0x0000000000000000);
	convert(0x00000000000314e0, 0xffffffffffffffff, 0xffffffff00000000);
	convert(0x0000000000031500, 0x00000000000381f0, 0x000030430000ffff);
	convert(0x0000000000031510, 0x000000000000ffff, 0x0000000000000000);
	convert(0x00000000000315a0, 0x00000020ff000201, 0x0000000000000000);
	convert(0x00000000000315b0, 0xffffffffffffffff, 0xffffffff00000001);
	convert(0x00000000000315d0, 0x0000000000038240, 0x00003f3f0000ffff);
	convert(0x00000000000315e0, 0x000000000000ffff, 0x0000000000000000);
	convert(0x0000000000031670, 0x00000000ff010201, 0x0000000000000000);
	convert(0x0000000000031680, 0xffffffffffffffff, 0x0000000100000002);
	convert(0x00000000000316a0, 0x0000000000038290, 0x000030430000ffff);
	convert(0x00000000000316b0, 0x000000000000ffff, 0x0000000000000000);
	convert(0x0000000000031740, 0x00000020ff000201, 0x0000000000000000);
	convert(0x0000000000031750, 0xffffffffffffffff, 0x0000000500000003);
	convert(0x0000000000031770, 0x00000000000382e0, 0x00003f3f0000ffff);
	convert(0x0000000000031780, 0x000000000000ffff, 0x0000000000000000);

	/*
	 * GDA initialization - mankato
	 */
	convert(0x8000000000002400, 0x0000000258464552, 0x000000000ead0000);
	convert(0x8000000000002480, 0xffffffff00010000, 0xffffffffffffffff);
	convert(0x8000000000002490, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x80000000000024a0, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x80000000000024b0, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x80000000000024c0, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x80000000000024d0, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x80000000000024e0, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x80000000000024f0, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x8000000000002500, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x8000000000002510, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x8000000000002520, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x8000000000002530, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x8000000000002540, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x8000000000002550, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x8000000000002560, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x8000000000002570, 0xffffffffffffffff, 0xffffffffffffffff);
	convert(0x8000000000002580, 0x000000000000ffff, 0x0000000000000000);
	
}

