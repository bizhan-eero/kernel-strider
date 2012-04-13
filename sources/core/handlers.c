/* handlers.c - operations provided by the framework to be used in the 
 * instrumented code: handling of function entry and exit, etc.
 * Some of these operations may be used during the instrumentation as well.
 * The wrapper and holder functions for these operations are also defined 
 * here. */

/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>

#include "config.h"
#include "core_impl.h"

#include "handlers.h"
#include "tid.h"

/* ====================================================================== */

/* KEDR_SAVE_SCRATCH_REGS_BUT_AX
 * Save scratch registers (except %eax/%rax) and flags on the stack.
 * 
 * KEDR_RESTORE_SCRATCH_REGS_BUT_AX
 * Restore scratch registers (except %eax/%rax) and flags from the stack.
 *
 * KEDR_PREPARE_ARG_FROM_AX
 * On x86-32, does nothing because the first argument should be passed in 
 * %eax (if 'regparm' compiler option is used) and it is already there now. 
 * On x86-64, copies the value from %rax to %rdi where the first argument 
 * of the function should reside on this architecture. 
 * 
 * These macros can be used when injecting a function call into the 
 * code or making a function call from the instrumented code. The function 
 * to be called is a C function, so, according to x86 ABI, it is responsible
 * for preserving the values of the non-scratch registers. %eax/%rax should 
 * be saved and restored separately by the caller of the wrapper code. This 
 * register is used to pass the argument to the function to be called and 
 * to contain its return value. */
#ifdef CONFIG_X86_64
# define KEDR_SAVE_SCRATCH_REGS_BUT_AX \
	"pushfq\n\t"		\
	"pushq %rcx\n\t"	\
	"pushq %rdx\n\t"	\
	"pushq %rsi\n\t"	\
	"pushq %rdi\n\t"	\
	"pushq %r8\n\t"		\
	"pushq %r9\n\t"		\
	"pushq %r10\n\t"	\
	"pushq %r11\n\t"

# define KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
	"popq %r11\n\t"	\
	"popq %r10\n\t"	\
	"popq %r9\n\t"	\
	"popq %r8\n\t"	\
	"popq %rdi\n\t"	\
	"popq %rsi\n\t"	\
	"popq %rdx\n\t"	\
	"popq %rcx\n\t"	\
	"popfq\n\t"

# define KEDR_PREPARE_ARG_FROM_AX \
	"movq %rax, %rdi\n\t"
	
#else /* CONFIG_X86_32 */
# define KEDR_SAVE_SCRATCH_REGS_BUT_AX \
	"pushf\n\t"		\
	"pushl %ecx\n\t"	\
	"pushl %edx\n\t"

# define KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
	"popl %edx\n\t"		\
	"popl %ecx\n\t"		\
	"popf\n\t"

# define KEDR_PREPARE_ARG_FROM_AX ""
#endif /* #ifdef CONFIG_X86_64 */

/* The "holder-wrapper" technique is inspired by the implementation of 
 * KProbes (kretprobe, actually) on x86. 
 *
 * Each function called via a wrapper of this kind takes its only parameter
 * in %eax/%rax. The return value of the function will also be stored in 
 * this register. Other general-purpose registers as well as the flags 
 * will be preserved by the wrappers.
 *
 * These wrappers allow to reduce code bloat. If it were not for them, one
 * would need to insert the code for saving and restoring registers directly
 * into the instrumented function.
 *
 * The holder functions are not intended to be called. Their main purpose is 
 * to contain the wrapper functions written in assembly. The wrappers must 
 * not have usual prologues and epilogues unlike the ordinary functions. 
 * This is because we need to control which registers the wrapper functions
 * use, as well as when and how they do it.
 * 
 * [NB] "__used" makes the compiler think the function is used even if it is
 * not clearly visible where and how. This prevents the compiler from 
 * removing this function. */

/* KEDR_DEFINE_WRAPPER(__func) defines the wrapper for function '__func',
 * along with its holder. */
