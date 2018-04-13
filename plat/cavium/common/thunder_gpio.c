/*
 *
 * Copyright (c) 2016-2017, Cavium Inc. All rights reserved.<BR>
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the
 * BSD License which accompanies this distribution.
 * The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 *
 */

#include <debug.h>
#include <psci.h>
#include <runtime_svc.h>
#include <thunder_svc.h>
#include <stdint.h>
#include <uuid.h>
#include <string.h>
#include <thunder_private.h>
#include <errno.h>
#include <interrupt_mgmt.h>
#include <thunder_io.h>
#include <platform.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#define MAX_GPIO_INTERRUPTS 80

struct gpio_irq {
	volatile uint64_t sp;
	volatile uint64_t cpu;
	volatile uint64_t ttbr;
	volatile uint64_t isr_base;
	volatile uint64_t tcr;
	volatile uint64_t counter;
	volatile int lock;
	volatile int in_use;
	volatile int in_use_ready;
};
void el3_start_el0_isr(uint64_t gpio_num, uint64_t sp,
		       uint64_t ttbr, uint64_t isr_base,
		       uint64_t spsr_el3, uint64_t scr_el3,
		       uint64_t tcr_el1);
/* -------------------------------------------------------
 * Save fpregs, indicate next SMC that it just need to go
 * stright to el3_exit.
 * Stop El1 interrupts before jumping. This assumes no
 * EL2 virtualization
 * X0 holds the gpio_num,
 * X1 holds sp
 * X2 holds ttbr
 * X3 holds isr_base
 * X4 holds spsr_el3
 * X5 holds scr_el3
 * X6 holds tcr_el1
 */
static volatile struct gpio_irq gpio_ints[MAX_GPIO_INTERRUPTS] = {0};

#define SPSR_ISR 0ULL | ((1 << 9) | (1 << 7) | (1 << 6))
#define SCR_ISR  (0 | (1 << 13) | (1 << 12) | (1 << 10) | (1))

#define THUNDER_ECAM_PER_NODE 4
#define THUNDER_ECAM_MAX_BUS  256
/*
 * Get configuration for GPIO device.
 */
uint64_t get_gpio_pconfig(void)
{
	unsigned int node, ecam, bus, dev, func,
	vendid = 0x177d, devid = 0xa00a;
	uint64_t ecam_pconfig, pconfig;
	union cavm_pccpf_xxx_id pccpf_id;

	node = 0;
	for (ecam = 0; ecam < THUNDER_ECAM_PER_NODE; ecam++) {
		ecam_pconfig = CSR_PA(node, CAVM_ECAMX_PF_BAR2(ecam));
		for (bus = 0; bus < THUNDER_ECAM_MAX_BUS; bus++)
			for (dev = 0; dev < 32; dev++)
				for (func = 0; func < 8; func++)	{
					pconfig = ecam_pconfig +
					    (bus << 20) + (dev << 15) +
					    (func << 12);
					pccpf_id.u = cavm_read32(pconfig
							  + CAVM_PCCPF_XXX_ID);
					if ((pccpf_id.s.vendid == vendid)
					    && (pccpf_id.s.devid == devid))
						return pconfig;
				}
	}
	return 0;
}

/*
 * The following is copied from thunder_ecam.c
 */
