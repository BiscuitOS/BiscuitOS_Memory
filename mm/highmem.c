/*
 * High memory handling common code and variables.
 *
 * (C) 1999 Andrea Arcangeli, SuSE GmbH, andrea@suse.de
 *          Gerhard Wichert, Siemens AG, Gerhard.Wichert@pdb.siemens.de
 *
 *
 * Redesigned the x86 32-bit VM architecture to deal with
 * 64-bit physical space. With current x86 CPUs this
 * means up to 64 Gigabytes physical RAM.
 *
 * Rewrote high memory support to move the page cache into
 * high memory. Implemented permanent (schedulable) kmaps
 * based on Linus' idea.
 *
 * Copyright (C) 1999 Ingo Molnar <mingo@redhat.com>
 */
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/hash.h>
#include "biscuitos/kernel.h"
#include "biscuitos/mm.h"
#include "asm-generated/highmem.h"

#define PA_HASH_ORDER_BS	7

/*
 * Describes one page->virtual association
 */
struct page_address_map_bs {
	struct page_bs *page;
	void *virtual;
	struct list_head list;
};

/*
 * Hash table bucket
 */
static struct page_address_slot_bs {
	struct list_head lh;		/* List of page_address_maps */
	spinlock_t lock;		/* Protect this bucket's list */
} ____cacheline_aligned_in_smp_bs page_address_htable_bs[1 << PA_HASH_ORDER_BS];

/*
 * page_address_map freelist, allocated from page_address_maps.
 */
static struct list_head page_address_pool_bs;	/* freelist */
static spinlock_t pool_lock_bs;
static struct page_address_map_bs page_address_maps_bs[LAST_PKMAP_BS];

void __init page_address_init_bs(void)
{
	int i;

	INIT_LIST_HEAD(&page_address_pool_bs);
	for (i = 0; i < ARRAY_SIZE(page_address_maps_bs); i++)
		list_add(&page_address_maps_bs[i].list, &page_address_pool_bs);
	for (i = 0; i < ARRAY_SIZE(page_address_htable_bs); i++) {
		INIT_LIST_HEAD(&page_address_htable_bs[i].lh);
		spin_lock_init(&page_address_htable_bs[i].lock);
	}
	spin_lock_init(&pool_lock_bs);
}

static struct page_address_slot_bs *page_slot_bs(struct page_bs *page)
{
	return &page_address_htable_bs[hash_ptr(page, PA_HASH_ORDER_BS)];
}

void *page_address_bs(struct page_bs *page)
{
	unsigned long flags;
	void *ret;
	struct page_address_slot_bs *pas;	

	if (!PageHighMem_bs(page))
		return lowmem_page_address_bs(page);

	pas = page_slot_bs(page);
	ret = NULL;
	spin_lock_irqsave(&pas->lock, flags);
	if (!list_empty(&pas->lh)) {
		struct page_address_map_bs *pam;

		list_for_each_entry(pam, &pas->lh, list) {
			if (pam->page == page) {
				ret = pam->virtual;
				goto done;
			}
		}
	}

done:
	spin_unlock_irqrestore(&pas->lock, flags);
	return ret;
}
EXPORT_SYMBOL_GPL(page_address_bs);

void set_page_address_bs(struct page_bs *page, void *virtual)
{

}


















