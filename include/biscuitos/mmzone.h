#ifndef _BISCUITOS_MMZONE_H
#define _BISCUITOS_MMZONE_H

struct bootmem_data;
typedef struct pglist_data {
	int nr_zones;
	struct bootmem_data *bdata;
} pg_data_t;

extern struct pglist_data contig_page_data_bs;
#define NODE_DATA_BS(nid)	(&contig_page_data_bs)

#endif
