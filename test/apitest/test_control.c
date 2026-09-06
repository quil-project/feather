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
  Fn *fn = il_create_function("test_ctrl", Kx, &lnk);
  ILBuilder *bd = il_create(fn);

  Blk *entry  = il_create_block(bd, "entry");
  Blk *b_br   = il_create_block(bd, "br");
  Blk *b_retw = il_create_block(bd, "retw");
  Blk *b_retl = il_create_block(bd, "retl");
  Blk *b_rets = il_create_block(bd, "rets");
  Blk *b_retd = il_create_block(bd, "retd");
  Blk *b_retv = il_create_block(bd, "retv");
  Blk *b_hlt  = il_create_block(bd, "hlt");

  il_set_insert_point(bd, entry);
  il_create_cond_br(bd, il_const_int_w(bd, 1), b_br, b_retl);

  il_set_insert_point(bd, b_br);
  il_create_br(bd, b_retw);

  il_set_insert_point(bd, b_retw);
  il_create_ret_w(bd, il_const_int_w(bd, 10));

  il_set_insert_point(bd, b_retl);
  il_create_ret_l(bd, il_const_int_l(bd, 20));

  il_set_insert_point(bd, b_rets);
  il_create_ret_s(bd, il_const_float_s(bd, 1.5f));

  il_set_insert_point(bd, b_retd);
  il_create_ret_d(bd, il_const_float_d(bd, 2.5));

  il_set_insert_point(bd, b_retv);
  il_create_ret_void(bd);

  il_set_insert_point(bd, b_hlt);
  il_create_unreachable(bd);

  fn = il_finish(bd);

  int ok = (fn->nblk == 8 &&
            entry->jmp.type == Jjnz && entry->s1 == b_br && entry->s2 == b_retl &&
            b_br->jmp.type == Jjmp && b_br->s1 == b_retw &&
            b_retw->jmp.type == Jretw &&
            b_retl->jmp.type == Jretl &&
            b_rets->jmp.type == Jrets &&
            b_retd->jmp.type == Jretd &&
            b_retv->jmp.type == Jret0 &&
            b_hlt->jmp.type == Jhlt);

  if(!ok){
    printfn(fn, stderr);
  }

  printf("test_control...%*s[%s]\n", (int)(45 - 15), "", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
