#include "../../filapi/include/module.h"
#include "../../filapi/include/type.h"
#include "../../config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Target T;
char debug['Z' + 1];
int optlevel = 0;
extern Target T_amd64_sysv;

#define CHECK(c, msg) do { if(!(c)){ fprintf(stderr,"fail: %s\n",msg); return 1; } } while(0)

int main(void){
  T = T_amd64_sysv;

  // ---- :P = { w, w }: size 8, align 2 ----
  IlType *pt = il_type_begin("point");
  il_type_add_w(pt, 2);
  int idxP = il_type_end(pt);
  CHECK(idxP >= 0, "point index");
  CHECK(typ[idxP].size == 8 && typ[idxP].align == 2, "point size/align");
  CHECK((*typ[idxP].fields)[0].type == Fw, "point f0");
  CHECK((*typ[idxP].fields)[1].type == Fw, "point f1");
  CHECK((*typ[idxP].fields)[2].type == FEnd, "point end");

  // ---- :Q = { b, l }: Fb, FPad(7), Fl, FEnd; size 16, align 3 ----
  IlType *qt = il_type_begin("mixed");
  il_type_add_b(qt, 1);
  il_type_add_l(qt, 1);
  int idxQ = il_type_end(qt);
  CHECK(typ[idxQ].size == 16 && typ[idxQ].align == 3, "mixed size/align");
  CHECK((*typ[idxQ].fields)[0].type == Fb, "mixed f0");
  CHECK((*typ[idxQ].fields)[1].type == FPad && (*typ[idxQ].fields)[1].len == 7, "mixed pad");
  CHECK((*typ[idxQ].fields)[2].type == Fl, "mixed f2");
  CHECK((*typ[idxQ].fields)[3].type == FEnd, "mixed end");

  // ---- :R = { :P, w }: FTyp(P), Fw, FEnd; size 12, align 2 ----
  IlType *rt = il_type_begin("haspoint");
  il_type_add_subtype(rt, idxP, 1);
  il_type_add_w(rt, 1);
  int idxR = il_type_end(rt);
  CHECK(typ[idxR].size == 12 && typ[idxR].align == 2, "nested size/align");
  CHECK((*typ[idxR].fields)[0].type == FTyp &&
        (*typ[idxR].fields)[0].len == (uint)idxP, "nested subtype");
  CHECK((*typ[idxR].fields)[1].type == Fw, "nested w");
  CHECK((*typ[idxR].fields)[2].type == FEnd, "nested end");

  // ---- fn useagg(:P %p): mixed call (:P addr, w 1), ret result ----
  Lnk flnk = {.export=1};
  Fn *fn = il_create_function("useagg", Kx, &flnk);
  ILBuilder *bd = il_create(fn);
  Blk *b = il_create_block(bd, "start");
  il_set_insert_point(bd, b);
  Ref p = il_add_parc(bd, idxP);
  (void)p;
  Ref mem = il_create_alloc8(bd, il_const_int_w(bd, 8));
  Ref callee = il_extern_sym(bd, "taker");
  il_call_arg_c(bd, idxP, mem);
  il_call_arg(bd, il_const_int_w(bd, 1));
  Ref r = il_call_emit(bd, Kw, Kx, callee);
  il_create_ret_w(bd, r);
  fn = il_finish(bd);

  CHECK(b->nins == 5, "useagg insn count");
  CHECK(b->ins[0].op == Oparc && b->ins[0].cls == Kl, "parc");
  CHECK(rtype(b->ins[0].arg[0]) == RType && b->ins[0].arg[0].val == idxP, "parc type tag");
  CHECK(b->ins[2].op == Oargc, "argc");
  CHECK(b->ins[3].op == Oarg && b->ins[4].op == Ocall, "arg+call pair");

  // ---- fn maker():PP via retty + ret_c ----
  Fn *fn2 = il_create_function("maker", Kx, &flnk);
  il_function_set_retty(fn2, idxP);
  CHECK(fn2->retty == idxP, "retty set");
  ILBuilder *bd2 = il_create(fn2);
  Blk *b2 = il_create_block(bd2, "start");
  il_set_insert_point(bd2, b2);
  Ref m2 = il_create_alloc8(bd2, il_const_int_w(bd2, 8));
  il_create_ret_c(bd2, m2);
  fn2 = il_finish(bd2);
  CHECK(b2->jmp.type == Jretc, "retc");

  // ---- full backend emit over aggregates ----
  IlModule *m = il_module_create();
  il_module_add_function(m, fn);
  il_module_add_function(m, fn2);
  FILE *out = tmpfile();
  CHECK(out, "tmpfile");
  il_module_emit(m, out);
  fseek(out, 0, SEEK_END);
  long sz = ftell(out);
  CHECK(sz > 0, "empty asm output");
  rewind(out);
  char *buf = malloc((size_t)sz + 1);
  CHECK(buf, "malloc");
  CHECK(fread(buf, 1, (size_t)sz, out) == (size_t)sz, "read asm");
  buf[sz] = '\0';
  fclose(out);
  CHECK(strstr(buf, "useagg") != NULL, "useagg in asm");
  CHECK(strstr(buf, "maker") != NULL, "maker in asm");
  free(buf);

  printf("test_types...%*s[%s]\n", (int)(45 - 10), "", "ok");
  return 0;
}
