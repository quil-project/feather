#include "../../filapi/include/ilbuilder.h"
#include "../../config.h"
#include <stdio.h>

Target T;
char debug['Z' + 1];
int optlevel = 0;
extern Target T_amd64_sysv;

int main(void){
  T = T_amd64_sysv;
  Lnk lnk = {.export=1};
  Fn *fn = il_create_function("test_phi_call_va", Kx, &lnk);
  ILBuilder *bd = il_create(fn);

  Blk *entry  = il_create_block(bd, "entry");
  Blk *then_b = il_create_block(bd, "then");
  Blk *else_b = il_create_block(bd, "else");
  Blk *join   = il_create_block(bd, "join");

  // Entry block
  il_set_insert_point(bd, entry);
  il_create_cond_br(bd, il_const_int_w(bd, 1), then_b, else_b);

  // Then block: one value per class
  il_set_insert_point(bd, then_b);
  Ref w1 = il_const_int_w(bd, 10);
  Ref l1 = il_const_int_l(bd, 10);
  Ref s1 = il_const_float_s(bd, 1.5f);
  Ref d1 = il_const_float_d(bd, 1.5);
  il_create_br(bd, join);

  // Else block: one value per class
  il_set_insert_point(bd, else_b);
  Ref w2 = il_const_int_w(bd, 20);
  Ref l2 = il_const_int_l(bd, 20);
  Ref s2 = il_const_float_s(bd, 2.5f);
  Ref d2 = il_const_float_d(bd, 2.5);
  il_create_br(bd, join);

  // Join block: phi/call/vaarg in every class
  il_set_insert_point(bd, join);
  Blk *preds[] = {then_b, else_b};
  Ref wvals[] = {w1, w2};
  Ref lvals[] = {l1, l2};
  Ref svals[] = {s1, s2};
  Ref dvals[] = {d1, d2};
  Ref pw = il_create_phi_w(bd, preds, wvals, 2);
  Ref pl = il_create_phi_l(bd, preds, lvals, 2);
  Ref ps = il_create_phi_s(bd, preds, svals, 2);
  Ref pd = il_create_phi_d(bd, preds, dvals, 2);

  Ref callee = il_const_int_l(bd, 0); // placeholder address / symbol
  Ref wargs[] = {pw};
  Ref largs[] = {pl};
  Ref sargs[] = {ps};
  Ref dargs[] = {pd};
  Ref cw = il_create_call_w(bd, callee, wargs, 1);
  Ref cl = il_create_call_l(bd, callee, largs, 1);
  Ref cs = il_create_call_s(bd, callee, sargs, 1);
  Ref cd = il_create_call_d(bd, callee, dargs, 1);
  (void)cl; (void)cs; (void)cd;

  Ref ap = il_create_alloc4(bd, il_const_int_w(bd, 8));
  il_create_vastart(bd, ap);
  Ref vw = il_create_vaarg_w(bd, ap);
  Ref vl = il_create_vaarg_l(bd, ap);
  Ref vs = il_create_vaarg_s(bd, ap);
  Ref vd = il_create_vaarg_d(bd, ap);
  (void)vw; (void)vl; (void)vs; (void)vd;

  il_create_ret_w(bd, cw);

  fn = il_finish(bd);

  // join emits: (Oarg+Ocall) x4 + alloc + vastart + vaarg x4 = 14 ins
  // (phis live on the phi chain, not in ins)
  int test_ok = (fn->nblk == 4 &&
                 entry->jmp.type == Jjnz &&
                 then_b->jmp.type == Jjmp && then_b->s1 == join &&
                 else_b->jmp.type == Jjmp && else_b->s1 == join &&
                 join->nins == 14 &&
                 join->ins[0].op == Oarg && join->ins[1].op == Ocall &&
                 join->ins[2].op == Oarg && join->ins[3].op == Ocall &&
                 join->ins[4].op == Oarg && join->ins[5].op == Ocall &&
                 join->ins[6].op == Oarg && join->ins[7].op == Ocall &&
                 join->ins[8].op == Oalloc4 && join->ins[9].op == Ovastart &&
                 join->jmp.type == Jretw);

  if(!test_ok){
    printfn(fn, stderr);
  }

  printf("test_phi_call_va...                          [%s]\n", test_ok ? "ok" : "FAIL");
  return test_ok ? 0 : 1;
}
