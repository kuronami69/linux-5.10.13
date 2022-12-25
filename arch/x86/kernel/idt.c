// SPDX-License-Identifier: GPL-2.0-only
/*
 * Interrupt descriptor table related code
 */
#include <linux/interrupt.h>

#include <asm/cpu_entry_area.h>
#include <asm/set_memory.h>
#include <asm/traps.h>
#include <asm/proto.h>
#include <asm/desc.h>
#include <asm/hw_irq.h>

#define DPL0		0x0
#define DPL3		0x3

#define DEFAULT_STACK	0

#define G(_vector, _addr, _ist, _type, _dpl, _segment)	\
	{						\
		.vector		= _vector,		\
		.bits.ist	= _ist,			\
		.bits.type	= _type,		\
		.bits.dpl	= _dpl,			\
		.bits.p		= 1,			\
		.addr		= _addr,		\
		.segment	= _segment,		\
	}

/**
 *  中断门 - Interrupt gate
 */
#define INTG(_vector, _addr)				\
	G(_vector, _addr, DEFAULT_STACK, GATE_INTERRUPT, DPL0, __KERNEL_CS)
    //{
    //	.vector		= _vector,
    //	.bits.ist	= DEFAULT_STACK,
    //	.bits.type	= GATE_INTERRUPT,
    //	.bits.dpl	= DPL0,
    //	.bits.p		= 1,
    //	.addr		= _addr,
    //	.segment	= __KERNEL_CS,
    //}


/**
 *  系统中断门 - System interrupt gate
 *
 */
#define SYSG(_vector, _addr)				\
	G(_vector, _addr, DEFAULT_STACK, GATE_INTERRUPT, DPL3, __KERNEL_CS)
    //{
    //  .vector     = _vector,
    //  .bits.ist   = DEFAULT_STACK,
    //  .bits.type  = GATE_INTERRUPT,
    //  .bits.dpl   = DPL3,
    //  .bits.p     = 1,
    //  .addr       = _addr,
    //  .segment    = __KERNEL_CS,
    //}



/*
 * Interrupt gate with interrupt stack. The _ist index is the index in
 * the tss.ist[] array, but for the descriptor it needs to start at 1.
 */
#define ISTG(_vector, _addr, _ist)			\
	G(_vector, _addr, _ist + 1, GATE_INTERRUPT, DPL0, __KERNEL_CS)
    //{
    //  .vector     = _vector,
    //  .bits.ist   = _ist + 1,
    //  .bits.type  = GATE_INTERRUPT,
    //  .bits.dpl   = DPL0,
    //  .bits.p     = 1,
    //  .addr       = _addr,
    //  .segment    = __KERNEL_CS,
    //}

/* Task gate */
#define TSKG(_vector, _gdt)				\
	G(_vector, NULL, DEFAULT_STACK, GATE_TASK, DPL0, _gdt << 3)
    //{
    //  .vector     = _vector,
    //  .bits.ist   = DEFAULT_STACK,
    //  .bits.type  = GATE_TASK,
    //  .bits.dpl   = DPL0,
    //  .bits.p     = 1,
    //  .addr       = NULL,
    //  .segment    = _gdt << 3,
    //}

#define IDT_TABLE_SIZE		(IDT_ENTRIES * sizeof(gate_desc))

static bool __initdata idt_setup_done ;

/*
 * Early traps running on the DEFAULT_STACK because the other interrupt
 * stacks work only after cpu_init().
 */
static const __initconst struct idt_data early_idts[] = {/* 中断描述符表 陷阱 trap */
    /**
     *  调试
     */
	INTG(X86_TRAP_DB,		asm_exc_debug),
#ifdef __rtoax_ //上宏展开
    {
      idt_data.vector     = X86_TRAP_DB,
      idt_data.bits.ist   = DEFAULT_STACK,
      idt_data.bits.type  = GATE_INTERRUPT,
      idt_data.bits.dpl   = DPL0,
      idt_data.bits.p     = 1,
      idt_data.addr       = asm_exc_debug,
      idt_data.segment    = __KERNEL_CS,
    }
#endif

    /**
     *  断点
     */
	SYSG(X86_TRAP_BP,		asm_exc_int3),
#ifdef __rtoax_ //上宏展开
    {
        idt_data.vector     = X86_TRAP_BP,
        idt_data.bits.ist   = DEFAULT_STACK,
        idt_data.bits.type  = GATE_INTERRUPT,
        idt_data.bits.dpl   = DPL3,
        idt_data.bits.p     = 1,
        idt_data.addr       = asm_exc_int3,
        idt_data.segment    = __KERNEL_CS,
    }
#endif

#ifdef CONFIG_X86_32
	/*
	 * Not possible on 64-bit. See idt_setup_early_pf() for details.
	 */
//	INTG(X86_TRAP_PF,		asm_exc_page_fault),
#endif
};

