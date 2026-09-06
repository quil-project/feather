/*
 * Original feather Codebase
 * Copyright (c) 2026-present Quil Project Authors
 *
 * Released under the MIT License.
 */

/* Aggregate type API for the frontend */

#include "../include/type.h"
#include <assert.h>

struct IlType {
        Typ ty;      /* name/align/size filled at end() */
        uint n;      /* fields used */
        uint64_t sz; /* running size */
        int al;      /* running max log-alignment */
};

IlType *il_type_begin(const char *name) {
        IlType *t     = emalloc(sizeof *t);
        t->ty.name    = strf(PHeap, "%s", name);
        t->ty.isdark  = 0;
        t->ty.isunion = 0;
        t->ty.align   = -1;
        t->ty.size    = 0;
        t->ty.nunion  = 0;
        t->ty.fields  = vnew(1, sizeof(*t->ty.fields), PHeap);
        t->n          = 0;
        t->sz         = 0;
        t->al         = -1;
        return t;
}

/* one member run: s = elem size, a = log align, flen = field len (or subtyp idx), c = count */
static void addm(IlType *t, int ftype, uint64_t s, int a, uint64_t flen, uint64_t c) {
        struct Field *fld = *t->ty.fields;
        uint n            = t->n;
        uint64_t am, pad;

        if (a > t->al) {
                t->al = a;
        }
        am  = (1u << a) - 1;
        pad = ((t->sz + am) & ~am) - t->sz;
        if (pad) {
                if (n < NField) {
                        fld[n].type = FPad;
                        fld[n].len  = (uint)pad;
                        n++;
                }
        }
        t->sz += pad + c * s;
        for (; c > 0 && n < NField; c--, n++) {
                fld[n].type = ftype;
                fld[n].len  = flen;
        }
        t->n = n;
}
void il_type_add_b(IlType *t, uint64_t n) { addm(t, Fb, 1, 0, 1, n); }
void il_type_add_h(IlType *t, uint64_t n) { addm(t, Fh, 2, 1, 2, n); }
void il_type_add_w(IlType *t, uint64_t n) { addm(t, Fw, 4, 2, 4, n); }
void il_type_add_l(IlType *t, uint64_t n) { addm(t, Fl, 8, 3, 8, n); }
void il_type_add_s(IlType *t, uint64_t n) { addm(t, Fs, 4, 2, 4, n); }
void il_type_add_d(IlType *t, uint64_t n) { addm(t, Fd, 8, 3, 8, n); }
void il_type_add_subtype(IlType *t, int idx, uint64_t n) {
        assert(idx >= 0 && (uint)idx < ntyp);
        addm(t, FTyp, typ[idx].size, typ[idx].align, (uint64_t)idx, n);
}
int il_type_end(IlType *t) {
        struct Field *fld = *t->ty.fields;
        int a, idx;

        assert(t->n > 0); /* empty structs are meaningless (parser UBs here too) */
        fld[t->n].type = FEnd;
        a              = 1 << t->al;
        t->ty.size     = (t->sz + a - 1) & -a;
        t->ty.align    = t->al;
        t->ty.nunion   = 1;
        if (!typ) {
                typ = vnew(0, sizeof(Typ), PHeap);
        }
        vgrow(&typ, ntyp + 1);
        typ[ntyp] = t->ty;
        idx       = ntyp++;
        free(t);
        return idx;
}
