#ifndef _MACHINE_VM_H_
#define _MACHINE_VM_H_

#ifdef _KERN_

#define VM_BOTTOM	0x00000000
#define VM_TOP		0xffffffff

/*
 * User Virtual Address Space
 *
 *                     |                              |
 *    VM_USERHI, ----> +==============================+ 0xf0000000
 *    VM_STACKHI       |                              |
 *                     |      User address space      |
 *                     |                              |
 *    VM_USERLO  ----> +==============================+ 0x40000000
 *                     |                              |
 *
 * - User address space is from VM_USERLO to VM_USERHI. User apps are allowed to
 *   access their user address spaces.
 *
 * - The user apps are not allowed to access the address space out of this
 *   range (< VM_USERLO or >= VM_USERHI).
 *
 * - CertiKOS kernel uses VM_STACKHI as the start address of the user stack.
 *
 */

#define VM_STACKHI	0xf0000000
#define VM_USERHI	0xf0000000
#define VM_USERLO	0x40000000

/*
 * Kernel Virtual Address Space
 *
 *
 *    VM_TOP,    ----> +==============================+ 0xffffffff
 *    VM_DEVHI         |                              |
 *                     |     Reserved for devices     |
 *                     |       e.g. local APIC        |
 *                     |                              |
 *    VM_DEVLO,  ----> +------------------------------+ 0xf0000000
 *    VM_HIGHMEMHI     |                              |
 *                     :      High Memory Space       :
 *                     |                              |
 *    VM_HIGHMEMLO, -> +------------------------------+ 0x40000000
 *    VM_KERNHI        |                              |
 *                     |    General-purpose kernel    |
 *                     |        address space         |
 *                     |                              |
 *    VM_KERNLO, ----> +------------------------------+ 0x00100000
 *    VM_BOOTHI        |                              |
 *                     |         Reserved for         |
 *                     |       bootloader & BIOS      |
 *                     |                              |
 *    VM_BOOTLO, ----> +==============================+ 0x00000000
 *    VM_BOTTOM
 */

#define VM_DEVHI	0xffffffff
#define VM_DEVLO	0xf0000000

#define VM_HIGHMEMHI	0xf0000000
#define VM_HIGHMEMLO	0x40000000

#define VM_KERNHI	0x40000000
#define VM_KERNLO	0x00100000

#define VM_BOOTHI	0x00100000
#define VM_BOOTLO	0x00000000

#endif /* _KERN_ */

#endif /* !_MACHINE_VM_H_ */
