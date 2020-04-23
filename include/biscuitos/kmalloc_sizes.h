#if (PAGE_SIZE_BS == 4096)
	CACHE_BS(32)
#endif
	CACHE_BS(64)
#if L1_CACHE_BYTES_BS < 64
	CACHE_BS(96)
#endif
	CACHE_BS(128)
#if L1_CACHE_BYTES_BS < 128
	CACHE_BS(192)
#endif
	CACHE_BS(256)
	CACHE_BS(512)
	CACHE_BS(1024)
	CACHE_BS(2048)
	CACHE_BS(4096)
	CACHE_BS(8192)
	CACHE_BS(16384)
	CACHE_BS(32768)
	CACHE_BS(65536)
	CACHE_BS(131072)
#ifndef CONFIG_MMU
	CACHE_BS(262144)
	CACHE_BS(524288)
	CACHE_BS(1048576)
#ifdef CONFIG_LARGE_ALLOCS
	CACHE_BS(2097152)
	CACHE_BS(4194304)
	CACHE_BS(8388608)
	CACHE_BS(16777216)
	CACHE_BS(33554432)
#endif /* CONFIG_LARGE_ALLOCS */
#endif /* CONFIG_MMU */
