#include "../../filapi/include/ilbuilder.h"
#include "../../config.h"
#include <stdio.h>

Target T;
char debug['Z' + 1];
int optlevel = 0;
extern Target T_amd64_sysv;

static int check_op(Blk *b,int idx,int op,const char *name){
  if(idx >= (int)b->nins){fprintf(stderr,"missing %s at %d nins %d\n",name,idx,b->nins);return 0;}
  if(b->ins[idx].op != op){fprintf(stderr,"%s op %d want %d\n",name,b->ins[idx].op,op);return 0;}
  return 1;
}
int main(void){
  T=T_amd64_sysv;
  Lnk lnk={.export=1};
  Fn *fn=il_create_function("bitwise",Kx,&lnk);
  ILBuilder *bd=il_create(fn);
  Blk *b=il_create_block(bd,"start");
  il_set_insert_point(bd,b);
  Ref cw1=il_const_int_w(bd,12), cw2=il_const_int_w(bd,10);
  Ref cl1=il_const_int_l(bd,12), cl2=il_const_int_l(bd,10);
  Ref csh=il_const_int_w(bd,2);
  int n=0;
  il_create_and_w(bd,cw1,cw2); if(!check_op(b,n++,Oand,"and_w")) return 1;
  il_create_and_l(bd,cl1,cl2); if(!check_op(b,n++,Oand,"and_l")) return 1;
  il_create_or_w(bd,cw1,cw2); if(!check_op(b,n++,Oor,"or_w")) return 1;
  il_create_or_l(bd,cl1,cl2); if(!check_op(b,n++,Oor,"or_l")) return 1;
  il_create_xor_w(bd,cw1,cw2); if(!check_op(b,n++,Oxor,"xor_w")) return 1;
  il_create_xor_l(bd,cl1,cl2); if(!check_op(b,n++,Oxor,"xor_l")) return 1;
  il_create_shl_w(bd,cw1,csh); if(!check_op(b,n++,Oshl,"shl_w")) return 1;
  il_create_shl_l(bd,cl1,csh); if(!check_op(b,n++,Oshl,"shl_l")) return 1;
  il_create_shr_w(bd,cw1,csh); if(!check_op(b,n++,Oshr,"shr_w")) return 1;
  il_create_shr_l(bd,cl1,csh); if(!check_op(b,n++,Oshr,"shr_l")) return 1;
  il_create_sar_w(bd,cw1,csh); if(!check_op(b,n++,Osar,"sar_w")) return 1;
  il_create_sar_l(bd,cl1,csh); if(!check_op(b,n++,Osar,"sar_l")) return 1;
  b->jmp.type=Jretw; b->jmp.arg=cw1;
  il_finish(bd);
  printf("test_bitwise...                              [ok]\n");
  return 0;
}
