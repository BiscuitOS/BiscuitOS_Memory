/*
 * include/linux/pagevec.h
 *
 * In many places it is efficient to batch an operation up against multiple
 * pages.  A pagevec is a multipage container which is used for that.
 */

/* 14 pointers + two long's align the pagevec structure to a power of two */
#define PAGEVEC_SIZE_BS		14

struct pagevec_bs {
	unsigned long nr;
	unsigned long cold;
	struct page_bs *pages[PAGEVEC_SIZE_BS];
};

static inline void pagevec_init_bs(struct pagevec_bs *pvec, int cold)
{
	pvec->nr = 0;
	pvec->cold = cold;
}

static inline void pagevec_reinit_bs(struct pagevec_bs *pvec)
{
	pvec->nr = 0;
}

static inline unsigned pagevec_count_bs(struct pagevec_bs *pvec)
{
	return pvec->nr;
}

static inline unsigned pagevec_space_bs(struct pagevec_bs *pvec)
{
	return PAGEVEC_SIZE_BS - pvec->nr;
}

/*
 * Add a page to a pagevec.  Returns the number of slots still available.
 */
static inline unsigned pagevec_add_bs(struct pagevec_bs *pvec,
						struct page_bs *page)
{
	pvec->pages[pvec->nr++] = page;
	return pagevec_space_bs(pvec);
}

extern void __pagevec_free_bs(struct pagevec_bs *pvec);

static inline void pagevec_free_bs(struct pagevec_bs *pvec)
{
	if (pagevec_count_bs(pvec))
		__pagevec_free_bs(pvec);
}
