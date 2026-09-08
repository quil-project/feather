# Feather IL API (filapi) — Tutorial & Reference

`filapi` is an LLVM `IRBuilder`-style C library for programmatically building Feather IL (QBE-derived) in memory and emitting native assembly directly — no text round-trip, no subprocess. If you are writing a language frontend that targets Feather, this is your backend API.

Include one umbrella header:

```c
#include "filapi/filapi.h" /* pulls ilbuilder.h + data.h + module.h + type.h */
```

> **Requires:** a C99 compiler, `make`, and the Feather checkout. Feather currently targets `amd64_sysv` (Linux), `amd64_apple` (macOS x86_64), `arm64` / `arm64_apple`, `rv64`, and `amd64_win`. Generated assembly is fed to your system `cc`/`as`/`ld`.

---

## Table of Contents

1. [Build & Setup](#1-build--setup)
2. [Core Concepts](#2-core-concepts)
3. [Host Boilerplate](#3-host-boilerplate)
4. [Tutorial 1 — Your First Function (integers & `ret`)](#4-tutorial-1--your-first-function)
5. [Tutorial 2 — Floats](#5-tutorial-2--floats)
6. [Tutorial 3 — Functions, Parameters & Calls](#6-tutorial-3--functions-parameters--calls)
7. [Tutorial 4 — Arithmetic, Bitwise, Converts & Compares](#7-tutorial-4--arithmetic-bitwise-converts--compares)
8. [Tutorial 5 — Memory: `alloc`, `load`, `store`, `blit`](#8-tutorial-5--memory-alloc-load-store-blit)
9. [Tutorial 6 — Control Flow & Phi](#9-tutorial-6--control-flow--phi)
10. [Tutorial 7 — Aggregate Types (structs)](#10-tutorial-7--aggregate-types-structs)
11. [Tutorial 8 — Globals, Data & Symbols](#11-tutorial-8--globals-data--symbols)
12. [Tutorial 9 — Modules & Emitting Assembly](#12-tutorial-9--modules--emitting-assembly)
13. [End-to-End Example](#13-end-to-end-example)
14. [Compiling Your Frontend](#14-compiling-your-frontend)
15. [API Cheat Sheet](#15-api-cheat-sheet)
16. [Contract Rules (read before extending filapi)](#16-contract-rules)
17. [Running Tests](#17-running-tests)

---

## 1. Build & Setup

```bash
git clone <feather-repo> && cd feather
make bin/feather          # builds lib objects under bin/ and the `feather` driver
# or just the library objects:
make bin/src/util/util.o bin/filapi/src/ilbuilder.o  # etc. — `make` builds all
```

The objects you will link against are (see `Makefile:30-32`):

```
bin/src/util/util.o bin/src/util/parse.o
bin/src/core/cfg.o bin/src/core/mem.o bin/src/core/ssa.o bin/src/core/alias.o bin/src/core/load.o bin/src/core/copy.o
bin/src/opt/fold.o bin/src/opt/gvn.o bin/src/opt/gcm.o bin/src/opt/simpl.o bin/src/opt/ifopt.o
bin/src/reg/live.o bin/src/reg/spill.o bin/src/reg/rega.o
bin/src/emit/emit.o bin/src/emit/abi.o
bin/amd64/targ.o bin/amd64/sysv.o bin/amd64/isel.o bin/amd64/emit.o bin/amd64/winabi.o
bin/arm64/targ.o bin/arm64/abi.o bin/arm64/isel.o bin/arm64/emit.o
bin/rv64/targ.o bin/rv64/abi.o bin/rv64/isel.o bin/rv64/emit.o
bin/filapi/src/ilbuilder.o bin/filapi/src/data.o bin/filapi/src/module.o bin/filapi/src/type.o
```

You do **not** need to invoke the `feather` binary when using `filapi` — you call the backend as a library and it writes `.s` assembly for you.

---

## 2. Core Concepts

| Concept | What it is |
|---------|------------|
| `Ref` | A value handle (`RTmp`/`RCon`/`RInt` …). Every SSA value you create returns a `Ref`. Pass `Ref`s as operands. |
| Base classes | `Kw` = 32-bit int (word), `Kl` = 64-bit int (long / pointer), `Ks` = 32-bit float, `Kd` = 64-bit float. `Kx` = "no class / aggregate return" sentinel. |
| Extended types | `b` (byte) and `h` (half) exist only inside aggregate types and data — never as SSA temporaries. |
| `ILBuilder` | Insertion-point wrapper (`Fn *fn` + `Blk *cur`). Like LLVM's `IRBuilder`. All `il_create_*` ops append to `cur`. |
| `Fn` / `Blk` | A function is a linked list of blocks; each block has `Phi*` + `Ins[]` + a `jmp` terminator. |
| `IlType` | Builder for aggregate types (`:name = { w, l, … }`). Produces an `int` index into the global `typ[]` array. |
| `IlData` | Builder for global data (`data $sym = { … }`). Produces a `DStart … DEnd` item stream. |
| `IlModule` | Owns `Fn*[] + IlData*[]`, runs the full optimizer pipeline (`ssa → isel → regalloc`) and emits assembly. |

**Key mental model:**

* Pointers are just `Kl` (`l`) values.
* Integers and floats are never implicit — pick `Kw`/`Kl`/`Ks`/`Kd` explicitly. The `_w`/`_l`/`_s`/`_d` suffix on each `il_create_*` call encodes the class.
* Structs/aggregates are **not** SSA values. You always handle them by address (`Kl` pointer) + `load`/`store`/`blit`. The type index (`int idx = il_type_end(...)`) is the only thing you pass around.

Reference: IL semantics are QBE's — see `doc/il.txt:1` for the full language spec and `ops.h:44` for the opcode table.

---

## 3. Host Boilerplate

Every host program **must** define three globals (tests do the same — `test/apitest/test_basic.c:5`):

```c
#include "filapi/filapi.h"
#include "config.h"          // defines Deftgt
#include <stdio.h>

Target T;
char debug['Z' + 1];
int optlevel = 0;            // 0 = no opt, 1 = gvn/gcm/ifconvert (module.c:64)
extern Target T_amd64_sysv;  // also T_amd64_apple, T_arm64, T_arm64_apple, T_rv64, T_amd64_win
```

Set the target **before** `il_module_emit`:

```c
T = T_amd64_sysv;
optlevel = 0; // or 1
```

Linkage control:

```c
Lnk lnk = { .export = 1 };          // visible outside the object (like `export` in IL)
Lnk local = {0};                    // internal linkage
Lnk tls   = { .thread = 1 };        // thread-local data (rare)
```

---

## 4. Tutorial 1 — Your First Function

The smallest complete program: a function that returns an integer constant.

```c
#include "filapi/filapi.h"
#include "config.h"
#include <stdio.h>

Target T; char debug['Z'+1]; int optlevel = 0;
extern Target T_amd64_sysv;

int main(void) {
    T = T_amd64_sysv;

    // 1. Create function:  w $main()  — Kx = plain return, not an aggregate
    Lnk lnk = {.export = 1};
    Fn *fn = il_create_function("main", Kx, &lnk);
    ILBuilder *bd = il_create(fn);

    // 2. Create entry block and set insertion point
    Blk *entry = il_create_block(bd, "start");
    il_set_insert_point(bd, entry);

    // 3. Create an integer constant (w = 32-bit)
    Ref c42 = il_const_int_w(bd, 42);          // also il_const_int_l, il_const_zero, il_const_undef

    // 4. Return it — _w / _l / _s / _d must match the function's return class
    il_create_ret_w(bd, c42);                  // Jretw

    // 5. Finalize the function's block list
    fn = il_finish(bd);
    // Don't il_destroy(bd) if you will emit — emit frees builders via freeall()

    // 6. Put it in a module and emit assembly
    IlModule *m = il_module_create();
    il_module_add_function(m, fn);

    FILE *out = fopen("out.s", "w");
    il_module_emit(m, out);   // runs ssa/isel/regalloc and writes AT&T assembly
    fclose(out);
    // out.s can now be assembled:  cc out.s -o a.out && ./a.out; echo $?
    return 0;
}
```

Compiles to roughly:

```
export function w $main() {
@start
    ret 42
}
```

Variants you will use daily:

```c
Ref cw = il_const_int_w(bd, -10);      // w  (32-bit)
Ref cl = il_const_int_l(bd, 1337LL);   // l  (64-bit / pointer)
Ref cz = il_const_zero(bd);            // canonical 0
Ref cu = il_const_undef(bd);           // UNDEF — uninitialized bit pattern
```

---

## 5. Tutorial 2 — Floats

Floats have their own constants and ops — same builder pattern:

```c
Ref fs = il_const_float_s(bd, 3.14f);   // s = single (32-bit)
Ref fd = il_const_float_d(bd, 2.71828); // d = double (64-bit)

Ref sum_s = il_create_add_s(bd, fs, fs);
Ref sum_d = il_create_add_d(bd, fd, fd);
Ref prod  = il_create_mul_s(bd, fs, fs);
Ref quot  = il_create_div_d(bd, fd, fd);
Ref neg   = il_create_neg_s(bd, fs);

il_create_ret_s(bd, sum_s); // or _d for double functions
```

A `w $fadd(s %a, s %b)` example:

```c
Lnk lnk = {.export=1};
Fn *fn = il_create_function("fadd", Kx, &lnk);
ILBuilder *bd = il_create(fn);
Blk *b = il_create_block(bd, "start");
il_set_insert_point(bd, b);
Ref a = il_add_param(bd, Ks);
Ref c = il_add_param(bd, Ks);
Ref r = il_create_add_s(bd, a, c);
il_create_ret_s(bd, r);
fn = il_finish(bd);
```

---

## 6. Tutorial 3 — Functions, Parameters & Calls

### Declaring parameters (must be first!)

```c
Lnk lnk = {.export=1};
Fn *fn = il_create_function("add2", Kx, &lnk);
ILBuilder *bd = il_create(fn);
Blk *entry = il_create_block(bd, "start");
il_set_insert_point(bd, entry);

// Plain params — one call per param, *before* any other instruction:
Ref a = il_add_param(bd, Kw);   // w %a
Ref b = il_add_param(bd, Kw);   // w %b
Ref l = il_add_param(bd, Kl);   // l %ptr  (pointers are Kl)
Ref s = il_add_param(bd, Ks);   // s %f
Ref d = il_add_param(bd, Kd);   // d %g

// Aggregate param — pass :type as pointer, returns Kl address tmp:
int point_idx = /* from il_type_end, see §10 */;
Ref p = il_add_parc(bd, point_idx); // :point %p  — actually l %p (address)

// Variadic / aggregate-return markers:
il_function_set_vararg(fn);             // function w $vargs(w %a, ...) { ... }
il_function_set_retty(fn, point_idx);   // function :point $maker() — must set before emit
```

> **Rule:** `Opar`/`Oparc` must lead the start block — asserted at `filapi/src/ilbuilder.c:52`.

### Simple calls

```c
// Inside some caller function with builder `bd`:
Ref callee = il_extern_sym(bd, "puts");       // extern symbol -> CAddr/SExt
// or module-local:
Ref local  = il_global_sym(bd, "my_func");

// No-arg call returning w:
Ref ret = il_create_call_w(bd, callee, NULL, 0);

// With args:
Ref args[] = { il_const_int_w(bd, 1), il_const_int_w(bd, 2) };
Ref sum = il_create_call_w(bd, callee, args, 2); // w call $callee(w 1, w 2)

// Other return classes:
Ref rl = il_create_call_l(bd, callee, args, 2);
Ref rs = il_create_call_s(bd, callee, args, 2);
Ref rd = il_create_call_d(bd, callee, args, 2);
```

### Mixed plain + aggregate args (split-phase)

When a call mixes plain and aggregate arguments you must use the split-phase API (`test/apitest/test_types.c:67`):

```c
Ref mem = il_create_alloc8(bd, il_const_int_w(bd, 8)); // address holding :point
Ref callee = il_extern_sym(bd, "taker");

il_call_arg_c(bd, point_idx, mem);          // :point argument by address
il_call_arg(bd, il_const_int_w(bd, 1));    // w argument
Ref r = il_call_emit(bd, Kw, Kx, callee);  // Kx = plain return; pass typ idx for aggregate return
il_create_ret_w(bd, r);
```

Set `retty` on callees that return aggregates (`test/apitest/test_types.c:81`):

```c
Fn *maker = il_create_function("maker", Kx, &lnk);
il_function_set_retty(maker, point_idx); // now function :point $maker()
ILBuilder *bd2 = il_create(maker);
Blk *b2 = il_create_block(bd2, "start");
il_set_insert_point(bd2, b2);
Ref slot = il_create_alloc8(bd2, il_const_int_w(bd2, 8));
il_create_ret_c(bd2, slot);              // Jretc — returns aggregate by address
maker = il_finish(bd2);
```

---

## 7. Tutorial 4 — Arithmetic, Bitwise, Converts & Compares

Every op has a class suffix. Use the typed helper or the generic form:

```c
// Arithmetic — T(T,T) / T(T)
Ref sum  = il_create_add_w(bd, a, b);
Ref diff = il_create_sub_l(bd, x, y);
Ref prod = il_create_mul_s(bd, f1, f2);
Ref quot = il_create_div_d(bd, d1, d2);   // signed div for ints
Ref q2   = il_create_udiv_w(bd, a, b);   // unsigned div
Ref rm   = il_create_rem_w(bd, a, b);    // signed rem
Ref ru   = il_create_urem_w(bd, a, b);   // unsigned rem
Ref negv = il_create_neg_w(bd, a);
// Generic — cls is Kw/Kl/Ks/Kd:
Ref g    = il_create_add(bd, Kw, a, b);

// Bitwise — I(I,I) / I(I,ww)
Ref bw_and = il_create_and_w(bd, a, b);
Ref bw_or  = il_create_or_w(bd, a, b);
Ref bw_xor = il_create_xor_w(bd, a, b);
Ref shl    = il_create_shl_l(bd, val, amt); // amt is w
Ref shr    = il_create_shr_w(bd, val, amt); // logical
Ref sar    = il_create_sar_w(bd, val, amt); // arithmetic

// Conversions & casts
Ref ext  = il_create_extsw_l(bd, w_val); // sign-extend w -> l
Ref zext = il_create_extuw_l(bd, w_val); // zero-extend
Ref s2d  = il_create_exts_d(bd, s_val);  // s -> d
Ref d2s  = il_create_truncd_s(bd, d_val);// d -> s
Ref f2i  = il_create_stosi_w(bd, s_val); // s -> signed w
Ref i2f  = il_create_swtof_s(bd, w_val); // signed w -> s
Ref bc   = il_create_cast_l(bd, s_val);  // bitcast
Ref cp   = il_create_copy_w(bd, w_val);  // copy

// Comparisons — I(TT,TT) -> w  (1 if true, 0 if false)
Ref icmp = il_create_icmp_slt_w(bd, a, b); // w signed <
Ref ucmp = il_create_icmp_ult_l(bd, x, y); // l unsigned <
Ref fcmp = il_create_fcmp_lt_s(bd, f1, f2);
Ref ord  = il_create_fcmp_o_d(bd, d1, d2); // ordered (no NaN)
```

Full opcode list and type strings are in `ops.h:45` and `doc/il.txt:669`.

---

## 8. Tutorial 5 — Memory: `alloc`, `load`, `store`, `blit`

Stack slots are `Kl` pointers. Feather will color them to registers or stack as needed.

```c
// Allocate n bytes with alignment 4 / 8 / 16:
Ref n   = il_const_int_w(bd, 16);
Ref p4  = il_create_alloc4(bd, n);
Ref p8  = il_create_alloc8(bd, n);
Ref p16 = il_create_alloc16(bd, n);

// Stores — (value, address):
il_create_store_w(bd, val_w, p4);   // storew
il_create_store_l(bd, val_l, p8);   // storel
il_create_store_s(bd, val_s, p4);   // stores
il_create_store_d(bd, val_d, p8);   // stored
il_create_store_b(bd, val_w, p4);   // storeb — low 8 bits
il_create_store_h(bd, val_w, p4);   // storeh — low 16 bits
il_create_store(bd, Kw, val_w, p4); // generic (picks op from cls)

// Loads — (address) -> value:
Ref lw = il_create_load_w(bd, p4);     // loadsw / loadw -> w
Ref ll = il_create_load_l(bd, p8);     // loadl -> l
Ref ls = il_create_load_s(bd, p4);     // loads -> s
Ref ld = il_create_load_d(bd, p8);     // loadd -> d
Ref sb = il_create_load_sb(bd, p4);    // loadsb -> w (sign-extended)
Ref ub = il_create_load_ub(bd, p4);    // loadub
Ref sh = il_create_load_sh(bd, p4);
Ref uh = il_create_load_uh(bd, p4);
Ref sw = il_create_load_sw(bd, p4);    // w -> l sign-extended
Ref uw = il_create_load_uw(bd, p4);    // w -> l zero-extended
Ref gv = il_create_load(bd, Kw, p4);   // generic

// memcpy n bytes (n must be a constant, not a Ref):
il_create_blit(bd, dst_ptr, src_ptr, 32);
```

Typical struct field access: compute field address with `add`, then `load`/`store`. Feather has no `getelementptr` — pointer arithmetic is explicit (`test/apitest/test_memory.c:23`).

---

## 9. Tutorial 6 — Control Flow & Phi

Blocks + terminators model the CFG (`doc/il.txt:532`):

```c
Blk *entry = il_create_block(bd, "entry");
Blk *then_ = il_create_block(bd, "then");
Blk *else_ = il_create_block(bd, "else");
Blk *merge = il_create_block(bd, "merge");

il_set_insert_point(bd, entry);
Ref cond = il_create_icmp_slt_w(bd, a, b);
il_create_cond_br(bd, cond, then_, else_); // jnz cond, then, else — cond is w

il_set_insert_point(bd, then_);
Ref v1 = il_const_int_w(bd, 1);
il_create_br(bd, merge);                   // jmp merge

il_set_insert_point(bd, else_);
Ref v2 = il_const_int_w(bd, 2);
il_create_br(bd, merge);

il_set_insert_point(bd, merge);
Blk *preds[] = {then_, else_};
Ref vals[]   = {v1, v2};
Ref phi = il_create_phi_w(bd, preds, vals, 2); // %phi =w phi @then 1, @else 2
il_create_ret_w(bd, phi);
```

Other terminators:

```c
il_create_br(bd, target);
il_create_cond_br(bd, cond, then_blk, else_blk);
il_create_ret_w(bd, v); il_create_ret_l(bd, v);
il_create_ret_s(bd, v); il_create_ret_d(bd, v);
il_create_ret_c(bd, agg_addr); // aggregate return (needs fn->retty set)
il_create_ret_void(bd);        // Jret0 — for `function $f() { ... }`
il_create_unreachable(bd);     // hlt — never returns
```

> Tip: you don't need `phi` for simple frontends. Allocate every source variable with `alloc4/alloc8` and use `load`/`store` — Feather's `promote`/`mem` passes will lift them to SSA for you (`doc/il.txt:1017`).

---

## 10. Tutorial 7 — Aggregate Types (structs)

Types are **global** and referenced by index — growing `typ[]` may relocate it, so always store the `int` index, never a `Typ*` (`filapi/src/type.c:66`).

```c
#include "filapi/include/type.h"

// type :point = { w, w }  — size 8, align 4 (max of members)
IlType *pt = il_type_begin("point");
il_type_add_w(pt, 2);                 // two words
int point_idx = il_type_end(pt);      // returns typ[] index; do NOT keep IlType*

// type :mixed = { b, l }  — Fb, FPad(7), Fl — size 16, align 8
IlType *mt = il_type_begin("mixed");
il_type_add_b(mt, 1);
il_type_add_l(mt, 1);
int mixed_idx = il_type_end(mt);

// Nested:  type :haspoint = { :point, w }  — size 12, align 4
IlType *ht = il_type_begin("haspoint");
il_type_add_subtype(ht, point_idx, 1);
il_type_add_w(ht, 1);
int haspoint_idx = il_type_end(ht);

// Query (optional — for debugging/codegen):
Typ *ty = &typ[point_idx];
printf("size=%" PRIu64 " align=%d\n", ty->size, 1 << ty->align);
```

Available field builders:

```c
il_type_add_b(t, n); il_type_add_h(t, n);
il_type_add_w(t, n); il_type_add_l(t, n);
il_type_add_s(t, n); il_type_add_d(t, n);
il_type_add_subtype(t, idx, n); // nested :type
```

Padding is computed automatically to satisfy each field's alignment. Unions/opaque types are not exposed via `filapi` yet — define them via hand-written IL if needed (`doc/il.txt:306`).

**Using the type:**

```c
// As param / return:
Ref p = il_add_parc(bd, point_idx);          // incoming struct address
il_function_set_retty(fn, point_idx);        // outgoing struct

// As call arg:
Ref slot = il_create_alloc8(bd, il_const_int_w(bd, typ[point_idx].size));
il_call_arg_c(bd, point_idx, slot);
Ref r = il_call_emit(bd, Kl, Kx, callee);

// Manual field access (explicit arithmetic):
Ref base = slot; // Kl
Ref field1 = base; // offset 0
Ref field2 = il_create_add_l(bd, base, il_const_int_l(bd, 4)); // offset 4
il_create_store_w(bd, il_const_int_w(bd, 10), field1);
Ref v = il_create_load_w(bd, field2);
```

---

## 11. Tutorial 8 — Globals, Data & Symbols

### Symbols (address constants)

```c
Ref puts = il_extern_sym(bd, "puts");  // extern $puts — GOT/PLT, SExt
Ref g    = il_global_sym(bd, "mystr"); // $mystr — SGlo, module-local
// Both are Con/CAddr values usable as call targets or address operands.
// Same name interns to the same Con — req(e1, e2) holds (test_data.c:65).
```

### Data (globals)

Data builders produce the item stream that `emitdat` lowers to `.data`/`.bss` directives (`filapi/src/data.c:37`):

```c
Lnk dlnk = {.export = 1};                  // or .export=0 for static
IlData *d = il_data_begin("mystr", &dlnk);
il_data_add_str(d, DB, "hello");           // b "hello" — quotes added for you
il_data_add_b(d, 0);                       // b 0  (NUL terminator)
il_data_add_w(d, 0x12345678);              // w 0x12345678
il_data_add_h(d, 0x1234);                  // h ...
il_data_add_l(d, 0x123456789abcdef0);      // l ...
il_data_add_s(d, 1.5f);                    // s (emitted as w bits)
il_data_add_d(d, 2.5);                     // d (emitted as l bits)
il_data_add_ref(d, DW, "extsym", 8);       // w $extsym+8  (relocation)
il_data_add_zero(d, 16);                   // z 16  (16 zero bytes)
il_data_end(d);                            // DEnd
```

Which corresponds to IL:

```
export data $mystr = { b "hello", b 0, w 0x12345678, ... , z 16 }
```

Symbols and data are owned by the `IlModule` (see next section).

---

## 12. Tutorial 9 — Modules & Emitting Assembly

`IlModule` owns the program and drives the backend — same pass list as `main.c:func()` (`filapi/src/module.c:36`):

```c
T = T_amd64_sysv;   // set target + optlevel BEFORE emit
optlevel = 0;

IlModule *m = il_module_create();
il_module_add_data(m, d);        // optional — can add many
il_module_add_function(m, fn);   // add each finished Fn (order = emission order)

// Emit AT&T assembly to any FILE*:
FILE *out = fopen("out.s", "w");
il_module_emit(m, out);          // ssa, isel, regalloc, emit; then freeall()
fclose(out);
                 // Builders and Fns are dead after this — never use or il_destroy them.
```

`out.s` is ready for your system toolchain:

```bash
cc out.s -o a.out        # assemble + link (uses system ld)
./a.out; echo $?
# or: cc -c out.s -o out.o && cc out.o -o a.out
```

**Lifetimes:** `il_module_emit` calls `freeall()` — all pool-tracked (`alloc`/`PFn`) memory (blocks, insns, tmps) dies. Anything that must outlive one `compilefn` — `IlModule`, `IlData`, `typ[]`, interned strings — uses untracked `emalloc`/`PHeap`. After `il_module_emit`, do not `il_destroy` a builder whose `Fn` was emitted (double free) (`filapi/src/module.c:120`).

---

## 13. End-to-End Example

A complete host program that builds a module with a struct type, a data string, an `add2` function, and a `main` that calls it and returns the result. Save as `demo.c`:

```c
#include "filapi/filapi.h"
#include "filapi/include/type.h"
#include "config.h"
#include <stdio.h>

Target T; char debug['Z'+1]; int optlevel = 0;
extern Target T_amd64_sysv;

int main(void) {
    T = T_amd64_sysv;

    // -- type :point = { w, w } --
    IlType *pt = il_type_begin("point");
    il_type_add_w(pt, 2);
    int point_idx = il_type_end(pt);

    // -- data $greet = { b "hi", b 0 } --
    Lnk dlnk = {.export = 1};
    IlData *greet = il_data_begin("greet", &dlnk);
    il_data_add_str(greet, DB, "hi");
    il_data_add_b(greet, 0);
    il_data_end(greet);

    // -- function w $add2(w %a, w %b) { ret %a + %b } --
    Lnk flnk = {.export = 1};
    Fn *add2 = il_create_function("add2", Kx, &flnk);
    ILBuilder *bd = il_create(add2);
    Blk *b = il_create_block(bd, "start");
    il_set_insert_point(bd, b);
    Ref a = il_add_param(bd, Kw);
    Ref bp = il_add_param(bd, Kw);
    il_create_ret_w(bd, il_create_add_w(bd, a, bp));
    add2 = il_finish(bd);

    // -- function w $main() { ret call $add2(w 20, w 22) } --
    Fn *mainfn = il_create_function("main", Kx, &flnk);
    bd = il_create(mainfn);
    b = il_create_block(bd, "start");
    il_set_insert_point(bd, b);
    Ref callee = il_global_sym(bd, "add2"); // or il_extern_sym for cross-object
    Ref args[] = { il_const_int_w(bd, 20), il_const_int_w(bd, 22) };
    Ref r = il_create_call_w(bd, callee, args, 2);
    il_create_ret_w(bd, r);
    mainfn = il_finish(bd);

    // -- also demonstrate aggregate param/return wiring (not called, just to show it compiles) --
    Fn *useagg = il_create_function("useagg", Kx, &flnk);
    bd = il_create(useagg);
    b = il_create_block(bd, "start");
    il_set_insert_point(bd, b);
    Ref p = il_add_parc(bd, point_idx); (void)p;
    Ref mem = il_create_alloc8(bd, il_const_int_w(bd, 8));
    Ref taker = il_extern_sym(bd, "taker");
    il_call_arg_c(bd, point_idx, mem);
    il_call_arg(bd, il_const_int_w(bd, 1));
    Ref rc = il_call_emit(bd, Kw, Kx, taker);
    il_create_ret_w(bd, rc);
    useagg = il_finish(bd);

    // -- emit --
    IlModule *m = il_module_create();
    il_module_add_data(m, greet);
    il_module_add_function(m, add2);
    il_module_add_function(m, mainfn);
    il_module_add_function(m, useagg);

    FILE *out = fopen("demo.s", "w");
    if (!out) return 1;
    il_module_emit(m, out);
    fclose(out);
    printf("wrote demo.s — try: cc demo.s -o demo && ./demo; echo $?\n");
    return 0;
}
```

Build and run (from the `feather/` checkout root):

```bash
make -j$(nproc)   # ensure bin/*.o exist
cc -std=c99 -O2 -g -Wall -I. demo.c bin/src/util/util.o bin/src/util/parse.o \
  bin/src/core/cfg.o bin/src/core/mem.o bin/src/core/ssa.o bin/src/core/alias.o bin/src/core/load.o bin/src/core/copy.o \
  bin/src/opt/fold.o bin/src/opt/gvn.o bin/src/opt/gcm.o bin/src/opt/simpl.o bin/src/opt/ifopt.o \
  bin/src/reg/live.o bin/src/reg/spill.o bin/src/reg/rega.o \
  bin/src/emit/emit.o bin/src/emit/abi.o \
  bin/amd64/targ.o bin/amd64/sysv.o bin/amd64/isel.o bin/amd64/emit.o bin/amd64/winabi.o \
  bin/arm64/targ.o bin/arm64/abi.o bin/arm64/isel.o bin/arm64/emit.o \
  bin/rv64/targ.o bin/rv64/abi.o bin/rv64/isel.o bin/rv64/emit.o \
  bin/filapi/src/ilbuilder.o bin/filapi/src/data.o bin/filapi/src/module.o bin/filapi/src/type.o \
  -o demo
./demo
cat demo.s
cc demo.s -o demo_bin && ./demo_bin; echo exit:$?
# expected: 42
```

To emit directly to stdout (as tests do — `test/apitest/test_module.c:49`), use `il_module_emit(m, stdout)` or a `tmpfile()`.

---

## 14. Compiling Your Frontend

`filapi` has no installed library yet — link the `bin/*.o` objects directly (as `tools/apitest.sh:12` does):

```bash
OBJS="
  bin/src/util/util.o bin/src/util/parse.o
  bin/src/core/cfg.o bin/src/core/mem.o bin/src/core/ssa.o bin/src/core/alias.o bin/src/core/load.o bin/src/core/copy.o
  bin/src/opt/fold.o bin/src/opt/gvn.o bin/src/opt/gcm.o bin/src/opt/simpl.o bin/src/opt/ifopt.o
  bin/src/reg/live.o bin/src/reg/spill.o bin/src/reg/rega.o
  bin/src/emit/emit.o bin/src/emit/abi.o
  bin/amd64/targ.o bin/amd64/sysv.o bin/amd64/isel.o bin/amd64/emit.o bin/amd64/winabi.o
  bin/arm64/targ.o bin/arm64/abi.o bin/arm64/isel.o bin/arm64/emit.o
  bin/rv64/targ.o bin/rv64/abi.o bin/rv64/isel.o bin/rv64/emit.o
  bin/filapi/src/ilbuilder.o bin/filapi/src/data.o bin/filapi/src/module.o bin/filapi/src/type.o
"

cc -std=c99 -O2 -g -Wall -I/path/to/feather your_frontend.c $OBJS -o your_frontend
```

If you only target one ISA, you can omit the other `amd64`/`arm64`/`rv64` objects — but keeping all is simplest.

---

## 15. API Cheat Sheet

All symbols are in `filapi/include/ilbuilder.h:16`, `data.h:15`, `type.h:18`, `module.h:16`.

### Function lifecycle

```c
Lnk lnk = {.export = 1};
Fn *fn = il_create_function("my_func", Kx, &lnk); // retty is typ[] index or Kx (never a class)
ILBuilder *bd = il_create(fn);

Blk *entry = il_create_block(bd, "entry");
il_set_insert_point(bd, entry);

// ... emit ...

fn = il_finish(bd);
il_destroy(bd); // only if you NEVER emit: emit frees builders via freeall()
```

### Parameters (must be first in the function)

```c
Ref a = il_add_param(bd, Kw);
Ref b = il_add_param(bd, Kw);
Ref p = il_add_parc(bd, type_idx); // aggregate param, returns Kl address tmp
il_function_set_vararg(fn);
il_function_set_retty(fn, idx);    // aggregate return type :typ[idx]
```

### Constants

```c
Ref c_int32  = il_const_int_w(bd, 42);
Ref c_int64  = il_const_int_l(bd, 1337);
Ref c_float  = il_const_float_s(bd, 3.14f);
Ref c_double = il_const_float_d(bd, 2.71828);
Ref c_zero   = il_const_zero(bd);
Ref c_undef  = il_const_undef(bd);
```

### Arithmetic & negation

```c
Ref sum  = il_create_add_w(bd, a, b);
Ref diff = il_create_sub_l(bd, x, y);
Ref prod = il_create_mul_s(bd, f1, f2);
Ref quot = il_create_div_d(bd, d1, d2);
Ref negv = il_create_neg_w(bd, a);
Ref gen  = il_create_add(bd, Kw, a, b); // generic: cls = Kw/Kl/Ks/Kd
```

### Bitwise & shifts

```c
Ref bit_and = il_create_and_w(bd, a, b);
Ref bit_or  = il_create_or_w(bd, a, b);
Ref bit_xor = il_create_xor_w(bd, a, b);
Ref shl     = il_create_shl_l(bd, val, amt);
Ref shr     = il_create_shr_w(bd, val, amt);
Ref sar     = il_create_sar_w(bd, val, amt);
```

### Memory

```c
Ref ptr  = il_create_alloc4(bd, il_const_int_w(bd, 16));
il_create_store_w(bd, val, ptr);
il_create_store(bd, Kw, val, ptr);       // generic
Ref wval = il_create_load_w(bd, ptr);
Ref lval = il_create_load(bd, Kl, ptr);  // generic
il_create_blit(bd, dst_ptr, src_ptr, 32);
```

### Conversions, casts & copies

```c
Ref ext   = il_create_extsw_l(bd, w_val);
Ref trunc = il_create_truncd_s(bd, d_val);
Ref ftoz  = il_create_stosi_w(bd, f_val);
Ref itof  = il_create_swtof_s(bd, i_val);
Ref cast  = il_create_cast_l(bd, s_val);
Ref cp    = il_create_copy_w(bd, w_val);
```

### Comparisons

```c
Ref icmp = il_create_icmp_slt_w(bd, a, b);
Ref fcmp = il_create_fcmp_lt_s(bd, f1, f2);
```

### Control flow & returns

```c
il_create_br(bd, target_block);
il_create_cond_br(bd, cond_ref, then_block, else_block);
il_create_ret_w(bd, ret_val);   // _l/_s/_d variants
il_create_ret_c(bd, agg_addr);  // aggregate return (needs set_retty)
il_create_ret_void(bd);
il_create_unreachable(bd);
```

### Phi nodes, calls & varargs

```c
Blk *preds[] = {block_a, block_b};
Ref vals[]   = {val_a, val_b};
Ref phi      = il_create_phi_w(bd, preds, vals, 2);

// Simple call:
Ref args[]   = {arg1, arg2};
Ref call_ret = il_create_call_w(bd, callee_ref, args, 2);

// Split-phase (needed for mixed plain/aggregate args):
il_call_arg(bd, plain_val);
il_call_arg_c(bd, type_idx, agg_addr);
Ref call2 = il_call_emit(bd, Kw, Kx, callee_ref); // Kx = plain return

il_create_vastart(bd, ap_ptr);
Ref va_arg = il_create_vaarg_w(bd, ap_ptr);
```

### Symbols & data (globals)

```c
Ref puts = il_extern_sym(bd, "puts");   // extern fn: CAddr/SExt
Ref gref = il_global_sym(bd, "mystr");  // module-local: CAddr/SGlo

Lnk dlnk = {.export = 1};
IlData *d = il_data_begin("mystr", &dlnk);
il_data_add_str(d, DB, "hi");           // quotes added automatically
il_data_add_b(d, 0);
il_data_add_w(d, 0x12345678);
il_data_add_ref(d, DW, "extsym", 8);    // $extsym+8
il_data_add_zero(d, 16);                // 16 zero bytes
il_data_end(d);
```

### Aggregate types

```c
IlType *t = il_type_begin("point");
il_type_add_w(t, 2);                    // two words
int idx = il_type_end(t);               // typ[] index (stable across grows)

IlType *u = il_type_begin("mixed");
il_type_add_b(u, 1);
il_type_add_l(u, 1);                    // padding auto-computed
int idx2 = il_type_end(u);

IlType *v = il_type_begin("haspoint");
il_type_add_subtype(v, idx, 1);         // nested :point
il_type_add_w(v, 1);
int idx3 = il_type_end(v);
```

### Module: own the program, emit asm

```c
T = T_amd64_sysv; // host sets target (and optlevel) before emit

IlModule *m = il_module_create();
il_module_add_data(m, d);
il_module_add_function(m, fn);
il_module_emit(m, stdout); // full pipeline (ssa, isel, regalloc) + asm
```

---

## 16. Contract Rules

1. **`retty` is a `typ[]` index, never a class.** Pass `Kx` for plain returns; the constructor asserts this (`filapi/src/ilbuilder.c:84`). Return *values* still carry their class via the `Jret*` jump.
2. **Params first.** `il_add_param`/`il_add_parc` must precede all other instructions (asserted at `filapi/src/ilbuilder.c:56`). The backend assumes `Opar`s lead the start block.
3. **Lifetimes vs `freeall()`.** Emit frees all pool-tracked (`alloc`/`PFn`) memory per module. Anything outliving one `compilefn` — module/data structs, type registry, interned strings — must use untracked `emalloc`/`PHeap`. After `il_module_emit`, builders and `Fn`s are dead; never `il_destroy` a builder whose function was emitted (double free) (`filapi/src/module.c:120`).
4. **Mirror-the-parser rule.** Builder output must match `parse.c` shapes exactly (`Opar`, `Oarg` runs, `DStart…DEnd` items, field layout). When in doubt, the parser is the reference and any divergence is a builder bug (`filapi/src/module.c:35`).
5. **Indices, not pointers, for types.** Growing `typ[]` can relocate it — always store/pass the `int` index (`filapi/src/type.c:66`).

---

## 17. Running Tests

API tests live in `test/apitest/` (one file per unit: `arith`, `bitwise`, `memory`, `conv`, `cmp`, `control`, `params`, `data`, `types`, `phi/call/va`, `module`):

```bash
make check-apitest
# or directly:
tools/apitest.sh all
```

Each test is a minimal host program — read them as executable examples:

* `test/apitest/test_basic.c:10` — minimal `main` returning a constant
* `test/apitest/test_arith.c:23` — every arithmetic op
* `test/apitest/test_memory.c:15` — alloc/load/store/blit
* `test/apitest/test_control.c:10` — br/cond_br/ret/hlt
* `test/apitest/test_types.c:21` — struct layout, aggregate calls, `retc`
* `test/apitest/test_data.c:19` — data items + extern/global symbols
* `test/apitest/test_module.c:20` — full module → assembly → string checks

For full IL semantics, see `doc/il.txt:1`. For how Feather lowers IL to machine code, see `src/emit/abi.c`, `amd64/isel.c`, and friends.