/*
 * The default IDT entries which are set up in trap_init() before
 * cpu_init() is invoked. Interrupt stacks cannot be used at that point and
 * the traps which use them are reinitialized with IST after cpu_init() has
 * set up TSS.
 */
static const __initconst struct idt_data def_idts[] = {/* 默认的 中断描述符表 */
	INTG(X86_TRAP_DE,		asm_exc_divide_error),
	INTG(X86_TRAP_NMI,		asm_exc_nmi),   //arch/x86/entry/entry_64.S
	INTG(X86_TRAP_BR,		asm_exc_bounds),
	INTG(X86_TRAP_UD,		asm_exc_invalid_op),
	INTG(X86_TRAP_NM,		asm_exc_device_not_available),
	INTG(X86_TRAP_OLD_MF,		asm_exc_coproc_segment_overrun),
	INTG(X86_TRAP_TS,		asm_exc_invalid_tss),
	INTG(X86_TRAP_NP,		asm_exc_segment_not_present),
	INTG(X86_TRAP_SS,		asm_exc_stack_segment),
	INTG(X86_TRAP_GP,		asm_exc_general_protection),
	INTG(X86_TRAP_SPURIOUS,		asm_exc_spurious_interrupt_bug),
	INTG(X86_TRAP_MF,		asm_exc_coprocessor_error),
	INTG(X86_TRAP_AC,		asm_exc_alignment_check),
	INTG(X86_TRAP_XF,		asm_exc_simd_coprocessor_error),

#ifdef CONFIG_X86_32
//	TSKG(X86_TRAP_DF,		GDT_ENTRY_DOUBLEFAULT_TSS),
#else
	INTG(X86_TRAP_DF,		asm_exc_double_fault),
#endif
	INTG(X86_TRAP_DB,		asm_exc_debug),

#ifdef CONFIG_X86_MCE
	INTG(X86_TRAP_MC,		asm_exc_machine_check),
#endif

	SYSG(X86_TRAP_OF,		asm_exc_overflow),
#if defined(CONFIG_IA32_EMULATION)
	SYSG(IA32_SYSCALL_VECTOR,	entry_INT80_compat),
#elif defined(CONFIG_X86_32)
//	SYSG(IA32_SYSCALL_VECTOR,	entry_INT80_32),
#endif
};

/**
 * The APIC and SMP idt entries
 * 中断描述符表
 *
 */
static const __initconst struct idt_data apic_idts[] = {
#ifdef CONFIG_SMP
	/**
	 * @brief Rescheduling interrupts
	 *	核间中断
	 */
	INTG(RESCHEDULE_VECTOR,			asm_sysvec_reschedule_ipi),
	INTG(CALL_FUNCTION_VECTOR,		asm_sysvec_call_function),
	INTG(CALL_FUNCTION_SINGLE_VECTOR,	asm_sysvec_call_function_single),
	INTG(IRQ_MOVE_CLEANUP_VECTOR,		asm_sysvec_irq_move_cleanup),
	INTG(REBOOT_VECTOR,			asm_sysvec_reboot),
#endif

#ifdef CONFIG_X86_THERMAL_VECTOR
	INTG(THERMAL_APIC_VECTOR,		asm_sysvec_thermal),
#endif

#ifdef CONFIG_X86_MCE_THRESHOLD
	INTG(THRESHOLD_APIC_VECTOR,		asm_sysvec_threshold),
#endif

#ifdef CONFIG_X86_MCE_AMD
	INTG(DEFERRED_ERROR_VECTOR,		asm_sysvec_deferred_error),
#endif

#ifdef CONFIG_X86_LOCAL_APIC
    /**
     *
     */
	INTG(LOCAL_TIMER_VECTOR,		asm_sysvec_apic_timer_interrupt),
	INTG(X86_PLATFORM_IPI_VECTOR,		asm_sysvec_x86_platform_ipi),
# ifdef CONFIG_HAVE_KVM
	INTG(POSTED_INTR_VECTOR,		asm_sysvec_kvm_posted_intr_ipi),
	INTG(POSTED_INTR_WAKEUP_VECTOR,		asm_sysvec_kvm_posted_intr_wakeup_ipi),
	INTG(POSTED_INTR_NESTED_VECTOR,		asm_sysvec_kvm_posted_intr_nested_ipi),
# endif
# ifdef CONFIG_IRQ_WORK
	INTG(IRQ_WORK_VECTOR,			asm_sysvec_irq_work),
# endif
	INTG(SPURIOUS_APIC_VECTOR,		asm_sysvec_spurious_apic_interrupt),
	INTG(ERROR_APIC_VECTOR,			asm_sysvec_error_interrupt),
#endif
};

