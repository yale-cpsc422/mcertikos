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

#ifndef _VIRT_VMX_EPT_H_
#define _VIRT_VMX_EPT_H_

#ifdef _KERN_

#include <sys/mem.h>

#include <sys/virt/vmm.h>

#define	EPT_PWLEVELS	4		/* page walk levels */
#define	EPTP(pml4)	((pml4) | (EPT_PWLEVELS - 1) << 3 | PAT_WRITE_BACK)

int       ept_init(void);
int       ept_create_mappings(uint64_t *pml4ept, size_t);
int       ept_add_mapping(uint64_t *pml4ept, uintptr_t gpa, uintptr_t hpa,
			  uint8_t mem_type, bool superpage);
void      ept_invalidate_mappings(uint64_t);
size_t    ept_copy_to_guest(uint64_t *pml4ept,
			    uintptr_t dest, uintptr_t src, size_t);
uintptr_t ept_gpa_to_hpa(uint64_t *pml4ept, uintptr_t gpa);
int       ept_set_permission(uint64_t *pml4ept, uintptr_t gpa, uint8_t perm);
int       ept_mmap(struct vm *, uintptr_t gpa, uintptr_t hpa);
int       ept_unmmap(struct vm *, uintptr_t gpa);

#endif /* _KERN_ */

#endif /* !_VIRT_VMX_EPT_H_ */
