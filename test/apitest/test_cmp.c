#include "../../filapi/include/ilbuilder.h"
#include "../../config.h"
#include <stdio.h>

Target T;
char debug['Z' + 1];
int optlevel = 0;
extern Target T_amd64_sysv;

static int check_op(Blk *b, int idx, int op, const char *name){
  if(idx >= (int)b->nins){ fprintf(stderr,"missing %s at idx %d nins %d\n",name,idx,b->nins); return 0; }
  if(b->ins[idx].op != op){ fprintf(stderr,"op mismatch %s: got %d want %d\n",name,b->ins[idx].op,op); return 0; }
  return 1;
}

int main(void){
  T = T_amd64_sysv;
  Lnk lnk = {.export=1};
  Fn *fn = il_create_function("test_cmp", Kx, &lnk);
  ILBuilder *bd = il_create(fn);
  Blk *b = il_create_block(bd, "start");
  il_set_insert_point(bd, b);

  Ref w1 = il_const_int_w(bd, 5);
  Ref w2 = il_const_int_w(bd, 10);
  Ref l1 = il_const_int_l(bd, 5);
  Ref l2 = il_const_int_l(bd, 10);
  Ref s1 = il_const_float_s(bd, 1.5f);
  Ref s2 = il_const_float_s(bd, 2.5f);
  Ref d1 = il_const_float_d(bd, 1.5);
  Ref d2 = il_const_float_d(bd, 2.5);

  int n = 0;
  // w integer comparisons (10)
  il_create_icmp_eq_w(bd,w1,w2);  if(!check_op(b,n++,Oceqw,"icmp_eq_w")) return 1;
  il_create_icmp_ne_w(bd,w1,w2);  if(!check_op(b,n++,Ocnew,"icmp_ne_w")) return 1;
  il_create_icmp_sge_w(bd,w1,w2); if(!check_op(b,n++,Ocsgew,"icmp_sge_w")) return 1;
  il_create_icmp_sgt_w(bd,w1,w2); if(!check_op(b,n++,Ocsgtw,"icmp_sgt_w")) return 1;
  il_create_icmp_sle_w(bd,w1,w2); if(!check_op(b,n++,Ocslew,"icmp_sle_w")) return 1;
  il_create_icmp_slt_w(bd,w1,w2); if(!check_op(b,n++,Ocsltw,"icmp_slt_w")) return 1;
  il_create_icmp_uge_w(bd,w1,w2); if(!check_op(b,n++,Ocugew,"icmp_uge_w")) return 1;
  il_create_icmp_ugt_w(bd,w1,w2); if(!check_op(b,n++,Ocugtw,"icmp_ugt_w")) return 1;
  il_create_icmp_ule_w(bd,w1,w2); if(!check_op(b,n++,Oculew,"icmp_ule_w")) return 1;
  il_create_icmp_ult_w(bd,w1,w2); if(!check_op(b,n++,Ocultw,"icmp_ult_w")) return 1;
  // l integer comparisons (10)
  il_create_icmp_eq_l(bd,l1,l2);  if(!check_op(b,n++,Oceql,"icmp_eq_l")) return 1;
  il_create_icmp_ne_l(bd,l1,l2);  if(!check_op(b,n++,Ocnel,"icmp_ne_l")) return 1;
  il_create_icmp_sge_l(bd,l1,l2); if(!check_op(b,n++,Ocsgel,"icmp_sge_l")) return 1;
  il_create_icmp_sgt_l(bd,l1,l2); if(!check_op(b,n++,Ocsgtl,"icmp_sgt_l")) return 1;
  il_create_icmp_sle_l(bd,l1,l2); if(!check_op(b,n++,Ocslel,"icmp_sle_l")) return 1;
  il_create_icmp_slt_l(bd,l1,l2); if(!check_op(b,n++,Ocsltl,"icmp_slt_l")) return 1;
  il_create_icmp_uge_l(bd,l1,l2); if(!check_op(b,n++,Ocugel,"icmp_uge_l")) return 1;
  il_create_icmp_ugt_l(bd,l1,l2); if(!check_op(b,n++,Ocugtl,"icmp_ugt_l")) return 1;
  il_create_icmp_ule_l(bd,l1,l2); if(!check_op(b,n++,Oculel,"icmp_ule_l")) return 1;
  il_create_icmp_ult_l(bd,l1,l2); if(!check_op(b,n++,Ocultl,"icmp_ult_l")) return 1;
  // s float comparisons (8)
  il_create_fcmp_eq_s(bd,s1,s2); if(!check_op(b,n++,Oceqs,"fcmp_eq_s")) return 1;
  il_create_fcmp_ne_s(bd,s1,s2); if(!check_op(b,n++,Ocnes,"fcmp_ne_s")) return 1;
  il_create_fcmp_ge_s(bd,s1,s2); if(!check_op(b,n++,Ocges,"fcmp_ge_s")) return 1;
  il_create_fcmp_gt_s(bd,s1,s2); if(!check_op(b,n++,Ocgts,"fcmp_gt_s")) return 1;
  il_create_fcmp_le_s(bd,s1,s2); if(!check_op(b,n++,Ocles,"fcmp_le_s")) return 1;
  il_create_fcmp_lt_s(bd,s1,s2); if(!check_op(b,n++,Oclts,"fcmp_lt_s")) return 1;
  il_create_fcmp_o_s(bd,s1,s2);  if(!check_op(b,n++,Ocos,"fcmp_o_s")) return 1;
  il_create_fcmp_uo_s(bd,s1,s2); if(!check_op(b,n++,Ocuos,"fcmp_uo_s")) return 1;
  // d float comparisons (8)
  il_create_fcmp_eq_d(bd,d1,d2); if(!check_op(b,n++,Oceqd,"fcmp_eq_d")) return 1;
  il_create_fcmp_ne_d(bd,d1,d2); if(!check_op(b,n++,Ocned,"fcmp_ne_d")) return 1;
  il_create_fcmp_ge_d(bd,d1,d2); if(!check_op(b,n++,Ocged,"fcmp_ge_d")) return 1;
  il_create_fcmp_gt_d(bd,d1,d2); if(!check_op(b,n++,Ocgtd,"fcmp_gt_d")) return 1;
  il_create_fcmp_le_d(bd,d1,d2); if(!check_op(b,n++,Ocled,"fcmp_le_d")) return 1;
  Ref last = il_create_fcmp_lt_d(bd,d1,d2); if(!check_op(b,n++,Ocltd,"fcmp_lt_d")) return 1;
  il_create_fcmp_o_d(bd,d1,d2);  if(!check_op(b,n++,Ocod,"fcmp_o_d")) return 1;
  il_create_fcmp_uo_d(bd,d1,d2); if(!check_op(b,n++,Ocuod,"fcmp_uo_d")) return 1;

  b->jmp.type = Jretw;
  b->jmp.arg = last;

  fn = il_finish(bd);

  int ok = (fn->nblk == 1 && (int)b->nins == 36 && b->jmp.type == Jretw);

  if(!ok){
    printfn(fn, stderr);
  }

  printf("test_cmp...%*s[%s]\n", (int)(45 - 9), "", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
