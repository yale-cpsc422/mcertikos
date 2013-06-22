#ifndef _SYS_VM_H_
#define _SYS_VM_H_

#ifdef _KERN_

/*
 *                     |                              |
 *    VM_USERHI, ----> +==============================+ 0xf0000000
 *    VM_STACKHI       |                              |
 *                     |     Per-thread user stack    |
 *                     |                              |
 *    VM_STACKLO,      +------------------------------+ 0xd0000000
 *    VM_SCRATCHHI     |                              |
 *                     |    Scratch address space     |
 *                     | for file reconciliation etc. |
 *                     |                              |
 *    VM_SCRATCHLO, -> +------------------------------+ 0xc0000000
 *    VM_FILEHI        |                              |
 *                     |      File system and         |
 *                     |   process management state   |
 *                     |                              |
 *    VM_FILELO, ----> +------------------------------+ 0x80000000
 *    VM_SHAREHI       |                              |
 *                     |  General-purpose use space   |
 *                     | shared between user threads: |
 *                     |   program text, data, heap   |
 *                     |                              |
 *    VM_SHARELO, ---> +==============================+ 0x40000000
 *    VM_USERLO        |                              |
 */

#define VM_TOP		0xffffffff

/* Standard area for the user-space stack (thread-private) */
#define VM_STACKHI	0xf0000000
#define VM_STACKLO	0xd0000000
#define VM_USERHI	0xf0000000
#define VM_USERLO	0x40000000

/* Scratch address space region for general use (e.g., by exec) */
#define VM_SCRATCHHI	0xd0000000
#define VM_SCRATCHLO	0xc0000000

/* Address space area for file system and Unix process state */
#define VM_FILEHI	0xc0000000
#define VM_FILELO	0x80000000

/*
 * General-purpose address space shared between "threads"
 * created via SYS_SNAP/SYS_MERGE.
 */
#define VM_SHAREHI	0x80000000
#define VM_SHARELO	0x40000000

#endif /* _KERN_ */

#endif /* !_SYS_VM_H_ */