static uint64_t get_bar_val(struct pcie_config *pconfig, int bar)
{
	uint32_t cap;
	int cap_offset = pconfig->cap_pointer & 0xfc;
	uint64_t h, l, ret = 0;

	do {
		cap = *(uint32_t *) ((uint8_t *) pconfig + cap_offset);
		if ((cap & 0xff) == 0x14)
			break;
		cap_offset = (cap >> 8) & 0xfc;
	} while (cap_offset);
	if (cap_offset) {
		/* Found EA */
		int es, bei;
		int ne = (cap >> 16) & 0x3f;

		cap_offset += 4;
		while (ne) {
			uint32_t dw0 = *(uint32_t *) ((uint8_t *) pconfig +
							cap_offset + 0);

			es = dw0 & 7;
			bei = (dw0 >> 4) & 0xf;
			if (bei == bar) {
				h = 0;
				l = *(uint32_t *) ((uint8_t *) pconfig +
							cap_offset + 4);
				if (l & 2)
					h =
					*(uint32_t *) ((uint8_t *) pconfig +
							cap_offset + 12);
				ret = (h << 32) | (l & ~0xfull);
				break;
			}
			cap_offset += (es + 1) * 4;
			ne--;
		}
	} else {
		h = 0;
		l = pconfig->baseaddress_reg[bar];
		if (l & 4)
			h = pconfig->baseaddress_reg[bar + 1];
		ret = (h << 32) | (l & ~0xfull);
	}
	return ret;
}


struct irq_cpu {
	volatile int cpu;
	volatile int counter;
};

static volatile struct irq_cpu el3_gpio_irqs[THUNDER_IRQ_GPIO_COUNT] = {
	{ -1, 0}, /* THUNDER_IRQ_GPIO_BASE */
	{ -1, 0}, /* THUNDER_IRQ_GPIO_BASE + 1 */
	{ -1, 0}, /* THUNDER_IRQ_GPIO_BASE + 2 */
	{ -1, 0}, /* THUNDER_IRQ_GPIO_BASE + 3 */
};
static volatile int irq_cpu_lock_counter;

/*
 * Macro to convert an mpidr to a value suitable for programming into a
 * GICD_IROUTER. Bits[31:24] in the MPIDR are cleared as they are not relevant
 * to GICv3.
 */
#define IROUTER_IRM_SHIFT	31
#define IROUTER_IRM_MASK	0x1
#define gicd_irouter_val_from_mpidr(mpidr, irm)			\
	((mpidr & ~(0xff << 24)) |				\
	(irm & IROUTER_IRM_MASK) << IROUTER_IRM_SHIFT)

static unsigned long long cpu_to_mpidr(unsigned long long cpu)
{
	unsigned long long coreid, clusterid, nodeid;

	coreid = cpu & 0x0f;
	clusterid = (cpu & 0x30) >> 4;
	nodeid = cpu >> 6;
	return coreid | (clusterid << 8) | (nodeid << 16);
}

#define MIN_SPI_ID		32
#define GICD_IROUTER		0x6000

static inline void gicd_write_irouter(uintptr_t base,
				      unsigned int id,
				      unsigned long long affinity)
{
	*((volatile uint64_t *)(base + GICD_IROUTER + (id << 3))) = affinity;
}

