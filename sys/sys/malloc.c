/*	malloc.c	4.1	11/9/80	*/

#include "../h/param.h"
#include "../h/systm.h"
#include "../h/map.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/mtpr.h"
#include "../h/text.h"

/*
 * Allocate 'size' units from the given
 * map. Return the base of the allocated
 * space.
 * In a map, the addresses are increasing and the
 * list is terminated by a 0 size.
 * The swap map unit is 512 bytes.
 * Algorithm is first-fit.
 */
malloc(mp, size)
struct map *mp;
{
	register int a;
	register struct map *bp;
	swblk_t first, rest;

	if (size <= 0 || mp == swapmap && size > DMMAX)
		panic("malloc");
	for (bp=mp; bp->m_size; bp++) {
		if (bp->m_size >= size) {
			if (mp == swapmap &&
			    (first = DMMAX - bp->m_addr%DMMAX) < bp->m_size) {
				if (bp->m_size - first < size)
					continue;
				a = bp->m_addr + first;
				rest = bp->m_size - first - size;
				bp->m_size = first;
				if (rest)
					mfree(swapmap, rest, a+size);
				return (a);
			}
			a = bp->m_addr;
			bp->m_addr += size;
			if ((bp->m_size -= size) == 0) {
				do {
					bp++;
					(bp-1)->m_addr = bp->m_addr;
				} while ((bp-1)->m_size = bp->m_size);
			}
			if (mp == swapmap && a % CLSIZE)
				panic("malloc swapmap");
			return(a);
		}
	}
	return(0);
}

/*
 * Free the previously allocated space aa
 * of size units into the specified map.
 * Sort aa into map and combine on
 * one or both ends if possible.
 */
mfree(mp, size, a)
struct map *mp;
register int size, a;
{
	register struct map *bp;
	register int t;

	if (a <= 0)
		panic("mfree addr");
	if (size <= 0)
		panic("mfree size");
	bp = mp;
	for (; bp->m_addr<=a && bp->m_size!=0; bp++)
		continue;
	if (bp>mp && (bp-1)->m_addr+(bp-1)->m_size > a)
		panic("mfree begov");
	if (a+size > bp->m_addr && bp->m_size)
		panic("mfree endov");
	if (bp>mp && (bp-1)->m_addr+(bp-1)->m_size == a) {
		(bp-1)->m_size += size;
		if (a+size == bp->m_addr) {
			(bp-1)->m_size += bp->m_size;
			while (bp->m_size) {
				bp++;
				(bp-1)->m_addr = bp->m_addr;
				(bp-1)->m_size = bp->m_size;
			}
		}
	} else {
		if (a+size == bp->m_addr && bp->m_size) {
			bp->m_addr -= size;
			bp->m_size += size;
		} else if (size) {
			do {
				t = bp->m_addr;
				bp->m_addr = a;
				a = t;
				t = bp->m_size;
				bp->m_size = size;
				bp++;
			} while (size = t);
		}
	}
	if ((mp == kernelmap) && kmapwnt) {
		kmapwnt = 0;
		wakeup((caddr_t)kernelmap);
	}
}
