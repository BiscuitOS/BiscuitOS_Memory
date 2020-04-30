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
#include <linux/sched.h>
#include <linux/wait.h>
#include "biscuitos/kernel.h"
#include "biscuitos/init.h"
#include "biscuitos/mm.h"
#include "biscuitos/mempool.h"
#include "biscuitos/highmem.h"
#include "asm-generated/highmem.h"
#include "asm-generated/pgtable.h"
#include "asm-generated/tlbflush.h"

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
	unsigned long flags;
	struct page_address_slot_bs *pas;
	struct page_address_map_bs *pam;

	BUG_ON_BS(!PageHighMem_bs(page));

	pas = page_slot_bs(page);
	if (virtual) {
		BUG_ON_BS(list_empty(&page_address_pool_bs));

		spin_lock_irqsave(&pool_lock_bs, flags);
		pam = list_entry(page_address_pool_bs.next,
				 struct page_address_map_bs, list);
		list_del(&pam->list);
		spin_unlock_irqrestore(&pool_lock_bs, flags);

		pam->page = page;
		pam->virtual = virtual;

		spin_lock_irqsave(&pas->lock, flags);
		list_add_tail(&pam->list, &pas->lh);
		spin_unlock_irqrestore(&pas->lock, flags);
	} else {	/* Remove */
		spin_lock_irqsave(&pas->lock, flags);
		list_for_each_entry(pam, &pas->lh, list) {
			if (pam->page == page) {
				list_del(&pam->list);
				spin_unlock_irqrestore(&pas->lock, flags);
				spin_lock_irqsave(&pool_lock_bs, flags);
				list_add_tail(&pam->list, 
						&page_address_pool_bs);
				spin_unlock_irqrestore(&pool_lock_bs, flags);
				goto done;
			}
		}
		spin_unlock_irqrestore(&pas->lock, flags);
	}
done:
	return;
}

#ifdef CONFIG_HIGHMEM_BS
static int pkmap_count_bs[LAST_PKMAP_BS];
static unsigned int last_pkmap_nr_bs;
static __cacheline_aligned_in_smp_bs DEFINE_SPINLOCK(kmap_lock_bs);
static DECLARE_WAIT_QUEUE_HEAD(pkmap_map_wait_bs);

pte_t_bs *pkmap_page_table_bs;

static void flush_all_zero_pkmaps_bs(void)
{
	int i;

	flush_cache_kmaps_bs();

	for (i = 0; i < LAST_PKMAP_BS; i++) {
		struct page_bs *page;

		/*
		 * zero means we don't have anything to do,
		 * >1 means that it is still in use. Only
		 * a count of 1 means that it is free but
		 * needs to be unmapped
		 */
		if (pkmap_count_bs[i] != 1)
			continue;
		pkmap_count_bs[i] = 0;

		/* sanity check */
		if (pte_none_bs(pkmap_page_table_bs[i]))
			BUG_BS();
		/*
		 * Don't need an atomic fetch-and-clear op here;
		 * no-one has the page mapped, and cannot get at
		 * its virtual address (and hence PTE) without first
		 * getting the kmap_lock (which is held here).
		 * So no dangers, even with speculative execution.
		 */
		page = pte_page_bs(pkmap_page_table_bs[i]);
		pte_clear_bs(&init_mm_bs,
			(unsigned long)page_address_bs(page),
			&pkmap_page_table_bs[i]);

		set_page_address_bs(page, NULL);
	}
	flush_tlb_kernel_range_bs(PKMAP_ADDR_BS(0),
				  PKMAP_ADDR_BS(LAST_PKMAP_BS));
}

static inline unsigned long map_new_virtual_bs(struct page_bs *page)
{
	unsigned long vaddr;
	int count;

start:
	count = LAST_PKMAP_BS;
	/* Find an empty entry */
	for (;;) {
		last_pkmap_nr_bs = (last_pkmap_nr_bs + 1) & LAST_PKMAP_MASK_BS;
		if (!last_pkmap_nr_bs) {
			flush_all_zero_pkmaps_bs();
			count = LAST_PKMAP_BS;
		}
		if (!pkmap_count_bs[last_pkmap_nr_bs])
			break; /* Found a usable entry */
		if (--count)
			continue;

		/*
		 * Sleep for somebody else to unmap their entries
		 */
		{
			DECLARE_WAITQUEUE(wait, current);

			__set_current_state(TASK_UNINTERRUPTIBLE);
			add_wait_queue(&pkmap_map_wait_bs, &wait);
			spin_unlock(&kmap_lock_bs);
			schedule();
			remove_wait_queue(&pkmap_map_wait_bs, &wait);
			spin_lock(&kmap_lock_bs);

			/* Somebody else might have mapped it while we slept */
			if (page_address_bs(page))
				return (unsigned long)page_address_bs(page);

			/* Re-start */
			goto start;
		}
	}
	vaddr = PKMAP_ADDR_BS(last_pkmap_nr_bs);
	set_pte_at_bs(&init_mm_bs, vaddr,
			&(pkmap_page_table_bs[last_pkmap_nr_bs]),
			mk_pte_bs(page, kmap_prot_bs));

	pkmap_count_bs[last_pkmap_nr_bs] = 1;
	set_page_address_bs(page, (void *)vaddr);

	return vaddr;
}

void fastcall_bs *kmap_high_bs(struct page_bs *page)
{
	unsigned long vaddr;

	/*
	 * For highmem pages, we can't trust "virtual" until
	 * after we have the lock.
	 *
	 * We cannot call this from interrupts, as it may block
	 */
	spin_lock(&kmap_lock_bs);
	vaddr = (unsigned long)page_address_bs(page);
	if (!vaddr)
		vaddr = map_new_virtual_bs(page);
	pkmap_count_bs[PKMAP_NR_BS(vaddr)]++;
	if (pkmap_count_bs[PKMAP_NR_BS(vaddr)] < 2)
		BUG_BS();
	spin_unlock(&kmap_lock_bs);
	return (void *)vaddr;
}
EXPORT_SYMBOL_GPL(kmap_high_bs);

