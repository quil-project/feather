#include "../../filapi/include/ilbuilder.h"
#include "../../config.h"
#include <stdio.h>

Target T;
char debug['Z' + 1];
int optlevel = 0;
extern Target T_amd64_sysv;

static int check_par(Blk *b, int idx, int cls, const char *name){
  if(idx >= (int)b->nins){ fprintf(stderr,"missing %s at idx %d nins %d\n",name,idx,b->nins); return 0; }
  if(b->ins[idx].op != Opar){ fprintf(stderr,"op mismatch %s: got %d want Opar\n",name,b->ins[idx].op); return 0; }
  if(b->ins[idx].cls != cls){ fprintf(stderr,"cls mismatch %s: got %d want %d\n",name,b->ins[idx].cls,cls); return 0; }
  if(rtype(b->ins[idx].to) != RTmp){ fprintf(stderr,"param %s result is not a tmp\n",name); return 0; }
  return 1;
}

int main(void){
  T = T_amd64_sysv;
  Lnk lnk = {.export=1};
  Fn *fn = il_create_function("test_params", Kx, &lnk);
  ILBuilder *bd = il_create(fn);
  Blk *b = il_create_block(bd, "start");
  il_set_insert_point(bd, b);

  // params must come first: one per class
  Ref pw = il_add_param(bd, Kw); if(!check_par(b,0,Kw,"par_w")) return 1;
  Ref pl = il_add_param(bd, Kl); if(!check_par(b,1,Kl,"par_l")) return 1;
  Ref ps = il_add_param(bd, Ks); if(!check_par(b,2,Ks,"par_s")) return 1;
  Ref pd = il_add_param(bd, Kd); if(!check_par(b,3,Kd,"par_d")) return 1;

  il_function_set_vararg(fn);

  // params are usable values: w + w -> ret
  Ref sum = il_create_add_w(bd, pw, pw);
  (void)pl; (void)ps; (void)pd;
  il_create_ret_w(bd, sum);

  fn = il_finish(bd);

  int ok = (fn->nblk == 1 && fn->vararg == 1 &&
            b->nins == 5 && b->jmp.type == Jretw);

  if(!ok){
    printfn(fn, stderr);
  }

  printf("test_params...%*s[%s]\n", (int)(45 - 11), "", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
