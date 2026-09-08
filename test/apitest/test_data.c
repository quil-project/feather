#include "../../config.h"
#include "../../filapi/include/data.h"
#include <stdio.h>
#include <string.h>

Target T;
char debug['Z' + 1];
int optlevel = 0;
extern Target T_amd64_sysv;

#define CHECK(c, msg)                                       \
        do {                                                \
                if (!(c)) {                                 \
                        fprintf(stderr, "fail: %s\n", msg); \
                        return 1;                           \
                }                                           \
        } while (0)

int main(void) {
        T = T_amd64_sysv;

        // ---- data items: data $mystr = { b 65, h 0x1234, w .., l ..,
        //      s 1.5, d 2.5, b "hi", w $extfn+8, z 16 } ----
        Lnk lnk   = {.export = 1};
        IlData *d = il_data_begin("mystr", &lnk);
        il_data_add_b(d, 65);
        il_data_add_h(d, 0x1234);
        il_data_add_w(d, 0x12345678);
        il_data_add_l(d, 0x123456789abcdef0ll);
        il_data_add_s(d, 1.5f);
        il_data_add_d(d, 2.5);
        il_data_add_str(d, DB, "hi");
        il_data_add_ref(d, DW, "extfn", 8);
        il_data_add_zero(d, 16);
        il_data_end(d);

        CHECK(d->n == 11, "item count");
        CHECK(d->items[0].type == DStart, "DStart");
        CHECK(strcmp(d->items[0].name, "mystr") == 0, "DStart name");
        CHECK(d->items[0].lnk == &d->lnk && d->lnk.export == 1, "DStart lnk owned copy");
        CHECK(d->items[1].type == DB && d->items[1].u.num == 65, "DB");
        CHECK(d->items[2].type == DH && d->items[2].u.num == 0x1234, "DH");
        CHECK(d->items[3].type == DW && d->items[3].u.num == 0x12345678, "DW");
        CHECK(d->items[4].type == DL && d->items[4].u.num == 0x123456789abcdef0ll, "DL");
        CHECK(d->items[5].type == DW && d->items[5].u.flts == 1.5f, "float s");
        CHECK(d->items[6].type == DL && d->items[6].u.fltd == 2.5, "float d");
        CHECK(d->items[7].type == DB && d->items[7].isstr &&
                  strcmp(d->items[7].u.str, "\"hi\"") == 0,
              "string");
        CHECK(d->items[8].type == DW && d->items[8].isref &&
                  strcmp(d->items[8].u.ref.name, "extfn") == 0 &&
                  d->items[8].u.ref.off == 8,
              "ref");
        CHECK(d->items[9].type == DZ && d->items[9].u.num == 16, "zero fill");
        CHECK(d->items[10].type == DEnd, "DEnd");

        // ---- symbols ----
        Lnk flnk      = {.export = 1};
        Fn *fn        = il_create_function("test_data", Kx, &flnk);
        ILBuilder *bd = il_create(fn);
        Blk *b        = il_create_block(bd, "start");
        il_set_insert_point(bd, b);

        Ref e1 = il_extern_sym(bd, "puts");
        Ref e2 = il_extern_sym(bd, "puts");
        CHECK(rtype(e1) == RCon, "extern is const");
        CHECK(req(e1, e2), "same symbol interns once");
        CHECK(fn->con[e1.val].type == CAddr, "extern CAddr");
        CHECK(fn->con[e1.val].sym.type == SExt, "extern SExt");
        Ref g = il_global_sym(bd, "mystr");
        CHECK(fn->con[g.val].type == CAddr, "global CAddr");
        CHECK(fn->con[g.val].sym.type == SGlo, "global SGlo");

        // ---- end to end: call the extern symbol, return its result ----
        Ref r = il_create_call_w(bd, e1, NULL, 0);
        il_create_ret_w(bd, r);
        fn = il_finish(bd);

        CHECK(fn->nblk == 1 && b->nins == 1, "call block shape");
        CHECK(b->ins[0].op == Ocall && b->jmp.type == Jretw, "call+ret");

        printf("test_data...%*s[%s]\n", (int)(45 - 9), "", "ok");
        return 0;
}
