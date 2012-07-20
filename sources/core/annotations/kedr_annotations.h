/* kedr_annotations.h - dynamic annotations that can be used to make the 
 * analysis more precise. 
 * The annotations provided here are "dynamic" in a sense that they are not
 * comments, i.e. they are not ignored by the compiler but rather they 
 * expand to the appropriate code fragments (function calls in this case). 
 * These calls do nothing by themselves. When the instrumentation system
 * processes the target module, it transforms them to generate appropriate 
 * events.
 *
 * To use these annotations, one needs to #include this header in the code 
 * of the target module and #define KEDR_ANNOTATIONS_ENABLED to a non-zero 
 * value. It is also needed to instruct the build system of the target 
 * module to compile kedr_annotations.� and link the resuling object file
 * into the target when the target is being built. 
 *
 * [NB] A target module containing such annotations can be used normally 
 * even if KernelStrider is not loaded. */

#ifndef KEDR_ANNOTATIONS_H_1536_INCLUDED
#define KEDR_ANNOTATIONS_H_1536_INCLUDED

#ifndef KEDR_ANNOTATIONS_ENABLED
#define KEDR_ANNOTATIONS_ENABLED 0
#endif

#if KEDR_ANNOTATIONS_ENABLED != 0

/* These annotations are similar to ANNOTATE_HAPPENS_BEFORE() and 
 * ANNOTATE_HAPPENS_AFTER() provided by ThreadSanitizer (see 
 * http://code.google.com/p/data-race-test/wiki/DynamicAnnotations
 * for details). 
 * 
 * These annotations indicate that the place in the code marked with 
 * KEDR_ANNOTATE_HAPPENS_BEFORE(id) cannot indeed execute after the place 
 * marked with KEDR_ANNOTATE_HAPPENS_AFTER() with the same 'id' if execution
 * of the former is observed first. Such pair of annotations specifies the
 * so called "happens-before" (or "happened-before") relationship between
 * these places in the code. For a formal description, see
 * http://en.wikipedia.org/wiki/Happened-before
 *
 * As an example, consider registration of a callback and execution of that
 * callback. Suppose we have an object and the kernel allows us to register
 * a function to be called when the state of that object changes somehow:
 * register_callback(&object, callback_func). Unless we use 'callback_func'
 * for some other purposes as well, it cannot start executing before 
 * register_callback() starts executing. This is a happens-before relation.
 * If the object is not involved in other happens-before relations, we can 
 * use its address as the ID of this particular relation. 
 * This way, the place in the code where register_callback() is called, can
 * be annotated as follows:
 *	<...>
 *	KEDR_ANNOTATE_HAPPENS_BEFORE(&object);
 *	register_callback(&object, callback_func);
 *	<...>
 * The matching annotation should be placed at the beginning of 
 * callback_func() in this case:
 *	static some_type callback_func(<some arguments>)
 *	{
 *		KEDR_ANNOTATE_HAPPENS_AFTER(&object);
 *		<the rest of the function>
 *	}
 * KEDR_ANNOTATE_HAPPENS_BEFORE() should usually be placed right before 
 * and KEDR_ANNOTATE_HAPPENS_AFTER - right after the relevant locations in 
 * the code to be annotated.
 * 
 * Note that it is not required to use the address of the object as the ID
 * of the relation. It is only needed that the matching annotations use the 
 * same ID for the analysis tools to be able to recognize this 
 * happens-before relation. */
 
/* It seems that nothing prevents the compiler from reordering the 
 * operations in the code and a function call in some cases. It is safer
 * to prohibit such reordering explicitly via barrier(). */
#define KEDR_ANNOTATE_HAPPENS_BEFORE(id) 			\
	do { 							\
		barrier();					\
		kedr_annotate_happens_before((id));		\
		barrier();					\
	} while (0)
	
#define KEDR_ANNOTATE_HAPPENS_AFTER(id) 			\
	do { 							\
		barrier();					\
		kedr_annotate_happens_after((id));		\
		barrier();					\
	} while (0)

/* The following two annotations allow to specify the "lifetime" of a memory
 * block the target module did not allocate but obtained from some other 
 * component of the kernel. 
 * For example, it can be convenient to annotate 'struct file' passed to 
 * the file operation callbacks as if it was allocated in open() and 
 * deallocated in release(). This allows to avoid confusion in case the 
 * memory occupied by the 'struct file' is reused after the structure has 
 * been destroyed. 
 * 
 * KEDR_ANNOTATE_MEMORY_ACQUIRED(addr, size) marks the memory area 
 * [addr, addr + size) as allocated ("the memory area has been made 
 * available to this module"). 
 * KEDR_ANNOTATE_MEMORY_RELEASED(addr) marks the memory area starting at 
 * 'addr' as deallocated ("the memory area is no longer available to this
 * module"). 
 * 
 * Note that KEDR_ANNOTATE_MEMORY_ACQUIRED(addr, size) should always be
 * paired with KEDR_ANNOTATE_MEMORY_RELEASED(addr) with the same 'addr'. */
#define KEDR_ANNOTATE_MEMORY_ACQUIRED(addr, size) 		\
	do { 							\
		barrier();					\
		kedr_annotate_memory_acquired((addr), (size)); 	\
		barrier();					\
	} while (0)
#define KEDR_ANNOTATE_MEMORY_RELEASED(addr) 			\
	do { 							\
		barrier();					\
		kedr_annotate_memory_released((addr));		\
		barrier();					\
	} while (0)

#else /* KEDR_ANNOTATIONS_ENABLED == 0 */

/* Define the annotations as no-ops. */
#define KEDR_ANNOTATE_HAPPENS_BEFORE(obj) do { } while (0)
#define KEDR_ANNOTATE_HAPPENS_AFTER(obj) do { } while (0)
#define KEDR_ANNOTATE_MEMORY_ACQUIRED(addr, size) do { } while (0)
#define KEDR_ANNOTATE_MEMORY_RELEASED(addr) do { } while (0)

#endif /* KEDR_ANNOTATIONS_ENABLED != 0 */

/* Do not use kedr_annotate_*() functions directly, use KEDR_ANNOTATE_*
 * instead. */
void kedr_annotate_happens_before(unsigned long id);
void kedr_annotate_happens_after(unsigned long id);
void kedr_annotate_memory_acquired(const void *addr, 
	unsigned long size);
void kedr_annotate_memory_released(const void *addr);

#endif /* KEDR_ANNOTATIONS_H_1536_INCLUDED */
