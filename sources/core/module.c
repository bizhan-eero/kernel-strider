/* module.c - initialization, cleanup, parameters and other common stuff. */

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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>

#include "core_impl.h"
#include "config.h"
/* ====================================================================== */

MODULE_AUTHOR("Eugene A. Shatokhin");
MODULE_LICENSE("GPL");
/* ====================================================================== */

/* Path to the helper script that obtains the list of sections of a target 
 * module */
//#define KEDR_UMH_GET_SECTIONS KEDR_UM_HELPER_PATH "/kedr_get_sections.sh"
/* ====================================================================== */

/* Name of the module to analyze, an empty name will match no module */
char *target_name = "";
module_param(target_name, charp, S_IRUGO);
/* ====================================================================== */

static struct kedr_event_handlers *eh_default = NULL;

/* The current set of event handlers. If no set is registered, 'eh_current' 
 * must be the address of the default set, i.e. eh_current == eh_default. 
 * Except the initial assignment, all accesses to 'eh_current' pointer must
 * be protected with 'target_mutex'. This way, we make sure the instrumented 
 * code will see the set of handlers in a consistent state. 
 * 
 * Note that calling the handlers from '*eh_current' is expected to be done 
 * without locking 'target_mutex'. As long as the structure pointed to by
 * 'eh_current' stays unchanged since its registration till its 
 * de-registration, this makes no harm. Only the changes in the pointer 
 * itself must be protected. */
struct kedr_event_handlers *eh_current = NULL;

/* The module being analyzed. NULL if the module is not currently loaded. 
 * The accesses to this variable must be protected with 'target_mutex'. */
static struct module *target_module = NULL;

/* If nonzero, module load and unload notifications will be handled,
 * if 0, they will not. */
static int handle_module_notifications = 0;

/* A mutex to protect the data related to the target module. */
static DEFINE_MUTEX(target_mutex);

/* This flag indicates whether try_module_get() failed for the owner of the
 * currently used set of callbacks in on_module_load(). */
static int module_get_failed = 0;
/* ====================================================================== */

static struct kedr_local_storage *
default_alloc_ls(struct kedr_ls_allocator *al)
{
	return (struct kedr_local_storage *)kzalloc(
		sizeof(struct kedr_local_storage), GFP_ATOMIC);
}

static void 
default_free_ls(struct kedr_ls_allocator *al, 
	struct kedr_local_storage *ls)
{
	kfree(ls);
	return;
}

static struct kedr_ls_allocator default_ls_allocator = {
	.alloc_ls = default_alloc_ls,
	.free_ls  = default_free_ls,
};

struct kedr_ls_allocator *ls_allocator = &default_ls_allocator;
/* ====================================================================== */

/* Non-zero if some set of event handlers has already been registered, 
 * 0 otherwise. 
 * Must be called with 'target_mutex' locked. */
static int
event_handlers_registered(void)
{
	return (eh_current != eh_default);
}

static int 
target_module_loaded(void)
{
	return (target_module != NULL);
}

int 
kedr_register_event_handlers(struct kedr_event_handlers *eh)
{
	int ret = 0;
	BUG_ON(eh == NULL || eh->owner == NULL);
	
	if (mutex_lock_killable(&target_mutex) != 0) {
		pr_warning(KEDR_MSG_PREFIX 
		"kedr_register_event_handlers(): failed to lock mutex\n");
		return -EINTR;
	}
	
	if (target_module_loaded()) {
		pr_warning(KEDR_MSG_PREFIX 
		"Unable to register event handlers: target module is "
		"loaded\n");
		ret = -EBUSY;
		goto out_unlock;
	}
	
	if (event_handlers_registered()) {
		pr_warning(KEDR_MSG_PREFIX 
		"Attempt to register event handlers while some set of "
		"handlers is already registered\n");
		ret = -EINVAL;
		goto out_unlock;
	}
	
	eh_current = eh;
	mutex_unlock(&target_mutex);
	return 0; /* success */

out_unlock:
	mutex_unlock(&target_mutex);
	return ret;
}
EXPORT_SYMBOL(kedr_register_event_handlers);

void 
kedr_unregister_event_handlers(struct kedr_event_handlers *eh)
{
	BUG_ON(eh == NULL || eh->owner == NULL);
	
	/* [NB] mutex_lock_killable() is not suitable here because we must
	 * lock the mutex anyway. The handlers must be restored to their 
	 * defaults even if their owner did something wrong. 
	 * If this mutex_lock() call hangs because some other code has taken 
	 * 'target_mutex' forever, it is our bug anyway and reboot will probably 
	 * be necessary among other things. It seems safer to let it hang than
	 * to allow the owner of the event handlers go away while these handlers
	 * might be in use. */
	mutex_lock(&target_mutex);
	
	if (target_module_loaded()) {
		pr_warning(KEDR_MSG_PREFIX 
		"Attempt to unregister event handlers while the target module is "
		"loaded\n");
		goto out;
	}
	
	if (eh != eh_current) {
		pr_warning(KEDR_MSG_PREFIX 
		"Attempt to unregister event handlers that are not "
		"registered\n");
		goto out;
	}

out:
	/* No matter if there were errors detected above or not, restore the
	 * handlers to their defaults, it is safer anyway. */
	eh_current = eh_default;
	mutex_unlock(&target_mutex);
	return;
}
EXPORT_SYMBOL(kedr_unregister_event_handlers);
/* ====================================================================== */

