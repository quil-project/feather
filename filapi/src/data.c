/*
 * Original feather Codebase
 * Copyright (c) 2026-present Quil Project Authors
 *
 * Released under the MIT License.
 */

/* Data (globals) and symbol API for the frontend */

#include "../include/data.h"
#include <stdint.h>

/*------------------ SYMBOLS -----------------*/
static Ref symref(ILBuilder *ilb, const char *name, int ext) {
        Con c = {.type = CAddr}; /* sym.type starts SGlo (0), like the parser */
        if (ext) {
                c.sym.type = SExt;
        }
        c.sym.id = intern((char *)name);
        return newcon(&c, ilb->fn);
}
Ref il_extern_sym(ILBuilder *ilb, const char *name) { return symref(ilb, name, 1); }
Ref il_global_sym(ILBuilder *ilb, const char *name) { return symref(ilb, name, 0); }

/*------------------ DATA -----------------*/
static void push(IlData *d, Dat item) {
        if (d->n > 0) {
                /* emitdat reads name/lnk off every item (the parser
                   reuses one Dat, so they carry over there too) */
                item.name = d->items[0].name;
                item.lnk  = d->items[0].lnk;
        }
        vgrow(&d->items, d->n + 1);
        d->items[d->n++] = item;
}
/* data outlives per-function freeall(): untracked emalloc, like the module */
IlData *il_data_begin(const char *name, Lnk *lnk) {
        IlData *d = emalloc(sizeof(IlData));
        d->items  = vnew(0, sizeof(Dat), PHeap);
        d->n      = 0;
        d->name   = strf(PHeap, "%s", name);
        d->lnk    = lnk ? *lnk : (Lnk){0};
        push(d, (Dat){.type = DStart, .name = d->name, .lnk = &d->lnk});
        return d;
}
static void addnum(IlData *d, int type, int64_t v) {
        push(d, (Dat){.type = type, .u.num = v});
}
void il_data_add_b(IlData *d, int64_t v) { addnum(d, DB, v); }
void il_data_add_h(IlData *d, int64_t v) { addnum(d, DH, v); }
void il_data_add_w(IlData *d, int64_t v) { addnum(d, DW, v); }
void il_data_add_l(IlData *d, int64_t v) { addnum(d, DL, v); }
void il_data_add_s(IlData *d, float v) { push(d, (Dat){.type = DW, .u.flts = v}); }
void il_data_add_d(IlData *d, double v) { push(d, (Dat){.type = DL, .u.fltd = v}); }
void il_data_add_str(IlData *d, int type, const char *s) {
        Dat item   = {.type = type, .isstr = 1};
        /* lexer keeps quotes on tokens, and .ascii emits verbatim */
        item.u.str = strf(PHeap, "\"%s\"", s);
        push(d, item);
}
void il_data_add_ref(IlData *d, int type, const char *sym, int64_t off) {
        Dat item        = {.type = type, .isref = 1};
        item.u.ref.name = strf(PHeap, "%s", sym);
        item.u.ref.off  = off;
        push(d, item);
}
void il_data_add_zero(IlData *d, uint64_t n) { push(d, (Dat){.type = DZ, .u.num = (int64_t)n}); }
void il_data_end(IlData *d) { push(d, (Dat){.type = DEnd}); }
