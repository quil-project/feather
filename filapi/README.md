# Feather IL API (`filapi`) & `ILBuilder` Tutorial

`filapi` is an LLVM `IRBuilder`-style C library designed for programmatically generating Feather intermediate representation (IL) instructions, basic blocks, and control flow. It enables compiler frontends (such as the `quil` compiler) to emit clean, correct IR without manually constructing raw instructions, temporary identifiers, or basic block linkage chains.

---

## Table of Contents
1. [Core Concepts](#core-concepts)
2. [Lifecycle & Basic Workflow](#lifecycle--basic-workflow)
3. [Tutorials & Code Examples](#tutorials--code-examples)
   - [Example 1: Returning a Constant Integer](#example-1-returning-a-constant-integer)
   - [Example 2: Control Flow (`if/else` branching)](#example-2-control-flow-ifelse-branching)
   - [Example 3: Stack Allocation, Load, and Store](#example-3-stack-allocation-load-and-store)
4. [Complete API Reference Summary](#complete-api-reference-summary)
   - [Lifecycle & Management](#lifecycle--management)
   - [Constants](#constants)
   - [Arithmetic & Logic](#arithmetic--logic)
   - [Memory Operations](#memory-operations)
   - [Conversions & Casts](#conversions--casts)
   - [Comparisons](#comparisons)
   - [Control Flow](#control-flow)
   - [Phi, Calls & Variadic Functions](#phi-calls--variadic-functions)

---

## Core Concepts

- **Convenience Header**: Users can simply `#include "filapi/filapi.h"` to bring in both configuration and the ILBuilder API in one include.
- **`ILBuilder`**: A wrapper struct holding the current function (`Fn *fn`) and the current insertion basic block (`Blk *cur`), analogous to LLVM's `IRBuilder`.
- **`Ref`**: Represents an operand in Feather IL—this can be a virtual register (temporary), a constant, or a global reference.
- **`Blk`**: Represents a basic block containing a sequence of instructions terminated by a jump/return instruction (`jmp`).

---

## Lifecycle & Basic Workflow

Using `filapi` follows a standard builder pattern:
1. **Create Function**: Call `il_create_function(...)` to allocate and initialize a new Feather function (`Fn`).
2. **Initialize Builder**: Wrap the function with `il_create(fn)`.
3. **Build Blocks & Instructions**:
   - Create basic blocks using `il_create_block(...)`.
   - Set the active insertion point using `il_set_insert_point(...)`.
   - Emit instructions (arithmetic, memory, comparisons, control flow) which return `Ref` handles for subsequent use.
4. **Finalize**: Call `il_finish(ilb)` to compute block post-order (RPO) lists and finalize function metadata.

---

## Tutorials & Code Examples

### Example 1: Returning a Constant Integer
This minimal example creates a function `main` that returns the constant `42`.

```c
#include "filapi/filapi.h"
#include <stdio.h>

Target T;
char debug['Z' + 1];
int optlevel = 0;
extern Target T_amd64_sysv;

int main(void) {
    T = T_amd64_sysv;
    Lnk lnk = {.export = 1};
    
    /* 1. Create function returning 32-bit word (Kw) */
    Fn *fn = il_create_function("main", Kx, &lnk);
    
    /* 2. Initialize builder */
    ILBuilder *bd = il_create(fn);
    
    /* 3. Create entry block and set insertion point */
    Blk *b = il_create_block(bd, "start");
    il_set_insert_point(bd, b);
    
    /* 4. Generate constant 42 and return */
    Ref c = il_const_int_w(bd, 42);
    il_create_ret_w(bd, c);
    
    /* 5. Finalize function */
    fn = il_finish(bd);
    il_destroy(bd);
    
    printf("Successfully built function '%s' with %d block(s).\n", fn->name, fn->nblk);
    return 0;
}
```

---

### Example 2: Control Flow (`if/else` branching)
This example demonstrates conditional branching (`jnz`) using `il_create_cond_br` across three basic blocks (`entry`, `then`, `else`).

```c
#include "filapi/filapi.h"
#include <stdio.h>

Target T;
char debug['Z' + 1];
int optlevel = 0;
extern Target T_amd64_sysv;

int main(void) {
    T = T_amd64_sysv;
    Lnk lnk = {.export = 1};
    
    Fn *fn = il_create_function("test_ctrl", Kx, &lnk);
    ILBuilder *bd = il_create(fn);

    /* Create blocks */
    Blk *b_entry = il_create_block(bd, "entry");
    Blk *b_then  = il_create_block(bd, "then");
    Blk *b_else  = il_create_block(bd, "else");

    /* Entry block: evaluate condition and branch */
    il_set_insert_point(bd, b_entry);
    Ref cond = il_const_int_w(bd, 1);
    il_create_cond_br(bd, cond, b_then, b_else);

    /* Then block: return 10 */
    il_set_insert_point(bd, b_then);
    Ref v1 = il_const_int_w(bd, 10);
    il_create_ret_w(bd, v1);

    /* Else block: return 20 */
    il_set_insert_point(bd, b_else);
    Ref v2 = il_const_int_w(bd, 20);
    il_create_ret_w(bd, v2);

    fn = il_finish(bd);
    il_destroy(bd);
    
    printf("Control flow test compiled successfully!\n");
    return 0;
}
```

---

### Example 3: Stack Allocation, Load, and Store
Frontends frequently need local variables allocated on the stack. Here is how to allocate space, store a value, load it back, and add to it.

```c
#include "filapi/filapi.h"

Target T;
char debug['Z' + 1];
int optlevel = 0;
extern Target T_amd64_sysv;

int main(void) {
    T = T_amd64_sysv;
    Fn *fn = il_create_function("local_var_func", Kx, NULL);
    ILBuilder *bd = il_create(fn);

    Blk *b = il_create_block(bd, "entry");
    il_set_insert_point(bd, b);

    /* Allocate 4 bytes on stack (aligned to 4) */
    Ref size = il_const_int_w(bd, 4);
    Ref slot = il_create_alloc4(bd, size);

    /* Store constant 100 into slot */
    Ref val = il_const_int_w(bd, 100);
    il_create_store_w(bd, val, slot);

    /* Load value back from slot */
    Ref loaded = il_create_load_w(bd, slot);

    /* Add 5 to loaded value */
    Ref five = il_const_int_w(bd, 5);
    Ref result = il_create_add_w(bd, loaded, five);

    il_create_ret_w(bd, result);
    il_finish(bd);
    il_destroy(bd);
    return 0;
}
```

---

## Complete API Reference Summary

### Lifecycle & Management
- `ILBuilder *il_create(Fn *fn)`: Creates an `ILBuilder` for function `fn`.
- `void il_destroy(ILBuilder *ilb)`: Frees the builder struct.
- `void il_set_insert_point(ILBuilder *ilb, Blk *blk)`: Sets the current insertion block.
- `Blk *il_get_insert_block(ILBuilder *ilb)`: Gets the current insertion block.
- `Blk *il_create_block(ILBuilder *ilb, const char *name)`: Creates a new basic block with `name`.
- `Fn *il_create_function(const char *name, int retty, Lnk *lnk)`: Allocates and initializes a new function.
- `Fn *il_finish(ILBuilder *ilb)`: Computes RPO and finalizes function structures.

### Constants
- `il_const_int_w(ilb, v)` / `il_const_int_l(ilb, v)`: 32-bit / 64-bit integer constants.
- `il_const_float_s(ilb, v)` / `il_const_float_d(ilb, v)`: Single / double precision float constants.
- `il_const_undef(ilb)`: Undefined constant (`UNDEF`).
- `il_const_zero(ilb)`: Zero constant (`0`).

### Arithmetic & Logic
- **Arithmetic**: `il_create_add_w`, `il_create_sub_w`, `il_create_mul_w`, `il_create_div_w`, `il_create_udiv_w`, `il_create_rem_w`, `il_create_urem_w`, `il_create_neg_w` (also available for `_l`, `_s`, `_d`).
- **Bitwise**: `il_create_and_w`, `il_create_or_w`, `il_create_xor_w`, `il_create_shl_w`, `il_create_shr_w`, `il_create_sar_w` (also `_l` variants).

### Memory Operations
- **Allocation**: `il_create_alloc4`, `il_create_alloc8`, `il_create_alloc16`.
- **Loads**: `il_create_load_w`, `il_create_load_l`, `il_create_load_s`, `il_create_load_d`, `il_create_load_sb`, `il_create_load_ub`, `il_create_load_sh`, `il_create_load_uh`, `il_create_load_sw`, `il_create_load_uw`.
- **Stores**: `il_create_store_w`, `il_create_store_l`, `il_create_store_s`, `il_create_store_d`, `il_create_store_b`, `il_create_store_h`.
- **Block Move**: `il_create_blit(ilb, dst, src, n)`.

### Conversions & Casts
- **Extensions & Truncations**: `il_create_extsb_w`, `il_create_extub_w`, `il_create_extsh_w`, `il_create_extuh_w`, `il_create_extsw_l`, `il_create_extuw_l`, `il_create_exts_d`, `il_create_truncd_s`.
- **Int/Float Conversions**: `il_create_stosi_w`, `il_create_stoui_w`, `il_create_dtosi_w`, `il_create_dtoui_w`, `il_create_swtof_s`, `il_create_uwtof_s`, `il_create_sltof_s`, `il_create_ultof_s`, etc.
- **Bitcasts & Copies**: `il_create_cast_w`, `il_create_cast_l`, `il_create_cast_s`, `il_create_cast_d`, `il_create_copy_w`, `il_create_copy_l`, etc.

### Comparisons
- **Integer Comparisons (`_w`, `_l`)**: `il_create_icmp_eq`, `ne`, `sge`, `sgt`, `sle`, `slt`, `uge`, `ugt`, `ule`, `ult`.
- **Floating-point Comparisons (`_s`, `_d`)**: `il_create_fcmp_eq`, `ne`, `ge`, `gt`, `le`, `lt`, `o` (ordered), `uo` (unordered).

### Control Flow
- `il_create_br(ilb, dst)`: Unconditional branch (`jmp`).
- `il_create_cond_br(ilb, cond, then_blk, else_blk)`: Conditional branch (`jnz`).
- `il_create_ret_w(ilb, v)` / `_l` / `_s` / `_d` / `il_create_ret_void(ilb)`: Return instructions.
- `il_create_unreachable(ilb)`: Unreachable instruction (`hlt`).

### Phi, Calls & Variadic Functions
- **Phi Nodes**: `il_create_phi_w(ilb, blks[], vals[], n)` (and `_l`, `_s`, `_d`).
- **Function Calls**: `il_create_call_w(ilb, fn, args[], nargs)` (and `_l`, `_s`, `_d`).
- **Variadic Support**: `il_create_vastart`, `il_create_vaarg_w` (and `_l`, `_s`, `_d`).
