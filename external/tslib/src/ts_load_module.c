/*
 *  tslib/src/ts_load_module.c
 *
 *  Copyright (C) 2001 Russell King.
 *
 * This file is placed under the LGPL.  Please see the file
 * COPYING for more details.
 *
 * $Id: ts_load_module.c,v 1.4 2004/10/19 22:01:27 dlowder Exp $
 *
 * Close a touchscreen device.
 */
#include <utils/Log.h>
#include "config.h"
#include<stdio.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "tslib-private.h"

int __ts_load_module(struct tsdev *ts, const char *module, const char *params, int raw)
{
    struct tslib_module_info * (*init)(struct tsdev *, const char *);
    struct tslib_module_info *info;
    char fn[1024];
    void *handle;
    int ret;
    char *plugin_directory=NULL;
    if(raw == 1)
        LOGV("tslib: entering ts_load_module_raw \n");
    else
        LOGV("tslib: entering ts_load_module \n");
    if( (plugin_directory = getenv("TSLIB_PLUGINDIR")) != NULL ) {
        strcpy(fn,plugin_directory);
    } else {
        strcpy(fn, PLUGIN_DIR);
    }

    strcat(fn, "/");
    strcat(fn, module);
    strcat(fn, ".so");

    LOGV("Loading module %s\n", fn);
    handle = dlopen(fn, RTLD_NOW);
    if (!handle)
        return -1;

    init = dlsym(handle, "mod_init");
    if (!init) {
        dlclose(handle);
        LOGE("tslib:mod_init failed \n");
        return -1;
    }

    info = init(ts, params);
    if (!info) {
        dlclose(handle);
        LOGE("tslib: mod_init failed \n");
        return -1;
    }

    info->handle = handle;

    if (raw) {
        ret = __ts_attach_raw(ts, info);
    } else {
        ret = __ts_attach(ts, info);
    }
    if (ret) {
        info->ops->fini(info);
        dlclose(handle);
    }

    return ret;
}


int ts_load_module(struct tsdev *ts, const char *module, const char *params)
{
    return __ts_load_module(ts, module, params, 0);
}

int ts_load_module_raw(struct tsdev *ts, const char *module, const char *params)
{
    return __ts_load_module(ts, module, params, 1);
}
