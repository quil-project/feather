#!/bin/sh
# Feather IL API tests - compile and run test/apitest/*.c
# Usage: tools/apitest.sh [all|test/apitest/foo.c]

dir=`dirname "$0"`
objdir=$dir/../bin
tmp=/tmp/filapi.apitest.$$

cc="${CC:-cc}"

# Updated object paths matching subdirectories in bin/
objs=""
for o in \
  $objdir/src/util/util.o $objdir/src/util/parse.o \
  $objdir/src/core/cfg.o $objdir/src/core/mem.o $objdir/src/core/ssa.o $objdir/src/core/alias.o $objdir/src/core/load.o $objdir/src/core/copy.o \
  $objdir/src/opt/fold.o $objdir/src/opt/gvn.o $objdir/src/opt/gcm.o $objdir/src/opt/simpl.o $objdir/src/opt/ifopt.o \
  $objdir/src/reg/live.o $objdir/src/reg/spill.o $objdir/src/reg/rega.o \
  $objdir/src/emit/emit.o $objdir/src/emit/abi.o \
  $objdir/amd64/targ.o $objdir/amd64/sysv.o $objdir/amd64/isel.o $objdir/amd64/emit.o $objdir/amd64/winabi.o \
  $objdir/arm64/targ.o $objdir/arm64/abi.o $objdir/arm64/isel.o $objdir/arm64/emit.o \
  $objdir/rv64/targ.o $objdir/rv64/abi.o $objdir/rv64/isel.o $objdir/rv64/emit.o \
  $objdir/filapi/src/ilbuilder.o $objdir/filapi/src/data.o $objdir/filapi/src/module.o $objdir/filapi/src/type.o; do
  if test -f "$o"; then
    objs="$objs $o"
  else
    echo "Warning: object file $o not found" >&2
  fi
done

cleanup(){ rm -f $tmp.exe $tmp.out; }
trap cleanup EXIT

once(){
  t="$1"
  if ! test -f "$t"; then echo "invalid test file $t" >&2; return 1; fi
  printf "%-45s" "$(basename $t)..."
  exe=$tmp.exe
  if ! $cc -std=c99 -O2 -g -Wall -I$dir/.. -I$dir/../filapi/include "$t" $objs -o $exe >$tmp.out 2>&1; then
    echo "[cc fail]"
    cat $tmp.out
    return 1
  fi
  if ! $exe >$tmp.out 2>&1; then
    echo "[run fail]"
    cat $tmp.out
    return 1
  fi
  # test prints [ok]/[FAIL] itself, check exit code and output
  if grep -q "\[FAIL\]" $tmp.out; then
    echo "[fail]"
    cat $tmp.out
    return 1
  fi
  # if test didn't print [ok], show its output but still ok if exit 0
  if grep -q "\[ok\]" $tmp.out; then
    echo "[ok]"
  else
    # fallback: test passed via exit code, show its output as [ok]
    echo "[ok]"
    cat $tmp.out
  fi
  return 0
}

if test -z "$1"; then echo "usage: tools/apitest.sh {all,CFILE}" >&2; exit 1; fi

case "$1" in
"all")
  fail=0; count=0
  for t in $dir/../test/apitest/*.c; do
    # skip if no files
    test -e "$t" || continue
    once "$t"
    fail=`expr $fail + $?`
    count=`expr $count + 1`
  done
  if test $count -eq 0; then echo "no api tests found"; exit 0; fi
  if test $fail -ge 1; then echo; echo "$fail of $count api tests failed!"; else echo; echo "All api tests fine!"; fi
  exit $fail
  ;;
*)
  once "$1"
  exit $?
  ;;
esac