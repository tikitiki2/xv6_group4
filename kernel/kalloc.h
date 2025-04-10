#ifndef _KALLOC_H_
#define _KALLOC_H_

#include "types.h"

void kinit(void);
void *kalloc(void);
void kfree(void *);
void krefinc(void *pa);
void krefdec(void *pa);
int krefcount(void *pa);

#endif // _KALLOC_H_ 