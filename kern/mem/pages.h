
#ifndef PIOS_KERN_PAGE_H
#define PIOS_KERN_PAGE_H


#define PAGE_SHIFT 12
#define PAGE_SIZE 4096


#define PAGE_SHIFT_4MB 22
#define PAGE_SIZE_4MB  ( 1 << PAGE_SHIFT_4MB )
#define PAGE_MASK_4MB  ( ~ ( PAGE_SIZE_4MB - 1 ) )

#define PAGE_SHIFT_2MB 21
#define PAGE_SIZE_2MB  ( 1 << PAGE_SHIFT_2MB )
//haind for 4kb page
#define PAGE_SHIFT_4KB 12
#define PAGE_SIZE_4KB (1 << PAGE_SHIFT_4KB)


#define PFN_UP_2MB(x)   (((x) + PAGE_SIZE_2MB - 1) >> PAGE_SHIFT_2MB)
#define PFN_DOWN_2MB(x) ((x) >> PAGE_SHIFT_2MB)


#define PAGE_MASK  ( ~ ( PAGE_SIZE - 1 ) )
#define PFN_UP(x)       (((x) + PAGE_SIZE - 1) >> PAGE_SHIFT)
#define PFN_DOWN(x)     ((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)     ((x) << PAGE_SHIFT)

#define PAGE_UP(p)    ( ( (p) + ( PAGE_SIZE - 1 ) ) & PAGE_MASK )       // round up to closest next 4KB page boundary
#define PAGE_DOWN(p)  ( (p) & PAGE_MASK )               // round down to closest previous 4KB page boundary

#define PAGE_UP_4MB(p)    ( ( (p) + ( PAGE_SIZE_4MB - 1 ) ) & PAGE_MASK_4MB )   // round up to closest next 4MB page boundary
#define PAGE_DOWN_4MB(p)  ( (p) & PAGE_MASK_4MB )               // round down to closest previous 4MB page boundary


enum pg_table_level {
        PGT_LEVEL_PML4 = 4,
        PGT_LEVEL_PDP  = 3,
        PGT_LEVEL_PD   = 2,
        PGT_LEVEL_PT   = 1 //haind for page table
};

// For 4MB page translation, PAE disabled, vol2 p124  */
struct pd4M_entry {
        uint16_t flags:      13; /* Bit 0-12 */
        uint32_t basehigh:   8;      /* Bit 13-20 of the entry => bit 32-39 of base */
        uint8_t  rsvr:               1;      /* Bit 21 */
        uint16_t baselow:    10;     /* Bit 22-31 of the entry => bit 22-31 of base */
} __attribute__ ((packed)) pd4M_entry;

/*haind - For 4KB page translation, PAE disabled*/
union pgt_entry_4kb
{

        struct pde {
                uint32_t flags: 12; /* Bit 0-11  */
                uint32_t base:  20; /* Bit 12-31 */

        } __attribute__ ((packed)) pde;

        struct pte {
                        uint32_t flags: 12; /* Bit 0-11  */
                        uint32_t base:  20; /* Bit 12-31 */

        } __attribute__ ((packed)) pte;
};


/* For 2-Mbyte page translation (long-mode) */
union pgt_entry_2mb
{
        /* 2-Mbyte PML4E and PDPE */
        struct non_term {
                uint16_t flags: 12; /* Bit 0-11  */
                uint64_t base:  40; /* Bit 12-51 */
                uint16_t avail: 11; /* Bit 52-62 */
                uint16_t nx:    1;  /* Bit 63    */
        } __attribute__ ((packed)) non_term;

        /* 2-Mbyte PDE */
        struct term {
                uint32_t flags: 21; /* Bit 0-20  */
                uint32_t base:  31; /* Bit 21-51 */
                uint16_t avail: 11; /* Bit 52-62 */
                uint16_t nx:    1;  /* Bit 63    */
        } __attribute__ ((packed)) term;
};

/* Page-Translation-Table Entry Fields
   [REF] vol.2, p. 168- */
#define _PTTEF_PRESENT   0
#define _PTTEF_RW        1 /* Read/Write */
#define _PTTEF_US        2 /* User/Supervisor */
#define _PTTEF_ACCESSED  5
#define _PTTEF_DIRTY     6
#define _PTTEF_PAGE_SIZE 7
#define _PTTEF_GLOBAL    8
#define PTTEF_PRESENT    (1 << _PTTEF_PRESENT)
#define PTTEF_RW         (1 << _PTTEF_RW)
#define PTTEF_US         (1 << _PTTEF_US)
#define PTTEF_ACCESSED   (1 << _PTTEF_ACCESSED)
#define PTTEF_DIRTY      (1 << _PTTEF_DIRTY)
#define PTTEF_PAGE_SIZE  (1 << _PTTEF_PAGE_SIZE)
#define PTTEF_GLOBAL     (1 << _PTTEF_GLOBAL)

#endif
