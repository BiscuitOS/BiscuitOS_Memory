#ifndef _BISCUITOS_ARM_SYSTEM_H
#define _BISCUITOS_ARM_SYSTEM_H

#define CPU_ARCH_UNKNOWN_BS		0
#define CPU_ARCH_ARMv3_BS		1
#define CPU_ARCH_ARMv4_BS		2
#define CPU_ARCH_ARMv4T_BS		3
#define CPU_ARCH_ARMv5_BS		4
#define CPU_ARCH_ARMv5T_BS		5
#define CPU_ARCH_ARMv5TE_BS		6
#define CPU_ARCH_ARMv5TEJ_BS 		7 
#define CPU_ARCH_ARMv6_BS		8
#define CPU_ARCH_ARMv7_BS		9
#define CPU_ARCH_ARMv7M_BS		10

/*
 * CR1 bits (CP#15 CR1)
 */
#define CR_M_BS	(1 << 0)	/* MMU enable                           */
#define CR_A_BS	(1 << 1)	/* Alignment abort enable               */
#define CR_C_BS	(1 << 2)	/* Dcache enable                        */
#define CR_W_BS	(1 << 3)	/* Write buffer enable                  */
#define CR_P_BS	(1 << 4)	/* 32-bit exception handler             */
#define CR_D_BS	(1 << 5)	/* 32-bit data address range            */
#define CR_L_BS	(1 << 6)	/* Implementation defined               */
#define CR_B_BS	(1 << 7)	/* Big endian                           */
#define CR_S_BS	(1 << 8)	/* System MMU protection                */
#define CR_R_BS	(1 << 9)	/* ROM MMU protection                   */
#define CR_F_BS	(1 << 10)	/* Implementation defined               */
#define CR_Z_BS	(1 << 11)	/* Implementation defined               */ 
#define CR_I_BS	(1 << 12)	/* Icache enable                        */
#define CR_V_BS	(1 << 13)	/* Vectors relocated to 0xffff0000      */
#define CR_RR_BS (1 << 14)	/* Round Robin cache replacement        */
#define CR_L4_BS (1 << 15)	/* LDR pc can set T bit                 */
#define CR_DT_BS (1 << 16)
#ifdef CONFIG_MMU
#define CR_HA_BS (1 << 17)	/* Hardware management of Access Flag   */
#else
#define CR_BR_BS (1 << 17)	/* MPU Background region enable (PMSA)  */
#endif
#define CR_IT_BS (1 << 18)
#define CR_ST_BS (1 << 19)
#define CR_FI_BS (1 << 21)	/* Fast interrupt (lower latency mode)  */
#define CR_U_BS  (1 << 22)	/* Unaligned access operation           */
#define CR_XP_BS (1 << 23)	/* Extended page tables                 */
#define CR_VE_BS (1 << 24)	/* Vectored interrupts                  */
#define CR_EE_BS (1 << 25)	/* Exception (Big) Endian               */
#define CR_TRE_BS (1 << 28)	/* TEX remap enable                     */
#define CR_AFE_BS (1 << 29)	/* Access flag enable                   */
#define CR_TE_BS  (1 << 30)	/* Thumb exception enable               */

#define get_cr_bs()						\
({								\
	unsigned int __val;					\
	asm volatile(						\
	"mrc	p15, 0, %0, c1, c0, 0	@ get CR"		\
	: "=r" (__val) :: "cc");				\
	__val;							\
})

#define vectors_high_bs()		(0)
#define cpu_is_xsc3_bs()		0

extern int cpu_architecture_bs(void);

#endif
