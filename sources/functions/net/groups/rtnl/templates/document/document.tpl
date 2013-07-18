/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Author: 
 *      Eugene A. Shatokhin
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>
#include <linux/rtnetlink.h>
#include <net/rtnetlink.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>
#include <kedr/fh_drd/common.h>

#include <util/fh_plugin.h>
#include <net/drd_net_common.h>

#include "config.h"
/* ====================================================================== */

/* TODO: describe HB relations
 */
/* ====================================================================== */

/* [NB] Provided by rtnl_link_ops subgroup. */
void
kedr_set_rtnl_link_ops_handlers(const struct rtnl_link_ops *ops);

/* ====================================================================== */
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */<$endif$>

static struct kedr_fh_handlers *handlers[] = {
	<$if concat(handlerItem)$><$handlerItem: join(,\n\t)$>,
	<$endif$>NULL
};
/* ====================================================================== */

static struct kedr_fh_group fh_group = {
	.handlers = NULL,
};

struct kedr_fh_group * __init
kedr_fh_get_group_rtnl(void)
{
	fh_group.handlers = &handlers[0];
	fh_group.num_handlers = (unsigned int)ARRAY_SIZE(handlers) - 1;
	return &fh_group;
}
/* ====================================================================== */