/* Module filter.
 * Should return nonzero if the core should watch for the module with the
 * specified name. We are interested in analyzing only the module with that 
 * name. */
static int 
filter_module(const char *module_name)
{
	return strcmp(module_name, target_name) == 0;
}

/* on_module_load() handles loading of the target module. This function is
 * called after the target module has been loaded into memory but before it
 * begins its initialization.
 *
 * Note that this function must be called with 'target_mutex' locked. */
static void 
on_module_load(struct module *mod)
{
	/*int ret = 0;*/
		
	pr_info(KEDR_MSG_PREFIX
		"Target module \"%s\" has just loaded. "
		"Estimated size of the code areas (in bytes): %u\n",
		module_name(mod), 
		(mod->init_text_size + mod->core_text_size));
	
	/* "Lock" the owner of the currently set event handlers in memory, that 
	 * is, prevent that module from unloading until the target is unloaded.
	 * Note that our module (THIS_MODULE) will also be "locked": either no
	 * external set of event handlers have been registered so far and 
	 * the default set is used - but eh_default->owner is our module and 
	 * it will be "locked" by try_module_get(). Or the current set of event
	 * handlers is provided by some other module, which will be locked. But
	 * that module uses API exported by our module and therefore our module
	 * will not be unloadable too until that set of handlers is 
	 * unregistered. */
	if (try_module_get(eh_current->owner) == 0)
	{
		pr_err(KEDR_MSG_PREFIX
			"try_module_get() failed for the module \"%s\".\n",
			module_name(eh_current->owner));
		module_get_failed = 1;
		
		/* If we failed to lock the module in memory, we should not
		 * instrument or otherwise affect the target module. */
		return;
	}
	
	/* Call the event handler, if set. */
	if (eh_current->on_target_loaded != NULL)
		eh_current->on_target_loaded(eh_current, mod);
	
	/* Initialize everything necessary to process the target module */
	/*ret = kedr_init_function_subsystem(mod);
	if (ret) {
		pr_err(KEDR_MSG_PREFIX
	"Failed to initialize function subsystem. Error code: %d\n",
			ret);
		goto out;
	}
	
	ret = kedr_process_target(mod);
	if (ret) {
		pr_err(KEDR_MSG_PREFIX
	"Error occured while processing \"%s\". Code: %d\n",
			module_name(mod), ret);
		goto out_cleanup_func;
	}*/
	return;
	
/*out_cleanup_func: 
	kedr_cleanup_function_subsystem();
out:	
	return;*/
}

/* on_module_unload() handles unloading of the target module. This function 
 * is called after the cleanup function of the latter has completed and the
 * module loader is about to unload that module.
 *
  * Note that this function must be called with 'target_mutex' locked.
 *
 * [NB] This function is called even if the initialization of the target
 * module fails. */
static void 
on_module_unload(struct module *mod)
{
	pr_info(KEDR_MSG_PREFIX
		"Target module \"%s\" is going to unload.\n",
		module_name(mod));
	
	if (!module_get_failed) {
		// TODO: cleanup what is left (if anything)
		/*kedr_cleanup_function_subsystem();*/
		
		/* Call the event handler, if set. */
		if (eh_current->on_target_about_to_unload != NULL)
			eh_current->on_target_about_to_unload(eh_current, 
				mod);
		
		/* Release the module, which is our module if no other set of event 
		 * handlers were registered or the owner of these handlers
		 * otherwise. */
		module_put(eh_current->owner);
	}
	module_get_failed = 0; /* reset the flag */
}

/* A callback function to handle loading and unloading of a module. 
 * Sets 'target_module' pointer among other things. */
static int 
detector_notifier_call(struct notifier_block *nb,
	unsigned long mod_state, void *vmod)
{
	struct module* mod = (struct module *)vmod;
	BUG_ON(mod == NULL);
    
	if (mutex_lock_killable(&target_mutex) != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
		"detector_notifier_call(): failed to lock target_mutex\n");
		return 0;
	}
    
	if (!handle_module_notifications)
		goto out;
	
	/* handle changes in the module state */
	switch(mod_state)
	{
	case MODULE_STATE_COMING: /* the module has just loaded */
		if(!filter_module(module_name(mod))) 
			break;

		BUG_ON(target_module_loaded());
		target_module = mod;
		on_module_load(mod);
		break;

	case MODULE_STATE_GOING: /* the module is going to unload */
		/* if the target module has already been unloaded,
		 * target_module is NULL, so (mod != target_module) 
		 * will be true. */
		if(mod != target_module) 
			break;

		on_module_unload(mod);
		target_module = NULL;
	}

