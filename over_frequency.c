/*
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "../../sr_module.h"
#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../lock_ops.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../pvar.h"
#include "../../error.h"
#include "../../timer.h"
#include "../../resolve.h"
#include "../../data_lump.h"
#include "../../mod_fix.h"
#include "../../script_cb.h"
#include "../../sl_cb.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_expires.h"
#include "../../parser/contact/parse_contact.h"
#include "../../cachedb/cachedb.h"
#include "../../cachedb/cachedb_cap.h"
#include "../dialog/dlg_load.h"
#include "../tm/tm_load.h"


static int  mod_init(void);
static void mod_destroy(void);

static unsigned int max_frequency;
static unsigned int of_interval;
static int CheckFrequency(struct sip_msg *msg, unsigned int ); 

static cmd_export_t commands[] = {
    {"check_frequency",   (cmd_function)CheckFrequency, 1,
        fixup_uint_null, 0, REQUEST_ROUTE},
    {0, 0, 0, 0, 0, 0}
};

static param_export_t parameters[] = {
    {"max_frequency",       INT_PARAM, &max_frequency},
    {"time_interval",       INT_PARAM, &of_interval},
    {0, 0, 0}
};

static dep_export_t deps = {
	{ /* OpenSIPS module dependencies */
		{ MOD_TYPE_CACHEDB, "cachedb_local",     DEP_ABORT},
		{ MOD_TYPE_NULL, NULL, 0 },
	},
	{ /* modparam dependencies */
		{ NULL, NULL },
	},
};

struct module_exports exports = {
    "over_frequency", // module name
    MOD_TYPE_DEFAULT,// class of this module
    MODULE_VERSION,  // module version
    DEFAULT_DLFLAGS, // dlopen flags
    &deps,           // OpenSIPS module dependencies
    commands,        // exported functions
    0,               // exported async functions
    parameters,      // exported parameters
    NULL,            
    NULL,            // exported MI functions
    NULL,            // exported pseudo-variables
    NULL,            // extra processes
    mod_init,        // module init function
                     // (before fork. kids will inherit)
    NULL,            // reply processing function
    mod_destroy,     // destroy function
    NULL             // child init function
};

/* local cache */
cachedb_funcs       overf_cdbf;
cachedb_con        *overf_cache;

static int
mod_init(void)
{
    /* 初始化本地缓存，缓存是全局共享的 */
    str local_url = {"local://", 8};
	if (cachedb_bind_mod(&local_url, &overf_cdbf) < 0) {
		LM_ERR("cannot bind functions for cachedb_url %.*s\n",
				local_url.len, local_url.s);
		return -1;
	}
	overf_cache = overf_cdbf.init(&local_url);
	if (!overf_cache) {
		LM_ERR("cannot connect to cachedb_url %.*s\n",
                local_url.len, local_url.s);
		return -1;
	}
    return 0;
}

static void
mod_destroy(void)
{
    if (overf_cache != NULL)
        overf_cdbf.destroy(overf_cache);
}

enum {
    DIRE_CALLER   = 1U << 0,
    DIRE_CALLEE   = 1U << 1
};

static int
CheckFrequency(struct sip_msg *msg, unsigned int direction)
{
    struct sip_uri *uri = NULL;
    int val;

    //如果是在direction使用to头域
    if (direction & DIRE_CALLER ){
        uri = parse_from_uri(msg);
    }else if (direction & DIRE_CALLEE ){
        uri = parse_to_uri(msg);
    }

    if (!uri || !uri->user.s || !uri->user.len){
        LM_ERR("get call number failed,"
                "direction:%d\n", direction);
        return -1;
    }

    if (overf_cdbf.add(overf_cache, &uri->user, 1,
                of_interval, &val) < 0){
        LM_ERR("local cache add failed!!!\n");
        return -1;
    }

    if (val > max_frequency)
        return -1;

    return 1;
}

