/* thunks_*.S - thunks used when handling function calls.
 * The thunks are responsible for calling pre- and post- handlers and the
 * replacement function for the target in the correct environment
 * (registers, etc.). */

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

.text

#include "kedr_asm_offsets.h"

/* Within the thunks, %ebx is used to contain the address of the
 * local storage, %ebp - the address of the call_info instance.
 * Both of these registers are non-scratch and are therefore preserved
 * by the handlers called from the thunks, which is convenient. */

#define save_sp_and_scratch_but_ax \
	mov %ecx, KEDR_LSTORAGE_cx(%ebx); \
	mov %edx, KEDR_LSTORAGE_dx(%ebx); \
	mov %esp, KEDR_LSTORAGE_sp(%ebx);

#define restore_scratch_but_ax \
	mov KEDR_LSTORAGE_dx(%ebx), %edx; \
	mov KEDR_LSTORAGE_cx(%ebx), %ecx;

/* ====================================================================== */

.global kedr_thunk_call
.type   kedr_thunk_call,@function;

kedr_thunk_call:
	/* We cannot use the spill slots for %ebx and %ebp to save the
	 * values of these registers there. This is because if one of
	 * these is %base in the caller function, we would corrupt the
	 * original value of %base this way. So, use 'temp_*'. */
	mov %ebx, KEDR_LSTORAGE_temp_bx(%eax);
	mov %eax, %ebx;

	mov %ebp, KEDR_LSTORAGE_temp_bp(%ebx);
	mov KEDR_LSTORAGE_info(%ebx), %ebp;

	/* The thunk has been called with CALL instruction, so the return
	 * address of the thunk should now be on the top of the stack.
	 * Below it lies the stack that the target function might expect.
	 * So we remove the return address from the stack restoring the
	 * latter and will place the return address back again later. */
	popl KEDR_LSTORAGE_ret_addr(%ebx);

	/* Now all the scratch registers except %eax have the needed
	 * values, and so does %esp. Save their values in the respective
	 * spill slots in the local storage, because the handlers may need
	 * them and are not obliged to preserve the scratch registers.
	 * Note that the needed value of %eax is already in its slot. */
	save_sp_and_scratch_but_ax;

	/* The first argument is passed via %eax on x86-32, so we are
	 * ready to call the pre-handler. */
	call *KEDR_CALL_INFO_pre_handler(%ebp);

	/* The values of the scratch registers might have been changed,
	 * restore them. */
	restore_scratch_but_ax;
	mov KEDR_LSTORAGE_ax(%ebx), %eax;

	/* Some exported kernel functions (like __put_user_<N>) actually
	 * clobber %ebx/%rbx. Fortunately, they seem not to clobber other
	 * non-scratch registers, so we use %edi on i586 and %r15 on x86_64
	 * to save %ebx/%rbx. local_storage::temp_aux is used for the
	 * original values of %edi/%r15 (for the same reason, 'temp_bx' and
	 * 'temp_bp' are used above). */
	mov %edi, KEDR_LSTORAGE_temp_aux(%ebx);
	mov %ebx, %edi;

	/* Call the replacement function for the target, in the same
	 * environment (registers, etc.) as the target would be called. */
	call *KEDR_CALL_INFO_repl(%ebp);

	mov %edi, %ebx;
	mov KEDR_LSTORAGE_temp_aux(%ebx), %edi;

	/* Push the return address of the thunk back on the stack. We can
	 * do it here because the original value of %esp is already in the
	 * local storage in case the post-handler needs that. */
	pushl KEDR_LSTORAGE_ret_addr(%ebx);

	/* Save %eax and %edx because the function might have returned
	 * something in these registers. If it did not, their contents are
	 * unspecified, which is OK (we give no stronger guarantees for
	 * the thunks than there are for the target function). */
	mov %eax, KEDR_LSTORAGE_ret_val(%ebx);
	mov %edx, KEDR_LSTORAGE_ret_val_high(%ebx);

	/* We do not need to restore the scratch registers again as the
	 * post-handler will take their values from the local storage
	 * anyway. We only need to prepare the argument of the handler. */
	mov %ebx, %eax;
	call *KEDR_CALL_INFO_post_handler(%ebp);

	/* Restore %eax and %edx from the 'ret_val*' slots, they may
	 * contain the return value of the replacement function. */
	mov KEDR_LSTORAGE_ret_val(%ebx), %eax;
	mov KEDR_LSTORAGE_ret_val_high(%ebx), %edx;

	/* It seems that the calls to the special Ftrace functions
	 * like mcount(), etc., give stronger guarantees w.r.t.
	 * preserving the registers. Namely, some of these functions
	 * preserve all the registers that can be used for parameter
	 * transfer (or even all?) and the compiler takes that into
	 * account. To avoid problems, restore the values of such
	 * scratch registers that have not been restored yet (%ecx).
	 * Note that %eax and %edx have the desired values already. */
