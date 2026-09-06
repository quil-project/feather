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
  Fn *fn = il_create_function("test_conv_all", Kx, &lnk);
  ILBuilder *bd = il_create(fn);
  Blk *b = il_create_block(bd, "start");
  il_set_insert_point(bd, b);

  Ref w_val = il_const_int_w(bd, 42);
  Ref l_val = il_const_int_l(bd, 42);
  Ref s_val = il_const_float_s(bd, 3.14f);
  Ref d_val = il_const_float_d(bd, 3.14159);

  // Extensions & Truncations
  (void)il_create_extsb_w(bd, w_val);
  (void)il_create_extsb_l(bd, w_val);
  (void)il_create_extub_w(bd, w_val);
  (void)il_create_extub_l(bd, w_val);
  (void)il_create_extsh_w(bd, w_val);
  (void)il_create_extsh_l(bd, w_val);
  (void)il_create_extuh_w(bd, w_val);
  (void)il_create_extuh_l(bd, w_val);
  (void)il_create_extsw_l(bd, w_val);
  (void)il_create_extuw_l(bd, w_val);
  (void)il_create_exts_d(bd, s_val);
  (void)il_create_truncd_s(bd, d_val);

  // Float to Int Conversions
  (void)il_create_stosi_w(bd, s_val);
  (void)il_create_stosi_l(bd, s_val);
  (void)il_create_stoui_w(bd, s_val);
  (void)il_create_stoui_l(bd, s_val);
  (void)il_create_dtosi_w(bd, d_val);
  (void)il_create_dtosi_l(bd, d_val);
  (void)il_create_dtoui_w(bd, d_val);
  (void)il_create_dtoui_l(bd, d_val);

  // Int to Float Conversions
  (void)il_create_swtof_s(bd, w_val);
  (void)il_create_swtof_d(bd, w_val);
  (void)il_create_uwtof_s(bd, w_val);
  (void)il_create_uwtof_d(bd, w_val);
  (void)il_create_sltof_s(bd, l_val);
  (void)il_create_sltof_d(bd, l_val);
  (void)il_create_ultof_s(bd, l_val);
  (void)il_create_ultof_d(bd, l_val);

  // Bitcasts
  (void)il_create_cast_s(bd, w_val);
  (void)il_create_cast_d(bd, l_val);
  (void)il_create_cast_w(bd, s_val);
  (void)il_create_cast_l(bd, d_val);

  // Copies
  (void)il_create_copy_w(bd, w_val);
  (void)il_create_copy_l(bd, l_val);
  (void)il_create_copy_s(bd, s_val);
  Ref final_cp = il_create_copy_d(bd, d_val);

  b->jmp.type = Jretd;
  b->jmp.arg = final_cp;

  fn = il_finish(bd);

  int ok = (fn->nblk == 1 && b->nins >= 36 && b->jmp.type == Jretd);

  if(!ok){
    printfn(fn, stderr);
  }

  printf("test_conv...%*s[%s]\n", (int)(45 - 9), "", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
