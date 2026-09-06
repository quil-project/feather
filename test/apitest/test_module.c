#include "../../filapi/include/module.h"
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

  // ---- function: w add2(w a, w b) { ret a + b } ----
  Lnk flnk = {.export=1};
  Fn *fn = il_create_function("add2", Kx, &flnk);
  ILBuilder *bd = il_create(fn);
  Blk *b = il_create_block(bd, "start");
  il_set_insert_point(bd, b);
  Ref a = il_add_param(bd, Kw);
  Ref bp = il_add_param(bd, Kw);
  il_create_ret_w(bd, il_create_add_w(bd, a, bp));
  fn = il_finish(bd);
  /* no il_destroy: builder memory is pool-tracked and dies at emit;
     destroying now would double-free inside freeall() */

  // ---- data: export data $greet = { b "hi", b 0 } ----
  Lnk dlnk = {.export=1};
  IlData *d = il_data_begin("greet", &dlnk);
  il_data_add_str(d, DB, "hi");
  il_data_add_b(d, 0);
  il_data_end(d);

  // ---- module: own both, emit real asm through the full pipeline ----
  IlModule *m = il_module_create();
  il_module_add_data(m, d);
  il_module_add_function(m, fn);

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

  CHECK(strstr(buf, "add2") != NULL, "fn name in asm");
  CHECK(strstr(buf, "greet") != NULL, "data name in asm");
  CHECK(strstr(buf, "end function") != NULL, "fn trailer");
  CHECK(strstr(buf, "end data") != NULL, "data trailer");
  free(buf);

  printf("test_module...%*s[%s]\n", (int)(45 - 11), "", "ok");
  return 0;
}
