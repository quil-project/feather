/*
 * Original QBE Codebase
 * Copyright (c) 2015-2026 Quentin Carbonneaux <quentin@c9x.me>
 *
 * Modifications for Quil Compiler Backend (feather)
 * Copyright (c) 2026-present Quil Project Authors
 *
 * Released under the MIT License.
 */

/*
 * file: all.h
 * Common inclusion among all files.
 * Holds all the important data structure and enums.
 */

#ifndef FEATHER_ALL_H
#define FEATHER_ALL_H

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int optlevel;
#define OPTIMIZE (optlevel >= 1)

#define MAKESURE(what, x) typedef char make_sure_##what[(x) ? 1 : -1]
#define die(...) die_(__FILE__, __VA_ARGS__)

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long bits;

typedef struct BSet BSet;
typedef struct Ref Ref;
typedef struct Op Op;
typedef struct Ins Ins;
typedef struct Phi Phi;
typedef struct Blk Blk;
typedef struct Use Use;
typedef struct Sym Sym;
typedef struct Num Num;
typedef struct Alias Alias;
typedef struct Tmp Tmp;
typedef struct Con Con;
typedef struct Addr Mem;
typedef struct Fn Fn;
typedef struct Typ Typ;
typedef struct Field Field;
typedef struct Dat Dat;
typedef struct Lnk Lnk;
typedef struct Target Target;

enum {
        NIns   = 1 << 20,
        NAlign = 3,
        NField = 32,
        NBit   = CHAR_BIT * sizeof(bits),
};

struct Target {
        char name[16];
        char apple;
        char windows;
        int gpr0; /* first general purpose reg */
        int ngpr;
        int fpr0; /* first floating point reg */
        int nfpr;
        bits rglob; /* globally live regs (e.g., sp, fp) */
        int nrglob;
        int *rsave; /* caller-save */
        int nrsave[2];
        bits (*retregs)(Ref, int[2]);
        bits (*argregs)(Ref, int[2]);
        int (*memargs)(int);
        void (*abi0)(Fn *);
        void (*abi1)(Fn *);
        void (*isel)(Fn *);
        void (*emitfn)(Fn *, FILE *);
        void (*emitfin)(FILE *);
        char asloc[4];
        char assym[4];
        uint cansel : 1;
};

#define BIT(n) ((bits)1 << (n))

enum {
        RXX  = 0,
        Tmp0 = NBit, /* first non-reg temporary */
};

/* bit-set */
struct BSet {
        uint nt; // Number of bits words allocated
        bits *t; // the array of bits word
};

/*
 * Reference to a value
 * By explicitly allocating, overrrides 32 bit allocation.
 */
struct Ref {
        uint type : 3; // 3 bit allocated
        uint val : 29; // 29 bit allocated
};

enum {
        RTmp,
        RCon,
        RInt,
        RType, /* last kind to come out of the parser */
        RSlot,
        RCall,
        RMem,
};

// clang-format off
#define R        (Ref){RTmp, 0}               // NULL Ref
#define UNDEF    (Ref){RCon, 0}               // represents uninitialized data
#define CON_Z    (Ref){RCon, 1}               // constant zero
#define TMP(x)   (Ref){RTmp, x}               // reference to temporary
#define CON(x)   (Ref){RCon, x}               // constant x
#define SLOT(x)  (Ref){RSlot, (x)&0x1fffffff} // stack slot reference
#define TYPE(x)  (Ref){RType, x}              // a type operand (used by alloc/aggregate ops)
#define CALL(x)  (Ref){RCall, x}              // result of a call site x
#define MEM(x)   (Ref){RMem, x}               // the memory state token (tracks aliasing across loads/stores)
#define INT(x)   (Ref){RInt, (x)&0x1fffffff}  // a small intager immediate
// clang-format on

/*
 * req - reference equal.
 * Check if the references are equal.
 */
static inline int req(Ref a, Ref b) {
        return a.type == b.type && a.val == b.val;
}
/*
 * return Ref type tag
 * -1 if the value is null
 */
