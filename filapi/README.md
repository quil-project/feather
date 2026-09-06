# Feather IL API (filapi)

`filapi` is an LLVM `IRBuilder`-style C library for programmatically generating Feather intermediate representation (IL). Frontends (such as the `quil` compiler) build functions, data, types, and whole modules in memory and emit assembly directly — no text round-trip, no subprocess.

Include one umbrella header:

```c
#include "filapi/filapi.h" /* ilbuilder.h + data.h + module.h + type.h */
```

Host programs must define the feather globals (as the tests do):

```c
Target T;
char debug['Z' + 1];
int optlevel = 0;
```

---

## Code Snippets by Category

### 1. Function Lifecycle
```c
Lnk lnk = {.export = 1};
/* retty is a typ[] index, NOT a class: Kx for plain returns */
Fn *fn = il_create_function("my_func", Kx, &lnk);
ILBuilder *bd = il_create(fn);

Blk *entry = il_create_block(bd, "entry");
il_set_insert_point(bd, entry);

// ... emit code ...

fn = il_finish(bd);
il_destroy(bd); /* only if you never emit: emit frees builders via freeall() */
```

### 2. Parameters (must come first in the function)
```c
Ref a = il_add_param(bd, Kw);
Ref b = il_add_param(bd, Kw);
Ref p = il_add_parc(bd, type_idx); /* aggregate param, returns address tmp */
il_function_set_vararg(fn);        /* variadic function */
il_function_set_retty(fn, idx);    /* aggregate return type */
```

### 3. Constants
```c
Ref c_int32  = il_const_int_w(bd, 42);
Ref c_int64  = il_const_int_l(bd, 1337);
Ref c_float  = il_const_float_s(bd, 3.14f);
Ref c_double = il_const_float_d(bd, 2.71828);
Ref c_zero   = il_const_zero(bd);
Ref c_undef  = il_const_undef(bd);
```

### 4. Arithmetic & Negation
```c
Ref sum  = il_create_add_w(bd, a, b);
Ref diff = il_create_sub_l(bd, x, y);
Ref prod = il_create_mul_s(bd, f1, f2);
Ref quot = il_create_div_d(bd, d1, d2);
Ref negv = il_create_neg_w(bd, a);
Ref gen_add = il_create_add(bd, Kw, a, b); /* generic: cls = Kw/Kl/Ks/Kd */
```

### 5. Bitwise & Shifts
```c
Ref bit_and = il_create_and_w(bd, a, b);
Ref bit_or  = il_create_or_w(bd, a, b);
Ref bit_xor = il_create_xor_w(bd, a, b);
Ref shl     = il_create_shl_l(bd, val, amt);
Ref shr     = il_create_shr_w(bd, val, amt);
Ref sar     = il_create_sar_w(bd, val, amt);
```

### 6. Memory Operations
```c
Ref ptr  = il_create_alloc4(bd, il_const_int_w(bd, 16));
il_create_store_w(bd, val, ptr);
il_create_store(bd, Kw, val, ptr);          /* generic */
Ref wval = il_create_load_w(bd, ptr);
Ref lval = il_create_load(bd, Kl, ptr);     /* generic */
il_create_blit(bd, dst_ptr, src_ptr, 32);
```

### 7. Conversions, Casts & Copies
```c
Ref ext   = il_create_extsw_l(bd, w_val);
Ref trunc = il_create_truncd_s(bd, d_val);
Ref ftoz  = il_create_stosi_w(bd, f_val);
Ref itof  = il_create_swtof_s(bd, i_val);
Ref cast  = il_create_cast_l(bd, s_val);
Ref cp    = il_create_copy_w(bd, w_val);
```

### 8. Comparisons
```c
Ref icmp = il_create_icmp_slt_w(bd, a, b);
Ref fcmp = il_create_fcmp_lt_s(bd, f1, f2);
```

