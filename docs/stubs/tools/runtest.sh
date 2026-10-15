#!/bin/bash
# usage: runtest.sh dll testname [arch]  (env: O=objdir, D=display number (default 217), H=home dir) -- one conformance test under Xvfb + shell desktop; TESTNAME "" = whole exe
DLL=$1; T=$2; ARCH=${3:-x86_64}; O=${O:?set O to the object dir}; D=${D:-217}; H=${H:-/var/tmp/d7/hx$D}
export HOME=$H WINEPREFIX=$H/p WINEDLLOVERRIDES='mscoree,mshtml=;winemenubuilder.exe=d' DISPLAY=:$D
mkdir -p $HOME
Xvfb :$D -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
trap '$O/server/wineserver -k 2>/dev/null; kill $XP 2>/dev/null' EXIT
for i in $(seq 50); do xdpyinfo -display :$D >/dev/null 2>&1 && break; sleep 0.2; done
$O/server/wineserver -k 2>/dev/null
if [ ! -f $WINEPREFIX/.done ]; then
  $O/wine wineboot -i >/dev/null 2>&1
  $O/wine reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
  $O/wine reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1
  touch $WINEPREFIX/.done
fi
EXE=$(ls $O/dlls/$DLL/tests/$ARCH-windows/*_test.exe | grep -v stripped | head -1)
timeout ${TMO:-300} $O/wine $EXE $T 2>&1
