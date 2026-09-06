/*
 * Original feather Codebase
 * Copyright (c) 2026-present Quil Project Authors
 *
 * Released under the MIT License.
 */

/* Data (globals) and symbol API for the frontend */

#ifndef FILAPI_DATA_H
#define FILAPI_DATA_H

#include "ilbuilder.h"

/* symbols: address constants usable as call targets or addresses */
Ref il_extern_sym(ILBuilder *ilb, const char *name); /* extern $name (SExt) */
Ref il_global_sym(ILBuilder *ilb, const char *name); /* module-local $name (SGlo) */

/* data builder: accumulates Dat items; emission happens via the module */
typedef struct IlData IlData;
struct IlData {
        Dat *items; /* vnew'd array (PHeap) */
        uint n;     /* item count */
        char *name; /* data symbol name */
        Lnk lnk;    /* linkage owned by the builder */
};

IlData *il_data_begin(const char *name, Lnk *lnk); /* DStart */
void il_data_add_b(IlData *d, int64_t v);
void il_data_add_h(IlData *d, int64_t v);
void il_data_add_w(IlData *d, int64_t v);
void il_data_add_l(IlData *d, int64_t v);
void il_data_add_s(IlData *d, float v);
void il_data_add_d(IlData *d, double v);
void il_data_add_str(IlData *d, int type, const char *s);                /* string item (DB/DH/DW/DL) */
void il_data_add_ref(IlData *d, int type, const char *sym, int64_t off); /* $sym+off item */
void il_data_add_zero(IlData *d, uint64_t n);                            /* DZ: n zero bytes */
void il_data_end(IlData *d);                                             /* DEnd */

#endif // !FILAPI_DATA_H
