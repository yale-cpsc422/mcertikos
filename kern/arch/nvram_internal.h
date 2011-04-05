// Definition's for the PC's nonvolatile RAM (NVRAM),
// part of the battery-backed real-timem clock.
// The kernel reads the NVRAM to detect how much memory we have.
// See COPYRIGHT for copyright information.
#ifndef PIOS_DEV_NVRAM_INTERNAL_H
#define PIOS_DEV_NVRAM_INTERNAL_H

#define	IO_RTC		0x070		/* RTC port */
#define	MC_NVRAM_SIZE	50	/* 50 bytes of NVRAM */

#endif	// !PIOS_DEV_NVRAM_INTERNAL_H
