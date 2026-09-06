/*
 * Original feather Codebase
 * Copyright (c) 2026-present Quil Project Authors
 *
 * Released under the MIT License.
 */

/* API for easy IL creation in the frontend */

#include "../include/ilbuilder.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*------------------ LIFECYCLE -----------------*/
ILBuilder *il_create(Fn *fn) {
        ILBuilder *ilb = alloc(sizeof(ILBuilder));
        ilb->fn        = fn;
        ilb->cur       = fn->start;
        return ilb;
}
void il_destroy(ILBuilder *ilb) {
        free(ilb);
}
void il_set_insert_point(ILBuilder *ilb, Blk *blk) {
        ilb->cur = blk;
}
Blk *il_get_insert_block(ILBuilder *ilb) {
        return ilb->cur;
}
Blk *il_create_block(ILBuilder *ilb, const char *name) {
        Blk *b  = newblk();
        b->name = strf(PFn, "%s", name);
        b->id   = ilb->fn->nblk++;

        if (!ilb->fn->start) {
                ilb->fn->start = b;
                if (!ilb->cur) {
                        ilb->cur = b;
                }
        } else {
                Blk *t = ilb->fn->start;
                while (t->link) {
                        t = t->link;
                }
                t->link = b;
        }

        return b;
}
/* function */
Ref il_add_param(ILBuilder *ilb, int cls) {
        assert(ilb->cur);
        assert(ilb->cur == ilb->fn->start);
        /* params must precede all other instructions */
        for (uint i = 0; i < ilb->cur->nins; i++)
                assert(ilb->cur->ins[i].op == Opar || ilb->cur->ins[i].op == Oparc);
        Ref r = newtmp(0, cls, ilb->fn);
        Ins i = {.op = Opar, .cls = cls, .to = r, .arg = {R, R}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &i);
        return r;
}
void il_function_set_vararg(Fn *fn) {
        fn->vararg = 1;
}
void il_function_set_retty(Fn *fn, int idx) {
        assert(idx >= 0 && (uint)idx < ntyp);
        fn->retty = idx;
}
Ref il_add_parc(ILBuilder *ilb, int idx) {
        assert(ilb->cur);
        assert(ilb->cur == ilb->fn->start);
        assert(idx >= 0 && (uint)idx < ntyp);
        /* params must precede all other instructions */
        for (uint i = 0; i < ilb->cur->nins; i++)
                assert(ilb->cur->ins[i].op == Opar || ilb->cur->ins[i].op == Oparc);
        Ref r = newtmp(0, Kl, ilb->fn);
        Ins in = {.op = Oparc, .cls = Kl, .to = r, .arg = {TYPE(idx), R}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &in);
        return r;
}
Fn *il_create_function(const char *name, int retty, Lnk *lnk) {
        /* retty is a typ[] index, not a class; Kx until aggregate types land */
        assert(retty == Kx);
        Fn *fn   = alloc(sizeof(Fn));
        *fn      = (Fn){0};
        fn->tmp  = vnew(0, sizeof(Tmp), PFn);
        fn->con  = vnew(2, sizeof(Con), PFn);
        fn->ncon = 2;
        for (int i = 0; i < Tmp0; i++) {
                newtmp(0, T.fpr0 <= i && i < T.fpr0 + T.nfpr ? Kd : Kl, fn);
        }

        fn->con[0] = (Con){.type = CBits};
        fn->con[1] = (Con){.type = CBits};
        fn->name   = strf(PFn, "%s", name);
        fn->retty  = Kx;
        fn->lnk    = lnk ? *lnk : (Lnk){0};
        fn->start  = NULL;
        fn->nblk   = 0; // blocks added via il_create_block

        fn->mem = vnew(0, sizeof(Mem), PFn);
        fn->rpo = vnew(0, sizeof(Blk *), PFn);

        return fn;
}
Fn *il_finish(ILBuilder *ilb) {
        Fn *fn = ilb->fn;
        // count blocks from fn->start->link chain, built via il_create_block
        int count = 0;
        for (Blk *b = fn->start; b; b = b->link) {
                count++;
        }
        fn->nblk = count;
        fn->rpo  = vnew(count, sizeof(Blk *), PFn);

        // copies link list of block into array
        int i = 0;
        for (Blk *b = fn->start; b; b = b->link) {
                fn->rpo[i++] = b;
        }
        if (!fn->mem) {
                fn->mem = vnew(0, sizeof(Mem), PFn);
        }

        return fn;
}

