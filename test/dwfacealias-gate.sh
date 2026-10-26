#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A face of a font that our font Replacements or the machine's FontSubstitutes name stands for
# gives that name among its family names, first (IDWriteFontFace3::GetFamilyNames) -- also when
# the face comes from a collection made of the font's file alone (patches/sg/7006). Chromium
# makes each font such a collection and checks that the face's family is the one the page asked
# for: "Consolas" (Cascadia Mono here) fell back to a proportional font in every Chromium and
# Electron program (VS Code's editor and terminal spaced wide). The probe uses Wine's own
# tahoma.ttf with a Replacements name, symbol.ttf with a FontSubstitutes name, and marlett.ttf
# (named by nothing) as the control.
#   WINE=/opt/wine-sg/bin/wine test/dwfacealias-gate.sh
# Mutant: SG_MUTANT_ALIAS_FACE_NAMES (dlls/dwrite/font.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dwfacealias.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for b in 64 32; do
    cc=x86_64-w64-mingw32-gcc; [ $b = 32 ] && cc=i686-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O1 -o "$T/probe$b.exe" "$HERE/dwfacealias-probe.c" -ldwrite -luuid || { fail "probe ($b-bit) did not build"; exit 1; }
done
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"
printf 'REGEDIT4\r\n\r\n[HKEY_CURRENT_USER\\Software\\Wine\\Fonts\\Replacements]\r\n"SgAliasMono"="Tahoma"\r\n\r\n[HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes]\r\n"SgSubstAlias"="Symbol"\r\n' > "$WINEPREFIX/drive_c/alias.reg"
timeout -s KILL 60 "$WINE" regedit /S 'C:\alias.reg'; "$WINESERVER" -w
for b in 64 32; do
    for alias in SgAliasMono SgSubstAlias; do
        font='C:\windows\Fonts\tahoma.ttf' own=Tahoma
        [ $alias = SgSubstAlias ] && font='C:\windows\Fonts\symbol.ttf' own=Symbol
        out=$(timeout -s KILL 120 "$WINE" "C:\\probe$b.exe" "$font" $own $alias 'C:\windows\Fonts\marlett.ttf' 2>/dev/null | tr -d '\r')
        printf '%s\n' "$out" | sed "s/^/      $b-bit $alias: /"
        check() {
            if printf '%s\n' "$out" | grep -Eq "^(.* )?$1=1( |$)"; then pass "$b-bit $alias: $2"
            else fail "$b-bit $alias: $2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
        }
        check face                  "a face from a collection of the font's file alone"
        check alias_is_family_name  "its face's (English) family name is the name that stands for it (what Chromium compares)"
        check family_keeps_own_name "the font's family in the system collection keeps its own name"
        check other_unchanged       "a font nothing stands for keeps its own names"
        check done                  "the probe ran to the end"
    done
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