static int set_gpio_msix_int_entry(uint64_t config_base,
				   int gpio_num,
				   int cpu,
				   int enable)
{
	struct pcie_config *pconfig;
	uint8_t cap_pointer;
	uint8_t bir;
	uint64_t vector_base, vector_ptr;
	volatile struct msix_cap *msicap = NULL;
	int i, select_irq = -1, intr_pinx, intr_pinx_clear;
	unsigned long long gic_affinity_val;

	intr_pinx = thunder_gpio_intr_pinx(gpio_num);
	intr_pinx_clear = thunder_gpio_intr_pinx_clear(gpio_num);

	if ((intr_pinx < 0) || (intr_pinx_clear < 0))
		return -1;

	pconfig = (struct pcie_config *)config_base;
	cap_pointer = pconfig->cap_pointer;
	bir = 0;

	while (cap_pointer) {
		msicap = (struct msix_cap *)(config_base + cap_pointer);
		if (msicap->cap_ID == PCI_MSIX_CAP_ID) {
			bir = (msicap->table_offset_and_bir & 0x7);
			break;
		}
		cap_pointer = msicap->next_pointer;
	}

	if (cap_pointer == 0)
	return -1;

	/*
	 * Global lock that protects CPU to IRQ mapping. Lock before
	 * accessing el3_gpio_irqs.
	 */
	while (__atomic_fetch_add(&irq_cpu_lock_counter, 1,
				 __ATOMIC_SEQ_CST) != 0)
	__atomic_fetch_sub(&irq_cpu_lock_counter, 1, __ATOMIC_SEQ_CST);

	if (enable) {
		for (i = 0, select_irq = -1;
		     (select_irq == -1) && (i < THUNDER_IRQ_GPIO_COUNT);
		     i++) {
			if ((el3_gpio_irqs[i].counter != 0)
			   && (el3_gpio_irqs[i].cpu == cpu)) {
				select_irq = THUNDER_IRQ_GPIO_BASE + i;
				el3_gpio_irqs[i].counter++;
			}
		}
		if (select_irq == -1) {
			for (i = 0;
			   (select_irq == -1) && (i < THUNDER_IRQ_GPIO_COUNT);
			     i++) {
				if (el3_gpio_irqs[i].counter == 0) {
					el3_gpio_irqs[i].cpu = cpu;
					el3_gpio_irqs[i].counter = 1;
					select_irq = THUNDER_IRQ_GPIO_BASE + i;
					gic_affinity_val =
					gicd_irouter_val_from_mpidr(
							 cpu_to_mpidr(cpu), 0);
					gicd_write_irouter(
					   CSR_PA(0, CAVM_GIC_PF_BAR0),
					   select_irq, gic_affinity_val);
				}
			}
		}
	} else {
		for (i = 0, select_irq = -1;
		     (select_irq == -1) && (i < THUNDER_IRQ_GPIO_COUNT); i++)
			if ((el3_gpio_irqs[i].counter != 0)
			    && (el3_gpio_irqs[i].cpu == cpu)) {
				select_irq = THUNDER_IRQ_GPIO_BASE + i;
				el3_gpio_irqs[i].counter--;
				if (el3_gpio_irqs[i].counter == 0)
					el3_gpio_irqs[i].cpu = -1;
				i = THUNDER_IRQ_GPIO_COUNT;
			}
	}

#if DEBUG
	if (enable)
		printf("IRQ 0x%x for CPU %d\n", select_irq, cpu);
#endif

	vector_base = get_bar_val(pconfig, bir);
	vector_ptr = vector_base + intr_pinx * 0x10 + 0x8;

	if (enable)
		*(volatile uint64_t *) vector_ptr = (uint64_t) select_irq;
	else
		*(volatile uint64_t *) vector_ptr = THUNDER_IRQ_GPIO_NSEC;

	vector_ptr = vector_base + intr_pinx_clear * 0x10 + 0x8;

	if (enable)
		*(volatile uint64_t *) vector_ptr = (uint64_t) select_irq;
	else
		*(volatile uint64_t *) vector_ptr = THUNDER_IRQ_GPIO_NSEC;

	__atomic_thread_fence(__ATOMIC_SEQ_CST);
	/* If disabling the entry, and IRQ was defined,
	 *  force clearing this IRQ
	 */
	if (!enable && (select_irq != -1))
		*(volatile uint64_t *) CAVM_GICD_CLRSPI_SR =
		    (uint64_t) select_irq;
	/* Unlock */
	__atomic_fetch_sub(&irq_cpu_lock_counter, 1, __ATOMIC_SEQ_CST);

	return 0;
}