/*------------------ CONSTANT -----------------*/
Ref il_const_int_w(ILBuilder *ilb, int32_t v) {
        return getcon((int64_t)v, ilb->fn);
}
Ref il_const_int_l(ILBuilder *ilb, int64_t v) {
        return getcon(v, ilb->fn);
}
Ref il_const_float_s(ILBuilder *ilb, float v) {
        Con c = {.type = CBits, .flt = 1, .bits.s = v};
        return newcon(&c, ilb->fn);
}
Ref il_const_float_d(ILBuilder *ilb, double v) {
        Con c = {.type = CBits, .flt = 2, .bits.d = v}; // flt 2=d
        return newcon(&c, ilb->fn);
}
Ref il_const_undef(ILBuilder *ilb) {
        (void)ilb;
        return UNDEF;
}
Ref il_const_zero(ILBuilder *ilb) {
        return getcon(0, ilb->fn);
}

/*------------------ ARITHMETIC HELPERS -----------------*/
// for binrary operations
static Ref mk2(ILBuilder *ilb, int op, int cls, Ref a, Ref b) {
        assert(ilb->cur);
        Ref r   = newtmp(0, cls, ilb->fn);
        Ins ins = {.op = op, .cls = cls, .to = r, .arg = {a, b}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &ins);
        return r;
}
// for unary operations
static Ref mk1(ILBuilder *ilb, int op, int cls, Ref a) {
        assert(ilb->cur);
        Ref r   = newtmp(0, cls, ilb->fn);
        Ins ins = {.op = op, .cls = cls, .to = r, .arg = {a, R}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &ins);
        return r;
}

/*----------------- ARITHMETIC -----------------*/
/* generic add */
Ref il_create_add(ILBuilder *ilb, int cls, Ref a, Ref b) { return mk2(ilb, Oadd, cls, a, b); }
/* add */
Ref il_create_add_w(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Oadd, Kw, a, b); }
Ref il_create_add_l(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Oadd, Kl, a, b); }
Ref il_create_add_s(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Oadd, Ks, a, b); }
Ref il_create_add_d(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Oadd, Kd, a, b); }
/* sub */
Ref il_create_sub_w(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Osub, Kw, a, b); }
Ref il_create_sub_l(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Osub, Kl, a, b); }
Ref il_create_sub_s(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Osub, Ks, a, b); }
Ref il_create_sub_d(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Osub, Kd, a, b); }
/* mul */
Ref il_create_mul_w(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Omul, Kw, a, b); }
Ref il_create_mul_l(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Omul, Kl, a, b); }
Ref il_create_mul_s(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Omul, Ks, a, b); }
Ref il_create_mul_d(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Omul, Kd, a, b); }
/* div (signed) */
Ref il_create_div_w(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Odiv, Kw, a, b); }
Ref il_create_div_l(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Odiv, Kl, a, b); }
Ref il_create_div_s(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Odiv, Ks, a, b); }
Ref il_create_div_d(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Odiv, Kd, a, b); }
/* udiv (unsigned) */
Ref il_create_udiv_w(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Oudiv, Kw, a, b); }
Ref il_create_udiv_l(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Oudiv, Kl, a, b); }
/* remainder (signed) */
Ref il_create_rem_w(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Orem, Kw, a, b); }
Ref il_create_rem_l(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Orem, Kl, a, b); }
/* remainder (unsigned) */
Ref il_create_urem_w(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Ourem, Kw, a, b); }
Ref il_create_urem_l(ILBuilder *ilb, Ref a, Ref b) { return mk2(ilb, Ourem, Kl, a, b); }
/* unary negative */
Ref il_create_neg_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Oneg, Kw, a); }
Ref il_create_neg_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Oneg, Kl, a); }
Ref il_create_neg_s(ILBuilder *ilb, Ref a) { return mk1(ilb, Oneg, Ks, a); }
Ref il_create_neg_d(ILBuilder *ilb, Ref a) { return mk1(ilb, Oneg, Kd, a); }