static inline int rtype(Ref r) {
        if (req(r, R)) {
                return -1;
        }
        return r.type;
}

/*
 * Ref Signed Value
 * Does sign extention for Ref.val
 * makes 29 bit number standard 32 bit
 */
static inline int rsval(Ref r) {
        return ((int)r.val ^ 0x10000000) - 0x10000000;
}

/*
 * Intager and Float COmparison kinds
 * C - compare
 * i, f - intager, float
 * s, u - signed, unisgned
 * eq, ne - equal, not equal
 * ge, gt, le, lt - greater or equal, greater than, less or equal, less than
 * o, uo - ordered, unordered
 */
enum CmpI {
        Cieq,
        Cine,
        Cisge,
        Cisgt,
        Cisle,
        Cislt,
        Ciuge,
        Ciugt,
        Ciule,
        Ciult,
        NCmpI, // Number of comparison, Intager
};
enum CmpF {
        Cfeq,
        Cfge,
        Cfgt,
        Cfle,
        Cflt,
        Cfne,
        Cfo,
        Cfuo,
        NCmpF,                // Number of comparison, float
        NCmp = NCmpI + NCmpF, // total number of comparison
};

enum O {
        Oxxx,
#define O(op, x, y) O##op,
#include "ops.h"
        NOp,
};

// clang-format off
enum J {
	Jxxx,
#define JMPS(X)                                 \
	X(retw)   X(retl)   X(rets)   X(retd)   \
	X(retsb)  X(retub)  X(retsh)  X(retuh)  \
	X(retc)   X(ret0)   X(jmp)    X(jnz)    \
	X(jfieq)  X(jfine)  X(jfisge) X(jfisgt) \
	X(jfisle) X(jfislt) X(jfiuge) X(jfiugt) \
	X(jfiule) X(jfiult) X(jffeq)  X(jffge)  \
	X(jffgt)  X(jffle)  X(jfflt)  X(jffne)  \
	X(jffo)   X(jffuo)  X(hlt)
#define X(j) J##j,
	JMPS(X)
#undef X
	NJmp
};
// clang-format on

enum {
        Ocmpw   = Oceqw,    // compare word: signed ==
        Ocmpw1  = Ocultw,   // compare word: unsigned <
        Ocmpl   = Oceql,    // compare long: signed ==
        Ocmpl1  = Ocultl,   // compare long: unsigned <
        Ocmps   = Oceqs,    // compare single (float): ==
        Ocmps1  = Ocuos,    // compare single: unordered (NaN)
        Ocmpd   = Oceqd,    // compare double: ==
        Ocmpd1  = Ocuod,    // compare double: unordered (NaN)
        Oalloc  = Oalloc4,  // stack alloc: 4 bytes
        Oalloc1 = Oalloc16, // stack alloc: 16 bytes
        Oflag   = Oflagieq, // read CPU flag: int equal
        Oflag1  = Oflagfuo, // read CPU flag: float unordered
        Oxsel   = Oxselieq, // select: int equal
        Oxsel1  = Oxselfuo, // select: float unordered
        NPubOp  = Onop,     // sentinel: ops below are user-visible IL
        Jjf     = Jjfieq,   // jump if false: int equal
        Jjf1    = Jjffuo,   // jump if false: float unordered
};

#define INRANGE(x, l, u) ((unsigned)(x) - l <= u - l) /* linear in x */
#define isstore(o) INRANGE(o, Ostoreb, Ostored)
#define isload(o) INRANGE(o, Oloadsb, Oload)
#define isalloc(o) INRANGE(o, Oalloc4, Oalloc16)
#define isext(o) INRANGE(o, Oextsb, Oextuw)
#define ispar(o) INRANGE(o, Opar, Opare)
#define isarg(o) INRANGE(o, Oarg, Oargv)
#define isret(j) INRANGE(j, Jretw, Jret0)
#define isparbh(o) INRANGE(o, Oparsb, Oparuh)
#define isargbh(o) INRANGE(o, Oargsb, Oarguh)
#define isretbh(j) INRANGE(j, Jretsb, Jretuh)
#define isxsel(o) INRANGE(o, Oxsel, Oxsel1)

