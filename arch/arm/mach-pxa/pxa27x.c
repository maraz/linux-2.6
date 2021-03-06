/*
 *  linux/arch/arm/mach-pxa/pxa27x.c
 *
 *  Author:	Nicolas Pitre
 *  Created:	Nov 05, 2002
 *  Copyright:	MontaVista Software Inc.
 *
 * Code specific to PXA27x aka Bulverde.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/sysdev.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/arch/irqs.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/pxa2xx-regs.h>
#include <asm/arch/mfp-pxa27x.h>
#include <asm/arch/ohci.h>
#include <asm/arch/pm.h>
#include <asm/arch/dma.h>
#include <asm/arch/i2c.h>

#include "generic.h"
#include "devices.h"
#include "clock.h"

/* Crystal clock: 13MHz */
#define BASE_CLK	13000000

/*
 * Get the clock frequency as reflected by CCSR and the turbo flag.
 * We assume these values have been applied via a fcs.
 * If info is not 0 we also display the current settings.
 */
unsigned int pxa27x_get_clk_frequency_khz(int info)
{
	unsigned long ccsr, clkcfg;
	unsigned int l, L, m, M, n2, N, S;
       	int cccr_a, t, ht, b;

	ccsr = CCSR;
	cccr_a = CCCR & (1 << 25);

	/* Read clkcfg register: it has turbo, b, half-turbo (and f) */
	asm( "mrc\tp14, 0, %0, c6, c0, 0" : "=r" (clkcfg) );
	t  = clkcfg & (1 << 0);
	ht = clkcfg & (1 << 2);
	b  = clkcfg & (1 << 3);

	l  = ccsr & 0x1f;
	n2 = (ccsr>>7) & 0xf;
	m  = (l <= 10) ? 1 : (l <= 20) ? 2 : 4;

	L  = l * BASE_CLK;
	N  = (L * n2) / 2;
	M  = (!cccr_a) ? (L/m) : ((b) ? L : (L/2));
	S  = (b) ? L : (L/2);

	if (info) {
		printk( KERN_INFO "Run Mode clock: %d.%02dMHz (*%d)\n",
			L / 1000000, (L % 1000000) / 10000, l );
		printk( KERN_INFO "Turbo Mode clock: %d.%02dMHz (*%d.%d, %sactive)\n",
			N / 1000000, (N % 1000000)/10000, n2 / 2, (n2 % 2)*5,
			(t) ? "" : "in" );
		printk( KERN_INFO "Memory clock: %d.%02dMHz (/%d)\n",
			M / 1000000, (M % 1000000) / 10000, m );
		printk( KERN_INFO "System bus clock: %d.%02dMHz \n",
			S / 1000000, (S % 1000000) / 10000 );
	}

	return (t) ? (N/1000) : (L/1000);
}

/*
 * Return the current mem clock frequency in units of 10kHz as
 * reflected by CCCR[A], B, and L
 */
unsigned int pxa27x_get_memclk_frequency_10khz(void)
{
	unsigned long ccsr, clkcfg;
	unsigned int l, L, m, M;
       	int cccr_a, b;

	ccsr = CCSR;
	cccr_a = CCCR & (1 << 25);

	/* Read clkcfg register: it has turbo, b, half-turbo (and f) */
	asm( "mrc\tp14, 0, %0, c6, c0, 0" : "=r" (clkcfg) );
	b = clkcfg & (1 << 3);

	l = ccsr & 0x1f;
	m = (l <= 10) ? 1 : (l <= 20) ? 2 : 4;

	L = l * BASE_CLK;
	M = (!cccr_a) ? (L/m) : ((b) ? L : (L/2));

	return (M / 10000);
}

/*
 * Return the current LCD clock frequency in units of 10kHz as
 */
static unsigned int pxa27x_get_lcdclk_frequency_10khz(void)
{
	unsigned long ccsr;
	unsigned int l, L, k, K;

	ccsr = CCSR;

	l = ccsr & 0x1f;
	k = (l <= 7) ? 1 : (l <= 16) ? 2 : 4;

	L = l * BASE_CLK;
	K = L / k;

	return (K / 10000);
}

static unsigned long clk_pxa27x_lcd_getrate(struct clk *clk)
{
	return pxa27x_get_lcdclk_frequency_10khz() * 10000;
}

static const struct clkops clk_pxa27x_lcd_ops = {
	.enable		= clk_cken_enable,
	.disable	= clk_cken_disable,
	.getrate	= clk_pxa27x_lcd_getrate,
};

static struct clk pxa27x_clks[] = {
	INIT_CK("LCDCLK", LCD,    &clk_pxa27x_lcd_ops, &pxa_device_fb.dev),
	INIT_CK("CAMCLK", CAMERA, &clk_pxa27x_lcd_ops, NULL),

	INIT_CKEN("UARTCLK", FFUART, 14857000, 1, &pxa_device_ffuart.dev),
	INIT_CKEN("UARTCLK", BTUART, 14857000, 1, &pxa_device_btuart.dev),
	INIT_CKEN("UARTCLK", STUART, 14857000, 1, NULL),