/*----------------- BITWISE -----------------*/
/* and & */
Ref il_create_and_w(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Oand, Kw, a, b); }
Ref il_create_and_l(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Oand, Kl, a, b); }
/* or | */
Ref il_create_or_w(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Oor, Kw, a, b); }
Ref il_create_or_l(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Oor, Kl, a, b); }
/* xor ^ */
Ref il_create_xor_w(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Oxor, Kw, a, b); }
Ref il_create_xor_l(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Oxor, Kl, a, b); }
/* shift left << */
Ref il_create_shl_w(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Oshl, Kw, a, b); } // b is w shift amount
Ref il_create_shl_l(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Oshl, Kl, a, b); }
/* shift right >> */
Ref il_create_shr_w(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Oshr, Kw, a, b); }
Ref il_create_shr_l(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Oshr, Kl, a, b); }
/* shift right arithmatic */
Ref il_create_sar_w(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Osar, Kw, a, b); }
Ref il_create_sar_l(ILBuilder *bd, Ref a, Ref b) { return mk2(bd, Osar, Kl, a, b); }

/*----------------- MEMORY HELPERS -----------------*/
Ref allocator(ILBuilder *ilb, int op, int cls, Ref n) {
        assert(ilb->cur);
        Ref r = newtmp(0, cls, ilb->fn);
        Ins i = {.op = op, .cls = cls, .to = r, .arg = {n, R}}; // size = n, second arg R = none
        addins(&ilb->cur->ins, &ilb->cur->nins, &i);
        return r;
}
// load reads memory
Ref load(ILBuilder *ilb, int op, int cls, Ref addr) {
        assert(ilb->cur);
        Ref r = newtmp(0, cls, ilb->fn);
        Ins i = {.op = op, .cls = cls, .to = r, .arg = {addr, R}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &i);
        return r;
}
// store writes memory
void store(ILBuilder *ilb, int op, int cls, Ref val, Ref addr) {
        assert(ilb->cur);
        Ins i = {.op = op, .cls = cls, .to = R, .arg = {val, addr}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &i);
}

