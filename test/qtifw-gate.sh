#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What 32-bit installers made with the Qt Installer Framework need (patches/sg/7902, 7903),
# found with Avidemux's setup:
#  - ProgramW6432Dir and CommonW6432Dir in the 32-bit registry view (WOW6432Node), as 64-bit
#    Windows has them: QIFW's @ApplicationsDirX64@ reads them from a 32-bit process, and without
#    them a 64-bit program's default folder was "\Avidemux ..." ("path not valid").
#  - Windows Script Host runs Unicode script files: QIFW renames the maintenance tool after an
#    install with a UTF-16LE .vbs WITHOUT a byte order mark, run by cscript; Wine read every
#    script as ANSI, so the rename never happened and the uninstaller stayed "...exe.new".
#    Checked with cscript and wscript: UTF-16LE with and without the BOM, and ANSI (unchanged).
#   WINE=/opt/wine-sg/bin/wine test/qtifw-gate.sh
# Mutants: wine.inf without the CommonW6432Dir line (run by hand: the inf is data);
# -DSG_MUTANT_WSCRIPT_BOM_ONLY (programs/wscript/main.c: only a BOM makes a script Unicode).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v iconv >/dev/null || { echo "SKIP: iconv missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-qtifw.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
unset DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
KEY='HKLM\Software\Microsoft\Windows\CurrentVersion'

regval() {  # $1 = reg.exe path, $2 = value, rest = extra args
    exe=$1; v=$2; shift 2
    (cd "$C" && timeout -s KILL 60 "$WINE" "$exe" query "$KEY" /v "$v" "$@" 2>/dev/null | tr -d '\r' |
        sed -n "s/^ *$v *REG_[A-Z_]* *//p")
}
v=$(regval 'C:\windows\syswow64\reg.exe' ProgramW6432Dir)
[ "$v" = 'C:\Program Files' ] && pass "a 32-bit process reads ProgramW6432Dir = C:\\Program Files" ||
    fail "a 32-bit process reads ProgramW6432Dir ('$v')"
v=$(regval 'C:\windows\syswow64\reg.exe' CommonW6432Dir)
[ "$v" = 'C:\Program Files\Common Files' ] && pass "a 32-bit process reads CommonW6432Dir" ||
    fail "a 32-bit process reads CommonW6432Dir ('$v')"
v=$(regval 'C:\windows\system32\reg.exe' ProgramW6432Dir /reg:32)
[ "$v" = 'C:\Program Files' ] && pass "the 32-bit view seen from a 64-bit process (/reg:32)" ||
    fail "the 32-bit view seen from a 64-bit process ('$v')"
v=$(regval 'C:\windows\system32\reg.exe' ProgramFilesDir /reg:32)
[ "$v" = 'C:\Program Files (x86)' ] && pass "the 32-bit view's ProgramFilesDir is still Program Files (x86)" ||
    fail "the 32-bit view's ProgramFilesDir ('$v')"

# a script that renames x.new to x, as QIFW's deferred rename does, and writes a mark
script() {  # $1 = tag
    printf 'Set fso = WScript.CreateObject("Scripting.FileSystemObject")\r\nfso.MoveFile "C:\\t\\%s.new", "C:\\t\\%s"\r\nSet f = fso.CreateTextFile("C:\\t\\%s.ran", True)\r\nf.WriteLine "ok"\r\nf.Close\r\n' "$1" "$1" "$1"
}
mkdir -p "$C/t"
for host in cscript wscript; do
    for kind in utf16 utf16bom ansi; do
        tag="$host-$kind"; : > "$C/t/$tag.new"
        case $kind in
            utf16) script "$tag" | iconv -f UTF-8 -t UTF-16LE > "$C/t/$tag.vbs" ;;
            utf16bom) { printf '\377\376'; script "$tag" | iconv -f UTF-8 -t UTF-16LE; } > "$C/t/$tag.vbs" ;;
            ansi) script "$tag" > "$C/t/$tag.vbs" ;;
        esac
        (cd "$C" && timeout -s KILL 60 "$WINE" $host //B //Nologo "C:\\t\\$tag.vbs" >/dev/null 2>&1)
        if [ -f "$C/t/$tag.ran" ] && [ -f "$C/t/$tag" ] && [ ! -f "$C/t/$tag.new" ]; then
            pass "$host runs the $kind script (the file renamed, the mark written)"
        else
            fail "$host runs the $kind script (ran=$([ -f "$C/t/$tag.ran" ] && echo y || echo n), renamed=$([ -f "$C/t/$tag" ] && echo y || echo n))"
        fi
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