enum {
        Kx = -1, /* "top" class (see usecheck() and clsmerge()) */
        Kw,      // word, 32 bit integer
        Kl,      // long, 64 bit integer
        Ks,      // single, 32 bit float
        Kd       // double, 64 bit float
};

#define KWIDE(k) ((k) & 1)
#define KBASE(k) ((k) >> 1)

struct Op {
        char *name;
        short argcls[2][4]; /* value classes (Kw/Kl/Ks/Kd) */
        uint canfold : 1;
        uint hasid : 1;     /* op identity value? */
        uint idval : 1;     /* identity value 0/1 */
        uint commutes : 1;  /* commutative op? */
        uint assoc : 1;     /* associative op? */
        uint idemp : 1;     /* idempotent op? */
        uint cmpeqwl : 1;   /* Kl/Kw cmp eq/ne? */
        uint cmplgtewl : 1; /* Kl/Kw cmp lt/gt/le/ge? */
        uint eqval : 1;     /* 1 for eq; 0 for ne */
        uint pinned : 1;    /* GCM pinned op? */
};

/* Instruction */
struct Ins {
        uint op : 30;
        uint cls : 2;
        Ref to; // the destination Ref: where the result is written (almost always an RTmp — a Tmp/SSA variable)
        Ref arg[2];
};

struct Phi {
        Ref to;
        short cls;
        int visit; // treversal marker, dfs coloring
        uint narg; // incoming predecessors
        Ref *arg;  // value per predecessor
        Blk **blk; // predecessor block for each, blk[i]
        Phi *link; // next phi in this block (phis are a linked list)
};

/* Basic block */
struct Blk {
        Phi *phi;  // pointer to the first phi node
        Ins *ins;  // pointer to the array of instructions
        uint nins; // number of instructions
        struct {
                short type;
                Ref arg;
        } jmp;     // terminator struct
        Blk *s1;   // successor 1 (fall-through / target; set for Jjmp/Jnz)
        Blk *s2;   // successor 2 (branch target for Jnz, else null)
        Blk *link; // next block

        uint id;
        uint visit;

        Blk *idom;
        Blk *dom, *dlink;
        Blk **fron;
        uint nfron;
        int depth;

        Blk **pred; // predecessor array
        uint npred;
        BSet in[1], out[1], gen[1];
        int nlive[2];
        int loop;
        char *name;
};

/* Tmp use site */
struct Use {
        enum {
                UXXX,
                UPhi,
                UIns,
                UJmp,
        } type;   // use kind
        uint bid; // Blk.id of use site
        union {
                Ins *ins;
                Phi *phi;
        } u; // site pointer, NULL for UJump
};

struct Sym {
        enum {
                SGlo    = 0,           /* direct access */
                SThr    = 1,           /* local-exec TLS */
                SExt    = 2,           /* GOT/PLT access */
                SExtThr = SExt | SThr, /* initial-exec TLS */
        } type;
        uint32_t id;
};

struct Num {
        uchar n;
        uchar nl, nr;
        Ref l, r;
};

enum {
        NoAlias,
        MayAlias,
        MustAlias
};

struct Alias {
        enum {
                ABot = 0,
                ALoc = 1, /* stack local */
                ACon = 2,
                AEsc = 3, /* stack escaping */
                ASym = 4,
                AUnk = 6,
#define astack(t) ((t) & 1)
        } type;
        int base;
        int64_t offset;
        union {
                Sym sym;
                struct {
                        int sz; /* -1 if > NBit */
                        bits m;
                } loc;
        } u;
        Alias *slot;
};

/* Temporary Variables */
struct Tmp {
        char *name;
        // pointers to the instructions defining and using this variable
        Ins *def;
        Use *use;

