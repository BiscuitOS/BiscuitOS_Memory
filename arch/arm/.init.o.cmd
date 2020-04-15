cmd_/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/init.o := /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/arm-linux-gnueabi/arm-linux-gnueabi/bin/arm-linux-gnueabi-gcc -Wp,-MD,/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/.init.o.d  -nostdinc -isystem /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/arm-linux-gnueabi/gcc-linaro-7.4.1-2019.02-x86_64_arm-linux-gnueabi/bin/../lib/gcc/arm-linux-gnueabi/7.4.1/include -I./arch/arm/include -I./arch/arm/include/generated  -I./include -I./arch/arm/include/uapi -I./arch/arm/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/kconfig.h -include ./include/linux/compiler_types.h -D__KERNEL__ -mlittle-endian -Wall -Wundef -Werror=strict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -Werror-implicit-function-declaration -Werror=implicit-int -Wno-format-security -std=gnu89 -fno-dwarf2-cfi-asm -fno-ipa-sra -mabi=aapcs-linux -mfpu=vfp -funwind-tables -marm -Wa,-mno-warn-deprecated -D__LINUX_ARM_ARCH__=7 -march=armv7-a -msoft-float -Uarm -fno-delete-null-pointer-checks -Wno-frame-address -Wno-format-truncation -Wno-format-overflow -Wno-int-in-bool-context -O2 --param=allow-store-data-races=0 -Wframe-larger-than=1024 -fstack-protector-strong -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-var-tracking-assignments -g -pg -Wdeclaration-after-statement -Wvla -Wno-pointer-sign -fno-strict-overflow -fno-merge-all-constants -fmerge-constants -fno-stack-check -fconserve-stack -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -DCONFIG_NODES_SHIFT=2 -DCONFIG_PAGE_OFFSET_BS=0x90000000 -DCONFIG_PHYS_OFFSET_BS=0x70000000 -DCONFIG_CMDLINE_BS="\"mem_bs=0x4000000@0x70000000\"" -I/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/include -I/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/include  -DMODULE  -DKBUILD_BASENAME='"init"' -DKBUILD_MODNAME='"BiscuitOS_mod"' -c -o /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/init.o /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/init.c

source_/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/init.o := /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/init.c

