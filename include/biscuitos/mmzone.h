#ifndef _BISCUITOS_MMZONE_H
#define _BISCUITOS_MMZONE_H

struct bootmem_data;
typedef struct pglist_data {
	int nr_zones;
	struct bootmem_data *bdata;
	struct pglist_data *pgdat_next;
} pg_data_t;

extern struct pglist_data contig_page_data_bs;
extern struct pglist_data *pgdat_list_bs;

#define NODE_DATA_BS(nid)	(&contig_page_data_bs)

/*
 * for_each_pgdat - helper macro to iterate over all nodes
 * @pgdat - pointer to a pg_data_t variable
 *
 * Meant to help with common loops of the from
 * pgdat = pgdat_list;
 * while (pgdat) {
 * 	...
 *	pgdat = pgdat->pgdat_next;
 * }
 */
#define for_each_pgdat_bs(pgdat) \
	for (pgdat = pgdat_list_bs; pgdat; pgdat = pgdat->pgdat_next)

#endif
