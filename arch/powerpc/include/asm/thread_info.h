/* thread_info.h: PowerPC low-level thread information
 * adapted from the i386 version by Paul Mackerras
 *
 * Copyright (C) 2002  David Howells (dhowells@redhat.com)
 * - Incorporating suggestions made by Linus Torvalds and Dave Miller
 */

#ifndef _ASM_POWERPC_THREAD_INFO_H
#define _ASM_POWERPC_THREAD_INFO_H

#ifdef __KERNEL__

/* We have 8k stacks on ppc32 and 16k on ppc64 */

#if defined(CONFIG_PPC64)
#define THREAD_SHIFT		14
#elif defined(CONFIG_PPC_256K_PAGES)
#define THREAD_SHIFT		15
#else
#define THREAD_SHIFT		13
#endif

#define THREAD_SIZE		(1 << THREAD_SHIFT)

#ifdef CONFIG_PPC64
#define CURRENT_THREAD_INFO(dest, sp)	stringify_in_c(clrrdi dest, sp, THREAD_SHIFT)
#else
#define CURRENT_THREAD_INFO(dest, sp)	stringify_in_c(rlwinm dest, sp, 0, 0, 31-THREAD_SHIFT)
#endif

#ifndef __ASSEMBLY__
#include <linux/cache.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <linux/stringify.h>
#include <asm/accounting.h>
#include <ipipe/thread_info.h>

/*
 * low level task data.
 */
struct thread_info {
	struct task_struct *task;		/* main task structure */
	int		cpu;			/* cpu we're on */
	int		preempt_count;		/* 0 => preemptable,
						   <0 => BUG */
	unsigned long	local_flags;		/* private flags for thread */
#ifdef CONFIG_LIVEPATCH
	unsigned long *livepatch_sp;
#endif
#if defined(CONFIG_VIRT_CPU_ACCOUNTING_NATIVE) && defined(CONFIG_PPC32)
	struct cpu_accounting_data accounting;
#endif
	/* low level flags - has atomic operations done on it */
	unsigned long	flags ____cacheline_aligned_in_smp;
#ifdef CONFIG_IPIPE
	unsigned long ipipe_flags;
#endif
	struct ipipe_threadinfo ipipe_data;
};

/*
 * macros/functions for gaining access to the thread information structure
 */
#define INIT_THREAD_INFO(tsk)			\
{						\
	.task =		&tsk,			\
	.cpu =		0,			\
	.preempt_count = INIT_PREEMPT_COUNT,	\
	.flags =	0,			\
}

#define init_thread_info	(init_thread_union.thread_info)
#define init_stack		(init_thread_union.stack)

#define THREAD_SIZE_ORDER	(THREAD_SHIFT - PAGE_SHIFT)

/* how to get the thread information struct from C */
static inline struct thread_info *current_thread_info(void)
{
	unsigned long val;

	asm (CURRENT_THREAD_INFO(%0,1) : "=r" (val));

	return (struct thread_info *)val;
}

#endif /* __ASSEMBLY__ */

/*
 * thread information flag bit numbers
 */
#define TIF_SYSCALL_TRACE	0	/* syscall trace active */
#define TIF_SIGPENDING		1	/* signal pending */
#define TIF_NEED_RESCHED	2	/* rescheduling necessary */
#define TIF_POLLING_NRFLAG	3	/* true if poll_idle() is polling
					   TIF_NEED_RESCHED */
#define TIF_32BIT		4	/* 32 bit binary */
#define TIF_RESTORE_TM		5	/* need to restore TM FP/VEC/VSX */
#define TIF_SYSCALL_AUDIT	7	/* syscall auditing active */
#define TIF_SINGLESTEP		8	/* singlestepping active */
#define TIF_NOHZ		9	/* in adaptive nohz mode */
#define TIF_SECCOMP		10	/* secure computing */
#define TIF_RESTOREALL		11	/* Restore all regs (implies NOERROR) */
#define TIF_NOERROR		12	/* Force successful syscall return */
#define TIF_NOTIFY_RESUME	13	/* callback before returning to user */
#define TIF_UPROBE		14	/* breakpointed or single-stepping */
#define TIF_SYSCALL_TRACEPOINT	15	/* syscall tracepoint instrumentation */
#define TIF_EMULATE_STACK_STORE	16	/* Is an instruction emulation
						for stack store? */