/*----------------- MEMORY -----------------*/
/* alloc */
Ref il_create_alloc4(ILBuilder *ilb, Ref n) { return allocator(ilb, Oalloc4, Kl, n); }
Ref il_create_alloc8(ILBuilder *ilb, Ref n) { return allocator(ilb, Oalloc8, Kl, n); }
Ref il_create_alloc16(ILBuilder *ilb, Ref n) { return allocator(ilb, Oalloc16, Kl, n); }
/* load */
Ref il_create_load_w(ILBuilder *ilb, Ref addr) { return load(ilb, Oloadsw, Kw, addr); }
Ref il_create_load_l(ILBuilder *ilb, Ref addr) { return load(ilb, Oload, Kl, addr); }
Ref il_create_load_s(ILBuilder *ilb, Ref addr) { return load(ilb, Oload, Ks, addr); }
Ref il_create_load_d(ILBuilder *ilb, Ref addr) { return load(ilb, Oload, Kd, addr); }
Ref il_create_load_sb(ILBuilder *ilb, Ref addr) { return load(ilb, Oloadsb, Kw, addr); }
Ref il_create_load_ub(ILBuilder *ilb, Ref addr) { return load(ilb, Oloadub, Kw, addr); }
Ref il_create_load_sh(ILBuilder *ilb, Ref addr) { return load(ilb, Oloadsh, Kw, addr); }
Ref il_create_load_uh(ILBuilder *ilb, Ref addr) { return load(ilb, Oloaduh, Kw, addr); }
Ref il_create_load_sw(ILBuilder *ilb, Ref addr) { return load(ilb, Oloadsw, Kl, addr); }
Ref il_create_load_uw(ILBuilder *ilb, Ref addr) { return load(ilb, Oloaduw, Kl, addr); }
Ref il_create_load(ILBuilder *ilb, int cls, Ref addr) { return load(ilb, Oload, cls, addr); }
/* store */
void il_create_store_w(ILBuilder *ilb, Ref val, Ref addr) { store(ilb, Ostorew, Kw, val, addr); }
void il_create_store_l(ILBuilder *ilb, Ref val, Ref addr) { store(ilb, Ostorel, Kl, val, addr); }
void il_create_store_s(ILBuilder *ilb, Ref val, Ref addr) { store(ilb, Ostores, Ks, val, addr); }
void il_create_store_d(ILBuilder *ilb, Ref val, Ref addr) { store(ilb, Ostored, Kd, val, addr); }
void il_create_store_b(ILBuilder *ilb, Ref val, Ref addr) { store(ilb, Ostoreb, Kw, val, addr); }
void il_create_store_h(ILBuilder *ilb, Ref val, Ref addr) { store(ilb, Ostoreh, Kw, val, addr); }
void il_create_store(ILBuilder *ilb, int cls, Ref val, Ref addr) {
        int op = Ostorew;
        if (cls == Kl) {
                op = Ostorel;
        } else if (cls == Ks) {
                op = Ostores;
        } else if (cls == Kd) {
                op = Ostored;
        }
        store(ilb, op, cls, val, addr);
}
/* copies n type from src to dst */
void il_create_blit(ILBuilder *ilb, Ref dst, Ref src, int64_t n) {
        Ins i0 = {.op = Oblit0, .cls = Kw, .to = R, .arg = {src, dst}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &i0);
        Ins i1 = {.op = Oblit1, .cls = Kw, .to = R, .arg = {getcon(n, ilb->fn), R}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &i1);
}

/*----------------- CONVERSIONS / CASTS -----------------*/
/* extend */
Ref il_create_extsb_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Oextsb, Kw, a); }
Ref il_create_extsb_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Oextsb, Kl, a); }
Ref il_create_extub_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Oextub, Kw, a); }
Ref il_create_extub_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Oextub, Kl, a); }
Ref il_create_extsh_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Oextsh, Kw, a); }
Ref il_create_extsh_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Oextsh, Kl, a); }
Ref il_create_extuh_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Oextuh, Kw, a); }
Ref il_create_extuh_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Oextuh, Kl, a); }
Ref il_create_extsw_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Oextsw, Kl, a); }
Ref il_create_extuw_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Oextuw, Kl, a); }
Ref il_create_exts_d(ILBuilder *ilb, Ref a) { return mk1(ilb, Oexts, Kd, a); }
Ref il_create_truncd_s(ILBuilder *ilb, Ref a) { return mk1(ilb, Otruncd, Ks, a); }
/* 32/64 float -> 32/64 int*/
Ref il_create_stosi_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Ostosi, Kw, a); }
Ref il_create_stosi_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Ostosi, Kl, a); }
Ref il_create_stoui_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Ostoui, Kw, a); }
Ref il_create_stoui_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Ostoui, Kl, a); }
Ref il_create_dtosi_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Odtosi, Kw, a); }
Ref il_create_dtosi_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Odtosi, Kl, a); }
Ref il_create_dtoui_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Odtoui, Kw, a); }
Ref il_create_dtoui_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Odtoui, Kl, a); }
/* 32/64 int -> 32/64 float*/
Ref il_create_swtof_s(ILBuilder *ilb, Ref a) { return mk1(ilb, Oswtof, Ks, a); }
Ref il_create_swtof_d(ILBuilder *ilb, Ref a) { return mk1(ilb, Oswtof, Kd, a); }
Ref il_create_uwtof_s(ILBuilder *ilb, Ref a) { return mk1(ilb, Ouwtof, Ks, a); }
Ref il_create_uwtof_d(ILBuilder *ilb, Ref a) { return mk1(ilb, Ouwtof, Kd, a); }
Ref il_create_sltof_s(ILBuilder *ilb, Ref a) { return mk1(ilb, Osltof, Ks, a); }
Ref il_create_sltof_d(ILBuilder *ilb, Ref a) { return mk1(ilb, Osltof, Kd, a); }
Ref il_create_ultof_s(ILBuilder *ilb, Ref a) { return mk1(ilb, Oultof, Ks, a); }
Ref il_create_ultof_d(ILBuilder *ilb, Ref a) { return mk1(ilb, Oultof, Kd, a); }
/* bitcast */
Ref il_create_cast_s(ILBuilder *ilb, Ref a) { return mk1(ilb, Ocast, Ks, a); }
Ref il_create_cast_d(ILBuilder *ilb, Ref a) { return mk1(ilb, Ocast, Kd, a); }
Ref il_create_cast_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Ocast, Kw, a); }
Ref il_create_cast_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Ocast, Kl, a); }
/* copy */
Ref il_create_copy_w(ILBuilder *ilb, Ref a) { return mk1(ilb, Ocopy, Kw, a); }
Ref il_create_copy_l(ILBuilder *ilb, Ref a) { return mk1(ilb, Ocopy, Kl, a); }
Ref il_create_copy_s(ILBuilder *ilb, Ref a) { return mk1(ilb, Ocopy, Ks, a); }
Ref il_create_copy_d(ILBuilder *ilb, Ref a) { return mk1(ilb, Ocopy, Kd, a); }

