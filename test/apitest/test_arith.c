#include "../../filapi/include/ilbuilder.h"
#include "../../config.h"
#include <stdio.h>
#include <string.h>

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
  Fn *fn = il_create_function("arith", Kx, &lnk);
  ILBuilder *bd = il_create(fn);
  Blk *b = il_create_block(bd, "start");
  il_set_insert_point(bd, b);

  // w/l/s/d constants
  Ref cw1 = il_const_int_w(bd, 10);
  Ref cw2 = il_const_int_w(bd, 3);
  Ref cl1 = il_const_int_l(bd, 10);
  Ref cl2 = il_const_int_l(bd, 3);
  Ref cs1 = il_const_float_s(bd, 1.5f);
  Ref cs2 = il_const_float_s(bd, 2.5f);
  Ref cd1 = il_const_float_d(bd, 1.5);
  Ref cd2 = il_const_float_d(bd, 2.5);

  int n=0;
  // add/sub/mul/div
  il_create_add_w(bd,cw1,cw2); if(!check_op(b,n++,Oadd,"add_w")) return 1;
  il_create_add_l(bd,cl1,cl2); if(!check_op(b,n++,Oadd,"add_l")) return 1;
  il_create_add_s(bd,cs1,cs2); if(!check_op(b,n++,Oadd,"add_s")) return 1;
  il_create_add_d(bd,cd1,cd2); if(!check_op(b,n++,Oadd,"add_d")) return 1;
  il_create_sub_w(bd,cw1,cw2); if(!check_op(b,n++,Osub,"sub_w")) return 1;
  il_create_sub_l(bd,cl1,cl2); if(!check_op(b,n++,Osub,"sub_l")) return 1;
  il_create_sub_s(bd,cs1,cs2); if(!check_op(b,n++,Osub,"sub_s")) return 1;
  il_create_sub_d(bd,cd1,cd2); if(!check_op(b,n++,Osub,"sub_d")) return 1;
  il_create_mul_w(bd,cw1,cw2); if(!check_op(b,n++,Omul,"mul_w")) return 1;
  il_create_mul_l(bd,cl1,cl2); if(!check_op(b,n++,Omul,"mul_l")) return 1;
  il_create_mul_s(bd,cs1,cs2); if(!check_op(b,n++,Omul,"mul_s")) return 1;
  il_create_mul_d(bd,cd1,cd2); if(!check_op(b,n++,Omul,"mul_d")) return 1;
  il_create_div_w(bd,cw1,cw2); if(!check_op(b,n++,Odiv,"div_w")) return 1;
  il_create_div_l(bd,cl1,cl2); if(!check_op(b,n++,Odiv,"div_l")) return 1;
  il_create_div_s(bd,cs1,cs2); if(!check_op(b,n++,Odiv,"div_s")) return 1;
  il_create_div_d(bd,cd1,cd2); if(!check_op(b,n++,Odiv,"div_d")) return 1;
  il_create_udiv_w(bd,cw1,cw2); if(!check_op(b,n++,Oudiv,"udiv_w")) return 1;
  il_create_udiv_l(bd,cl1,cl2); if(!check_op(b,n++,Oudiv,"udiv_l")) return 1;
  il_create_rem_w(bd,cw1,cw2); if(!check_op(b,n++,Orem,"rem_w")) return 1;
  il_create_rem_l(bd,cl1,cl2); if(!check_op(b,n++,Orem,"rem_l")) return 1;
  il_create_urem_w(bd,cw1,cw2); if(!check_op(b,n++,Ourem,"urem_w")) return 1;
  il_create_urem_l(bd,cl1,cl2); if(!check_op(b,n++,Ourem,"urem_l")) return 1;
  il_create_neg_w(bd,cw1); if(!check_op(b,n++,Oneg,"neg_w")) return 1;
  il_create_neg_l(bd,cl1); if(!check_op(b,n++,Oneg,"neg_l")) return 1;
  il_create_neg_s(bd,cs1); if(!check_op(b,n++,Oneg,"neg_s")) return 1;
  il_create_neg_d(bd,cd1); if(!check_op(b,n++,Oneg,"neg_d")) return 1;
  il_create_add(bd, Kw, cw1, cw2); if(!check_op(b,n++,Oadd,"il_create_add")) return 1;

  b->jmp.type = Jretw;
  b->jmp.arg = cw1;
  il_finish(bd);

  printf("test_arith...                                [ok]\n");
  return 0;
}