### 9. Control Flow & Returns
```c
il_create_br(bd, target_block);
il_create_cond_br(bd, cond_ref, then_block, else_block);
il_create_ret_w(bd, ret_val);   /* _l/_s/_d variants */
il_create_ret_c(bd, agg_addr);  /* aggregate return (needs set_retty) */
il_create_ret_void(bd);
il_create_unreachable(bd);
```

### 10. Phi Nodes, Calls & Varargs
```c
Blk *preds[] = {block_a, block_b};
Ref vals[]   = {val_a, val_b};
Ref phi      = il_create_phi_w(bd, preds, vals, 2);

/* simple call */
Ref args[]   = {arg1, arg2};
Ref call_ret = il_create_call_w(bd, callee_ref, args, 2);

/* split-phase call (needed for mixed plain/aggregate args) */
il_call_arg(bd, plain_val);
il_call_arg_c(bd, type_idx, agg_addr);
Ref call2 = il_call_emit(bd, Kw, Kx, callee_ref); /* Kx = plain return */

il_create_vastart(bd, ap_ptr);
Ref va_arg   = il_create_vaarg_w(bd, ap_ptr);
```

### 11. Symbols & Data (globals)
```c
Ref puts = il_extern_sym(bd, "puts");   /* extern fn: CAddr/SExt const */
Ref gref = il_global_sym(bd, "mystr");  /* module-local symbol: CAddr/SGlo */

Lnk dlnk = {.export = 1};
IlData *d = il_data_begin("mystr", &dlnk);
il_data_add_str(d, DB, "hi");           /* quotes added automatically */
il_data_add_b(d, 0);
il_data_add_w(d, 0x12345678);
il_data_add_ref(d, DW, "extsym", 8);    /* $extsym+8 */
il_data_add_zero(d, 16);                /* 16 zero bytes */
il_data_end(d);
```

### 12. Aggregate Types
```c
IlType *t = il_type_begin("point");
il_type_add_w(t, 2);                    /* two words */
int idx = il_type_end(t);               /* typ[] index (stable across grows) */

IlType *u = il_type_begin("mixed");
il_type_add_b(u, 1);
il_type_add_l(u, 1);                    /* padding auto-computed */
int idx2 = il_type_end(u);

IlType *v = il_type_begin("haspoint");
il_type_add_subtype(v, idx, 1);         /* nested :point */
il_type_add_w(v, 1);
int idx3 = il_type_end(v);
```

### 13. Module: Own the Program, Emit Asm
```c
T = T_amd64_sysv; /* host sets target (and optlevel) before emit */

IlModule *m = il_module_create();
il_module_add_data(m, d);
il_module_add_function(m, fn);
il_module_emit(m, stdout); /* full pipeline (ssa, isel, regalloc) + asm */
```

---

## Contract Rules (read before extending filapi)

1. **`retty` is a `typ[]` index, never a class.** Pass `Kx` for plain returns; the constructor asserts this. Return *values* still carry their class via the `Jret*` jump.
2. **Params first.** `il_add_param`/`il_add_parc` must precede all other instructions (asserted). The backend assumes `Opar`s lead the start block.
3. **Lifetimes vs `freeall()`.** Emit frees all pool-tracked (`alloc`/`PFn`) memory per module. Anything outliving one `compilefn` — module/data structs, type registry, interned strings — must use untracked `emalloc`/`PHeap`. After `il_module_emit`, builders and `Fn`s are dead; never `il_destroy` a builder whose function was emitted (double free).
4. **Mirror-the-parser rule.** Builder output must match `parse.c` shapes exactly (`Opar`, `Oarg` runs, `DStart…DEnd` items, field layout). When in doubt, the parser is the reference and any divergence is a builder bug.
5. **Indices, not pointers, for types.** Growing `typ[]` can relocate it — always store/pass the `int` index.

---

## Running Tests

API tests live in `test/apitest/` (one file per unit: arith, bitwise, memory, conv, cmp, control, params, data, types, phi/call/va, module):

```bash
make check-apitest
# or directly:
tools/apitest.sh all
```
