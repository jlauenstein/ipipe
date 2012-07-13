/* -*- linux-c -*-
 * include/asm-powerpc/ipipe_hwirq.h
 *
 * Copyright (C) 2009 Philippe Gerum.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 * USA; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _ASM_POWERPC_IPIPE_HWIRQ_H

#ifdef CONFIG_PPC32

#if defined(CONFIG_BOOKE)
#define hard_local_irq_restore_notrace(x)	__asm__ __volatile__("wrtee %0" : : "r" (x) : "memory")
#else
#define hard_local_irq_restore_notrace(x)	mtmsr(x)
#endif

static inline void hard_local_irq_disable_notrace(void)
{
#ifdef CONFIG_BOOKE
	__asm__ __volatile__("wrteei 0": : :"memory");
#else
	unsigned long msr = mfmsr();
	mtmsr(msr & ~MSR_EE);
#endif
}

static inline void hard_local_irq_enable_notrace(void)
{
#ifdef CONFIG_BOOKE
	__asm__ __volatile__("wrteei 1": : :"memory");
#else
	unsigned long msr = mfmsr();
	mtmsr(msr | MSR_EE);
#endif
}

static inline unsigned long hard_local_irq_save_notrace(void)
{
	unsigned long msr = mfmsr();
#ifdef CONFIG_BOOKE
	__asm__ __volatile__("wrteei 0": : :"memory");
#else
	mtmsr(msr & ~MSR_EE);
#endif
	return msr;
}

#else /* CONFIG_PPC64 */

#include <asm/paca.h>

#ifdef CONFIG_PPC_BOOK3E
static inline void hard_local_irq_disable_notrace(void)
{
	__asm__ __volatile__("wrteei 0": : :"memory");
}

static inline void hard_local_irq_enable_notrace(void)
{
	__asm__ __volatile__("wrteei 1": : :"memory");
}
#else /* !CONFIG_PPC_BOOK3E */
static inline void hard_local_irq_disable_notrace(void)
{
	__mtmsrd(mfmsr() & ~MSR_EE, 1);
}

static inline void hard_local_irq_enable_notrace(void)
{
	__mtmsrd(mfmsr() | MSR_EE, 1);
}
#endif /* !CONFIG_PPC_BOOK3E */

static inline unsigned long hard_local_irq_save_notrace(void)
{
	unsigned long msr = mfmsr();
	hard_local_irq_disable_notrace();
	return msr;
}

#define hard_local_irq_restore_notrace(x)	__mtmsrd(x, 1)

#endif /* CONFIG_PPC64 */

#ifdef CONFIG_IPIPE

#include <linux/ipipe_base.h>
#include <linux/ipipe_trace.h>

static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return (flags & (MSR_EE|MSR_SOFTEE)) == 0;
}

static inline unsigned long arch_local_irq_disable(void)
{
	unsigned long flags;

	flags = soft_local_irq_disable() << MSR_SOFTEE_LG;
	barrier();
	flags |= (!ipipe_test_and_stall_root()) << MSR_EE_LG;
	barrier();

	return flags;
}

static inline void arch_local_irq_enable(void)
{
	barrier();
	ipipe_unstall_root();
	soft_local_irq_restore(1);
}

static inline void arch_local_irq_restore(unsigned long flags)
{
	barrier();
	if (flags & MSR_EE)
		ipipe_unstall_root();
	if (flags & MSR_SOFTEE)
		soft_local_irq_restore(1);
}

static inline unsigned long arch_local_irq_save(void)
{
	return arch_local_irq_disable();
}

static inline unsigned long arch_local_save_flags(void)
{
	unsigned long flags;

	flags = soft_local_save_flags() << MSR_SOFTEE_LG;
	flags |= (!ipipe_test_root()) << MSR_EE_LG;

	return flags;
}

static inline int arch_irqs_disabled(void)
{
	unsigned long flags = arch_local_save_flags();

	return arch_irqs_disabled_flags(flags);
}

static inline unsigned long arch_mangle_irq_bits(int virt, unsigned long real)
{
	/* Merge virtual and real interrupt mask bits. */
	return (real & ~MSR_VIRTEE) | ((long)(virt != 0) << MSR_VIRTEE_LG);
}

static inline int arch_demangle_irq_bits(unsigned long *x)
{
	int virt = (*x & MSR_VIRTEE) != 0;

	*x &= ~MSR_VIRTEE;

	return virt;
}

static inline int hard_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(mfmsr());
}

static inline unsigned long hard_local_save_flags(void)
{
	return mfmsr();
}

#ifdef CONFIG_IPIPE_TRACE_IRQSOFF

static inline void hard_local_irq_disable(void)
{
	if (!hard_irqs_disabled()) {
		hard_local_irq_disable_notrace();
		ipipe_trace_begin(0x80000000);
	}
}

static inline void hard_local_irq_enable(void)
{
	if (hard_irqs_disabled()) {
		ipipe_trace_end(0x80000000);
		hard_local_irq_enable_notrace();
	}
}

static inline unsigned long hard_local_irq_save(void)
{
	unsigned long flags;

	flags = hard_local_irq_save_notrace();
	if (!arch_irqs_disabled_flags(flags))
		ipipe_trace_begin(0x80000001);

	return flags;
}

static inline void hard_local_irq_restore(unsigned long x)
{
	if (!arch_irqs_disabled_flags(x))
		ipipe_trace_end(0x80000001);

	hard_local_irq_restore_notrace(x);
}

#else /* !CONFIG_IPIPE_TRACE_IRQSOFF */

#define hard_local_irq_disable    hard_local_irq_disable_notrace
#define hard_local_irq_enable     hard_local_irq_enable_notrace
#define hard_local_irq_save       hard_local_irq_save_notrace
#define hard_local_irq_restore    hard_local_irq_restore_notrace

#endif /* CONFIG_IPIPE_TRACE_IRQSOFF */

#else /* !CONFIG_IPIPE */

#define hard_local_irq_save()		arch_local_irq_save()
#define hard_local_irq_restore(x)	arch_local_irq_restore(x)
#define hard_local_irq_enable()		arch_local_irq_enable()
#define hard_local_irq_disable()	arch_local_irq_disable()
#define hard_irqs_disabled()		arch_irqs_disabled()

#define hard_cond_local_irq_enable()		do { } while(0)
#define hard_cond_local_irq_disable()		do { } while(0)
#define hard_cond_local_irq_save()		0
#define hard_cond_local_irq_restore(flags)	do { (void)(flags); } while(0)

#endif /* !CONFIG_IPIPE */

#if defined(CONFIG_SMP) && defined(CONFIG_IPIPE)
#define hard_smp_local_irq_save()		hard_local_irq_save()
#define hard_smp_local_irq_restore(flags)	hard_local_irq_restore(flags)
#else /* !CONFIG_SMP */
#define hard_smp_local_irq_save()		0
#define hard_smp_local_irq_restore(flags)	do { (void)(flags); } while(0)
#endif /* CONFIG_SMP */

#endif /* !_ASM_POWERPC_IPIPE_HWIRQ_H */
