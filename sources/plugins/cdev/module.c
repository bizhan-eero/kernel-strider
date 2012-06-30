/* A plugin to the function handling subsystem that allows to use KEDR-COI
 * to establish needed happens-before links for character devices. */

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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include <kedr/kedr_mem/functions.h>
#include <kedr/kedr_mem/core_api.h>
#include <kedr/object_types.h>

#include <kedr-coi/interceptors/file_operations_interceptor.h>
#include "cdev_file_operations_interceptor.h"
/* ====================================================================== */

MODULE_AUTHOR("Andrey Tsyvarev");
MODULE_LICENSE("GPL");
/* ====================================================================== */
/* 
 * Helpers for generate events.
 * 
 * (Really, them should be defined by the core).
 */

/* Pattern for handlers wrappers */
#define GENERATE_HANDLER_CALL(handler_name, ...) do {               \
    struct kedr_event_handlers *eh = kedr_get_event_handlers();     \
    if(eh && eh->handler_name) eh->handler_name(eh, ##__VA_ARGS__); \
}while(0)

static inline void generate_signal_pre(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_signal_pre, tid, pc, obj_id, type);
}

static inline void generate_signal_post(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_signal_post, tid, pc, obj_id, type);
}

static inline void generate_wait_pre(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_wait_pre, tid, pc, obj_id, type);
}

static inline void generate_wait_post(unsigned long tid, unsigned long pc,
    unsigned long obj_id, enum kedr_sw_object_type type)
{
    GENERATE_HANDLER_CALL(on_wait_post, tid, pc, obj_id, type);
}

static inline void generate_alloc_pre(unsigned long tid, unsigned long pc,
    unsigned long size)
{
    GENERATE_HANDLER_CALL(on_alloc_pre, tid, pc, size);
}

static inline void generate_alloc_post(unsigned long tid, unsigned long pc,
    unsigned long size, void* pointer)
{
    GENERATE_HANDLER_CALL(on_alloc_post, tid, pc, size, (unsigned long)pointer);
}

static inline void generate_free_pre(unsigned long tid, unsigned long pc,
    void* pointer)
{
    GENERATE_HANDLER_CALL(on_free_pre, tid, pc, (unsigned long)pointer);
}

static inline void generate_free_post(unsigned long tid, unsigned long pc,
    void* pointer)
{
    GENERATE_HANDLER_CALL(on_free_post, tid, pc, (unsigned long)pointer);
}

/* Derived events generation and identificators */

/* 
 * Model for refcount-like mechanizm.
 * Useful for implement "after all" relation.
 * 
 * 'ref_get' acquires reference on some object(reference address).
 * 'ref_put' releases reference,
 * 'ref_last' is executed after all other references are released.
 */
static inline void generate_ref_get(unsigned long tid,
    unsigned long pc, unsigned long ref_addr)
{
    (void)tid;
    (void)pc;
    (void)ref_addr;
    /* do nothing */
}

static inline void generate_ref_put(unsigned long tid,
    unsigned long pc, unsigned long ref_addr)
{
    generate_signal_pre(tid, pc, ref_addr, KEDR_SWT_COMMON);
    generate_signal_post(tid, pc, ref_addr, KEDR_SWT_COMMON);
}

static inline void generate_ref_last(unsigned long tid,
    unsigned long pc, unsigned long ref_addr)
{
    generate_wait_post(tid, pc, ref_addr, KEDR_SWT_COMMON);
    generate_wait_post(tid, pc, ref_addr, KEDR_SWT_COMMON);
}

/* release() should be called after all other file callbacks*/
static inline unsigned long filp_ref(struct file* filp) {return (unsigned long)filp;}
/* module_exit() should be called after all module references are put. */
static inline unsigned long module_ref(struct module* m) {return (unsigned long)m;}
/* open() should be called before all other file callbacks */
static inline unsigned long filp_created(struct file* filp) {return (unsigned long)&filp->f_op;}
/* cdev_add should be called before devices may be opened */
static inline unsigned long cdev_added(struct cdev* dev) {return (unsigned long)&dev->ops;}


/* ====================================================================== */
/* Interception of file callbacks. */

static void 
fop_open_pre(struct inode* inode, struct file* filp,
	struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();

    /* File may be opened only if corresponded device is added.*/
    generate_wait_pre(tid, pc, cdev_added(inode->i_cdev), KEDR_SWT_COMMON);
    generate_wait_post(tid, pc, cdev_added(inode->i_cdev), KEDR_SWT_COMMON);
    
	/* Allocation of file structure has done. */
    generate_alloc_pre(tid, pc, sizeof(*filp));
    generate_alloc_post(tid, pc, sizeof(*filp), filp);

    /* Owner of operations cannot be unloaded now. */
    if(filp->f_op->owner)
        generate_ref_get(tid, pc, module_ref(filp->f_op->owner));
}

