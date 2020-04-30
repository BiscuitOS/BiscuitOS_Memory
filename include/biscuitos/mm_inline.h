
static inline void
add_page_to_active_list_bs(struct zone_bs *zone, struct page_bs *page)
{
	list_add(&page->lru, &zone->active_list);
	zone->nr_active++;
}

static inline void
add_page_to_inactive_list_bs(struct zone_bs *zone, struct page_bs *page)
{
	list_add(&page->lru, &zone->inactive_list);
	zone->nr_inactive++;
}

static inline void
del_page_from_lru_bs(struct zone_bs *zone, struct page_bs *page)
{
	list_del(&page->lru);
	if (PageActive_bs(page)) {
		ClearPageActive_bs(page);
		zone->nr_active--;
	} else {
		zone->nr_inactive--;
	}
}
