#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 9408 (issue #13): a .reg file is "Registration Entries" with
# regedit's own icon for it and the verbs Merge (its double-click), Edit and
# Print; Merge asks "Are you sure you want to continue?" first and then says
# whether the keys and values went in, as Windows' regedit does; regedit asks
# for the highest level available, so an administrator's double-click goes
# through consent (the elevation broker), as on Windows.
#
# .reg files had no verbs at all: a double-click did nothing ("not registered as
# a valid filetype"), and "regedit FILE" merged without asking or reporting.
#
# Part one runs in a prefix of this user's own, on the gate's own display; part
# two as SG_OTHER (default sgconf, in SG_GROUP) in a shared prefix, as
# elevreq-gate.sh does, with a stand-in broker that records what it was given.
#   WINE=... test/regmerge-gate.sh
# Mutants: SG_MUTANT_MERGE_NO_CONFIRM (programs/regedit/regedit.c).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=$(sg_wineserver)
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
sg_gate_display || exit $?
W=$(mktemp -d /var/tmp/sg-regmerge.XXXXXX)
chmod 755 "$W"
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
cleanup() {
    set +e
    WINEPREFIX=$W/own "$WINESERVER" -k 2>/dev/null
    [ -d "$W/shared" ] && sudo -n -u "$SG_OTHER" env WINEPREFIX="$W/shared" "$WINESERVER" -k 2>/dev/null
    WINEPREFIX=$W/shared "$WINESERVER" -k 2>/dev/null
    sleep 1
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W" 2>/dev/null || rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -O2 -municode -o "$W/probe.exe" "$HERE/regmerge-probe.c" -lshlwapi -lshell32 -luser32 -lgdi32 \
    || { echo "FAIL build"; exit 1; }
chmod 755 "$W/probe.exe"

# ---- part one: the association, the merge's questions and reports ----
export WINEPREFIX=$W/own WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
"$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
probe() { timeout -s KILL 60 "$WINE" "$W/probe.exe" "$@" 2>/dev/null | tr -d '\r'; }
out=$(probe assoc)
printf '%s\n' "$out" | sed 's/^/  /'
grep -qx 'TYPE Registration Entries' <<<"$out" && pass "a .reg file is \"Registration Entries\"" || fail "type name"
grep -qix 'ICON C:\\windows\\regedit.exe,1' <<<"$out" && pass "its icon is regedit's second, as on Windows" || fail "DefaultIcon"
grep -qix 'VERB open "C:\\windows\\regedit.exe" "%1"' <<<"$out" && pass "Merge runs regedit on the file" || fail "open verb"
grep -qix 'VERB edit "C:\\windows\\system32\\notepad.exe" "%1"' <<<"$out" && pass "Edit opens it in Notepad" || fail "edit verb"
grep -qix 'VERB print "C:\\windows\\system32\\notepad.exe" /p "%1"' <<<"$out" && pass "Print prints it from Notepad" || fail "print verb"
grep -qix 'DEFAULT "C:\\windows\\regedit.exe" "%1"' <<<"$out" && pass "the double-click (default verb) is Merge" || fail "default verb"
grep -qx 'OPENNAME Mer&ge' <<<"$out" && pass "the verb is called Merge" || fail "Merge name"
n=$(sed -n 's/^ICONS //p' <<<"$out")
[ "${n:-0}" -ge 2 ] && grep -qx 'ICON1 own' <<<"$out" && pass "regedit.exe carries the .reg icon at index 1 ($n icons)" \
    || fail "regedit's icons: $n, $(grep ICON1 <<<"$out")"

zp() { printf 'Z:%s' "${1//\//\\}"; }
reg_of() { "$WINE" reg query 'HKCU\Software\SGRegMerge' /v "$1" 2>/dev/null | tr -d '\r' | awk -v v="$1" '$1==v {print $3}'; }
printf 'Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software\\SGRegMerge]\r\n"yes"="one"\r\n' > "$W/yes.reg"
printf 'Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software\\SGRegMerge]\r\n"no"="two"\r\n' > "$W/no.reg"
printf 'Windows Registry Editor Version 5.00\r\n\r\n[HKEY_NOWHERE\\Software\\SGRegMerge]\r\n"x"="y"\r\n\r\n[HKEY_CURRENT_USER\\Software\\SGRegMerge]\r\n"part"="three"\r\n' > "$W/part.reg"
printf 'just some text\r\n' > "$W/text.reg"
printf 'Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software\\SGRegMerge]\r\n"quiet"="four"\r\n' > "$W/quiet.reg"

merge() {   # FILE ANSWERS...: regedit FILE (as its double-click would), answer its boxes
    local f=$1; shift
    timeout -s KILL 90 "$WINE" regedit "$(zp "$f")" >/dev/null 2>&1 &
    local rp=$!
    probe answer "$@"
    wait "$rp" 2>/dev/null
}
out=$(merge "$W/yes.reg" Y O)
printf '%s\n' "$out" | sed 's/^/  /'
first=$(sed -n 1p <<<"$out"); second=$(sed -n 2p <<<"$out")
case "$first" in "BOX "*"yes.reg"*"Are you sure you want to continue?"*) pass "Merge asks first, naming the file" ;;
    *) fail "the question: '$first'" ;; esac