static void 
fop_open_post(struct inode* inode, struct file* filp, int ret_val,
	struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();

	if (ret_val != 0) {
		
		/* If open() has failed, we may inform the interceptor that
		 * it does not need to bother watching the current '*filp' 
		 * object. */
		file_operations_interceptor_forget(filp);
	
        /* Owner of operations may be unloaded now. */
        if(filp->f_op->owner)
            generate_ref_put(tid, pc, module_ref(filp->f_op->owner));

		/* Specify that the struct file instance pointed to by 
		 * 'filp' is no longer available ("memory released"). */
        generate_free_pre(tid, pc, filp);
        generate_free_post(tid, pc, filp);
	}
    /* Operations may be changed in open(), so update watching. */
	file_operations_interceptor_watch(filp);

    /* The first 'Reference' on filp is acquired. */
    generate_ref_get(tid, pc, filp_ref(filp));

    /* Relation: "Callbacks may be called only after open() returns. */
    generate_signal_pre(tid, pc, filp_created(filp), KEDR_SWT_COMMON);
    generate_signal_post(tid, pc, filp_created(filp), KEDR_SWT_COMMON);
}

static void 
fop_release_pre(struct inode* inode, struct file* filp,
	struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();
	
    /* Relation: "release() may be called only after open() returns. */
    generate_wait_pre(tid, pc, filp_created(filp), KEDR_SWT_COMMON);
    generate_wait_post(tid, pc, filp_created(filp), KEDR_SWT_COMMON);

    /* Relation: "All callbacks should be finished at release call. */
    generate_ref_last(tid, pc, filp_ref(filp));
}


static void 
fop_release_post(struct inode* inode, struct file* filp, int ret_val,
	struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();

	if (ret_val == 0) {
		/* If release() has been successful, the interceptor may
		 * stop watching '*filp'. */
		file_operations_interceptor_forget(filp);
		
        /* Owner of operations may be unloaded now. */
        if(filp->f_op->owner)
            generate_ref_put(tid, pc, module_ref(filp->f_op->owner));
        
        /* Specify that the struct file instance pointed to by 
		 * 'filp' is no longer available ("memory released"). */
		generate_free_pre(tid, pc, filp);
        generate_free_post(tid, pc, filp);
	}
}

/* Other callback operations in file */
static void fop_write_pre(struct file * filp, const char __user * buf,
    size_t count, loff_t * f_pos,
    struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();
    /* Relation: "write() may be called only after open() returns. */
    generate_wait_pre(tid, pc, filp_created(filp), KEDR_SWT_COMMON);
    generate_wait_post(tid, pc, filp_created(filp), KEDR_SWT_COMMON);
    /* Acquire reference on file('filp' cannot be destroyed now)*/
    generate_ref_get(tid, pc, filp_ref(filp));
}

static void fop_write_post(struct file * filp, const char __user * buf,
    size_t count, loff_t * f_pos, ssize_t result,
    struct kedr_coi_operation_call_info* call_info)
{
    unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();

    /* Drop reference on file('filp' may be destroyed now)*/
    generate_ref_put(tid, pc, filp_ref(filp));
}

static void fop_read_pre(struct file * filp, char __user * buf,
    size_t count, loff_t * f_pos,
    struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();

    /* Relation: "read() may be called only after open() returns. */
    generate_wait_pre(tid, pc, filp_created(filp), KEDR_SWT_COMMON);
    generate_wait_post(tid, pc, filp_created(filp), KEDR_SWT_COMMON);
    /* Acquire reference on file('filp' cannot be destroyed now)*/
    generate_ref_get(tid, pc, filp_ref(filp));
}

static void fop_read_post(struct file * filp, char __user * buf,
    size_t count, loff_t * f_pos, ssize_t result,
    struct kedr_coi_operation_call_info* call_info)
{
    unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();
    /* Drop reference on file('filp' may be destroyed now)*/
    generate_ref_put(tid, pc, filp_ref(filp));
}


static void fop_llseek_pre(struct file *filp, loff_t off, int whence,
    struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();

    /* Relation: "llseek() may be called only after open() returns. */
    generate_wait_pre(tid, pc, filp_created(filp), KEDR_SWT_COMMON);
    generate_wait_post(tid, pc, filp_created(filp), KEDR_SWT_COMMON);
    /* Acquire reference on file('filp' cannot be destroyed now)*/
    generate_ref_get(tid, pc, filp_ref(filp));
}