/*----------------- COMPARISON HELPERS -----------------*/
Ref compare(ILBuilder *ilb, int op, int cls, Ref a, Ref b) {
        assert(ilb->cur);
        Ref r = newtmp(0, cls, ilb->fn);
        Ins i = {.op = op, .cls = cls, .to = r, .arg = {a, b}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &i);

        return r;
}

/*----------------- COMPARISON -----------------*/
/* 32 bit intagers */
Ref il_create_icmp_eq_w(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Oceqw, Kw, a, b); }
Ref il_create_icmp_ne_w(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocnew, Kw, a, b); }
Ref il_create_icmp_sge_w(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocsgew, Kw, a, b); }
Ref il_create_icmp_sgt_w(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocsgtw, Kw, a, b); }
Ref il_create_icmp_sle_w(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocslew, Kw, a, b); }
Ref il_create_icmp_slt_w(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocsltw, Kw, a, b); }
Ref il_create_icmp_uge_w(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocugew, Kw, a, b); }
Ref il_create_icmp_ugt_w(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocugtw, Kw, a, b); }
Ref il_create_icmp_ule_w(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Oculew, Kw, a, b); }
Ref il_create_icmp_ult_w(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocultw, Kw, a, b); }
/* 64 bit intager */
Ref il_create_icmp_eq_l(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Oceql, Kw, a, b); }
Ref il_create_icmp_ne_l(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocnel, Kw, a, b); }
Ref il_create_icmp_sge_l(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocsgel, Kw, a, b); }
Ref il_create_icmp_sgt_l(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocsgtl, Kw, a, b); }
Ref il_create_icmp_sle_l(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocslel, Kw, a, b); }
Ref il_create_icmp_slt_l(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocsltl, Kw, a, b); }
Ref il_create_icmp_uge_l(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocugel, Kw, a, b); }
Ref il_create_icmp_ugt_l(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocugtl, Kw, a, b); }
Ref il_create_icmp_ule_l(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Oculel, Kw, a, b); }
Ref il_create_icmp_ult_l(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocultl, Kw, a, b); }
/* Single-precision float comparisons */
Ref il_create_fcmp_eq_s(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Oceqs, Kw, a, b); }
Ref il_create_fcmp_ne_s(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocnes, Kw, a, b); }
Ref il_create_fcmp_ge_s(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocges, Kw, a, b); }
Ref il_create_fcmp_gt_s(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocgts, Kw, a, b); }
Ref il_create_fcmp_le_s(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocles, Kw, a, b); }
Ref il_create_fcmp_lt_s(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Oclts, Kw, a, b); }
Ref il_create_fcmp_o_s(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocos, Kw, a, b); }
Ref il_create_fcmp_uo_s(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocuos, Kw, a, b); }
/* Double-precision float comparisons */
Ref il_create_fcmp_eq_d(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Oceqd, Kw, a, b); }
Ref il_create_fcmp_ne_d(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocned, Kw, a, b); }
Ref il_create_fcmp_ge_d(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocged, Kw, a, b); }
Ref il_create_fcmp_gt_d(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocgtd, Kw, a, b); }
Ref il_create_fcmp_le_d(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocled, Kw, a, b); }
Ref il_create_fcmp_lt_d(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocltd, Kw, a, b); }
Ref il_create_fcmp_o_d(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocod, Kw, a, b); }
Ref il_create_fcmp_uo_d(ILBuilder *ilb, Ref a, Ref b) { return compare(ilb, Ocuod, Kw, a, b); }

