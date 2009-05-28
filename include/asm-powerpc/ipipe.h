/*
 *   include/asm-powerpc/ipipe.h
 *
 *   I-pipe 32/64bit merge - Copyright (C) 2007 Philippe Gerum.
 *   I-pipe PA6T support - Copyright (C) 2007 Philippe Gerum.
 *   I-pipe 64-bit PowerPC port - Copyright (C) 2005 Heikki Lindholm.
 *   I-pipe PowerPC support - Copyright (C) 2002-2005 Philippe Gerum.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 *   USA; either version 2 of the License, or (at your option) any later
 *   version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __ASM_POWERPC_IPIPE_H
#define __ASM_POWERPC_IPIPE_H

#ifdef CONFIG_IPIPE

#include <asm/ptrace.h>
#include <asm/hw_irq.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <linux/ipipe_percpu.h>
#include <linux/list.h>
#include <linux/cpumask.h>
#include <linux/cache.h>
#include <linux/threads.h>

#define IPIPE_ARCH_STRING	"1.9-00"
#define IPIPE_MAJOR_NUMBER	1
#define IPIPE_MINOR_NUMBER	9
#define IPIPE_PATCH_NUMBER	0

#define prepare_arch_switch(next)			\
	do {						\
		ipipe_schedule_notify(current ,next);	\
		local_irq_disable_hw();			\
	} while(0)

#define task_hijacked(p)						\
	({								\
		int __x__ = ipipe_root_domain_p;			\
		__clear_bit(IPIPE_SYNC_FLAG, &ipipe_root_cpudom_var(status)); \
		if (__x__) local_irq_enable_hw();			\
		!__x__;							\
	})

struct ipipe_domain;

struct ipipe_sysinfo {

	int ncpus;			/* Number of CPUs on board */
	u64 cpufreq;			/* CPU frequency (in Hz) */

	/* Arch-dependent block */

	struct {
		unsigned tmirq;		/* Decrementer virtual IRQ */
		u64 tmfreq;		/* Timebase frequency */
	} archdep;
};

#define ipipe_read_tsc(t)					\
	({							\
		unsigned long __tbu;				\
		__asm__ __volatile__ ("1: mftbu %0\n"		\
				      "mftb %1\n"		\
				      "mftbu %2\n"		\
				      "cmpw %2,%0\n"		\
				      "bne- 1b\n"		\
				      :"=r" (((unsigned long *)&t)[0]),	\
				       "=r" (((unsigned long *)&t)[1]),	\
				       "=r" (__tbu));			\
		t;							\
	})

#define __ipipe_read_timebase()					\
	({							\
 	unsigned long long t;					\
 	ipipe_read_tsc(t);					\
 	t;							\
 	})

#ifdef CONFIG_PPC_MERGE
extern unsigned long tb_ticks_per_jiffy;
#else
extern unsigned int tb_ticks_per_jiffy;
#endif

extern unsigned long disarm_decr[];

#define ipipe_cpu_freq()	(HZ * tb_ticks_per_jiffy)
#define ipipe_tsc2ns(t)		((((unsigned long)(t)) * 1000) / (ipipe_cpu_freq() / 1000000))
#define ipipe_tsc2us(t)						\
	({							\
		unsigned long long delta = (t);			\
		do_div(delta, ipipe_cpu_freq()/1000000+1);	\
		(unsigned long)delta;				\
	})

/* Private interface -- Internal use only */

#define __ipipe_check_platform()	do { } while(0)
#define __ipipe_enable_irq(irq)		enable_irq(irq)
#define __ipipe_disable_irq(irq)	disable_irq(irq)

void __ipipe_enable_irqdesc(struct ipipe_domain *ipd, unsigned irq);

static inline void __ipipe_disable_irqdesc(struct ipipe_domain *ipd, unsigned irq)
{
}

void __ipipe_init_platform(void);

void __ipipe_enable_pipeline(void);

int __ipipe_ack_irq(unsigned irq);

void __ipipe_end_irq(unsigned irq);

#ifdef CONFIG_SMP
struct ipipe_ipi_struct {
	volatile unsigned long value;
} ____cacheline_aligned;

void __ipipe_hook_critical_ipi(struct ipipe_domain *ipd);

void __ipipe_register_ipi(unsigned int irq);
#else
#define __ipipe_hook_critical_ipi(ipd)	do { } while(0)
#endif /* CONFIG_SMP */

extern unsigned long __ipipe_decr_ticks;

DECLARE_PER_CPU(unsigned long long, __ipipe_decr_next);

DECLARE_PER_CPU(struct pt_regs, __ipipe_tick_regs);

void __ipipe_handle_irq(int irq, struct pt_regs *regs);

struct irq_desc;
void __ipipe_ack_level_irq(unsigned irq, struct irq_desc *desc);
void __ipipe_end_level_irq(unsigned irq, struct irq_desc *desc);
void __ipipe_ack_edge_irq(unsigned irq, struct irq_desc *desc);
void __ipipe_end_edge_irq(unsigned irq, struct irq_desc *desc);

void __ipipe_serial_debug(const char *fmt, ...);

static inline unsigned long __ipipe_ffnz(unsigned long ul)
{
	__asm__ __volatile__("cntlzw %0, %1":"=r"(ul):"r"(ul & (-ul)));
	return 31 - ul;
}

/* When running handlers, enable hw interrupts for all domains but the
 * one heading the pipeline, so that IRQs can never be significantly
 * deferred for the latter. */
#define __ipipe_run_isr(ipd, irq)					\
do {									\
	if (!__ipipe_pipeline_head_p(ipd))				\
		local_irq_enable_hw();					\
	if (ipd == ipipe_root_domain)					\
		if (likely(!ipipe_virtual_irq_p(irq)))			\
			ipd->irqs[irq].handler(irq, NULL);		\
		else {							\
			irq_enter();					\
			ipd->irqs[irq].handler(irq, ipd->irqs[irq].cookie);\
			irq_exit();					\
		}							\
	else {								\
		__clear_bit(IPIPE_SYNC_FLAG, &ipipe_cpudom_var(ipd, status)); \
		ipd->irqs[irq].handler(irq, ipd->irqs[irq].cookie);	\
		__set_bit(IPIPE_SYNC_FLAG, &ipipe_cpudom_var(ipd, status)); \
	}								\
	local_irq_disable_hw();						\
} while(0)

#define __ipipe_syscall_watched_p(p, sc)	\
	(((p)->flags & PF_EVNOTIFY) || (unsigned long)sc >= NR_syscalls)

#else /* !CONFIG_IPIPE */

#define task_hijacked(p)	0

#endif /* CONFIG_IPIPE */

#endif /* !__ASM_POWERPC_IPIPE_H */
