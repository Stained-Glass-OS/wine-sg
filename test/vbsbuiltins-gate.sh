#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# VBScript built-ins (patches/sg/1651): test/vbsbuiltins.vbs under 64- and
# 32-bit cscript in the shell's desktop (Xvfb): the byte functions (LenB,
# LeftB, RightB, MidB, InStrB, AscB, ChrB), AscW/ChrW, String with a code,
# SetLocale/GetLocale, DateValue, TimeValue, DateDiff, DatePart, Join,
# Filter, the Erase statement on fixed and dynamic arrays, UBound of an
# unsized array, TypeName/VarType of arrays, Escape/Unescape, LoadPicture,
# argument errors, and InputBox and MsgBox (help file) answered by
# test/vbsbuiltins-answer.vbs through WScript.Shell SendKeys. These were
# E_NOTIMPL stubs, FIXMEs, or crashed (ReDim of "Dim a()").
#
#   WINE=/opt/wine-sg/bin/wine test/vbsbuiltins-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants: SG_MUTANT_ERASE_NOOP (vbscript/interp.c),
# SG_MUTANT_LENB_CHARS (vbscript/global.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-vbsbuiltins.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

cp "$HERE/vbsbuiltins.vbs" "$HERE/vbsbuiltins-answer.vbs" "$T/"
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w

ZT='Z:'"$(printf '%s' "$T" | tr / '\\')"
cat > "$T/session.sh" <<EOF
#!/bin/sh
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 2
echo "== 64-bit" > "$T/probe.out"
timeout -s KILL 240 "$WINE" cscript //nologo '$ZT\\vbsbuiltins.vbs' '$ZT' 2>/dev/null </dev/null | tr -d '\r' >> "$T/probe.out"
echo "== 32-bit" >> "$T/probe.out"
timeout -s KILL 240 "$WINE" 'C:\\windows\\syswow64\\cscript.exe' //nologo '$ZT\\vbsbuiltins.vbs' '$ZT' 2>/dev/null </dev/null | tr -d '\r' >> "$T/probe.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 600 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh"
cat "$T/probe.out"
[ "$(grep -cx 'RESULT: PASS' "$T/probe.out")" = 2 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"
exit 1