/* Must be page-aligned because the real IDT is used in the cpu entry area */
/**
 *  中断描述符表
 *  中断描述符表, 所有中断 256
 *
 * To create an IDT, simply create a 256-entry array of descriptors.
 * https://wiki.osdev.org/Interrupts_tutorial
 *
 * IDT_ENTRIES = 256
 */
static gate_desc __page_aligned_bss idt_table[IDT_ENTRIES];

/**
 *  中断描述符表描述符
 */
static struct desc_ptr __ro_after_init idt_descr  = {
	idt_descr.size		= IDT_TABLE_SIZE - 1,
	idt_descr.address	= (unsigned long) idt_table,
};

void load_current_idt(void)
{
	lockdep_assert_irqs_disabled();
	load_idt(&idt_descr);
}

#ifdef CONFIG_X86_F00F_BUG
//bool idt_is_f00f_address(unsigned long address)
//{
//	return ((address - idt_descr.address) >> 3) == 6;
//}
#endif

/**
 *  拷贝中断描述符表
 */
static __init void
idt_setup_from_table(gate_desc *idt, const struct idt_data *t, int size, bool sys)
{
    /* 门 */
	gate_desc desc;

	for (; size > 0; t++, size--) {
        /**
         *  初始化
         */
		idt_init_desc(&desc, t);
        /**
         *  将 desc 拷贝至 idt 对应 vector 中(写入 CPU)
         */
		write_idt_entry(idt, t->vector, &desc);
		if (sys)
			set_bit(t->vector, system_vectors);
	}
}

/**
 * @brief
 *
 * @n       中断号
 * @addr    中断/异常处理函数的基地址
 */
static __init void set_intr_gate(unsigned int n, const void *addr)
{
	struct idt_data data;

    /**
     * @brief 生成一个中断门(赋值)
     *
     */
	init_idt_data(&data, n, addr);

    /**
     * @brief 将中断门插入至 `IDT` 表中
     *
     */
	idt_setup_from_table(idt_table, &data, 1, false);
}

/**
 * idt_setup_early_traps - Initialize the idt table with early traps
 *
 * On X8664 these traps do not use interrupt stacks as they can't work
 * before cpu_init() is invoked and sets up TSS. The IST variants are
 * installed after that.
 *
 * 中断描述符表
 */
void __init idt_setup_early_traps(void)
{
    /**
     *  拷贝
     */
	idt_setup_from_table(idt_table, early_idts, ARRAY_SIZE(early_idts), true);

    //调用 `load_idt` 函数来执行 `ldtr` 指令来重新加载 `IDT` 表
	load_idt(&idt_descr);/* 中断描述符 */
}

/**
 * idt_setup_traps - Initialize the idt table with default traps
 */
void __init idt_setup_traps(void)   /*  中断描述符表*/
{
    /* 默认的中断描述附表 */
	idt_setup_from_table(idt_table, def_idts, ARRAY_SIZE(def_idts), true);
}

#ifdef CONFIG_X86_64
/*
 * Early traps running on the DEFAULT_STACK because the other interrupt
 * stacks work only after cpu_init().
 */
static const __initconst struct idt_data early_pf_idts[] = {
    //exc_page_fault(struct pt_regs *regs, int error_code) ???
	INTG(X86_TRAP_PF,		asm_exc_page_fault),    /* Page Fault */
};

/*
 * The exceptions which use Interrupt stacks. They are setup after
 * cpu_init() when the TSS has been initialized.
 */
static const __initconst struct idt_data ist_idts[] = { /* IST(Interrupt Stack Table) */
	ISTG(X86_TRAP_DB,	asm_exc_debug,			IST_INDEX_DB),
	ISTG(X86_TRAP_NMI,	asm_exc_nmi,			IST_INDEX_NMI), ////arch/x86/entry/entry_64.S
	ISTG(X86_TRAP_DF,	asm_exc_double_fault,		IST_INDEX_DF),
#ifdef CONFIG_X86_MCE
	ISTG(X86_TRAP_MC,	asm_exc_machine_check,		IST_INDEX_MCE),
#endif
#ifdef CONFIG_AMD_MEM_ENCRYPT
	ISTG(X86_TRAP_VC,	asm_exc_vmm_communication,	IST_INDEX_VC),
#endif
};

