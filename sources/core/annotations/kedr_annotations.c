/* kedr_annotations.c - definition of the annotation functions used by
 * KEDR tools. */

/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin
 *      Andrey V. Tsyvarev
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/module.h>
/* ====================================================================== */

/* Notes
 * 1. 
 * The volatile annotation IDs are here to prevent the compiler and linker
 * from merging the functions ("identical code folding"). 
 * If new types of annotations are added, they must have IDs different
 * from the previously existing ones, which would make the bodies of the 
 * functions look different from each other to the linker. 
 * 
 * 2.
 * The empty inline assembly blocks prevent the compiler from optimizing
 * away the calls to these functions. */
void noinline
kedr_annotate_happens_before(unsigned long id)
{
	volatile int ann_id = __LINE__;
	(void)ann_id;
	(void)id;
	asm volatile ("");
}
EXPORT_SYMBOL(kedr_annotate_happens_before);

void noinline
kedr_annotate_happens_after(unsigned long id)
{
	volatile int ann_id = __LINE__;
	(void)ann_id;
	(void)id;
	asm volatile ("");
}
EXPORT_SYMBOL(kedr_annotate_happens_after);

void noinline
kedr_annotate_memory_acquired(const void *addr, unsigned long size)
{
	volatile int ann_id = __LINE__;
	(void)ann_id;
	(void)addr;
	(void)size;
	asm volatile ("");
}
EXPORT_SYMBOL(kedr_annotate_memory_acquired);

void noinline
kedr_annotate_memory_released(const void *addr)
{
	volatile int ann_id = __LINE__;
	(void)ann_id;
	(void)addr;
	asm volatile ("");
}
EXPORT_SYMBOL(kedr_annotate_memory_released);
/* ================================================================ */