/*----------------- CONTROL HELPER -----------------*/
static void set_jmp(ILBuilder *ilb, int jmp_typ, Ref arg, Blk *s1, Blk *s2) {
        Blk *b = ilb->cur;
        if (!b) {
                return;
        }

        b->jmp.type = jmp_typ;
        b->jmp.arg  = arg;
        b->s1       = s1;
        b->s2       = s2;

        // terminate current block
        ilb->cur = NULL;
}

/*----------------- CONTROL -----------------*/
/* branch */
void il_create_br(ILBuilder *ilb, Blk *dst) { set_jmp(ilb, Jjmp, R, dst, NULL); }
void il_create_cond_br(ILBuilder *ilb, Ref cond, Blk *then_blk, Blk *else_blk) { set_jmp(ilb, Jjnz, cond, then_blk, else_blk); }
/* return */
void il_create_ret_w(ILBuilder *ilb, Ref v) { set_jmp(ilb, Jretw, v, NULL, NULL); }
void il_create_ret_l(ILBuilder *ilb, Ref v) { set_jmp(ilb, Jretl, v, NULL, NULL); }
void il_create_ret_s(ILBuilder *ilb, Ref v) { set_jmp(ilb, Jrets, v, NULL, NULL); }
void il_create_ret_d(ILBuilder *ilb, Ref v) { set_jmp(ilb, Jretd, v, NULL, NULL); }
void il_create_ret_void(ILBuilder *ilb) { set_jmp(ilb, Jret0, R, NULL, NULL); }
void il_create_ret_c(ILBuilder *ilb, Ref v) { set_jmp(ilb, Jretc, v, NULL, NULL); }
void il_create_unreachable(ILBuilder *ilb) { set_jmp(ilb, Jhlt, R, NULL, NULL); }