/**
 * idt_setup_early_pf - Initialize the idt table with early pagefault handler
 *
 * On X8664 this does not use interrupt stacks as they can't work before
 * cpu_init() is invoked and sets up TSS. The IST variant is installed
 * after that.
 *
 * Note, that X86_64 cannot install the real #PF handler in
 * idt_setup_early_traps() because the memory intialization needs the #PF
 * handler from the early_idt_handler_array to initialize the early page
 * tables.
 *
 * 用于建立 `#PF` 处理函数
 */
void __init idt_setup_early_pf(void)    /* page fault */
{
	idt_setup_from_table(idt_table, early_pf_idts, ARRAY_SIZE(early_pf_idts), true);
}

/**
 * idt_setup_ist_traps - Initialize the idt table with traps using IST
 */
void __init idt_setup_ist_traps(void)/* IST(Interrupt Stack Table) */
{
	idt_setup_from_table(idt_table, ist_idts, ARRAY_SIZE(ist_idts), true);
}
#endif

static void __init idt_map_in_cea(void)
{
	/*
	 * Set the IDT descriptor to a fixed read-only location in the cpu
	 * entry area, so that the "sidt" instruction will not leak the
	 * location of the kernel, and to defend the IDT against arbitrary
	 * memory write vulnerabilities.
	 */
	cea_set_pte(CPU_ENTRY_AREA_RO_IDT_VADDR, __pa_symbol(idt_table),
		    PAGE_KERNEL_RO);
	idt_descr.address = CPU_ENTRY_AREA_RO_IDT;
}

/**
 * idt_setup_apic_and_irq_gates - Setup APIC/SMP and normal interrupt gates
 */
void __init idt_setup_apic_and_irq_gates(void)
{
	int i = FIRST_EXTERNAL_VECTOR;
	void *entry;

	idt_setup_from_table(idt_table, apic_idts, ARRAY_SIZE(apic_idts), true);

	/**
	 * @brief 填充 idt_table
	 *	system_vectors 是 per-CPU bitmap
	 */
	for_each_clear_bit_from(i, system_vectors, FIRST_SYSTEM_VECTOR) {
		/**
		 * @brief irq_entries_start
		 *
		 */
		entry = irq_entries_start + 8 * (i - FIRST_EXTERNAL_VECTOR);
		set_intr_gate(i, entry);
	}

#ifdef CONFIG_X86_LOCAL_APIC
	for_each_clear_bit_from(i, system_vectors, NR_VECTORS) {
		/*
		 * Don't set the non assigned system vectors in the
		 * system_vectors bitmap. Otherwise they show up in
		 * /proc/interrupts.
		 */
		entry = spurious_entries_start + 8 * (i - FIRST_SYSTEM_VECTOR);
		set_intr_gate(i, entry);
	}
#endif
	/* Map IDT into CPU entry area and reload it. */
	idt_map_in_cea();
	load_idt(&idt_descr);

	/* Make the IDT table read only */
	set_memory_ro((unsigned long)&idt_table, 1);

	idt_setup_done = true;
}

/**
 * idt_setup_early_handler - Initializes the idt table with early handlers
 */
void __init idt_setup_early_handler(void)
{
	int i;
    /**
     * 在整个初期设置阶段，中断是禁用的
     * early_idt_handler_array` 数组中的每一项指向的都是同一个通用中断处理程序
	 *
	 * NUM_EXCEPTION_VECTORS=32
     */
	for (i = 0; i < NUM_EXCEPTION_VECTORS; i++)
		set_intr_gate(i, early_idt_handler_array[i]);

#ifdef CONFIG_X86_32
    /* 忽略32 - 255 */
	for ( ; i < NR_VECTORS; i++)
		set_intr_gate(i, early_ignore_irq);
#endif

    /**
     *  加载到lidt 寄存器
     */
	load_idt(&idt_descr);
}

/**
 * idt_invalidate - Invalidate interrupt descriptor table
 * @addr:	The virtual address of the 'invalid' IDT
 */
void idt_invalidate(void *addr)
{
	struct desc_ptr idt = { .address = (unsigned long) addr, .size = 0 };

	load_idt(&idt);
}

void __init alloc_intr_gate(unsigned int n, const void *addr)
{
	if (WARN_ON(n < FIRST_SYSTEM_VECTOR))
		return;

	if (WARN_ON(idt_setup_done))
		return;

	if (!WARN_ON(test_and_set_bit(n, system_vectors)))
		set_intr_gate(n, addr);
}
