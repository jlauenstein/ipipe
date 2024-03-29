#ifndef __ASM_POWERPC_MMU_CONTEXT_H
#define __ASM_POWERPC_MMU_CONTEXT_H
#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/ipipe_debug.h>
#include <asm/mmu.h>	
#include <asm/cputable.h>
#include <asm/cputhreads.h>

/*
 * Most if the context management is out of line
 */
extern int init_new_context(struct task_struct *tsk, struct mm_struct *mm);
extern void destroy_context(struct mm_struct *mm);
#ifdef CONFIG_SPAPR_TCE_IOMMU
struct mm_iommu_table_group_mem_t;

extern int isolate_lru_page(struct page *page);	/* from internal.h */
extern bool mm_iommu_preregistered(struct mm_struct *mm);
extern long mm_iommu_get(struct mm_struct *mm,
		unsigned long ua, unsigned long entries,
		struct mm_iommu_table_group_mem_t **pmem);
extern long mm_iommu_put(struct mm_struct *mm,
		struct mm_iommu_table_group_mem_t *mem);
extern void mm_iommu_init(struct mm_struct *mm);
extern void mm_iommu_cleanup(struct mm_struct *mm);
extern struct mm_iommu_table_group_mem_t *mm_iommu_lookup(struct mm_struct *mm,
		unsigned long ua, unsigned long size);
extern struct mm_iommu_table_group_mem_t *mm_iommu_find(struct mm_struct *mm,
		unsigned long ua, unsigned long entries);
extern long mm_iommu_ua_to_hpa(struct mm_iommu_table_group_mem_t *mem,
		unsigned long ua, unsigned long *hpa);
extern long mm_iommu_mapped_inc(struct mm_iommu_table_group_mem_t *mem);
extern void mm_iommu_mapped_dec(struct mm_iommu_table_group_mem_t *mem);
#endif
extern void switch_slb(struct task_struct *tsk, struct mm_struct *mm);
extern void set_context(unsigned long id, pgd_t *pgd);

#ifdef CONFIG_PPC_BOOK3S_64
extern void radix__switch_mmu_context(struct mm_struct *prev,
				     struct mm_struct *next);
static inline void switch_mmu_context(struct mm_struct *prev,
				      struct mm_struct *next,
				      struct task_struct *tsk)
{
	if (radix_enabled())
		return radix__switch_mmu_context(prev, next);
	return switch_slb(tsk, next);
}

extern int __init_new_context(void);
extern void __destroy_context(int context_id);
static inline void mmu_context_init(void) { }
#else
extern void switch_mmu_context(struct mm_struct *prev, struct mm_struct *next,
			       struct task_struct *tsk);
extern unsigned long __init_new_context(void);
extern void __destroy_context(unsigned long context_id);
extern void mmu_context_init(void);
#endif

extern void switch_cop(struct mm_struct *next);
extern int use_cop(unsigned long acop, struct mm_struct *mm);
extern void drop_cop(unsigned long acop, struct mm_struct *mm);

#if !defined(CONFIG_IPIPE) || defined(CONFIG_IPIPE_WANT_PREEMPTIBLE_SWITCH)

#define ipipe_mm_switch_protect(flags)		\
  do { (void)(flags); } while (0)

#define ipipe_mm_switch_unprotect(flags)	\
  do { (void)(flags); } while (0)

#else /* CONFIG_IPIPE && !CONFIG_IPIPE_WANT_PREEMPTIBLE_SWITCH */

#define ipipe_mm_switch_protect(flags)		\
  do {						\
    	(flags) = hard_cond_local_irq_save();	\
	barrier();				\
  } while (0)					\

#define ipipe_mm_switch_unprotect(flags)	\
  do {						\
	barrier();				\
    	hard_cond_local_irq_restore(flags);	\
  } while (0)					\

#endif /* CONFIG_IPIPE && !CONFIG_IPIPE_WANT_PREEMPTIBLE_SWITCH */

  /*
 * switch_mm is the entry point called from the architecture independent
 * code in kernel/sched/core.c
 */
static inline void __do_switch_mm(struct mm_struct *prev, struct mm_struct *next,
				  struct task_struct *tsk, bool irq_sync_p)
{
	/* Mark this context has been used on the new CPU */
	if (!cpumask_test_cpu(raw_smp_processor_id(), mm_cpumask(next))) {
		cpumask_set_cpu(raw_smp_processor_id(), mm_cpumask(next));

		/*
		 * This full barrier orders the store to the cpumask above vs
		 * a subsequent operation which allows this CPU to begin loading
		 * translations for next.
		 *
		 * When using the radix MMU that operation is the load of the
		 * MMU context id, which is then moved to SPRN_PID.
		 *
		 * For the hash MMU it is either the first load from slb_cache
		 * in switch_slb(), and/or the store of paca->mm_ctx_id in
		 * copy_mm_to_paca().
		 *
		 * On the read side the barrier is in pte_xchg(), which orders
		 * the store to the PTE vs the load of mm_cpumask.
		 */
		smp_mb();
	}

	/* 32-bit keeps track of the current PGDIR in the thread struct */
#ifdef CONFIG_PPC32
	tsk->thread.pgdir = next->pgd;
#endif /* CONFIG_PPC32 */

