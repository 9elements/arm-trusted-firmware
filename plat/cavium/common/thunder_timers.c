/** @file

  Copyright (c) 2016-2017, Cavium Inc. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <bl_common.h>
#include <hw_timers.h>
#include <interrupt_mgmt.h>
#include <stdio.h>
#include <thunder_private.h>
#include <delay_timer.h>

#undef DEBUG_TIMERS

#ifdef DEBUG_TIMERS
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...)
#endif

static uint64_t curr_period;
static uint32_t timer_enabled;
static hw_timer_isr_t timer_isr = NULL;

/* Secure architectural physical counter methods */

static void cntps_enable(int enable)
{
	uint32_t ctl;

	__asm__ volatile ("mrs %[ctl], cntps_ctl_el1" : [ctl] "=r" (ctl));

	if (enable) {
		/* enable timer with masked interrupts */
		ctl = 0x1;
	} else {
		/* disable timer */
		ctl &= ~(1 << 0);
	}

	/* write ctl value */
	__asm__ volatile ("msr cntps_ctl_el1, %[ctl]" :: [ctl] "r" (ctl));
}

/* max period is over 18e10 seconds */
void cntps_set_period(uint64_t period)
{
	uint64_t timer_status;

	/* read current timer counter value */
	__asm__ volatile ("mrs %[stat], cntpct_el0" : [stat] "=r" (timer_status));

	period += timer_status;

	__asm__ volatile ("msr cntps_cval_el1, %[period]" :: [period] "r" (period));
}

uint64_t cntps_ms_to_ticks(uint32_t time)
{
	uint32_t cnt_freq; /* in Hz */

	/* gather counter frequency */
	__asm__ volatile("mrs %[freq], cntfrq_el0" : [freq] "=r" (cnt_freq));

	return((cnt_freq / 1000) * time);
}

static uint64_t cntps_get_remaining(void)
{
	uint64_t timer_status, period;

	/* read current timer counter value */
	__asm__ volatile ("mrs %[stat], cntpct_el0" : [stat] "=r" (timer_status));

	/* read programmed value */
	__asm__ volatile ("mrs %[period], cntps_cval_el1" : [period] "=r" (period));

	return (period - timer_status);
}

/* Platform timers API */

uint64_t plat_timer_irq_handler(uint32_t id, uint32_t flags, void *cookie)
{
#ifdef DEBUG_TIMERS
	uint64_t cval1, cval2;

	__asm__ volatile("mrs %[val], cntpct_el0" : [val] "=r" (cval1));
#endif

	/* call registered handler */
	if (timer_isr != NULL)
		timer_isr();

#ifdef DEBUG_TIMERS
	__asm__ volatile("mrs %[val], cntpct_el0" : [val] "=r" (cval2));
	printf("%s: COUNT diff  %ld\n", __func__, cval2 - cval1);
#endif

	return 0;
}

int plat_timer_register_irq(hw_timer_isr_t isr)
{
	uint32_t flags = 0;
	int rc = 0;

	timer_isr = isr;

	set_interrupt_rm_flag(flags, SECURE);
	rc = register_interrupt_handler(INTR_TYPE_EL3,
			THUNDER_IRQ_SEC_PHY_TIMER,
			plat_timer_irq_handler);
	if (rc) {
		printf("err %d while registering ARCH_PHYS_TIMER "
		       "secure interrupt handler\n", rc);
		return rc;
	}

	return rc;
}

void plat_timer_enable(int enable)
{
	debug_printf("%s(%d, %d)\n", __func__, timer_id, enable);

	/* Set saved period just before enabling timer */
	if (enable)
		cntps_set_period(curr_period);
	timer_enabled = enable;
	cntps_enable(enable);
}

uint64_t plat_timer_ms_to_ticks(uint32_t time)
{
	return cntps_ms_to_ticks(time);
}

uint64_t plat_timer_get_remainig()
{
	if (timer_enabled)
		return UINT64_MAX;

	return cntps_get_remaining();
}

void plat_timer_set_period(uint64_t period)
{
	debug_printf("plat_timer_set_period(%d, %lu)\n", timer_id, period);

	cntps_set_period(period);
	/* Store new period to program counter while enabling it */
	curr_period = period;
}

static uint32_t plat_get_timer_value(void)
{
	/* Generic delay timer implementation expects the timer to be a down
	 * counter. We apply bitwise NOT operator to the tick values returned
	 * by read_cntpct_el0() to simulate the down counter. */
	volatile uint64_t count = CSR_READ_PA(0, CAVM_RST_REF_CNTR);

	return count;
}

static timer_ops_t plat_timer_ops;

int plat_timers_init(void)
{
	plat_timer_enable(0);

	plat_timer_ops.get_timer_value	= plat_get_timer_value;
	plat_timer_ops.clk_mult		= 1;
	plat_timer_ops.clk_div		= 50;

	timer_init(&plat_timer_ops);

	/* return number of available hw counters */
	return 1;
}