deps_/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/init.o := \
  include/linux/kconfig.h \
    $(wildcard include/config/cpu/big/endian.h) \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
  include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/arch/supports/optimized/inlining.h) \
    $(wildcard include/config/optimize/inlining.h) \
  include/linux/compiler_attributes.h \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/retpoline.h) \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  include/linux/kernel.h \
    $(wildcard include/config/lbdaf.h) \
    $(wildcard include/config/preempt/voluntary.h) \
    $(wildcard include/config/debug/atomic/sleep.h) \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/prove/locking.h) \
    $(wildcard include/config/arch/has/refcount.h) \
    $(wildcard include/config/panic/timeout.h) \
    $(wildcard include/config/tracing.h) \
    $(wildcard include/config/ftrace/mcount/record.h) \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/arm-linux-gnueabi/gcc-linaro-7.4.1-2019.02-x86_64_arm-linux-gnueabi/lib/gcc/arm-linux-gnueabi/7.4.1/include/stdarg.h \
  include/linux/linkage.h \
  include/linux/compiler_types.h \
  include/linux/stringify.h \
  include/linux/export.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/module/rel/crcs.h) \
    $(wildcard include/config/have/arch/prel32/relocations.h) \
    $(wildcard include/config/trim/unused/ksyms.h) \
    $(wildcard include/config/unused/symbols.h) \
  arch/arm/include/asm/linkage.h \
  include/linux/stddef.h \
  include/uapi/linux/stddef.h \
  include/linux/types.h \
    $(wildcard include/config/have/uid16.h) \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  include/uapi/linux/types.h \
  arch/arm/include/uapi/asm/types.h \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  arch/arm/include/generated/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/uapi/asm-generic/bitsperlong.h \
  include/uapi/linux/posix_types.h \
  arch/arm/include/uapi/asm/posix_types.h \
  include/uapi/asm-generic/posix_types.h \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/stack/validation.h) \
    $(wildcard include/config/kasan.h) \
  arch/arm/include/asm/barrier.h \
    $(wildcard include/config/cpu/32v6k.h) \
    $(wildcard include/config/thumb2/kernel.h) \
    $(wildcard include/config/cpu/xsc3.h) \
    $(wildcard include/config/cpu/fa526.h) \
    $(wildcard include/config/arm/heavy/mb.h) \
    $(wildcard include/config/arm/dma/mem/bufferable.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/cpu/spectre.h) \
  include/asm-generic/barrier.h \
  include/linux/kasan-checks.h \
  include/linux/bitops.h \
  include/linux/bits.h \
  arch/arm/include/asm/bitops.h \
  include/linux/irqflags.h \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
  include/linux/typecheck.h \
  arch/arm/include/asm/irqflags.h \
    $(wildcard include/config/cpu/v7m.h) \
  arch/arm/include/asm/ptrace.h \
    $(wildcard include/config/arm/thumb.h) \
  arch/arm/include/uapi/asm/ptrace.h \
    $(wildcard include/config/cpu/endian/be8.h) \
  arch/arm/include/asm/hwcap.h \
  arch/arm/include/uapi/asm/hwcap.h \
  include/asm-generic/irqflags.h \
  include/asm-generic/bitops/non-atomic.h \
  include/asm-generic/bitops/builtin-__fls.h \
  include/asm-generic/bitops/builtin-__ffs.h \
  include/asm-generic/bitops/builtin-fls.h \
  include/asm-generic/bitops/builtin-ffs.h \
  include/asm-generic/bitops/ffz.h \
  include/asm-generic/bitops/fls64.h \
  include/asm-generic/bitops/sched.h \
  include/asm-generic/bitops/hweight.h \
  include/asm-generic/bitops/arch_hweight.h \
  include/asm-generic/bitops/const_hweight.h \
  include/asm-generic/bitops/lock.h \
  include/linux/atomic.h \
    $(wildcard include/config/generic/atomic64.h) \
  arch/arm/include/asm/atomic.h \
    $(wildcard include/config/arm/lpae.h) \
  include/linux/prefetch.h \
  arch/arm/include/asm/processor.h \
    $(wildcard include/config/have/hw/breakpoint.h) \
    $(wildcard include/config/binfmt/elf/fdpic.h) \
    $(wildcard include/config/arm/errata/754327.h) \
  arch/arm/include/asm/hw_breakpoint.h \
  arch/arm/include/asm/unified.h \
  arch/arm/include/asm/cache.h \
    $(wildcard include/config/arm/l1/cache/shift.h) \
    $(wildcard include/config/aeabi.h) \
  arch/arm/include/asm/cmpxchg.h \
    $(wildcard include/config/cpu/sa1100.h) \
    $(wildcard include/config/cpu/sa110.h) \
    $(wildcard include/config/cpu/v6.h) \
  include/asm-generic/cmpxchg-local.h \
  include/asm-generic/atomic-long.h \
  include/asm-generic/bitops/find.h \
    $(wildcard include/config/generic/find/first/bit.h) \
  include/asm-generic/bitops/le.h \
  arch/arm/include/uapi/asm/byteorder.h \
  include/linux/byteorder/little_endian.h \
  include/uapi/linux/byteorder/little_endian.h \
  include/linux/swab.h \
  include/uapi/linux/swab.h \
  arch/arm/include/asm/swab.h \
  arch/arm/include/uapi/asm/swab.h \
  include/linux/byteorder/generic.h \
  include/asm-generic/bitops/ext2-atomic-setbit.h \
  include/linux/log2.h \
    $(wildcard include/config/arch/has/ilog2/u32.h) \
    $(wildcard include/config/arch/has/ilog2/u64.h) \
  include/linux/printk.h \
    $(wildcard include/config/message/loglevel/default.h) \
    $(wildcard include/config/console/loglevel/default.h) \
    $(wildcard include/config/console/loglevel/quiet.h) \
    $(wildcard include/config/early/printk.h) \
    $(wildcard include/config/printk/nmi.h) \
    $(wildcard include/config/printk.h) \
    $(wildcard include/config/dynamic/debug.h) \
  include/linux/init.h \
    $(wildcard include/config/strict/kernel/rwx.h) \
    $(wildcard include/config/strict/module/rwx.h) \
  include/linux/kern_levels.h \
  include/linux/cache.h \
    $(wildcard include/config/arch/has/cache/line/size.h) \
  include/uapi/linux/kernel.h \
  include/uapi/linux/sysinfo.h \
  include/linux/build_bug.h \
  arch/arm/include/asm/div64.h \
  arch/arm/include/asm/compiler.h \
  include/asm-generic/div64.h \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/include/biscuitos/kernel.h \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/include/asm-generated/bug.h \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/include/biscuitos/mm.h \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/include/asm-generated/page.h \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/include/asm-generated/memory.h \
    $(wildcard include/config/phys/offset/bs.h) \
    $(wildcard include/config/page/offset/bs.h) \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/include/asm-generated/pgtable.h \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/include/biscuitos/bootmem.h \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/include/biscuitos/mmzone.h \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/include/biscuitos/nodemask.h \
  include/linux/bitmap.h \
  include/linux/string.h \
    $(wildcard include/config/binary/printf.h) \
    $(wildcard include/config/fortify/source.h) \
  include/uapi/linux/string.h \
  arch/arm/include/asm/string.h \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/include/asm-generated/arch.h \
    $(wildcard include/config/nodes/shift.h) \
  /xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/include/asm-generated/setup.h \

/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/init.o: $(deps_/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/init.o)

$(deps_/xspace/OpenSource/BiscuitOS/BiscuitOS/output/linux-5.0-arm32/package/mm_bs-0.0.1/mm_bs-0.0.1/arch/arm/init.o):
