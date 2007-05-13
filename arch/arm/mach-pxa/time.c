/*
 * arch/arm/mach-pxa/time.c
 *
 * Author:	Nicolas Pitre
 * Created:	Jun 15, 2001
 * Copyright:	MontaVista Software Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/module.h>

#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/leds.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/mach/time.h>
#include <asm/arch/pxa-regs.h>


#ifdef CONFIG_IPIPE
#ifdef CONFIG_NO_IDLE_HZ
#error "dynamic tick timer not yet supported with IPIPE"
#endif /* CONFIG_NO_IDLE_HZ */
int __ipipe_mach_timerint = IRQ_OST0;
EXPORT_SYMBOL(__ipipe_mach_timerint);

int __ipipe_mach_timerstolen = 0;
EXPORT_SYMBOL(__ipipe_mach_timerstolen);

unsigned int __ipipe_mach_ticks_per_jiffy = LATCH;
EXPORT_SYMBOL(__ipipe_mach_ticks_per_jiffy);

static int pxa_timer_initialized;
static unsigned long last_jiffy_time;
#endif /* CONFIG_IPIPE */

static inline unsigned long pxa_get_rtc_time(void)
{
	return RCNR;
}

static int pxa_set_rtc(void)
{
	unsigned long current_time = xtime.tv_sec;

	if (RTSR & RTSR_ALE) {
		/* make sure not to forward the clock over an alarm */
		unsigned long alarm = RTAR;
		if (current_time >= alarm && alarm >= RCNR)
			return -ERESTARTSYS;
	}
	RCNR = current_time;
	return 0;
}

/* IRQs are disabled before entering here from do_gettimeofday() */
static unsigned long pxa_gettimeoffset (void)
{
	long ticks_to_match, elapsed, usec;

#ifdef CONFIG_IPIPE
	if (!__ipipe_mach_timerstolen) {
#endif
	/* Get ticks before next timer match */
	ticks_to_match = OSMR0 - OSCR;

	/* We need elapsed ticks since last match */
	elapsed = LATCH - ticks_to_match;

	/* don't get fooled by the workaround in pxa_timer_interrupt() */
	if (elapsed <= 0)
		return 0;
#ifdef CONFIG_IPIPE
	} else
		elapsed = OSCR - last_jiffy_time;
#endif

	/* Now convert them to usec */
	usec = (unsigned long)(elapsed * (tick_nsec / 1000))/LATCH;

	return usec;
}

#ifdef CONFIG_NO_IDLE_HZ
static unsigned long initial_match;
static int match_posponed;
#endif

static irqreturn_t
pxa_timer_interrupt(int irq, void *dev_id)
{
	int next_match;

	write_seqlock(&xtime_lock);

#ifdef CONFIG_NO_IDLE_HZ
	if (match_posponed) {
		match_posponed = 0;
		OSMR0 = initial_match;
	}
#endif

	/* Loop until we get ahead of the free running timer.
	 * This ensures an exact clock tick count and time accuracy.
	 * Since IRQs are disabled at this point, coherence between
	 * lost_ticks(updated in do_timer()) and the match reg value is
	 * ensured, hence we can use do_gettimeofday() from interrupt
	 * handlers.
	 *
	 * HACK ALERT: it seems that the PXA timer regs aren't updated right
	 * away in all cases when a write occurs.  We therefore compare with
	 * 8 instead of 0 in the while() condition below to avoid missing a
	 * match if OSCR has already reached the next OSMR value.
	 * Experience has shown that up to 6 ticks are needed to work around
	 * this problem, but let's use 8 to be conservative.  Note that this
	 * affect things only when the timer IRQ has been delayed by nearly
	 * exactly one tick period which should be a pretty rare event.
	 */
#ifdef CONFIG_IPIPE
	/*
	 * - if Linux is running natively (no ipipe), ack and reprogram the timer
	 * - if Linux is running under ipipe, but it still has the control over
	 *   the timer (no Xenomai for example), then reprogram the timer (ipipe
	 *   has already acked it)
	 * - if some other domain has taken over the timer, then do nothing
	 *   (ipipe has acked it, and the other domain has reprogramed it)
	 */
	if (__ipipe_mach_timerstolen) {
		timer_tick();
		last_jiffy_time += LATCH;
	} else
#endif /* CONFIG_IPIPE */
	do {
		timer_tick();
#ifdef CONFIG_IPIPE
		last_jiffy_time += LATCH;
#else /* !CONFIG_IPIPE */
		OSSR = OSSR_M0;  /* Clear match on timer 0 */
#endif /* !CONFIG_IPIPE */
		next_match = (OSMR0 += LATCH);
	} while( (signed long)(next_match - OSCR) <= 8 );

	write_sequnlock(&xtime_lock);

	return IRQ_HANDLED;
}

static struct irqaction pxa_timer_irq = {
	.name		= "PXA Timer Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER,
	.handler	= pxa_timer_interrupt,
};

