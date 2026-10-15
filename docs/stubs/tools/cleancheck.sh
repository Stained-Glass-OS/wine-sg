#!/bin/bash
# usage: cleancheck.sh WORKTREE "dlls/a/all dlls/a/tests/all"  (env N=unique name, default cc) -- snapshot of the worktree's COMMITTED HEAD, full series on a fresh tarball (patches that cannot apply because of other branches' patches are skipped and listed in $W/clean.skipped), configure, build the targets. Prints CLEAN-OK/CLEAN-FAIL. Run in background with nohup and output to a file.
set -e
R=$1; T=$2; N=${N:-cc}; W=/var/tmp/d7/$N
rm -rf $W; mkdir -p $W/snap && git -C $R archive HEAD | tar -x -C $W/snap
/var/tmp/d7/prep2.sh $W/snap $W/tree
mv $W/tree.skipped $W/clean.skipped 2>/dev/null || true
mkdir -p $W/obj && cd $W/obj
$W/tree/configure --enable-archs=i386,x86_64 --with-mingw --with-sane --without-oss --without-netapi > configure.log 2>&1
make -j3 $T > make.log 2>&1 && echo CLEAN-OK || echo CLEAN-FAIL