static void fop_llseek_post(struct file *filp, loff_t off, int whence,
    loff_t result,
    struct kedr_coi_operation_call_info* call_info)
{
	unsigned long pc = (unsigned long)call_info->op_orig;
	unsigned long tid = kedr_get_thread_id();
    /* Drop reference on file('filp' may be destroyed now)*/
    generate_ref_put(tid, pc, filp_ref(filp));
}
// Other callbacks in same manner.
// Templates are needed.


static struct kedr_coi_pre_handler fop_pre_handlers[] =
{
	file_operations_open_pre_external(fop_open_pre),
    file_operations_release_pre_external(fop_release_pre),
    
    file_operations_write_pre(fop_write_pre),
    file_operations_read_pre(fop_read_pre),
    file_operations_llseek_pre(fop_llseek_pre),
	kedr_coi_pre_handler_end
};

static struct kedr_coi_post_handler fop_post_handlers[] =
{
    file_operations_open_post_external(fop_open_post),
	file_operations_release_post_external(fop_release_post),
    
    file_operations_write_post(fop_write_post),
    file_operations_read_post(fop_read_post),
    file_operations_llseek_post(fop_llseek_post),
	kedr_coi_post_handler_end
};

static struct kedr_coi_payload fop_payload =
{
	.mod = THIS_MODULE,
    
	.pre_handlers = fop_pre_handlers,
	.post_handlers = fop_post_handlers,
};

/* Initialization tasks needed to use KEDR-COI. */
static int 
coi_init(void)
{
	int ret = 0;
	
	ret = file_operations_interceptor_init(NULL);
	if (ret != 0) 
		goto out;
	
	ret = cdev_file_operations_interceptor_init(
		file_operations_interceptor_factory_interceptor_create,
		NULL);
	if (ret != 0) 
		goto out_fop;

	ret = file_operations_interceptor_payload_register(&fop_payload);
	if (ret != 0) 
		goto out_cdev;

    return 0;

out_cdev:
	cdev_file_operations_interceptor_destroy();
out_fop:
	file_operations_interceptor_destroy();
out:
	return ret;
}

static void 
coi_cleanup(void)
{
	file_operations_interceptor_payload_unregister(&fop_payload);
	cdev_file_operations_interceptor_destroy();
	file_operations_interceptor_destroy();
}
/* ====================================================================== */
/* Interception of character device functions. */
static int 
repl_cdev_add(struct cdev *p, dev_t dev, unsigned int count)
{
	int ret;
	
	unsigned long pc = (unsigned long)&cdev_add;
	unsigned long tid = kedr_get_thread_id();
	
	cdev_file_operations_interceptor_watch(p);
	
	/* 
     * Relation: Files for device(es) may be opened only after device(es)
     * registration.
     */
	generate_signal_pre(tid, pc, cdev_added(p), KEDR_SWT_COMMON);
    /* Call the target function itself. */
	ret = cdev_add(p, dev, count);
    
    generate_signal_post(tid, pc, cdev_added(p), KEDR_SWT_COMMON);	
	
	/* If cdev_add() has failed, no need to watch the object. */
	if (ret != 0)
		cdev_file_operations_interceptor_forget(p);
		
	return ret;
}

static void 
repl_cdev_del(struct cdev *p)
{
	/* It is caller who should order this call wrt others. */
    
    cdev_file_operations_interceptor_forget(p);
}

struct kedr_repl_pair rp[] = {
	{&cdev_add, &repl_cdev_add},
	{&cdev_del, &repl_cdev_del},

	{NULL, NULL}
};
/* ====================================================================== */

static void 
on_load(struct module *mod)
{
    file_operations_interceptor_start();
}

static void 
on_unload(struct module *mod)
{
	file_operations_interceptor_stop();
}

/* ====================================================================== */

struct kedr_fh_plugin fh_plugin = {
	.owner = THIS_MODULE,
	.on_target_loaded = on_load,
    .on_target_about_to_unload = on_unload,
	.repl_pairs = rp
};
/* ====================================================================== */

static void __exit
plugin_coi_exit(void)
{
	kedr_fh_plugin_unregister(&fh_plugin);
	coi_cleanup();
	
	return;
}

static int __init
plugin_coi_init(void)
{
	int ret = 0;
	
	ret = coi_init();
	if (ret != 0)
		goto out_unreg;
	
	ret = kedr_fh_plugin_register(&fh_plugin);
	if (ret != 0)
		goto out;
	
	return 0;

out_unreg:
	kedr_fh_plugin_unregister(&fh_plugin);
out:
	return ret;
}

module_init(plugin_coi_init);
module_exit(plugin_coi_exit);
/* ====================================================================== */