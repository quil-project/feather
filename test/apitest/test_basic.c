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
  Fn *fn = il_create_function("main", Kx, &lnk);
  ILBuilder *bd = il_create(fn);
  Blk *b = il_create_block(bd, "start");
  il_set_insert_point(bd, b);
  Ref c = il_const_int_w(bd, 42);
  b->jmp.type = Jretw;
  b->jmp.arg = c;
  fn = il_finish(bd);
  int ok = (fn->nblk==1 && fn->ncon>=3 && b->jmp.type==Jretw);
  // keep printfn for debug on fail
  if(!ok){ printfn(fn, stderr); }
  printf("test_basic...%*s[%s]\n", (int)(45 - 12), "", ok?"ok":"FAIL");
  return ok?0:1;
}
