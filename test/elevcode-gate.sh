#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# An elevated program's full exit code reaches its requester (wine-sg 7729). A Unix exit status
# has eight bits, so the elevation broker saw an installer's 1223 (REAPER's success code) as 199
# and 3010 as 194, and the Store called a good install a failure. Two halves:
#  - ntdll: SG_EXIT_CODE_FILE (an absolute Unix path) in the Unix environment of the process the
#    wine loader starts gets that process's full exit code (decimal) when it ends; the variable is
#    taken out at once, so neither the program nor its children see it;
#  - shell32's SgElevate relay: when sg-elevate's features list "code", it passes --code FILE and
#    ends with the code found there (else with sg-elevate's status, as before).
# The stand-in sg-elevate here does what sg-elevated-run and the broker do: runs the program with
# SG_EXIT_CODE_FILE and copies the code to --code. ShellExecuteEx "runas" as a standard user
# (SG_OTHER, default sgconf, in SG_GROUP); the requester's GetExitCodeProcess must read 1223 and
# 3010; an sg-elevate without "code" still gives its eight-bit status. test/elevcode-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/elevcode-gate.sh
# Mutants: SG_MUTANT_RELAY_IGNORES_CODE (dlls/shell32/shlexec.c), SG_MUTANT_NO_EXIT_CODE_FILE
# (dlls/ntdll/unix/env.c).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
W=$(mktemp -d /var/tmp/sg-elevcode.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$W/probe.exe" "$HERE/elevcode-probe.c" -lshell32 || { echo "FAIL probe did not build"; exit 1; }
chmod 755 "$W/probe.exe"
# the stand-in broker client: answers --ready, runs the program with SG_EXIT_CODE_FILE as
# sg-elevated-run does, copies the code to --code as the broker and sg-elevate do
cat > "$W/elevate" <<EOS
#!/bin/sh
ready=""; code=""
while :; do case "\$1" in --ready) ready=\$2; shift 2 ;; --code) code=\$2; shift 2 ;; *) break ;; esac; done
[ "\$1" = --wine ] && shift
[ -n "\$code" ] && printf 'code\n' >> "$W/out/code.log"
[ -n "\$ready" ] && printf 0 > "\$ready"
f=\$(mktemp /var/tmp/sg-elevcode-x.XXXXXX)
SG_EXIT_CODE_FILE=\$f "$WINE" "\$@" >/dev/null 2>&1; st=\$?
cp "\$f" "$W/out/last-file" 2>/dev/null; chmod 644 "$W/out/last-file" 2>/dev/null
[ -n "\$code" ] && [ -s "\$f" ] && cat "\$f" > "\$code"
rm -f "\$f"
exit \$st
EOS
chmod 755 "$W/elevate"
mkdir -m 777 "$W/out"
printf 'ready\ncode\n' > "$W/features-code"
printf 'ready\n' > "$W/features-old"

mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null
zp() { printf 'Z:%s' "${1//\//\\}"; }
runas() {   # FEATURES CODE
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
        HOME=/var/tmp WINE="$WINE" SG_ELEVATE="$W/elevate" SG_ELEVATE_FEATURES="$W/$1" \
        timeout -s KILL 180 "$WINE" "$(zp "$W/probe.exe")" runas "$2" 2>/dev/null | tr -d '\r' | grep '^RUNAS '
}

out=$(runas features-code 1223)
[ "$out" = "RUNAS code 1223" ] && pass "an elevated program ending with 1223: the requester reads 1223" \
    || fail "elevated 1223: '$out' (file: $(cat "$W/out/last-file" 2>/dev/null))"
out=$(runas features-code 3010)
[ "$out" = "RUNAS code 3010" ] && pass "... and 3010 (success, restart required): 3010" || fail "elevated 3010: '$out'"
out=$(runas features-code 7)
[ "$out" = "RUNAS code 7" ] && pass "a small code is unchanged" || fail "elevated 7: '$out'"
[ "$(grep -c code "$W/out/code.log" 2>/dev/null)" = 3 ] && pass "the relay passed --code when sg-elevate lists the feature" \
    || fail "--code was not passed ($(cat "$W/out/code.log" 2>/dev/null | wc -l))"
out=$(runas features-old 1223)
[ "$out" = "RUNAS code 199" ] && pass "an sg-elevate without \"code\": its eight-bit status, as before" || fail "old sg-elevate: '$out'"
[ "$(grep -c code "$W/out/code.log" 2>/dev/null)" = 3 ] && pass "... and no --code is passed to it" || fail "--code passed to an old sg-elevate"
# neither the program nor its children see the variable (the probe ends with 99 if one does)
out=$(runas features-code 0)
[ "$out" = "RUNAS code 0" ] && pass "SG_EXIT_CODE_FILE is taken out: the program and its child do not see it" || fail "variable visible: '$out'"
# a program started straight from Unix with the variable
f=$W/out/direct-code
sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all HOME=/var/tmp SG_EXIT_CODE_FILE="$f" \
    timeout -s KILL 120 "$WINE" "$(zp "$W/probe.exe")" exit 4294967295 >/dev/null 2>&1
[ "$(sudo -n -u "$SG_OTHER" cat "$f" 2>/dev/null)" = 4294967295 ] && pass "the full 32-bit code is written, as decimal (0xffffffff)" \
    || fail "direct: '$(sudo -n -u "$SG_OTHER" cat "$f" 2>/dev/null)'"
[ "$fails" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$fails"