#define TIF_MEMDIE		17	/* is terminating due to OOM killer */
#if defined(CONFIG_PPC64)
#define TIF_ELF2ABI		18	/* function descriptors must die! */
#endif
#define TIF_MMSWITCH_INT	20	/* MMU context switch interrupted */

/* as above, but as bit values */
#define _TIF_SYSCALL_TRACE	(1<<TIF_SYSCALL_TRACE)
#define _TIF_SIGPENDING		(1<<TIF_SIGPENDING)
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)
#define _TIF_POLLING_NRFLAG	(1<<TIF_POLLING_NRFLAG)
#define _TIF_32BIT		(1<<TIF_32BIT)
#define _TIF_RESTORE_TM		(1<<TIF_RESTORE_TM)
#define _TIF_SYSCALL_AUDIT	(1<<TIF_SYSCALL_AUDIT)
#define _TIF_SINGLESTEP		(1<<TIF_SINGLESTEP)
#define _TIF_SECCOMP		(1<<TIF_SECCOMP)
#define _TIF_RESTOREALL		(1<<TIF_RESTOREALL)
#define _TIF_NOERROR		(1<<TIF_NOERROR)
#define _TIF_NOTIFY_RESUME	(1<<TIF_NOTIFY_RESUME)
#define _TIF_UPROBE		(1<<TIF_UPROBE)
#define _TIF_SYSCALL_TRACEPOINT	(1<<TIF_SYSCALL_TRACEPOINT)
#define _TIF_EMULATE_STACK_STORE	(1<<TIF_EMULATE_STACK_STORE)
#define _TIF_NOHZ		(1<<TIF_NOHZ)
#define _TIF_MMSWITCH_INT	(1<<TIF_MMSWITCH_INT)
#define _TIF_SYSCALL_DOTRACE	(_TIF_SYSCALL_TRACE | _TIF_SYSCALL_AUDIT | \
				 _TIF_SECCOMP | _TIF_SYSCALL_TRACEPOINT | \
				 _TIF_NOHZ)

#define _TIF_USER_WORK_MASK	(_TIF_SIGPENDING | _TIF_NEED_RESCHED | \
				 _TIF_NOTIFY_RESUME | _TIF_UPROBE | \
				 _TIF_RESTORE_TM)
#define _TIF_PERSYSCALL_MASK	(_TIF_RESTOREALL|_TIF_NOERROR)

/* ti->ipipe_flags */
#define TIP_MAYDAY	0	/* MAYDAY call is pending */
#define TIP_NOTIFY	1	/* Notify head domain about kernel events */
#define TIP_HEAD	2	/* Runs in head domain */

#define _TIP_MAYDAY	(1<<TIP_MAYDAY)
#define _TIP_NOTIFY	(1<<TIP_NOTIFY)
#define _TIP_HEAD	(1<<TIP_HEAD)

/* Bits in local_flags */
/* Don't move TLF_NAPPING without adjusting the code in entry_32.S */
#define TLF_NAPPING		0	/* idle thread enabled NAP mode */
#define TLF_SLEEPING		1	/* suspend code enabled SLEEP mode */
#define TLF_LAZY_MMU		3	/* tlb_batch is active */
#define TLF_RUNLATCH		4	/* Is the runlatch enabled? */

#define _TLF_NAPPING		(1 << TLF_NAPPING)
#define _TLF_SLEEPING		(1 << TLF_SLEEPING)
#define _TLF_LAZY_MMU		(1 << TLF_LAZY_MMU)
#define _TLF_RUNLATCH		(1 << TLF_RUNLATCH)

#ifndef __ASSEMBLY__

static inline bool test_thread_local_flags(unsigned int flags)
{
	struct thread_info *ti = current_thread_info();
	return (ti->local_flags & flags) != 0;
}

#ifdef CONFIG_PPC64
#define is_32bit_task()	(test_thread_flag(TIF_32BIT))
#else
#define is_32bit_task()	(1)
#endif

#if defined(CONFIG_PPC64)
#define is_elf2_task() (test_thread_flag(TIF_ELF2ABI))
#else
#define is_elf2_task() (0)
#endif

#endif	/* !__ASSEMBLY__ */

#endif /* __KERNEL__ */

#endif /* _ASM_POWERPC_THREAD_INFO_H */