#define KEDR_DEFINE_WRAPPER(__func) 		\
static __used void __func ## _holder(void) 	\
{						\
	asm volatile (				\
		".global " #__func "_wrapper\n"	\
		#__func "_wrapper: \n\t"	\
		KEDR_SAVE_SCRATCH_REGS_BUT_AX	\
		KEDR_PREPARE_ARG_FROM_AX	\
		"call " #__func " \n\t"		\
		KEDR_RESTORE_SCRATCH_REGS_BUT_AX \
		"ret\n");			\
}
/* ====================================================================== */

/* The operations that can be used in the instrumented code.
 * These functions are static because they should only be called only via 
 * the wrappers. The description of these functions is given in
 * internal_api.h in the comments for the respective wrappers. */
static __used unsigned long
kedr_on_function_entry(unsigned long orig_func)
{
	struct kedr_local_storage *ls;
	
	ls = ls_allocator->alloc_ls(ls_allocator);
	if (ls == NULL)
		return 0;
	
	ls->orig_func = orig_func;
	ls->tid = kedr_get_thread_id();
	
	if (eh_current->on_function_entry != NULL)
		eh_current->on_function_entry(eh_current, ls->tid, 
			ls->orig_func);

	return (unsigned long)ls;
}
KEDR_DEFINE_WRAPPER(kedr_on_function_entry);
/* ====================================================================== */

static __used void
kedr_on_function_exit(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	
	if (eh_current->on_function_exit != NULL)
		eh_current->on_function_exit(eh_current, ls->tid, 
			ls->orig_func);

	ls_allocator->free_ls(ls_allocator, ls);
}
KEDR_DEFINE_WRAPPER(kedr_on_function_exit);
/* ====================================================================== */

/* Default pre-handler for a function call, just reports the respective
 * event. */
static void 
default_pre_handler(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	
	if (eh_current->on_call_pre != NULL)
		eh_current->on_call_pre(eh_current, ls->tid, info->pc,
			info->target);
}

/* Default post-handler for a function call, just reports the respective
 * event. */
static void 
default_post_handler(struct kedr_local_storage *ls)
{
	struct kedr_call_info *info = (struct kedr_call_info *)(ls->info);
	
	if (eh_current->on_call_post != NULL)
		eh_current->on_call_post(eh_current, ls->tid, info->pc,
			info->target);
}

void
kedr_fill_call_info(unsigned long ci)
{
	struct kedr_call_info *info = (struct kedr_call_info *)ci;

	if (function_handlers->fill_call_info != NULL &&
	    function_handlers->fill_call_info(function_handlers, info) != 0)
		return;
	
	/* Either no function handlers are set or no handlers have been
	 * found for info->target, using the defaults. */
	info->repl = info->target;
	info->pre_handler = default_pre_handler;
	info->post_handler = default_post_handler;
}
KEDR_DEFINE_WRAPPER(kedr_fill_call_info);
/* ====================================================================== */

/* For each memory access event that could happen in the block, executes 
 * on_memory_event() handler if set. 
 * 'data' is the pointer, the address of which has been passed to 
 * begin_memory_events() callback. */
static void
report_events(struct kedr_local_storage *ls, void *data)
{
	unsigned long i;
	unsigned long nval = 0;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	u32 mask_bit = 1;
	u32 write_mask = info->write_mask | ls->write_mask;
	
	if (eh_current->on_memory_event == NULL)
		return;
	
	for (i = 0; i < info->max_events; ++i, mask_bit <<= 1) {
		unsigned long n = nval;
		unsigned long size;
		enum kedr_memory_event_type type = KEDR_ET_MREAD;
		
		if (info->string_mask & mask_bit) {
			size = ls->values[n + 1];
			nval += 2;
		}
		else {
			size = info->events[i].size;
			nval += 1;
		}
		
		if (write_mask & mask_bit) {
			type = ((info->read_mask & mask_bit) != 0) ? 
				KEDR_ET_MUPDATE :
				KEDR_ET_MWRITE;
		}
		
		eh_current->on_memory_event(eh_current, ls->tid, 
			info->events[i].pc, ls->values[n], size, type,
			data);
	}
}

