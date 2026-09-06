/*
 * Original feather Codebase
 * Copyright (c) 2026-present Quil Project Authors
 *
 * Released under the MIT License.
 */

/* module: owns functions + data, drives the backend to asm.
   Host must set T (target) and optlevel before il_module_emit. */

#include "../include/module.h"

/*------------------ REGISTRY -----------------*/
/* module must survive freeall() (called per function at emit),
   so it uses untracked emalloc, not pool-tracked alloc() */
IlModule *il_module_create(void) {
        IlModule *m = emalloc(sizeof(IlModule));
        m->fns      = vnew(0, sizeof(Fn *), PHeap);
        m->nfn      = 0;
        m->datas    = vnew(0, sizeof(IlData *), PHeap);
        m->ndat     = 0;
        return m;
}
void il_module_add_function(IlModule *m, Fn *fn) {
        vgrow(&m->fns, m->nfn + 1);
        m->fns[m->nfn++] = fn;
}
void il_module_add_data(IlModule *m, IlData *d) {
        vgrow(&m->datas, m->ndat + 1);
        m->datas[m->ndat++] = d;
}

/*------------------ EMIT -----------------*/
/* same pass list as main.c func(): passes assume text-parser-shaped IR,
   so any divergence here is a bug — keep the two in sync */
static void compilefn(Fn *fn, FILE *out) {
        uint n;

        if (debug['P']) {
                fprintf(stderr, "\n> After parsing:\n");
                printfn(fn, stderr);
        }
        T.abi0(fn);
        fillcfg(fn);
        filluse(fn);
        promote(fn);
        filluse(fn);
        ssa(fn);
        filluse(fn);
        ssacheck(fn);
        fillalias(fn);
        loadopt(fn);
        filluse(fn);
        fillalias(fn);
        coalesce(fn);
        filluse(fn);
        filldom(fn);
        ssacheck(fn);
        // simplcfg must run even at -O0 to fold constant jnz (e.g. jnz 1) before isel seljmp RTmp assert
        fillcfg(fn);
        simplcfg(fn);
        filluse(fn);
        filldom(fn);
        if (OPTIMIZE) {
                gvn(fn);
                fillcfg(fn);
                simplcfg(fn);
                filluse(fn);
                filldom(fn);
                gcm(fn);
                filluse(fn);
                ssacheck(fn);
                if (T.cansel) {
                        ifconvert(fn);
                        fillcfg(fn);
                        filluse(fn);
                        filldom(fn);
                        ssacheck(fn);
                }
        }
        T.abi1(fn);
        simpl(fn);
        fillcfg(fn);
        filluse(fn);
        T.isel(fn);
        fillcfg(fn);
        filllive(fn);
        fillloop(fn);
        fillcost(fn);
        spill(fn);
        rega(fn);
        fillcfg(fn);
        simpljmp(fn);
        fillcfg(fn);
        assert(fn->rpo[0] == fn->start);
        for (n = 0;; n++) {
                if (n == fn->nblk - 1) {
                        fn->rpo[n]->link = 0;
                        break;
                } else {
                        fn->rpo[n]->link = fn->rpo[n + 1];
                }
        }
        T.emitfn(fn, out);
        fprintf(out, "/* end function %s */\n\n", fn->name);
        /* no freeall() here: unlike main.c (one fn at a time), the module
           holds all fns upfront, so freeing would kill not-yet-emitted fns */
}
void il_module_emit(IlModule *m, FILE *out) {
        for (uint i = 0; i < m->ndat; i++) {
                IlData *d = m->datas[i];
                for (uint j = 0; j < d->n; j++) {
                        emitdat(&d->items[j], out);
                }
                fputs("/* end data */\n\n", out);
        }
        for (uint i = 0; i < m->nfn; i++) {
                compilefn(m->fns[i], out);
        }
        freeall(); /* all fns dead after this point, like main.c per function */
}
