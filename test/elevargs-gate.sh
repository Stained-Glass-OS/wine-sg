#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# An elevated program gets its arguments as the caller wrote them (wine-sg 8400), as on Windows,
# where ShellExecuteEx "runas" hands lpParameters over verbatim. shell32's SgElevate relay passed
# the command to sg-elevate as separate arguments and Wine's loader wrote the command line again
# from them, moving quotes: advpack's LaunchINFSectionEx "C:\P F\x.inf",DefaultUninstall arrived
# as "C:\P F\x.inf,DefaultUninstall" (Notepad2's uninstaller left everything in place), an MSI's
# PROP="a b" as "PROP=a b". Now, when writing them again would change the text, the relay passes
# "/sg-raw-args:TEXT" after the program, which ntdll (build_initial_params) -- and start.exe
# /exec, for a program the loader starts through it (a .cmd) -- puts in the command line as it is.
# The stand-in sg-elevate runs the program as sg-elevated-run does ("wine PROGRAM ARGS..."); the
# probe asks "runas" as a standard user (SG_OTHER, in SG_GROUP) with each case's lpParameters and
# the elevated side records the arguments part of its own command line. test/elevargs-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/elevargs-gate.sh
# Mutants: SG_MUTANT_RUNAS_REQUOTES (dlls/shell32/shlexec.c), SG_MUTANT_NO_RAW_ARGS
# (dlls/ntdll/unix/env.c, programs/start/start.c).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
WINESERVER=$(sg_wineserver)
W=$(mktemp -d /var/tmp/sg-elevargs.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
cleanup() {
    set +e
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" "$WINESERVER" -k 2>/dev/null
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -municode -o "$W/probe.exe" "$HERE/elevargs-probe.c" -lshell32 || { echo "FAIL probe did not build"; exit 1; }
chmod 755 "$W/probe.exe"
# the stand-in broker client: answers --ready and --code, runs the program as sg-elevated-run does
cat > "$W/elevate" <<EOS
#!/bin/sh
ready=""
while :; do case "\$1" in --ready) ready=\$2; shift 2 ;; --code) shift 2 ;; *) break ;; esac; done
[ "\$1" = --wine ] && shift
printf '%s\n' "\$*" >> "$W/out/argv.log"
[ -n "\$ready" ] && printf 0 > "\$ready"
"$WINE" "\$@" >/dev/null 2>&1
EOS
chmod 755 "$W/elevate"
mkdir -m 777 "$W/out"
printf 'ready\n' > "$W/features"

mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null
zp() { printf 'Z:%s' "${1//\//\\}"; }
REC=$PFX/drive_c/users/Public/elevargs.txt
# a .cmd the loader starts through start.exe /exec: it hands its arguments to the probe
printf '@"%s" %%*\r\n' "$(zp "$W/probe.exe")" > "$W/relay.cmd"
chmod 644 "$W/relay.cmd"
runas() {   # FILE CASE: prints the elevated side's record of its arguments
    sudo -n rm -f "$REC"
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
        HOME=/var/tmp SG_ELEVATE="$W/elevate" SG_ELEVATE_FEATURES="$W/features" \
        timeout -s KILL 180 "$WINE" "$(zp "$W/probe.exe")" runas "$1" "$2" >/dev/null 2>&1
    sudo -n cat "$REC" 2>/dev/null | tr -d '\r'
}
want() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all HOME=/var/tmp \
        timeout -s KILL 60 "$WINE" "$(zp "$W/probe.exe")" case "$1" 2>/dev/null | tr -d '\r'
}
check() {   # FILE CASE WHAT
    local got exp
    got=$(runas "$1" "$2"); exp=$(want "$2")
    [ -n "$exp" ] && [ "$got" = "$exp" ] && pass "$3: $got" || fail "$3: got '$got', want '$exp'"
}
P=$(zp "$W/probe.exe")
check "$P" 0 "advpack's LaunchINFSectionEx line with a quoted path inside an argument (Notepad2's uninstall)"
check "$P" 1 "an MSI property with a quoted value (INSTALLDIR=\"...\")"
check "$P" 2 "plain arguments and a quoted path, as before"
check "$P" 3 "NSIS's /UAC: left out, the rest as written"
check "$P" 4 "blanks between arguments as written, escaped quotes"
check "$P" 5 "no arguments"
grep -q 'sg-raw-args:show advpack' "$W/out/argv.log" && ! grep -q 'sg-raw-args:show /S /D' "$W/out/argv.log" \
    && pass "the raw form only where writing the arguments again would change them" \
    || fail "raw form use: $(cat "$W/out/argv.log")"
check "$(zp "$W/relay.cmd")" 0 "a .cmd (the loader starts it through start.exe /exec): its %* as written"
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$fails"