void fastcall_bs kunmap_high_bs(struct page_bs *page)
{
	unsigned long vaddr;
	unsigned long nr;
	int need_wakeup;

	spin_lock(&kmap_lock_bs);
	vaddr = (unsigned long)page_address_bs(page);
	if (!vaddr)
		BUG_BS();
	nr = PKMAP_NR_BS(vaddr);

	/*
	 * A count must never go down to zero
	 * without a TLB flush!
	 */
	need_wakeup = 0;
	switch (--pkmap_count_bs[nr]) {
	case 0:
		BUG_BS();
	case 1:
		/*
		 * Avoid an unnecessary wake_up() function call.
		 * The common case is pkmap_count[] == 1, but
		 * no waiters.
		 * The tasks queued in the wait-queue are guarded
		 * by both the lock in the wait-queue-head and by
		 * the kmap_lock.  As the kmap_lock is held here,
		 * no need for the wait-queue-head's lock.  Simply
		 * test if the queue is empty.
		 */
		need_wakeup = waitqueue_active(&pkmap_map_wait_bs);
	}
	spin_unlock(&kmap_lock_bs);

	/* do wake-up, if needed, race-free outside of the spin lock */
	if (need_wakeup)
		wake_up(&pkmap_map_wait_bs);
}
EXPORT_SYMBOL_GPL(kunmap_high_bs);

void *kmap_bs(struct page_bs *page)
{
	might_sleep();
	if (!PageHighMem_bs(page))
		return page_address_bs(page);
	return kmap_high_bs(page);
}
EXPORT_SYMBOL_GPL(kmap_bs);

void kunmap_bs(struct page_bs *page)
{
	if (in_interrupt())
		BUG_BS();
	if (!PageHighMem_bs(page))
		return;
	kunmap_high_bs(page);
}
EXPORT_SYMBOL_GPL(kunmap_bs);

/*
 * kmap_atomic/kunmap_atomic is significantly faster than kmap/kunmap because
 * no global lock is needed and because the kmap code must perform a global TLB
 * invalidation when the kmap pool wraps.
 *
 * However when holding an atomic kmap is is not legal to sleep, so atomic
 * kmaps are appropriate for short, tight code paths only.
 */
void *kmap_atomic_bs(struct page_bs *page, enum km_type_bs type)
{
	enum fixed_addresses_bs idx;
	unsigned long vaddr;

	if (!PageHighMem_bs(page))
		return page_address_bs(page);

	idx = type + KM_TYPE_NR_BS * smp_processor_id();
	vaddr = __fix_to_virt_bs(FIX_KMAP_BEGIN_BS + idx);
#ifdef CONFIG_DEBUG_HIGHMEM
	if (!pte_none_bs(*(kmap_pte_bs - idx)))
		BUG_BS();
#endif
	set_pte_bs(kmap_pte_bs - idx, mk_pte_bs(page, kmap_prot_bs));
	__flush_tlb_one_bs(vaddr);

	return (void *)vaddr;
}
EXPORT_SYMBOL_GPL(kmap_atomic_bs);

void kunmap_atomic_bs(void *kvaddr, enum km_type_bs type)
{
#ifdef CONFIG_DEBUG_HIGHMEM
	unsigned long vaddr = (unsigned long)kvaddr & PAGE_MASK_BS;
	enum fixed_addresses_bs idx = type + KM_TYPE_NR_BS*smp_processor_id();

	if (vaddr < FIXADDR_START_BS) { // FIXME
		return;
	}
	
	if (vaddr != __fix_to_virt_bs(FIX_KMAP_BEGIN_BS + idx))
		BUG_BS();

	/*
	 * force other mappings to Oops if they'll try to access
	 * this pte without first remap it
	 */
	pte_clear_bs(&init_mm_bs, vaddr, kmap_pte_bs - idx);
	__flush_tlb_one_bs(vaddr);
#endif
	/* Nothing */
}
EXPORT_SYMBOL_GPL(kunmap_atomic_bs);

struct page_bs *kmap_atomic_to_page_bs(void *ptr)
{
	unsigned long idx, vaddr = (unsigned long)ptr;
	pte_t_bs *pte;

	if (vaddr < FIXADDR_START_BS)
		return virt_to_page_bs(ptr);

	idx = virt_to_fix_bs(vaddr);
	pte = kmap_pte_bs - (idx - FIX_KMAP_BEGIN_BS);
	return pte_page_bs(*pte);
}

#endif

static mempool_t_bs *page_pool_bs;

static void *page_pool_alloc_bs(unsigned int __nocast gfp_mask, void *data)
{
	unsigned int gfp = gfp_mask | (unsigned int)(long)data;

	return alloc_page_bs(gfp);
}

static void page_pool_free_bs(void *page, void *data)
{
	__free_page_bs(page);
}

#define POOL_SIZE_BS		64

static __init int init_emergency_pool_bs(void)
{
	struct sysinfo_bs i;

	si_meminfo_bs(&i);

	if (!i.totalhigh)
		return 0;

	page_pool_bs = mempool_create_bs(POOL_SIZE_BS,
				page_pool_alloc_bs,
				page_pool_free_bs,
				NULL);
	if (!page_pool_bs)
		BUG_BS();

	printk("Highmem bounch pool size: %d pages\n", POOL_SIZE_BS);

	return 0;
}
module_initcall_bs(init_emergency_pool_bs);
