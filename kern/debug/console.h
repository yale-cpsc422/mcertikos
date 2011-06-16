/* See COPYRIGHT for copyright information. */

#ifndef PIOS_KERN_CONSOLE_H_
#define PIOS_KERN_CONSOLE_H_

#include <architecture/types.h>


#define CONSBUFSIZE 512

struct iocons;


void cons_init(void);
void cons_intr(int (*proc)(void));
static void cons_putc(int c);

#endif /* PIOS_KERN_CONSOLE_H_ */
