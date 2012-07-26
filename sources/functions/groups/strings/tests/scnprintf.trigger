[group]
# Name of the target function
function.name = scnprintf
	
# The code to trigger a call to this function.
trigger.code =>>
	char *bytes = NULL;
	char *buf = NULL;
	const char *str = "-16179-";
	
	bytes = kmalloc(20, GFP_KERNEL);
	buf = kmalloc(20, GFP_KERNEL);
	if (bytes != NULL) {
		strcpy(&bytes[0], str);
		scnprintf(buf, 20, "@%s@", bytes);
	}
	kfree(bytes);
	kfree(buf);
<<
#######################################################################
