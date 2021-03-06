//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#include "pxa_TIMR.h"
#include "pxa_IC.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"



#define PXA_TIMR_BASE	0x40A00000UL
#define PXA_TIMR_SIZE	0x00010000UL


struct PxaTimr {

	struct SocIc *ic;
	
	uint32_t OSMR[4];	//Match Register 0-3
	uint32_t OSCR;		//Counter Register
	uint8_t OIER;		//Interrupt Enable
	uint8_t OWER;		//Watchdog enable
	uint8_t OSSR;		//Status Register
};


static void pxaTimrPrvRaiseLowerInts(struct PxaTimr *timr)
{
	socIcInt(timr->ic, PXA_I_TIMR0, (timr->OSSR & 1) != 0);
	socIcInt(timr->ic, PXA_I_TIMR1, (timr->OSSR & 2) != 0);
	socIcInt(timr->ic, PXA_I_TIMR2, (timr->OSSR & 4) != 0);
	socIcInt(timr->ic, PXA_I_TIMR3, (timr->OSSR & 8) != 0);
}

static void pxaTimrPrvCheckMatch(struct PxaTimr *timr, uint_fast8_t idx)
{
	uint_fast8_t v = 1UL << idx;
	
	if ((timr->OSCR == timr->OSMR[idx]) && (timr->OIER & v))
		timr->OSSR |= v;
}

static void pxaTimrPrvUpdate(struct PxaTimr *timr)
{
	uint_fast8_t i;
	
	for (i = 0; i < 4; i++)
		pxaTimrPrvCheckMatch(timr, i);
	
	pxaTimrPrvRaiseLowerInts(timr);
}

static bool pxaTimrPrvMemAccessF(void* userData, uint32_t pa, uint_fast8_t size, bool write, void* buf)
{
	struct PxaTimr *timr = (struct PxaTimr*)userData;
	uint32_t val = 0;
	
	if (size != 4) {
		fprintf(stderr, "%s: Unexpected %s of %u bytes to 0x%08lx\n", __func__, write ? "write" : "read", size, (unsigned long)pa);
		return false;		//we do not support non-word accesses
	}
	
	pa = (pa - PXA_TIMR_BASE) >> 2;
	
	if(write){
		val = *(uint32_t*)buf;
		
		switch(pa){
			case 0:
			case 1:
			case 2:
			case 3:
				timr->OSMR[pa] = val;
				pxaTimrPrvUpdate(timr);
				break;
			
			case 4:
				timr->OSCR = val;
				break;
			
			case 5:
				timr->OSSR = timr->OSSR &~ val;
				pxaTimrPrvUpdate(timr);
				break;
			
			case 6:
				if (val & 1)
					return false;	//not supported
				timr->OWER = val;
				break;
			
			case 7:
				timr->OIER = val;
				pxaTimrPrvUpdate(timr);
				break;
		}
	}
	else{
		switch(pa){
			case 0:
			case 1:
			case 2:
			case 3:
				val = timr->OSMR[pa];
				break;
			
			case 4:
				val = timr->OSCR;
				break;
			
			case 5:
				val = timr->OSSR;
				break;
			
			case 6:
				val = timr->OWER;
				break;
			
			case 7:
				val = timr->OIER;
				break;
		}
		*(uint32_t*)buf = val;
	}
	
	return true;
}


struct PxaTimr* pxaTimrInit(struct ArmMem *physMem, struct SocIc *ic)
{
	struct PxaTimr *timr = (struct PxaTimr*)malloc(sizeof(*timr));
	
	if (!timr)
		ERR("cannot alloc OSTIMER");
	
	memset(timr, 0, sizeof (*timr));
	timr->ic = ic;
	
	if (!memRegionAdd(physMem, PXA_TIMR_BASE, PXA_TIMR_SIZE, pxaTimrPrvMemAccessF, timr))
		ERR("cannot add OSTIMER to MEM\n");
	
	return timr;
}

void pxaTimrTick(struct PxaTimr *timr)
{
	timr->OSCR++;
	pxaTimrPrvUpdate(timr);
}