	INIT_CKEN("I2SCLK",  I2S,  14682000, 0, &pxa_device_i2s.dev),
	INIT_CKEN("I2CCLK",  I2C,  32842000, 0, &pxa_device_i2c.dev),
	INIT_CKEN("UDCCLK",  USB,  48000000, 5, &pxa27x_device_udc.dev),
	INIT_CKEN("MMCCLK",  MMC,  19500000, 0, &pxa_device_mci.dev),
	INIT_CKEN("FICPCLK", FICP, 48000000, 0, &pxa_device_ficp.dev),

	INIT_CKEN("USBCLK", USBHOST, 48000000, 0, &pxa27x_device_ohci.dev),
	INIT_CKEN("I2CCLK", PWRI2C, 13000000, 0, &pxa27x_device_i2c_power.dev),
	INIT_CKEN("KBDCLK", KEYPAD, 32768, 0, &pxa27x_device_keypad.dev),

	INIT_CKEN("SSPCLK", SSP1, 13000000, 0, &pxa27x_device_ssp1.dev),
	INIT_CKEN("SSPCLK", SSP2, 13000000, 0, &pxa27x_device_ssp2.dev),
	INIT_CKEN("SSPCLK", SSP3, 13000000, 0, &pxa27x_device_ssp3.dev),
	INIT_CKEN("PWMCLK", PWM0, 13000000, 0, &pxa27x_device_pwm0.dev),
	INIT_CKEN("PWMCLK", PWM1, 13000000, 0, &pxa27x_device_pwm1.dev),

	INIT_CKEN("AC97CLK",     AC97,     24576000, 0, NULL),
	INIT_CKEN("AC97CONFCLK", AC97CONF, 24576000, 0, NULL),

	/*
	INIT_CKEN("MSLCLK",  MSL,  48000000, 0, NULL),
	INIT_CKEN("USIMCLK", USIM, 48000000, 0, NULL),
	INIT_CKEN("MSTKCLK", MEMSTK, 19500000, 0, NULL),
	INIT_CKEN("IMCLK",   IM,   0, 0, NULL),
	INIT_CKEN("MEMCLK",  MEMC, 0, 0, NULL),
	*/
};

#ifdef CONFIG_PM

#define SAVE(x)		sleep_save[SLEEP_SAVE_##x] = x
#define RESTORE(x)	x = sleep_save[SLEEP_SAVE_##x]

/*
 * List of global PXA peripheral registers to preserve.
 * More ones like CP and general purpose register values are preserved
 * with the stack pointer in sleep.S.
 */
enum {	SLEEP_SAVE_PGSR0, SLEEP_SAVE_PGSR1, SLEEP_SAVE_PGSR2, SLEEP_SAVE_PGSR3,

	SLEEP_SAVE_GAFR0_L, SLEEP_SAVE_GAFR0_U,
	SLEEP_SAVE_GAFR1_L, SLEEP_SAVE_GAFR1_U,
	SLEEP_SAVE_GAFR2_L, SLEEP_SAVE_GAFR2_U,
	SLEEP_SAVE_GAFR3_L, SLEEP_SAVE_GAFR3_U,

	SLEEP_SAVE_PSTR,

	SLEEP_SAVE_CKEN,

	SLEEP_SAVE_MDREFR,
	SLEEP_SAVE_PWER, SLEEP_SAVE_PCFR, SLEEP_SAVE_PRER,
	SLEEP_SAVE_PFER, SLEEP_SAVE_PKWR,

	SLEEP_SAVE_COUNT
};

void pxa27x_cpu_pm_save(unsigned long *sleep_save)
{
	SAVE(PGSR0); SAVE(PGSR1); SAVE(PGSR2); SAVE(PGSR3);

	SAVE(GAFR0_L); SAVE(GAFR0_U);
	SAVE(GAFR1_L); SAVE(GAFR1_U);
	SAVE(GAFR2_L); SAVE(GAFR2_U);
	SAVE(GAFR3_L); SAVE(GAFR3_U);

	SAVE(MDREFR);
	SAVE(PWER); SAVE(PCFR); SAVE(PRER);
	SAVE(PFER); SAVE(PKWR);

	SAVE(CKEN);
	SAVE(PSTR);
}

void pxa27x_cpu_pm_restore(unsigned long *sleep_save)
{
	/* ensure not to come back here if it wasn't intended */
	PSPR = 0;

	/* restore registers */
	RESTORE(GAFR0_L); RESTORE(GAFR0_U);
	RESTORE(GAFR1_L); RESTORE(GAFR1_U);
	RESTORE(GAFR2_L); RESTORE(GAFR2_U);
	RESTORE(GAFR3_L); RESTORE(GAFR3_U);
	RESTORE(PGSR0); RESTORE(PGSR1); RESTORE(PGSR2); RESTORE(PGSR3);

	RESTORE(MDREFR);
	RESTORE(PWER); RESTORE(PCFR); RESTORE(PRER);
	RESTORE(PFER); RESTORE(PKWR);

	PSSR = PSSR_RDH | PSSR_PH;

	RESTORE(CKEN);

	RESTORE(PSTR);
}

