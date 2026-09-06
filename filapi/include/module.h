/*
 * Original feather Codebase
 * Copyright (c) 2026-present Quil Project Authors
 *
 * Released under the MIT License.
 */

#ifndef FILAPI_MODULE_H
#define FILAPI_MODULE_H

#include "data.h"
#include <stdio.h>

/* module: owns functions + data, drives the backend to asm.
   Host must set T (target) and optlevel before il_module_emit. */
typedef struct IlModule IlModule;
struct IlModule {
        Fn **fns; /* vnew'd array (PHeap) */
        uint nfn;
        IlData **datas; /* vnew'd array (PHeap) */
        uint ndat;
};

IlModule *il_module_create(void);
void il_module_add_function(IlModule *m, Fn *fn);
void il_module_add_data(IlModule *m, IlData *d);
void il_module_emit(IlModule *m, FILE *out); /* full pipeline + asm; consumes PFn pools like main.c */

#endif // !FILAPI_MODULE_H
