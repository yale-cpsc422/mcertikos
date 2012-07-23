/*
 * Derived from BHyVe (svn 237539).
 * Adapted for CertiKOS by Haozhong Zhang at Yale.
 */

/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/debug.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include "vmx_msr.h"

static bool
vmx_ctl_allows_one_setting(uint64_t msr_val, int bitpos)
{

	if (msr_val & (1ULL << (bitpos + 32)))
		return (TRUE);
	else
		return (FALSE);
}

static bool
vmx_ctl_allows_zero_setting(uint64_t msr_val, int bitpos)
{

	if ((msr_val & (1ULL << bitpos)) == 0)
		return (TRUE);
	else
		return (FALSE);
}

uint32_t
vmx_revision(void)
{
	return (rdmsr(MSR_VMX_BASIC) & 0xffffffff);
}

/*
 * Generate a bitmask to be used for the VMCS execution control fields.
 *
 * The caller specifies what bits should be set to one in 'ones_mask'
 * and what bits should be set to zero in 'zeros_mask'. The don't-care
 * bits are set to the default value. The default values are obtained
 * based on "Algorithm 3" in Section 27.5.1 "Algorithms for Determining
 * VMX Capabilities".
 *
 * Returns zero on success and non-zero on error.
 */
int
vmx_set_ctlreg(int ctl_reg, int true_ctl_reg, uint32_t ones_mask,
	       uint32_t zeros_mask, uint32_t *retval)
{
	int i;
	uint64_t val, trueval;
	bool true_ctls_avail, one_allowed, zero_allowed;

	/* We cannot ask the same bit to be set to both '1' and '0' */
	if ((ones_mask ^ zeros_mask) != (ones_mask | zeros_mask)) {
		KERN_DEBUG("0's mask 0x%08x and 1's mask 0x%08x are overlapped.\n",
			   zeros_mask, ones_mask);
		return 1;
	}

	if (rdmsr(MSR_VMX_BASIC) & (1ULL << 55))
		true_ctls_avail = TRUE;
	else
		true_ctls_avail = FALSE;

	val = rdmsr(ctl_reg);
	if (true_ctls_avail)
		trueval = rdmsr(true_ctl_reg);		/* step c */
	else
		trueval = val;				/* step a */

	for (i = 0; i < 32; i++) {
		one_allowed = vmx_ctl_allows_one_setting(trueval, i);
		zero_allowed = vmx_ctl_allows_zero_setting(trueval, i);

		KERN_ASSERT(one_allowed || zero_allowed);

		if (zero_allowed && !one_allowed) {		/* b(i),c(i) */
			if (ones_mask & (1 << i)) {
				KERN_DEBUG("Cannot set bit %d to zero.\n", i);
				return 1;
			}
			*retval &= ~(1 << i);
		} else if (one_allowed && !zero_allowed) {	/* b(i),c(i) */
			if (zeros_mask & (1 << i)) {
				KERN_DEBUG("Cannot set bit %d to one.\n", i);
				return 2;
			}
			*retval |= 1 << i;
		} else {
			if (zeros_mask & (1 << i))	/* b(ii),c(ii) */
				*retval &= ~(1 << i);
			else if (ones_mask & (1 << i)) /* b(ii), c(ii) */
				*retval |= 1 << i;
			else if (!true_ctls_avail)
				*retval &= ~(1 << i);	/* b(iii) */
			else if (vmx_ctl_allows_zero_setting(val, i))/* c(iii)*/
				*retval &= ~(1 << i);
			else if (vmx_ctl_allows_one_setting(val, i)) /* c(iv) */
				*retval |= 1 << i;
			else {
				KERN_PANIC("Cannot determine the value of bit %d "
					   "of MSR 0x%08x and true MSR 0x%08x.\n",
					   i, ctl_reg, true_ctl_reg);
			}
		}
	}

	return (0);
}

void
msr_bitmap_initialize(char *bitmap)
{
	memset(bitmap, 0xff, PAGE_SIZE);
}

int
msr_bitmap_change_access(char *bitmap, uint32_t msr, int access)
{
	int byte, bit;

	if (msr >= 0x00000000 && msr <= 0x00001FFF)
		byte = msr / 8;
	else if (msr >= 0xC0000000 && msr <= 0xC0001FFF)
		byte = 1024 + (msr - 0xC0000000) / 8;
	else
		return 1;

	bit = msr & 0x7;

	if (access & MSR_BITMAP_ACCESS_READ)
		bitmap[byte] &= ~(1 << bit);
	else
		bitmap[byte] |= 1 << bit;

	byte += 2048;
	if (access & MSR_BITMAP_ACCESS_WRITE)
		bitmap[byte] &= ~(1 << bit);
	else
		bitmap[byte] |= 1 << bit;

	return (0);
}