	/* 64-bit Book3E keeps track of current PGD in the PACA */
#ifdef CONFIG_PPC_BOOK3E_64
	get_paca()->pgd = next->pgd;
#endif
	/* Nothing else to do if we aren't actually switching */
	if (prev == next)
		return;

	if (irq_sync_p)
		hard_local_irq_enable();
	
#ifdef CONFIG_PPC_ICSWX
	/* Switch coprocessor context only if prev or next uses a coprocessor */
	if (prev->context.acop || next->context.acop)
		switch_cop(next);
#endif /* CONFIG_PPC_ICSWX */

	/* We must stop all altivec streams before changing the HW
	 * context
	 */
#ifdef CONFIG_ALTIVEC
	if (cpu_has_feature(CPU_FTR_ALTIVEC))
		asm volatile ("dssall");
#endif /* CONFIG_ALTIVEC */
	/*
	 * The actual HW switching method differs between the various
	 * sub architectures. Out of line for now
	 */
	switch_mmu_context(prev, next, tsk);

	if (irq_sync_p)
		hard_local_irq_disable();
}

static inline void __switch_mm(struct mm_struct *prev, struct mm_struct *next,
			       struct task_struct *tsk)
{
#ifdef CONFIG_IPIPE_WANT_PREEMPTIBLE_SWITCH
#if defined(CONFIG_PPC_MMU_NOHASH) && defined(CONFIG_SMP)
 /*
  * mmu_context_nohash in SMP mode is tracking an activity counter
  * into the mm struct. Therefore, we make sure the kernel always sees
  * the ipipe_percpu.active_mm update and the actual switch as a
  * single atomic operation. Since the related code already requires
  * to hard disable irqs all through the switch, there is no
  * additional penalty anyway.
  */
#define mmswitch_irq_sync false
#else
#define mmswitch_irq_sync true
#endif
	IPIPE_WARN_ONCE(hard_irqs_disabled());
	for (;;) {
		hard_local_irq_disable();
		__this_cpu_write(ipipe_percpu.active_mm, NULL);
		barrier();
		__do_switch_mm(prev, next, tsk, mmswitch_irq_sync);
		if (!test_and_clear_thread_flag(TIF_MMSWITCH_INT)) {
			__this_cpu_write(ipipe_percpu.active_mm, next);
			hard_local_irq_enable();
			return;
		}
	}
#else /* !CONFIG_IPIPE_WANT_PREEMPTIBLE_SWITCH */
	IPIPE_WARN_ONCE(!hard_irqs_disabled());
	__do_switch_mm(prev, next, tsk, false);
#endif /* !CONFIG_IPIPE_WANT_PREEMPTIBLE_SWITCH */
}

static inline void switch_mm_irqs_off(struct mm_struct *prev,
				      struct mm_struct *next,
				      struct task_struct *tsk)
{
	unsigned long flags;

	ipipe_mm_switch_protect(flags);
	__switch_mm(prev, next, tsk);
	ipipe_mm_switch_unprotect(flags);
}

#ifdef CONFIG_IPIPE
/*
 * ipipe_switch_mm_head() is reserved to the head domain for switching
 * mmu context.
 */
static inline
void ipipe_switch_mm_head(struct mm_struct *prev, struct mm_struct *next,
			  struct task_struct *tsk)
{
	__do_switch_mm(prev, next, tsk, false);
}

#endif /* CONFIG_IPIPE */

static inline void switch_mm(struct mm_struct *prev, struct mm_struct *next,
			     struct task_struct *tsk)
{
#ifndef CONFIG_IPIPE
	unsigned long flags;

	local_irq_save(flags);
#endif
	switch_mm_irqs_off(prev, next, tsk);
#ifndef CONFIG_IPIPE
	local_irq_restore(flags);
#endif
}
#define switch_mm_irqs_off switch_mm_irqs_off


#define deactivate_mm(tsk,mm)	do { } while (0)

/*
 * After we have set current->mm to a new value, this activates
 * the context for the new mm so we see the new mappings.
 */
static inline void activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	switch_mm(prev, next, current);
}

/* We don't currently use enter_lazy_tlb() for anything */
static inline void enter_lazy_tlb(struct mm_struct *mm,
				  struct task_struct *tsk)
{
	/* 64-bit Book3E keeps track of current PGD in the PACA */
#ifdef CONFIG_PPC_BOOK3E_64
	get_paca()->pgd = NULL;
#endif
}

static inline void arch_dup_mmap(struct mm_struct *oldmm,
				 struct mm_struct *mm)
{
}

static inline void arch_exit_mmap(struct mm_struct *mm)
{
}

static inline void arch_unmap(struct mm_struct *mm,
			      struct vm_area_struct *vma,
			      unsigned long start, unsigned long end)
{
	if (start <= mm->context.vdso_base && mm->context.vdso_base < end)
		mm->context.vdso_base = 0;
}

static inline void arch_bprm_mm_init(struct mm_struct *mm,
				     struct vm_area_struct *vma)
{
}

static inline bool arch_vma_access_permitted(struct vm_area_struct *vma,
		bool write, bool execute, bool foreign)
{
	/* by default, allow everything */
	return true;
}

static inline bool arch_pte_access_permitted(pte_t pte, bool write)
{
	/* by default, allow everything */
	return true;
}
#endif /* __KERNEL__ */
#endif /* __ASM_POWERPC_MMU_CONTEXT_H */
