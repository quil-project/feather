#
# Original QBE Codebase
# Copyright (c) 2015-2026 Quentin Carbonneaux <quentin@c9x.me>
#
# Modifications for Quil Compiler Backend (feather)
# Copyright (c) 2026-present Quil Project Authors
#
# Released under the MIT License.
#

.POSIX:
.SUFFIXES: .o .c

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

BUILDDIR = bin

MAIN_OBJ = main.o
UTIL_OBJ = src/util/util.o src/util/parse.o
CORE_OBJ = src/core/cfg.o src/core/mem.o src/core/ssa.o src/core/alias.o src/core/load.o src/core/copy.o
OPT_OBJ  = src/opt/fold.o src/opt/gvn.o src/opt/gcm.o src/opt/simpl.o src/opt/ifopt.o
REG_OBJ  = src/reg/live.o src/reg/spill.o src/reg/rega.o
EMIT_OBJ = src/emit/emit.o src/emit/abi.o
COMMOBJ  = $(UTIL_OBJ) $(CORE_OBJ) $(OPT_OBJ) $(REG_OBJ) $(EMIT_OBJ)
AMD64OBJ = amd64/targ.o amd64/sysv.o amd64/isel.o amd64/emit.o amd64/winabi.o
ARM64OBJ = arm64/targ.o arm64/abi.o arm64/isel.o arm64/emit.o
RV64OBJ  = rv64/targ.o rv64/abi.o rv64/isel.o rv64/emit.o
FILAPIOBJ = filapi/src/ilbuilder.o filapi/src/data.o filapi/src/module.o

# Objects required for core library functionality (without main.o)
CORE_LIB_OBJ = $(addprefix $(BUILDDIR)/,$(COMMOBJ) $(AMD64OBJ) $(ARM64OBJ) $(RV64OBJ) $(FILAPIOBJ))

# Objects required for standalone binary
OBJ          = $(BUILDDIR)/$(MAIN_OBJ) $(CORE_LIB_OBJ)

REL_CFLAGS   = -std=c99 -O2 -Wall -Wextra -Wpedantic

FILAPI_SRC   = filapi/src/ilbuilder.c filapi/src/data.c filapi/src/module.c

SRCALL       = $(UTIL_OBJ:.o=.c) $(CORE_OBJ:.o=.c) $(OPT_OBJ:.o=.c) $(REG_OBJ:.o=.c) $(EMIT_OBJ:.o=.c) \
               $(AMD64OBJ:.o=.c) $(ARM64OBJ:.o=.c) $(RV64OBJ:.o=.c) $(FILAPI_SRC) main.c

CC           = cc
CFLAGS       = -std=c99 -O2 -g -Wall -Wextra -Wpedantic

feather: bin/feather

bin/feather: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(addprefix $(BUILDDIR)/,$(COMMOBJ)): all.h ops.h
$(addprefix $(BUILDDIR)/,$(FILAPIOBJ)): filapi/include/ilbuilder.h filapi/include/data.h filapi/include/module.h all.h ops.h config.h
$(addprefix $(BUILDDIR)/,$(AMD64OBJ)): amd64/all.h
$(addprefix $(BUILDDIR)/,$(ARM64OBJ)): arm64/all.h
$(addprefix $(BUILDDIR)/,$(RV64OBJ)): rv64/all.h
$(BUILDDIR)/main.o: config.h main.c

config.h:
	@case `uname` in                               \
	*Darwin*)                                      \
		case `uname -m` in                     \
		*arm64*)                               \
			echo "#define Deftgt T_arm64_apple";\
			;;                             \
		*)                                     \
			echo "#define Deftgt T_amd64_apple";\
			;;                             \
		esac                                   \
		;;                                     \
	*)                                             \
		case `uname -m` in                     \
		*aarch64*|*arm64*)                     \
			echo "#define Deftgt T_arm64"; \
			;;                             \
		*riscv64*)                             \
			echo "#define Deftgt T_rv64";  \
			;;                             \
		*)                                     \
			echo "#define Deftgt T_amd64_sysv";\
			;;                             \
		esac                                   \
		;;                                     \
	esac > $@

install: feather
	mkdir -p "$(DESTDIR)$(BINDIR)"
	install -m755 bin/feather "$(DESTDIR)$(BINDIR)/feather"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/feather"

clean:
	rm -rf $(BUILDDIR)/*

release:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(REL_CFLAGS)" bin/feather

clean-gen: clean
	rm -f config.h

check-apitest: $(CORE_LIB_OBJ)
	@tools/apitest.sh all

check: feather
	bin=bin/feather tools/test.sh all

check-all: check check-apitest

check-x86_64: feather
	TARGET=x86_64 bin=bin/feather tools/test.sh all

check-arm64: feather
	TARGET=arm64 bin=bin/feather tools/test.sh all

check-rv64: feather
	TARGET=rv64 bin=bin/feather tools/test.sh all

check-amd64_win: feather
	TARGET=amd64_win bin=bin/feather tools/test.sh all

src:
	@echo $(SRCALL)

80:
	@for F in $(SRCALL);                       \
	do                                         \
		awk "{                             \
			gsub(/\\t/, \"        \"); \
			if (length(\$$0) > $@)     \
				printf(\"$$F:%d: %s\\n\", NR, \$$0); \
		}" < $$F;                          \
	done

wc:
	@wc -l $(SRCALL)

.PHONY: clean clean-gen check check-arm64 check-rv64 check-amd64_win check-apitest check-all src 80 wc install uninstall release