/*----------------- Phi / Call / Variadic HELPERS -----------------*/
Ref createphi(ILBuilder *ilb, int cls, Blk *blks[], Ref vals[], int n) {
        assert(ilb->cur);
        Ref r   = newtmp(0, cls, ilb->fn);
        Phi *p  = alloc(sizeof *p);
        p->to   = r;
        p->cls  = cls;
        p->narg = n;
        p->arg  = vnew(n, sizeof(Ref), PFn);
        memcpy(p->arg, vals, n * sizeof(Ref));
        p->blk = vnew(n, sizeof(Blk *), PFn);
        memcpy(p->blk, blks, n * sizeof(Blk *));

        // Append to the block's phi chain
        p->link       = ilb->cur->phi;
        ilb->cur->phi = p;
        return r;
}
/* class of a value: tmp class for RTmp, float kind for float consts, w otherwise */
static int refclass(ILBuilder *ilb, Ref r) {
        if (rtype(r) == RTmp) {
                return ilb->fn->tmp[r.val].cls;
        }
        if (rtype(r) == RCon) {
                Con *c = &ilb->fn->con[r.val];
                if (c->flt == 1) {
                        return Ks;
                }
                if (c->flt == 2) {
                        return Kd;
                }
        }
        return Kw;
}
void il_call_arg(ILBuilder *ilb, Ref val) {
        assert(ilb->cur);
        int k = refclass(ilb, val);
        Ins a = {.op = Oarg, .cls = k, .to = R, .arg = {val, R}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &a);
}
void il_call_arg_c(ILBuilder *ilb, int idx, Ref addr) {
        assert(ilb->cur);
        assert(idx >= 0 && (uint)idx < ntyp);
        Ins a = {.op = Oargc, .cls = Kl, .to = R, .arg = {TYPE(idx), addr}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &a);
}
Ref il_call_emit(ILBuilder *ilb, int retcls, int retty_idx, Ref fn) {
        assert(ilb->cur);
        assert(retty_idx == Kx || (retty_idx >= 0 && (uint)retty_idx < ntyp));
        /* parser lowers :typ call returns to Kl (parse.c) */
        int cls   = retty_idx >= 0 ? Kl : retcls;
        Ref extra = retty_idx >= 0 ? TYPE(retty_idx) : R;
        Ref r     = newtmp(0, cls, ilb->fn);
        Ins i     = {.op = Ocall, .cls = cls, .to = r, .arg = {fn, extra}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &i);
        return r;
}
Ref createcall(ILBuilder *ilb, int cls, Ref fn, Ref args[], int nargs) {
        assert(ilb->cur);
        // QBE calls pass args via Oarg insns preceding the Ocall
        for (int i = 0; i < nargs; i++)
                il_call_arg(ilb, args[i]);
        return il_call_emit(ilb, cls, Kx, fn);
}
Ref createvaarg(ILBuilder *ilb, int cls, Ref ap) {
        assert(ilb->cur);
        Ref r = newtmp(0, cls, ilb->fn);
        Ins i = {.op = Ovaarg, .cls = cls, .to = r, .arg = {ap}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &i);
        return r;
}

/*----------------- Phi / Call / Variadic -----------------*/
/* phi */
Ref il_create_phi_w(ILBuilder *ilb, Blk *blks[], Ref vals[], int n) { return createphi(ilb, Kw, blks, vals, n); }
Ref il_create_phi_l(ILBuilder *ilb, Blk *blks[], Ref vals[], int n) { return createphi(ilb, Kl, blks, vals, n); }
Ref il_create_phi_s(ILBuilder *ilb, Blk *blks[], Ref vals[], int n) { return createphi(ilb, Ks, blks, vals, n); }
Ref il_create_phi_d(ILBuilder *ilb, Blk *blks[], Ref vals[], int n) { return createphi(ilb, Kd, blks, vals, n); }
/* call */
Ref il_create_call_w(ILBuilder *ilb, Ref fn, Ref args[], int nargs) { return createcall(ilb, Kw, fn, args, nargs); }
Ref il_create_call_l(ILBuilder *ilb, Ref fn, Ref args[], int nargs) { return createcall(ilb, Kl, fn, args, nargs); }
Ref il_create_call_s(ILBuilder *ilb, Ref fn, Ref args[], int nargs) { return createcall(ilb, Ks, fn, args, nargs); }
Ref il_create_call_d(ILBuilder *ilb, Ref fn, Ref args[], int nargs) { return createcall(ilb, Kd, fn, args, nargs); }
/* variadic */
void il_create_vastart(ILBuilder *ilb, Ref ap) {
        assert(ilb->cur);
        Ins i = {.op = Ovastart, .to = R, .arg = {ap}};
        addins(&ilb->cur->ins, &ilb->cur->nins, &i);
}
Ref il_create_vaarg_w(ILBuilder *ilb, Ref ap) { return createvaarg(ilb, Kw, ap); }
Ref il_create_vaarg_l(ILBuilder *ilb, Ref ap) { return createvaarg(ilb, Kl, ap); }
Ref il_create_vaarg_s(ILBuilder *ilb, Ref ap) { return createvaarg(ilb, Ks, ap); }
Ref il_create_vaarg_d(ILBuilder *ilb, Ref ap) { return createvaarg(ilb, Kd, ap); }