out:
	mutex_unlock(&target_mutex);
	return 0;
}

/* A struct for watching for loading/unloading of modules.*/
struct notifier_block detector_nb = {
	.notifier_call = detector_notifier_call,
	.next = NULL,
	.priority = -1, 
	/* Priority 0 would also do but a lower priority value is safer.
	 * Our handler should be called after ftrace does its job
	 * (the notifier registered by ftrace uses priority 0). 
	 * ftrace seems to instrument the beginning of each function in the 
	 * newly loaded modules for its own purposes.  
	 * If our handler is called first, WARN_ON is triggered in ftrace.
	 * Everything seems to work afterwards but still the warning is 
	 * annoying. I suppose it is better to just let ftrace do its 
	 * work first and only then instrument the resulting code of 
	 * the target module. */
};
/* ====================================================================== */

void
kedr_set_ls_allocator(struct kedr_ls_allocator *al)
{
	int ret = 0;
	
	/* Because we need to check 'target_module' and update the allocator
	 * atomically w.r.t. the loading of the target module, we should
	 * lock 'target_mutex'. 
	 * It is only allowed to change the allocator if the target module
	 * is not loaded: different allocators can be incompatible with 
	 * each other. If the local storage has been allocated by a given 
	 * allocator, it must be freed by the same allocator. */
	ret = mutex_lock_killable(&target_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
		"kedr_set_ls_allocator(): failed to lock target_mutex\n");
		goto out;
	}

	if (target_module_loaded()) {
		pr_warning(KEDR_MSG_PREFIX
	"Attempt to change local storage allocator while the target "
	"is loaded. The allocator will not be changed.\n");
		goto out_unlock;
	}
	
	ls_allocator = (al == NULL) ? &default_ls_allocator : al;
	
out_unlock:	
	mutex_unlock(&target_mutex);
out:
	return;
}
EXPORT_SYMBOL(kedr_set_ls_allocator);

struct kedr_ls_allocator *
kedr_get_ls_allocator(void)
{
	return ls_allocator;
}
EXPORT_SYMBOL(kedr_get_ls_allocator);
/* ====================================================================== */

static int __init
core_init_module(void)
{
	int ret = 0;
	
	pr_info(KEDR_MSG_PREFIX "Initializing\n");
	
	eh_default = (struct kedr_event_handlers *)kzalloc(
		sizeof(struct kedr_event_handlers), GFP_KERNEL);
	if (eh_default == NULL)
		return -ENOMEM;
	
	eh_default->owner = THIS_MODULE;
	/* Initialize 'eh_current' before registering with the notification 
	 * system. */
	eh_current = eh_default;
	
	/* TODO: if something else needs to be initialized, do it before 
	 * registering our callbacks with the notification system.
	 * Do not forget to re-check labels in the error path after that. */
	
	/* find_module() requires 'module_mutex' to be locked. */
	ret = mutex_lock_killable(&module_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX 
			"Failed to lock module_mutex\n");
		goto out_free_eh;
	}
    
	ret = register_module_notifier(&detector_nb);
	if (ret < 0) {
		pr_warning(KEDR_MSG_PREFIX 
			"register_module_notifier() failed with error %d\n",
			ret);
		goto out_unlock;
	}
    
	/* Check if the target is already loaded */
	if (find_module(target_name) != NULL)
	{
		pr_warning(KEDR_MSG_PREFIX
		"Target module \"%s\" is already loaded. Processing of "
		"already loaded target modules is not supported\n",
		target_name);

		ret = -EEXIST;
		goto out_unreg_notifier;
	}
    
	ret = mutex_lock_killable(&target_mutex);
	if (ret != 0)
	{
		pr_warning(KEDR_MSG_PREFIX
			"init(): failed to lock target_mutex\n");
		goto out_unreg_notifier;
	}

	handle_module_notifications = 1;
	mutex_unlock(&target_mutex);

	mutex_unlock(&module_mutex);
        
/* From now on, our module will be notified when the target module
 * is loaded or have finished cleaning-up and is just about to unload. */
	return 0; /* success */

out_unreg_notifier:
	unregister_module_notifier(&detector_nb);

out_unlock:
	mutex_unlock(&module_mutex);

out_free_eh:
	kfree(eh_default);
	return ret;
}

static void __exit
core_exit_module(void)
{
	pr_info(KEDR_MSG_PREFIX "Cleaning up\n");
	
	/* Unregister notifications before cleaning up the rest. */
	unregister_module_notifier(&detector_nb);
	
	// TODO: more cleanup here
	
	kfree(eh_default);
	return;
}

module_init(core_init_module);
module_exit(core_exit_module);
/* ================================================================ */
