#!/bin/bash
# usage: suite.sh dll arch outfile  (env: O=objdir, D=display (default 218), H=home) -- every test name of a DLL's conformance exe, one run per name
DLL=$1; ARCH=$2; OUT=$3; O=${O:?set O}; D=${D:-218}; H=${H:-/var/tmp/d7/hs$D}
export HOME=$H WINEPREFIX=$H/p WINEDLLOVERRIDES='mscoree,mshtml=;winemenubuilder.exe=d' DISPLAY=:$D
mkdir -p $HOME
Xvfb :$D -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
trap '$O/server/wineserver -k 2>/dev/null; kill $XP 2>/dev/null' EXIT
for i in $(seq 50); do xdpyinfo -display :$D >/dev/null 2>&1 && break; sleep 0.2; done
if [ ! -f $WINEPREFIX/.done ]; then $O/wine wineboot -i >/dev/null 2>&1; touch $WINEPREFIX/.done; fi
EXE=$(ls $O/dlls/$DLL/tests/$ARCH-windows/*_test.exe | grep -v stripped | head -1)
: > $OUT
NAMES=$(timeout 60 $O/wine $EXE 2>&1 </dev/null | sed -n '/Valid test names/,$p' | grep '^    ' | tr -d ' \r')
if [ -z "$NAMES" ]; then timeout ${TMO:-900} $O/wine $EXE >> $OUT 2>&1 </dev/null; echo "exit=$?" >> $OUT
else for n in $NAMES; do echo "=== $n" >> $OUT; timeout ${TMO:-900} $O/wine $EXE $n >> $OUT 2>&1 </dev/null; echo "exit($n)=$?" >> $OUT; done; fi
echo "failures: $(grep -c 'Test failed' $OUT)" >> $OUT