static __used void
kedr_on_common_block_end(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	
	void *data = NULL;
	
	if (eh_current->begin_memory_events != NULL) {
		eh_current->begin_memory_events(eh_current, ls->tid, 
			info->max_events, &data);
	}
	
	report_events(ls, data);
	
	if (eh_current->end_memory_events != NULL)
		eh_current->end_memory_events(eh_current, ls->tid, data);
	
	/* Prepare the storage for later use. */
	memset(&ls->values[0], 0, 
		KEDR_MAX_LOCAL_VALUES * sizeof(unsigned long));
	ls->write_mask = 0;
	ls->dest_addr = 0;
}
KEDR_DEFINE_WRAPPER(kedr_on_common_block_end);
/* ====================================================================== */

static __used void
kedr_on_locked_op_pre(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	
	if (eh_current->on_locked_op_pre != NULL) {
		ls->temp = 0;
		eh_current->on_locked_op_pre(eh_current, ls->tid,
			info->events[0].pc, (void **)&ls->temp);
	}
}
KEDR_DEFINE_WRAPPER(kedr_on_locked_op_pre);

static __used void
kedr_on_locked_op_post(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	
	/* [NB] Here we make use of the fact that a locked update cannot be
	 * a string operation and it is the only operation 'info' contains
	 * the data for. 
	 *
	 * [NB] A locked operation is not necessarily an update. For 
	 * example, it can be a "read" in case of CMPXCHG*. */
	if (eh_current->on_locked_op_post != NULL) {
		u32 write_mask = info->write_mask | ls->write_mask;
		enum kedr_memory_event_type type = KEDR_ET_MREAD;
		if (write_mask & 1) {
			type = ((info->read_mask & 1) != 0) ? 
				KEDR_ET_MUPDATE :
				KEDR_ET_MWRITE;
		}
		
		eh_current->on_locked_op_post(eh_current, ls->tid,
			info->events[0].pc, ls->values[0], 
			info->events[0].size, type, 
			(void *)ls->temp);
	}
	
	/* Prepare the storage for later use. */
	ls->values[0] = 0;
	ls->write_mask = 0;
}
KEDR_DEFINE_WRAPPER(kedr_on_locked_op_post);
/* ====================================================================== */

static __used void
kedr_on_io_mem_op_pre(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	
	if (eh_current->on_io_mem_op_pre != NULL) {
		ls->temp = 0;
		eh_current->on_io_mem_op_pre(eh_current, ls->tid,
			info->events[0].pc, (void **)&ls->temp);
	}
}
KEDR_DEFINE_WRAPPER(kedr_on_io_mem_op_pre);

static __used void
kedr_on_io_mem_op_post(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	struct kedr_block_info *info = (struct kedr_block_info *)ls->info;
	
	/* [NB] Here we make use of the fact that an instruction in this 
	 * block is INS or OUTS, that is, a string operation of type X or Y
	 * but not XY. It is either read or write but not update. */
	
	if (eh_current->on_io_mem_op_post != NULL) {
		enum kedr_memory_event_type type = KEDR_ET_MREAD;
		if (info->write_mask & 1)
			type = KEDR_ET_MWRITE;
	
		eh_current->on_io_mem_op_post(eh_current, ls->tid, 
			info->events[0].pc, ls->values[0], ls->values[1],
			type, (void *)ls->temp);
	}
	
	/* Prepare the storage for later use. */
	ls->values[0] = 0;
	ls->values[1] = 0;
}
KEDR_DEFINE_WRAPPER(kedr_on_io_mem_op_post);
/* ====================================================================== */

static __used void
kedr_on_barrier_pre(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	enum kedr_barrier_type bt = (enum kedr_barrier_type)(u8)ls->temp;
	unsigned long pc = ls->temp1;
	
	if (eh_current->on_memory_barrier_pre != NULL)
		eh_current->on_memory_barrier_pre(eh_current, ls->tid, pc,
			bt);
}
KEDR_DEFINE_WRAPPER(kedr_on_barrier_pre);

static __used void
kedr_on_barrier_post(unsigned long storage)
{
	struct kedr_local_storage *ls = 
		(struct kedr_local_storage *)storage;
	enum kedr_barrier_type bt = (enum kedr_barrier_type)(u8)ls->temp;
	unsigned long pc = ls->temp1;
	
	if (eh_current->on_memory_barrier_post != NULL)
		eh_current->on_memory_barrier_post(eh_current, ls->tid, pc,
			bt);
}
KEDR_DEFINE_WRAPPER(kedr_on_barrier_post);
/* ====================================================================== */