case "$second" in "BOX The keys and values contained in "*"yes.reg have been successfully added to the registry.") pass "and reports the keys and values added" ;;
    *) fail "the report: '$second'" ;; esac
[ "$(reg_of yes)" = one ] && pass "the value is in the registry" || fail "value after Yes: '$(reg_of yes)'"

out=$(merge "$W/no.reg" N)
[ -z "$(reg_of no)" ] && grep -q 'Are you sure' <<<"$out" && pass "No: nothing is added" || fail "after No: '$(reg_of no)' ($out)"
out=$(probe nobox 3)
[ "$out" = NOBOX ] && pass "and nothing more is said" || fail "after No a box: $out"

out=$(merge "$W/part.reg" Y O)
second=$(sed -n 2p <<<"$out")
case "$second" in "BOX Cannot import "*"part.reg: Not all data was successfully written to the registry."*) pass "a key it cannot write: one report, not all data written" ;;
    *) fail "partial import report: '$second'" ;; esac
[ "$(reg_of part)" = three ] && pass "and the rest of the file is still merged" || fail "rest of a partial import: '$(reg_of part)'"
out=$(probe nobox 3)
[ "$out" = NOBOX ] && pass "no box per failed line" || fail "an extra box: $out"

out=$(merge "$W/text.reg" Y O)
second=$(sed -n 2p <<<"$out")
case "$second" in "BOX Cannot import "*"text.reg: The specified file is not a registry script."*) pass "a file that is no registry script is said to be one" ;;
    *) fail "not a registry script: '$second'" ;; esac

timeout -s KILL 60 "$WINE" regedit /s "$(zp "$W/quiet.reg")" >/dev/null 2>&1 &
rp=$!
out=$(probe nobox 4)
wait "$rp" 2>/dev/null
[ "$out" = NOBOX ] && [ "$(reg_of quiet)" = four ] && pass "regedit /s still merges without a word" || fail "regedit /s: $out, '$(reg_of quiet)'"
"$WINESERVER" -k 2>/dev/null
"$WINESERVER" -w 2>/dev/null

# ---- part two: an administrator's double-click goes through consent ----
if id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null; then
    PFX=$W/shared
    mkdir -m 777 "$W/out"
    cat > "$W/elevate" <<EOS
#!/bin/sh
ready=""
if [ "\$1" = --ready ]; then ready=\$2; shift 2; fi
printf '%s\n' "\$*" >> "$W/out/elevate.log"
[ -n "\$ready" ] && printf 0 > "\$ready"
exit 0
EOS
    printf 'ready\n' > "$W/elevate.features"
    chmod 755 "$W/elevate"
    mkdir "$PFX"
    chgrp "$SG_GROUP" "$PFX"
    chmod 2770 "$PFX"
    touch "$PFX/.sg-system-prefix"
    sg "$SG_GROUP" -c "umask 002; WINEPREFIX='$PFX' '$WINESERVER' -p"
    sg "$SG_GROUP" -c "umask 002; WINEPREFIX='$PFX' '$WINE' wineboot -i" >/dev/null 2>&1
    chmod -R g+rwX "$PFX" 2>/dev/null
    cp "$W/yes.reg" "$W/admin.reg"; chmod 644 "$W/admin.reg"
    other() {
        sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" \
            HOME=/var/tmp SG_ELEVATE="$W/elevate" SG_ELEVATE_FEATURES="$W/elevate.features" \
            ${SG_USER_ADMIN:+SG_USER_ADMIN=$SG_USER_ADMIN} "$@" 2>/dev/null | tr -d '\r' | grep -E '^(CREATE|SHELL) '
    }
    out=$(SG_USER_ADMIN=1 other "$WINE" "$W/probe.exe" create 'C:\windows\regedit.exe /s C:\nothing.reg')
    [ "$out" = "CREATE err 740" ] && pass "for an administrator regedit needs elevation (highestAvailable): ERROR_ELEVATION_REQUIRED" \
        || fail "regedit, administrator: '$out'"
    out=$(other "$WINE" "$W/probe.exe" create 'C:\windows\regedit.exe /s C:\nothing.reg')
    [ "$out" = "CREATE ok 0" ] && pass "a standard user's regedit runs as the user, as on Windows" || fail "regedit, standard user: '$out'"
    rm -f "$W/out/elevate.log"
    out=$(SG_USER_ADMIN=1 other "$WINE" "$W/probe.exe" shell "$(zp "$W/admin.reg")")
    sleep 1
    log=$(cat "$W/out/elevate.log" 2>/dev/null)
    if [ "$out" = "SHELL ok" ] && grep -qi 'regedit.exe' <<<"$log" && grep -q 'admin.reg' <<<"$log"; then
        pass "an administrator's double-click on a .reg file asks for consent: the broker is given regedit and the file ($log)"
    else fail "double-click as an administrator: '$out', broker saw '$log'"; fi
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" "$WINESERVER" -k 2>/dev/null
else
    echo "SKIP part two: no $SG_OTHER/sudo"
fi

if [ "$fails" -eq 0 ]; then echo "RESULT: PASS"; exit 0; fi
echo "RESULT: FAIL ($fails)"; exit 1