        uint ndef, nuse; // number of times this variable defined and used
        uint bid;        /* id of a defining block */
        uint cost;       // spill cost
        int slot;        /* -1 for unset */
        short cls;       // type w,l,s,d
        struct {
                int r;  /* register or -1 */
                int w;  /* weight */
                bits m; /* avoid these registers */
        } hint;
        int phi;
        Alias alias;
        enum {
                WFull,
                Wsb, /* must match Oload/Oext order */
                Wub,
                Wsh,
                Wuh,
                Wsw,
                Wuw
        } width;
        int visit;
        uint gcmbid;
};

struct Con {
        enum {
                CUndef, // empty
                CBits,  // plain number
                CAddr,  // symbol + offset like $str+8
        } type;
        Sym sym;
        union {
                int64_t i;
                double d;
                float s;
        } bits;
        char flt; /* 1 to print as s, 2 to print as d */
};

typedef struct Addr Addr;

struct Addr { /* amd64 addressing */
        Con offset;
        Ref base;
        Ref index;
        int scale;
};

// how the linker is linked in the linker
struct Lnk {
        char export;
        char thread;
        char common;
        char align; // log2 alignment
        char *sec;
        char *secf;
};

struct Fn {
        Blk *start;
        // Dynamic arrays holding temporaries, constants and memory offset
        Tmp *tmp;
        Con *con;
        Mem *mem;
        // number of tmp, con, mem and Blks
        int ntmp;
        int ncon;
        int nmem;
        uint nblk;
        int retty; /* index in typ[], -1 if no aggregate return */
        Ref retr;
        Blk **rpo; // array of Blk**
        bits reg;  // bitmask of live regs
        int slot;
        int salign;    // stack alignment constant
        char vararg;   // accepts variable args?
        char dynalloc; // function allocates dynamic stack memory?
        char leaf;     // function makes call to no other functions?
        char *name;    // symbol name of the function
        Lnk lnk;
};

struct Typ {
        char *name;
        char isdark;
        char isunion;
        int align;
        uint64_t size;
        uint nunion;
        struct Field {
                enum {
                        FEnd,
                        Fb,
                        Fh,
                        Fw,
                        Fl,
                        Fs,
                        Fd,
                        FPad,
                        FTyp,
                } type;
                uint len; /* or index in typ[] for FTyp */
        } (*fields)[NField + 1];
};

struct Dat {
        enum {
                DStart,
                DEnd,
                DB,
                DH,
                DW,
                DL,
                DZ
        } type;
        char *name;
        Lnk *lnk;
        union {
                int64_t num;
                double fltd;
                float flts;
                char *str;
                struct {
                        char *name;
                        int64_t off;
                } ref;
        } u;
        char isref;
        char isstr;
};

/* main.c */
extern Target T;
extern char debug['Z' + 1];

/* util.c */
typedef enum {
        PHeap, /* free() necessary */
        PFn,   /* discarded after processing the function */
} Pool;

extern Typ *typ;
extern uint ntyp; /* type registry count (parse.c) */
extern Ins insb[NIns], *curi;
uint32_t hash(char *);
void die_(char *, char *, ...) __attribute__((noreturn));
void *emalloc(size_t);
void *alloc(size_t);
void freeall(void);
void *vnew(ulong, size_t, Pool);
void vfree(void *);
void vgrow(void *, ulong);
void addins(Ins **, uint *, Ins *);
void addbins(Ins **, uint *, Blk *);
char *strf(Pool, char *, ...);
uint32_t intern(char *);
char *str(uint32_t);
int argcls(Ins *, int);
int isreg(Ref);
int iscmp(int, int *, int *);
void igroup(Blk *, Ins *, Ins **, Ins **);
void emit(int, int, Ref, Ref, Ref);
void emiti(Ins);
void idup(Blk *, Ins *, ulong);
Ins *icpy(Ins *, Ins *, ulong);
int cmpop(int);
int cmpwlneg(int);
int clsmerge(short *, short);
int phicls(int, Tmp *);
uint phiargn(Phi *, Blk *);
Ref phiarg(Phi *, Blk *);
Ref newtmp(char *, int, Fn *);
void chuse(Ref, int, Fn *);
int symeq(Sym, Sym);
Ref newcon(Con *, Fn *);
Ref getcon(int64_t, Fn *);
int addcon(Con *, Con *, int);
int isconbits(Fn *fn, Ref r, int64_t *v);
void salloc(Ref, Ref, Fn *);
void dumpts(BSet *, Tmp *, FILE *);
void runmatch(uchar *, Num *, Ref, Ref *);
void bsinit(BSet *, uint);
void bszero(BSet *);
uint bscount(BSet *);
void bsset(BSet *, uint);
void bsclr(BSet *, uint);
void bscopy(BSet *, BSet *);
void bsunion(BSet *, BSet *);
void bsinter(BSet *, BSet *);
void bsdiff(BSet *, BSet *);
int bsequal(BSet *, BSet *);
int bsiter(BSet *, int *);

