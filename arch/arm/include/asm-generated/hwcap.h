#ifndef _BISCUITOS_ARM_HWCAP_H
#define _BISCUITOS_ARM_HWCAP_H

/*      
 * HWCAP flags - for elf_hwcap (in kernel) and AT_HWCAP
 */     
#define HWCAP_SWP_BS		(1 << 0)
#define HWCAP_HALF_BS		(1 << 1)
#define HWCAP_THUMB_BS		(1 << 2)
#define HWCAP_26BIT_BS		(1 << 3)        /* Play it safe */
#define HWCAP_FAST_MULT_BS	(1 << 4) 
#define HWCAP_FPA_BS		(1 << 5)
#define HWCAP_VFP_BS		(1 << 6)
#define HWCAP_EDSP_BS		(1 << 7)
#define HWCAP_JAVA_BS		(1 << 8)
#define HWCAP_IWMMXT_BS		(1 << 9)
#define HWCAP_CRUNCH_BS		(1 << 10)
#define HWCAP_THUMBEE_BS	(1 << 11)
#define HWCAP_NEON_BS		(1 << 12)
#define HWCAP_VFPv3_BS		(1 << 13)
#define HWCAP_VFPv3D16_BS	(1 << 14)       /* also set for VFPv4-D16 */
#define HWCAP_TLS_BS		(1 << 15)
#define HWCAP_VFPv4_BS		(1 << 16)
#define HWCAP_IDIVA_BS		(1 << 17)
#define HWCAP_IDIVT_BS		(1 << 18)
#define HWCAP_VFPD32_BS		(1 << 19) /* set if VFP has 32 regs (not 16) */
#define HWCAP_IDIV_BS		(HWCAP_IDIVA_BS | HWCAP_IDIVT_BS)
#define HWCAP_LPAE_BS		(1 << 20)
#define HWCAP_EVTSTRM_BS	(1 << 21)

/*
 * HWCAP2 flags - for elf_hwcap2 (in kernel) and AT_HWCAP2
 */
#define HWCAP2_AES_BS		(1 << 0)
#define HWCAP2_PMULL_BS		(1 << 1)
#define HWCAP2_SHA1_BS		(1 << 2)
#define HWCAP2_SHA2_BS		(1 << 3)
#define HWCAP2_CRC32_BS		(1 << 4)

#endif
