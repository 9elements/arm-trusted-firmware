#include <stdint.h>
#include <assert.h>
#include <debug.h>
#include <stdio.h>
#include <spinlock.h>
#include <timers.h>
#include <hw_timers.h>

#define TM_MAX_TIMERS 10

#ifndef DEBUG_TIMERS
#define DEBUG_TIMERS 0
#endif

#if DEBUG_TIMERS == 1
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...)
#endif

/* Array of timers */
timer_data timers[TM_MAX_TIMERS];

/* HW timer period in ticks */
uint64_t hw_timer_period = UINT64_MAX;

/* Global spinlock to protect timer structures */
spinlock_t tm_lock;

static int num_started_timers = 0;

static void hw_timer_set_period(uint32_t ticks) {
	hw_timer_period = ticks;
	plat_timer_set_period(ticks);
}

static void hw_timer_isr(void)
{
	uint64_t min_time = UINT64_MAX;

	debug_printf("timer ISR:\n");

	/* Disable timer interrupt */
	plat_timer_enable(0);

	spin_lock(&tm_lock);
	for (int i = 0; i < TM_MAX_TIMERS; i++) {
		if (timers[i].is_started) {
			if (timers[i].fire_in <= hw_timer_period) {
				/*
				 * Disable before calling callback function,
				 * timer may be reenabled.
				 */
				if (timers[i].type == TM_ONE_SHOT) {
					timers[i].is_started = 0;
					num_started_timers--;
					if (num_started_timers == 0) {
						plat_timer_enable(0);
					}
				}

				spin_unlock(&tm_lock);
				timers[i].cb(i);
				spin_lock(&tm_lock);
				/*
				 * Timer can be modified in callback, but only set_timer_period
				 * and timer_start are allowed, even delete will not hurt
				 * if a timer is deleted or stopped and it has minimal
				 * time... we just get extra ISR.
				 */
				timers[i].fire_in = timers[i].period;
			} else {
				timers[i].fire_in -= hw_timer_period;
			}

			/* Calculate time for the next ISR */
			if (timers[i].is_started && (min_time > timers[i].fire_in)) {
				min_time = timers[i].fire_in;
			}
		}
	}
	spin_unlock(&tm_lock);
	if (min_time != UINT64_MAX) {
		hw_timer_set_period(min_time);
	}

	/* Reenable timer with new value */
	if (num_started_timers >= 1)
		plat_timer_enable(1);
}

/*
 * This function creates a timer with specified period in ms and kind
 * defined by timer_kind_t. It returns timer handle which can be passed
 * to other timer related functions. Timer is not started after this
 * function returns, it's necessary to call timer_start explicitly.
 */
int timer_create(timer_kind_t type, uint32_t period, timer_callback_t cb)
{
	int i;

	if (!cb)
		return -1;

	debug_printf("TIMER: %s type=%d period=%d\n", __func__, type, period);

	spin_lock(&tm_lock);

	for (i = 0; i < TM_MAX_TIMERS; i++) {
		if (timers[i].is_created)
			continue;
		timers[i].is_created = 1;
		timers[i].is_started = 0;
		timers[i].period = plat_timer_ms_to_ticks(period);
		timers[i].type = type;
		timers[i].cb = cb;

		debug_printf("TIMER: %s hd=%d\n", __func__, i);

		spin_unlock(&tm_lock);
		return i;
	}
	ERROR("TIMER: No more handles available!\n");

	spin_unlock(&tm_lock);
	return -1;
}

int timer_start(int hd)
{
	assert(hd >= 0 && hd < TM_MAX_TIMERS);

	debug_printf("TIMER: timer_start(%d)\n", hd);

	spin_lock(&tm_lock);

	if (!timers[hd].is_created) {
		WARN("TIMER: timer with hd =%d isn't created!", hd);
		spin_unlock(&tm_lock);
		return -1;
	}

	if (timers[hd].is_started) {
		WARN("TIMER: timer with hd =%d already started!", hd);
		spin_unlock(&tm_lock);
		return -1;
	}

	timers[hd].is_started = 1;
	num_started_timers++;

	/* Need shorter timeout or HW timer is not running at all */
	if (plat_timer_get_remainig() > timers[hd].period) {
		timers[hd].fire_in = timers[hd].period;
		hw_timer_set_period(timers[hd].period);
	} else {
		timers[hd].fire_in = (timers[hd].period -
				     plat_timer_get_remainig());
	}
	if (num_started_timers == 1) {
		plat_timer_enable(1);
	}
	spin_unlock(&tm_lock);

	return 0;
}

int timer_stop(int hd)
{
	assert(hd >= 0 && hd < TM_MAX_TIMERS);

	spin_lock(&tm_lock);

	if (timers[hd].is_started) {
		timers[hd].is_started = 0;
		num_started_timers--;
		if (num_started_timers == 0)
			plat_timer_enable(0);
		spin_unlock(&tm_lock);
		return 0;
	}

	spin_unlock(&tm_lock);
	return -1;
}

/* Set timer interval in ms */
int timer_set_period(int hd, uint32_t period)
{
	assert(hd >= 0 && hd < TM_MAX_TIMERS);

	debug_printf("TIMER: timer_set_period(%d, %d ms)\n", hd, period);

	spin_lock(&tm_lock);

	if (!timers[hd].is_created) {
		spin_unlock(&tm_lock);
		return -1;
	}

	/* Can modify only stopped one-shot timer */
	if (!timers[hd].is_started && timers[hd].type == TM_ONE_SHOT) {
		timers[hd].period = plat_timer_ms_to_ticks(period);
	} else {
		spin_unlock(&tm_lock);
		return -1;
	}

	spin_unlock(&tm_lock);
	return 0;
}

/*
 * Perform basic initialization of hardware and gather
 * number of available HW timers on platform
 */
int timers_init(void)
{
	ERROR("TIMER: %s\n", __func__);
	plat_timers_init();
	return plat_timer_register_irq(hw_timer_isr);
}