void gpio_irq_handler(uint64_t gpio_num, uint64_t counter)
{
	uint64_t sp;
	uint64_t ttbr;
	uint64_t isr_base;
	uint64_t tcr;

	/*
	 * This system of lock and counter looks somewhat cumbersome,
	 * however it works well for its purpose, preventing making any
	 * calls with inconsistent parameters, or calls to ISRs that are
	 * already removed.
	 *
	 * For most of the time, no locks are held, .in_use_ready
	 * indicates that the interrupt has a registered ISR, and the data
	 * is consistent. .counter increments on every update, so if by
	 * any chance it will be updated during the call, to this
	 * function, it will be found in the last moment, and ISR will not
	 * be called.
	 *
	 * Lock is held only while the ISR is called. This should be
	 * invisible for anything on the local CPU, because only we are
	 * running at this time. Other CPUs, however, will see it, and
	 * calls from them will be delayed until the ISR is finished if
	 * they will wait for this lock.
	 *
	 * If someone else is holding this lock, ISR is not called at all.
	 *
	 */
	if (__atomic_fetch_add(&gpio_ints[gpio_num].lock, 1,
			      __ATOMIC_SEQ_CST) != 0)
		goto finish;

	sp = __atomic_load_n(&gpio_ints[gpio_num].sp, __ATOMIC_SEQ_CST);
	ttbr = __atomic_load_n(&gpio_ints[gpio_num].ttbr, __ATOMIC_SEQ_CST);
	isr_base = __atomic_load_n(&gpio_ints[gpio_num].isr_base,
				   __ATOMIC_SEQ_CST);
	tcr = __atomic_load_n(&gpio_ints[gpio_num].tcr, __ATOMIC_SEQ_CST);
	if (__atomic_load_n(&gpio_ints[gpio_num].in_use_ready,
			   __ATOMIC_SEQ_CST) == 0)
		goto finish;
	if (__atomic_load_n(&gpio_ints[gpio_num].counter,
			    __ATOMIC_SEQ_CST) != counter)
		goto finish;
	el3_start_el0_isr(gpio_num, sp,
			  ttbr,
			  isr_base, SPSR_ISR, SCR_ISR,
			  tcr);
 finish:
	__atomic_fetch_sub(&gpio_ints[gpio_num].lock, 1, __ATOMIC_SEQ_CST);
}

uint64_t plat_gpio_irq_handler(uint32_t id, uint32_t flags, void *cookie)
{
	uint64_t gpio_intr_ptr;
	uint64_t counter;
	int i, in_use_ready, index, matched = 0;
	uint64_t cpu, assigned_cpu;
	int mapped_cpu, mapped_counter;

	cpu = plat_my_core_pos();

	index = id - THUNDER_IRQ_GPIO_BASE;

	/* For all invalid interrupts, clear interrupt and exit. */
	if ((index < 0) || (index >= THUNDER_IRQ_GPIO_COUNT)) {
		*(volatile uint64_t *) CAVM_GICD_CLRSPI_SR = (uint64_t) id;
		printf("Invalid GPIO interrupt %x\n", id);
		return 0;
	}
	/*
	 * Global lock that protects CPU to IRQ mapping. Lock before
	 * accessing el3_gpio_irqs.
	 */
	while (__atomic_fetch_add(&irq_cpu_lock_counter, 1,
				 __ATOMIC_SEQ_CST) != 0)
		__atomic_fetch_sub(&irq_cpu_lock_counter, 1, __ATOMIC_SEQ_CST);

	mapped_cpu = el3_gpio_irqs[index].cpu;
	mapped_counter = el3_gpio_irqs[index].counter;

	/* Unlock */
	__atomic_fetch_sub(&irq_cpu_lock_counter, 1, __ATOMIC_SEQ_CST);

	/* For all mis-routed interrupts, clear interrupt and exit. */
	if ((mapped_counter < 1) || (mapped_cpu != cpu)) {
		*(volatile uint64_t *) CAVM_GICD_CLRSPI_SR = (uint64_t) id;
		printf("Mis-routed GPIO interrupt %x\n", id);
		return 0;
	}

	for (i = 0; i < MAX_GPIO_INTERRUPTS; i++) {
		gpio_intr_ptr = CAVM_GPIO_INTRX(i);
		if (gpio_intr_ptr != 0) {
			/* Check for the interrupt for this GPIO */
			if ((*(volatile uint64_t *)gpio_intr_ptr) & 1) {
				/*
				 * Get the value of .counter before reading
				 * anything from the descriptor. If
				 * anything is or will become invalid, we
				 * will see either .in_use_ready reset, or
				 * .counter incremented.
				 */
				in_use_ready = 0;
				assigned_cpu = 0;
				counter = __atomic_load_n(
					      &gpio_ints[i].counter,
					      __ATOMIC_SEQ_CST);

				in_use_ready =
				    __atomic_load_n(&gpio_ints[i].in_use_ready,
						    __ATOMIC_SEQ_CST);
				assigned_cpu =
				    __atomic_load_n(&gpio_ints[i].cpu,
						    __ATOMIC_SEQ_CST);
				if ((in_use_ready != 0)
				    && (assigned_cpu == cpu)) {
					matched = 1;
					/* Call ISR wrapper */
					gpio_irq_handler(i, counter);
				}
				if ((in_use_ready == 0) ||
				   (assigned_cpu == cpu)) {
					/*
					 * Acknowledge interrupt
					 * for this GPIO
					 */
					*(volatile uint64_t *) gpio_intr_ptr
					    = 0x01;
				}
			}
		}
	}
	/*
	 * For all interrupts with no apparent cause on this CPU,
	 * clear interrupt and exit.
	 */
	if (!matched) {
		*(volatile uint64_t *) CAVM_GICD_CLRSPI_SR = (uint64_t) id;
		printf("Stale GPIO interrupt %x\n", id);
		return 0;
	}
	return 0;
}

