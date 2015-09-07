#ifndef _KERN_MM_MALINIT_H_
#define _KERN_MM_MALINIT_H_

unsigned int get_nps(void);
void set_nps(unsigned int nps);

unsigned int is_norm(unsigned int idx);
void set_norm(unsigned int idx, unsigned int val);

unsigned int at_get(unsigned int idx);
void at_set(unsigned int idx, unsigned int val);


#ifdef _KERN_