static inline int
bshas(BSet *bs, uint elt) {
        assert(elt < bs->nt * NBit);
        return (bs->t[elt / NBit] & BIT(elt % NBit)) != 0;
}

/* parse.c */
extern Op optab[NOp];
void parse(FILE *, char *, void(char *), void(Dat *), void(Fn *));
void printfn(Fn *, FILE *);
void printref(Ref, Fn *, FILE *);
void err(char *, ...) __attribute__((noreturn));

/* abi.c */
void elimsb(Fn *);

/* cfg.c */
Blk *newblk(void);
void fillpreds(Fn *);
void fillcfg(Fn *);
void filldom(Fn *);
int sdom(Blk *, Blk *);
int dom(Blk *, Blk *);
void fillfron(Fn *);
void loopiter(Fn *, void (*)(Blk *, Blk *));
void filldepth(Fn *);
Blk *lca(Blk *, Blk *);
void fillloop(Fn *);
void simpljmp(Fn *);
int reaches(Fn *, Blk *, Blk *);
int reachesnotvia(Fn *, Blk *, Blk *, Blk *);
int ifgraph(Blk *, Blk **, Blk **, Blk **);
void simplcfg(Fn *);

/* mem.c */
void promote(Fn *);
void coalesce(Fn *);

/* alias.c */
void fillalias(Fn *);
void getalias(Alias *, Ref, Fn *);
int alias(Ref, int, int, Ref, int, int *, Fn *);
int escapes(Ref, Fn *);

/* load.c */
int loadsz(Ins *);
int storesz(Ins *);
void loadopt(Fn *);

/* ssa.c */
void adduse(Tmp *, int, Blk *, ...);
void filluse(Fn *);
void ssa(Fn *);
void ssacheck(Fn *);

/* copy.c */
void narrowpars(Fn *fn);
Ref copyref(Fn *, Blk *, Ins *);
Ref phicopyref(Fn *, Blk *, Phi *);

/* fold.c */
int foldint(Con *, int, int, Con *, Con *);
Ref foldref(Fn *, Ins *);

/* gvn.c */
extern Ref con01[2]; /* 0 and 1 */
int zeroval(Fn *, Blk *, Ref, int, int *);
void gvn(Fn *);

/* gcm.c */
int pinned(Ins *);
void gcm(Fn *);

/* ifopt.c */
void ifconvert(Fn *fn);

/* simpl.c */
void simpl(Fn *);

/* live.c */
void liveon(BSet *, Blk *, Blk *);
void filllive(Fn *);

/* spill.c */
void fillcost(Fn *);
void spill(Fn *);

/* rega.c */
void rega(Fn *);

/* emit.c */
void emitfnlnk(char *, Lnk *, FILE *);
void emitdat(Dat *, FILE *);
void emitdbgfile(char *, FILE *);
void emitdbgloc(uint, uint, FILE *);
int stashbits(bits, int);
void elf_emitfnfin(char *, FILE *);
void elf_emitfin(FILE *);
void macho_emitfin(FILE *);
void pe_emitfin(FILE *);

#endif /* FEATHER_ALL_H */