static uint64_t thunder_gpio_pconfig;
static int thunder_gpio_intercept_interrupts;

void gpio_enable_install_irq(int enable)
{
	thunder_gpio_intercept_interrupts = !!enable;
}

int gpio_install_irq(uint64_t gpio_num, uint64_t sp, uint64_t  cpu,
		     uint64_t isr_base)
{
	int retval;

	/* Check if this functionality is available */
	if (thunder_gpio_intercept_interrupts == 0)
		return -1;

	if (thunder_gpio_pconfig == 0)
		thunder_gpio_pconfig = get_gpio_pconfig();
	if (thunder_gpio_pconfig == 0)
		return -1;
	/* Lock */
	while (__atomic_fetch_add(&gpio_ints[gpio_num].lock, 1,
				 __ATOMIC_SEQ_CST) != 0)
		__atomic_fetch_sub(&gpio_ints[gpio_num].lock, 1,
				   __ATOMIC_SEQ_CST);

	/*
	 * .in_use indicates that this GPIO is not free to be set without
	 *  releasing. It may be still in the process of being configured,
	 *  so interrupt handler can't rely on it, and should only use
	 *  .in_use_ready .
	 */
	if (__atomic_fetch_add(&gpio_ints[gpio_num].in_use, 1,
			      __ATOMIC_SEQ_CST) != 0) {
		__atomic_fetch_sub(&gpio_ints[gpio_num].in_use, 1,
				   __ATOMIC_SEQ_CST);
		retval = -1;
		goto finish;
	}
	gpio_ints[gpio_num].sp = sp;
	gpio_ints[gpio_num].cpu = cpu;
	gpio_ints[gpio_num].ttbr = 0;
	gpio_ints[gpio_num].isr_base = isr_base;
	asm volatile("mrs %0, ttbr0_el1\n\t" : "=r"(gpio_ints[gpio_num].ttbr));
	gpio_ints[gpio_num].tcr = 0;
	asm volatile("mrs %0, tcr_el1\n\t" : "=r"(gpio_ints[gpio_num].tcr));
	gpio_ints[gpio_num].tcr &= ~(1ULL << 22);
	retval = set_gpio_msix_int_entry(thunder_gpio_pconfig, gpio_num,
					 cpu, 1);
	if (retval != 0) {
		gpio_ints[gpio_num].in_use = 0;
		gpio_ints[gpio_num].sp = 0;
		gpio_ints[gpio_num].cpu = 0;
		gpio_ints[gpio_num].ttbr = 0;
#if DEBUG
		printf("Can't install irq handlerfor gpio:%lu cpu:%lu\n",
		       gpio_num, cpu);
#endif
		__atomic_thread_fence(__ATOMIC_SEQ_CST);
		/* Failure, this GPIO is free to be configured. */
		__atomic_fetch_sub(&gpio_ints[gpio_num].in_use, 1,
				   __ATOMIC_SEQ_CST);
	} else {
#if DEBUG
		printf("Installed irq handler for gpio:%lu ttrb:%lx sp:%lx\n"
		       "\tisr_base:%lx cpu:%lu tcr:%lx\n",
		       gpio_num, gpio_ints[gpio_num].ttbr,
		       gpio_ints[gpio_num].sp,
		       gpio_ints[gpio_num].isr_base, gpio_ints[gpio_num].cpu,
		       gpio_ints[gpio_num].tcr);
#endif
		__atomic_thread_fence(__ATOMIC_SEQ_CST);
		/*
		 * Success, increment .counter, and indicate that GPIO is
		 * ready to be used by setting .in_use_ready .
		 */
		__atomic_fetch_add(&gpio_ints[gpio_num].counter, 1,
				   __ATOMIC_SEQ_CST);
		__atomic_store_n(&gpio_ints[gpio_num].in_use_ready, 1,
				 __ATOMIC_SEQ_CST);
	}

 finish:
	/* Unlock */
	__atomic_fetch_sub(&gpio_ints[gpio_num].lock, 1, __ATOMIC_SEQ_CST);
	return retval;
}