#ifdef CONFIG_FUNCTION_TRACER
	mov KEDR_LSTORAGE_cx(%ebx), %ecx;
#endif

	/* Restore %ebx and %ebp. Among the remaining registers,
	 * %eax and %edx have already been restored, the non-scratch
	 * registers did not change their values except %ebx and %ebp.
	 * As for the remaining scratch registers, the target itself
	 * does not guarantee they will not change except if it is
	 * mcount() or something similar from Ftrace. */
	mov KEDR_LSTORAGE_temp_bp(%ebx), %ebp
	mov KEDR_LSTORAGE_temp_bx(%ebx), %ebx

	ret;
.size kedr_thunk_call, .-kedr_thunk_call
/* ====================================================================== */

.global kedr_thunk_jmp
.type   kedr_thunk_jmp,@function;

kedr_thunk_jmp:
	/* This thunk gets called via JMP instruction. All registers except
	 * %eax are restored before that and the original value of %eax is
	 * in its spill slot. An important consequence:
	 * - unlike kedr_thunk_call(), the spill slots can now be used to
	 * store the values of %ebx and %ebp in the thunk, the saving and
	 * restoring instructions will be 3 bytes shorter. */
	mov %ebx, KEDR_LSTORAGE_bx(%eax)
	mov %eax, %ebx

	mov %ebp, KEDR_LSTORAGE_bp(%ebx)
	mov KEDR_LSTORAGE_info(%ebx), %ebp

	/* The thunk has been called using JMP instruction, so the return
	 * address for the function the thunk has been called from
	 * ("caller") should now be on the top of the stack.
	 * Below it lies the stack that the target function might expect.
	 * The target/replacement function will be called with CALL rather
	 * than with JMP instruction. To make sure the callee will find
	 * the stack-based arguments exactly where it expects them, we
	 * need to remove the return address from the stack here and
	 * place it there back again after the callee exits. */
	popl KEDR_LSTORAGE_ret_addr(%ebx);

	/* [NB] If the target receives some of its arguments on stack, the
	 * saved value of %esp will point to the first of such arguments. */
	save_sp_and_scratch_but_ax;
	call *KEDR_CALL_INFO_pre_handler(%ebp);

	restore_scratch_but_ax;
	mov KEDR_LSTORAGE_ax(%ebx), %eax;

	mov %edi, KEDR_LSTORAGE_temp_aux(%ebx);
	mov %ebx, %edi;
	call *KEDR_CALL_INFO_repl(%ebp);
	mov %edi, %ebx;
	mov KEDR_LSTORAGE_temp_aux(%ebx), %edi;

	pushl KEDR_LSTORAGE_ret_addr(%ebx);

	/* Save the possible return value of the function. */
	mov %eax, KEDR_LSTORAGE_ret_val(%ebx);
	mov %edx, KEDR_LSTORAGE_ret_val_high(%ebx);

	mov %ebx, %eax;
	call *KEDR_CALL_INFO_post_handler(%ebp);

	/* Restore the possible return value of the function. */
	mov KEDR_LSTORAGE_ret_val(%ebx), %eax;
	mov KEDR_LSTORAGE_ret_val_high(%ebx), %edx;

	/* Restore %ebp. */
	mov KEDR_LSTORAGE_bp(%ebx), %ebp;

#ifdef CONFIG_FUNCTION_TRACER
	mov KEDR_LSTORAGE_cx(%ebx), %ecx;
#endif

	/* Now all the registers except %ebx have their needed values.
	 * Restore %ebx and call the exit handler for the function
	 * passing the pointer to the local storage to it via %eax.
	 * No need to bother saving the value of %edx before calling
	 * kedr_on_function_exit_wrapper() because the wrappers preserve
	 * all registers except %eax. */
	push  %eax;
	mov   %ebx, %eax;
	mov   KEDR_LSTORAGE_bx(%eax), %ebx;
	call  kedr_on_function_exit_wrapper;
	pop   %eax;

	ret;
.size kedr_thunk_jmp, .-kedr_thunk_jmp
/* ====================================================================== */
