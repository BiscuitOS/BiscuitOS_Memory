#ifndef _BISCUITOS_RMAP_H
#define _BISCUITOS_RMAP_H

/*
 * The anon_vma heads a list of private "related" vmas, to scan if
 * an anonymous page pointing to this anon_vma needs to be unmapped:
 * the vmas on the list will be related by forking, or by splitting.
 *
 * Since vmas come and go as they are split and merged (particularly
 * in mprotect), the mapping field of an anonymous page cannot point
 * directly to a vma: instead it points to an anon_vma, on whose list
 * the related vmas can be easily linked or unlinked.
 *
 * After unlinking the last vma on the list, we must garbage collect
 * the anon_vma object itself: we're guaranteed no page can be
 * pointing to this anon_vma once its vma list is empty.
 */
struct anon_vma_bs {
	spinlock_t lock;	/* Serialize access to vma list */
	struct list_head head;	/* List of private "related" vmas */
};

extern int page_referenced_bs(struct page_bs *, int, int);

#endif
