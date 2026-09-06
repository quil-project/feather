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
  Fn *fn=il_create_function("mem",Kx,&lnk);
  ILBuilder *bd=il_create(fn);
  Blk *b=il_create_block(bd,"start");
  il_set_insert_point(bd,b);

  Ref sz=il_const_int_w(bd,16);
  Ref ptr4 = il_create_alloc4(bd,sz);
  Ref ptr8 = il_create_alloc8(bd,sz);
  Ref ptr16 = il_create_alloc16(bd,sz);
  (void)ptr8; (void)ptr16;
  if(!check_op(b,0,Oalloc4,"alloc4")) return 1;
  if(!check_op(b,1,Oalloc8,"alloc8")) return 1;
  if(!check_op(b,2,Oalloc16,"alloc16")) return 1;

  Ref val_w=il_const_int_w(bd,0x41424344);
  Ref val_l=il_const_int_l(bd,0x41424344);
  Ref val_s=il_const_float_s(bd,1.0f);
  Ref val_d=il_const_float_d(bd,1.0);

  il_create_store_w(bd,val_w,ptr4);
  il_create_store_l(bd,val_l,ptr4);
  il_create_store_s(bd,val_s,ptr4);
  il_create_store_d(bd,val_d,ptr4);
  il_create_store_b(bd,val_w,ptr4);
  il_create_store_h(bd,val_w,ptr4);
  il_create_store(bd, Kw, val_w, ptr4);

  Ref loaded_w = il_create_load_w(bd, ptr4);
  Ref loaded_l = il_create_load_l(bd, ptr4);
  Ref loaded_s = il_create_load_s(bd, ptr4);
  Ref loaded_d = il_create_load_d(bd, ptr4);
  Ref loaded_gen = il_create_load(bd, Kw, ptr4);
  (void)loaded_l; (void)loaded_s; (void)loaded_d; (void)loaded_gen;

  (void)il_create_load_sb(bd,ptr4);
  (void)il_create_load_ub(bd,ptr4);
  (void)il_create_load_sh(bd,ptr4);
  (void)il_create_load_uh(bd,ptr4);
  (void)il_create_load_sw(bd,ptr4);
  (void)il_create_load_uw(bd,ptr4);

  Ref dst=il_create_alloc4(bd,il_const_int_w(bd,16));
  Ref src=il_create_alloc4(bd,il_const_int_w(bd,16));
  il_create_blit(bd,dst,src,16);

  b->jmp.type=Jretw;
  b->jmp.arg=loaded_w;
  il_finish(bd);

  printf("test_memory...                             [ok]\n");
  return 0;
}