void gpio_clear_irq(uint64_t gpio_num)
{
	uint64_t gpio_intr_ptr;

	if (thunder_gpio_pconfig == 0)
		return;

	/* Lock */
	while (__atomic_fetch_add(&gpio_ints[gpio_num].lock, 1,
				 __ATOMIC_SEQ_CST) != 0)
	__atomic_fetch_sub(&gpio_ints[gpio_num].lock, 1, __ATOMIC_SEQ_CST);

	/*
	 * Prevent ISRs from being called while hardware and descriptors
	 * are being updated.
	 */
	__atomic_store_n(&gpio_ints[gpio_num].in_use_ready, 0,
			 __ATOMIC_SEQ_CST);
	__atomic_thread_fence(__ATOMIC_SEQ_CST);

	set_gpio_msix_int_entry(thunder_gpio_pconfig, gpio_num,
				gpio_ints[gpio_num].cpu, 0);
	gpio_ints[gpio_num].sp = 0;
	gpio_ints[gpio_num].cpu = 0;
	gpio_ints[gpio_num].ttbr = 0;
	gpio_intr_ptr = CAVM_GPIO_INTRX(gpio_num);
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
	if (gpio_intr_ptr != 0) {
		do {
			/* Acknowledge interrupt for this GPIO */
			*(volatile uint64_t *)gpio_intr_ptr = 0x01;
		/* Check for interrupt */
		} while ((*(volatile uint64_t *)gpio_intr_ptr) & 1);
	}
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
	/*
	 * Leave this entry free for use. If by now it was not allocated in the
	 * first place, restore the in_use value.
	 */
	if (__atomic_fetch_sub(&gpio_ints[gpio_num].in_use, 1,
			       __ATOMIC_SEQ_CST) == 0)
		__atomic_fetch_add(&gpio_ints[gpio_num].in_use, 1,
				   __ATOMIC_SEQ_CST);
	/* Unlock */
	__atomic_fetch_sub(&gpio_ints[gpio_num].lock, 1, __ATOMIC_SEQ_CST);
#if DEBUG
	printf("Removed irq handler for gpio:%lu\n", gpio_num);
#endif
}