void pxa27x_cpu_pm_enter(suspend_state_t state)
{
	extern void pxa_cpu_standby(void);

	/* ensure voltage-change sequencer not initiated, which hangs */
	PCFR &= ~PCFR_FVC;

	/* Clear edge-detect status register. */
	PEDR = 0xDF12FE1B;

	/* Clear reset status */
	RCSR = RCSR_HWR | RCSR_WDR | RCSR_SMR | RCSR_GPR;

	switch (state) {
	case PM_SUSPEND_STANDBY:
		pxa_cpu_standby();
		break;
	case PM_SUSPEND_MEM:
		/* set resume return address */
		PSPR = virt_to_phys(pxa_cpu_resume);
		pxa27x_cpu_suspend(PWRMODE_SLEEP);
		break;
	}
}

static int pxa27x_cpu_pm_valid(suspend_state_t state)
{
	return state == PM_SUSPEND_MEM || state == PM_SUSPEND_STANDBY;
}

static struct pxa_cpu_pm_fns pxa27x_cpu_pm_fns = {
	.save_count	= SLEEP_SAVE_COUNT,
	.save		= pxa27x_cpu_pm_save,
	.restore	= pxa27x_cpu_pm_restore,
	.valid		= pxa27x_cpu_pm_valid,
	.enter		= pxa27x_cpu_pm_enter,
};

static void __init pxa27x_init_pm(void)
{
	pxa_cpu_pm_fns = &pxa27x_cpu_pm_fns;
}
#else
static inline void pxa27x_init_pm(void) {}
#endif

/* PXA27x:  Various gpios can issue wakeup events.  This logic only
 * handles the simple cases, not the WEMUX2 and WEMUX3 options
 */
static int pxa27x_set_wake(unsigned int irq, unsigned int on)
{
	int gpio = IRQ_TO_GPIO(irq);
	uint32_t mask;

	if (gpio >= 0 && gpio < 128)
		return gpio_set_wake(gpio, on);

	if (irq == IRQ_KEYPAD)
		return keypad_set_wake(on);

	switch (irq) {
	case IRQ_RTCAlrm:
		mask = PWER_RTC;
		break;
	case IRQ_USB:
		mask = 1u << 26;
		break;
	default:
		return -EINVAL;
	}

	if (on)
		PWER |= mask;
	else
		PWER &=~mask;

	return 0;
}

void __init pxa27x_init_irq(void)
{
	pxa_init_irq(34, pxa27x_set_wake);
	pxa_init_gpio(128, pxa27x_set_wake);
}

/*
 * device registration specific to PXA27x.
 */

static struct resource i2c_power_resources[] = {
	{
		.start	= 0x40f00180,
		.end	= 0x40f001a3,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_PWRI2C,
		.end	= IRQ_PWRI2C,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device pxa27x_device_i2c_power = {
	.name		= "pxa2xx-i2c",
	.id		= 1,
	.resource	= i2c_power_resources,
	.num_resources	= ARRAY_SIZE(i2c_power_resources),
};

void __init pxa_set_i2c_power_info(struct i2c_pxa_platform_data *info)
{
	local_irq_disable();
	PCFR |= PCFR_PI2CEN;
	local_irq_enable();
	pxa27x_device_i2c_power.dev.platform_data = info;
}

static struct platform_device *devices[] __initdata = {
	&pxa27x_device_udc,
	&pxa_device_ffuart,
	&pxa_device_btuart,
	&pxa_device_stuart,
	&pxa_device_i2s,
	&pxa_device_rtc,
	&pxa27x_device_i2c_power,
	&pxa27x_device_ssp1,
	&pxa27x_device_ssp2,
	&pxa27x_device_ssp3,
	&pxa27x_device_pwm0,
	&pxa27x_device_pwm1,
};

static struct sys_device pxa27x_sysdev[] = {
	{
		.cls	= &pxa_irq_sysclass,
	}, {
		.cls	= &pxa_gpio_sysclass,
	},
};

static int __init pxa27x_init(void)
{
	int i, ret = 0;

	if (cpu_is_pxa27x()) {
		clks_register(pxa27x_clks, ARRAY_SIZE(pxa27x_clks));

		if ((ret = pxa_init_dma(32)))
			return ret;

		pxa27x_init_pm();

		for (i = 0; i < ARRAY_SIZE(pxa27x_sysdev); i++) {
			ret = sysdev_register(&pxa27x_sysdev[i]);
			if (ret)
				pr_err("failed to register sysdev[%d]\n", i);
		}

		ret = platform_add_devices(devices, ARRAY_SIZE(devices));
	}

	return ret;
}

postcore_initcall(pxa27x_init);
