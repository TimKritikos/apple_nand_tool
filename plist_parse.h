#include <stdint.h>

struct ipdp_plist_info{
	int64_t page_bytes;
	int64_t meta_per_logical_page;
	int64_t block_pages;
	int64_t ce_blocks;
	int64_t ce;
};

struct ipdp_plist_info *parse_ipdp_plist(char *filename);