static void __init pxa_timer_init(void)
{
	struct timespec tv;

	set_rtc = pxa_set_rtc;

	tv.tv_nsec = 0;
	tv.tv_sec = pxa_get_rtc_time();
	do_settimeofday(&tv);

	OIER = 0;		/* disable any timer interrupts */
	OSCR = LATCH*2;		/* push OSCR out of the way */
	OSMR0 = LATCH;		/* set initial match */
	OSSR = 0xf;		/* clear status on all timers */
	setup_irq(IRQ_OST0, &pxa_timer_irq);
	OIER = OIER_E0;		/* enable match on timer 0 to cause interrupts */
	OSCR = 0;		/* initialize free-running timer */

#ifdef CONFIG_IPIPE
	pxa_timer_initialized = 1;
#endif /* CONFIG_IPIPE */
}

#ifdef CONFIG_NO_IDLE_HZ
static int pxa_dyn_tick_enable_disable(void)
{
	/* nothing to do */
	return 0;
}

static void pxa_dyn_tick_reprogram(unsigned long ticks)
{
	if (ticks > 1) {
		initial_match = OSMR0;
		OSMR0 = initial_match + ticks * LATCH;
		match_posponed = 1;
	}
}

static irqreturn_t
pxa_dyn_tick_handler(int irq, void *dev_id)
{
	if (match_posponed) {
		match_posponed = 0;
		OSMR0 = initial_match;
		if ( (signed long)(initial_match - OSCR) <= 8 )
			return pxa_timer_interrupt(irq, dev_id);
	}
	return IRQ_NONE;
}

static struct dyn_tick_timer pxa_dyn_tick = {
	.enable		= pxa_dyn_tick_enable_disable,
	.disable	= pxa_dyn_tick_enable_disable,
	.reprogram	= pxa_dyn_tick_reprogram,
	.handler	= pxa_dyn_tick_handler,
};
#endif

#ifdef CONFIG_PM
static unsigned long osmr[4], oier;

static void pxa_timer_suspend(void)
{
	osmr[0] = OSMR0;
	osmr[1] = OSMR1;
	osmr[2] = OSMR2;
	osmr[3] = OSMR3;
	oier = OIER;
}

static void pxa_timer_resume(void)
{
	OSMR0 = osmr[0];
	OSMR1 = osmr[1];
	OSMR2 = osmr[2];
	OSMR3 = osmr[3];
	OIER = oier;

	/*
	 * OSMR0 is the system timer: make sure OSCR is sufficiently behind
	 */
	OSCR = OSMR0 - LATCH;
}
#else
#define pxa_timer_suspend NULL
#define pxa_timer_resume NULL
#endif

struct sys_timer pxa_timer = {
	.init		= pxa_timer_init,
	.suspend	= pxa_timer_suspend,
	.resume		= pxa_timer_resume,
	.offset		= pxa_gettimeoffset,
#ifdef CONFIG_NO_IDLE_HZ
	.dyn_tick	= &pxa_dyn_tick,
#endif
};

#ifdef CONFIG_IPIPE
void __ipipe_mach_acktimer(void)
{
	OSSR = OSSR_M0;  /* Clear match on timer 0 */
}

notrace unsigned long long __ipipe_mach_get_tsc(void)
{
	if (likely(pxa_timer_initialized)) {
		static union {
#ifdef __BIG_ENDIAN
			struct {
				unsigned long high;
				unsigned long low;
			};
#else /* __LITTLE_ENDIAN */
			struct {
				unsigned long low;
				unsigned long high;
			};
#endif /* __LITTLE_ENDIAN */
			unsigned long long full;
		} tsc[NR_CPUS], *local_tsc;
		unsigned long stamp, flags;
		unsigned long long result;

		local_irq_save_hw_notrace(flags);
		local_tsc = &tsc[ipipe_processor_id()];
		stamp = OSCR;
		if (unlikely(stamp < local_tsc->low))
			/* 32 bit counter wrapped, increment high word. */
			local_tsc->high++;
		local_tsc->low = stamp;
		result = local_tsc->full;
		local_irq_restore_hw_notrace(flags);

		return result;
	}
	
        return 0;
}
EXPORT_SYMBOL(__ipipe_mach_get_tsc);

/*
 * Reprogram the timer
 */

void __ipipe_mach_set_dec(unsigned long delay)
{
	if (delay > 8) {
		unsigned long flags;

		local_irq_save_hw(flags);
		OSMR0 = delay + OSCR;
		local_irq_restore_hw(flags);
	} else
		ipipe_trigger_irq(IRQ_OST0);
}
EXPORT_SYMBOL(__ipipe_mach_set_dec);

void __ipipe_mach_release_timer(void)
{
       __ipipe_mach_set_dec(__ipipe_mach_ticks_per_jiffy);
}
EXPORT_SYMBOL(__ipipe_mach_release_timer);

unsigned long __ipipe_mach_get_dec(void)
{
	return OSMR0 - OSCR;
}
#endif /* CONFIG_IPIPE */
