#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 9409 (issue #14): Tab completes file and directory names in
# cmd, as on Windows. Tab gives the first name that starts with the word before
# the cursor and the next on each Tab; Shift+Tab goes back; a name with spaces
# comes quoted; after cd (chdir, pushd, rd, rmdir) only directories; it works
# after any command; CompletionChar 0x40 in HKCU\Software\Microsoft\Command
# Processor turns it off. Tab used to put a tab character (^I) in the line.
#
# The probe (test/cmdcomplete-probe.c) runs cmd on a pseudo console -- conhost,
# as a terminal runs it -- and types keys; the commands write what they got to
# files, which the gate reads.
#   WINE=... test/cmdcomplete-gate.sh
# Mutants: SG_MUTANT_NO_COMPLETION, SG_MUTANT_COMPLETE_ANY_AFTER_CD,
#          SG_MUTANT_COMPLETE_NO_QUOTES (programs/cmd/complete.c).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=$(sg_wineserver)
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
command -v "$MINGW" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
W=$(mktemp -d /var/tmp/sg-cmdcomplete.XXXXXX)
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
cleanup() { WINEPREFIX=$W/prefix "$WINESERVER" -k 2>/dev/null; sleep 1; rm -rf "$W"; }
trap cleanup EXIT
"$MINGW" -O1 -o "$W/probe.exe" "$HERE/cmdcomplete-probe.c" || { echo "FAIL build"; exit 1; }
export WINEPREFIX=$W/prefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES"
unset DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

D=$W/play
mkdir -p "$D/docs" "$D/My Files" "$D/deep/inner"
: > "$D/alpha.txt"; : > "$D/alpine.txt"; : > "$D/data.txt"; printf 'hello-note\r\n' > "$D/My Files/note.txt"; : > "$D/beta file.txt"
zp() { printf 'Z:%s' "${1//\//\\}"; }
run() { timeout -s KILL 120 "$WINE" "$W/probe.exe" "$(zp "$D")" "$@" 2>/dev/null | tr -d '\r'; }
got() { tr -d '\r' < "$D/$1" 2>/dev/null | sed 's/ *$//'; }

out=$(run 'echo al{T}>o1.txt' 'echo al{T}{T}>o2.txt' 'echo al{S}>o3.txt' 'echo al{T}{T}{T}>o4.txt' \
          'echo My{T}>o5.txt' 'echo "My F{T}>o6.txt' 'echo be{T}>o7.txt' 'echo My{T}\{T}>o8.txt' \
          'cd d{T}' 'cd>o9.txt' 'cd ..' 'type My{T}\no{T}>o10.txt' 'echo zz{T}>o11.txt' \
          'echo de{T}\i{T}>o12.txt')
printf '  probe: %s\n' "$out"
[ "$(got o1.txt)" = alpha.txt ] && pass "Tab completes the word to the first name that starts with it" || fail "Tab: '$(got o1.txt)'"
[ "$(got o2.txt)" = alpine.txt ] && pass "Tab again gives the next name" || fail "Tab Tab: '$(got o2.txt)'"
[ "$(got o3.txt)" = alpine.txt ] && pass "Shift+Tab goes back (from the first, to the last)" || fail "Shift+Tab: '$(got o3.txt)'"
[ "$(got o4.txt)" = alpha.txt ] && pass "and it wraps around" || fail "Tab Tab Tab: '$(got o4.txt)'"
[ "$(got o5.txt)" = '"My Files"' ] && pass "a name with a space comes quoted" || fail "quoting: '$(got o5.txt)'"
[ "$(got o6.txt)" = '"My Files"' ] && pass "a word that begins with a quote completes inside it" || fail "open quote: '$(got o6.txt)'"
[ "$(got o7.txt)" = '"beta file.txt"' ] && pass "files too, quoted" || fail "file with a space: '$(got o7.txt)'"
[ "$(got o8.txt)" = '"My Files\note.txt"' ] && pass "inside a quoted folder" || fail "in a quoted folder: '$(got o8.txt)'"
case "$(got deep/o9.txt)" in *'\play\deep') pass "after cd only directories: cd d<Tab> went to deep, not data.txt" ;;
    *) fail "cd d<Tab>: o9 in deep '$(got deep/o9.txt)', in play '$(got o9.txt)'" ;; esac
[ "$(got o10.txt)" = hello-note ] && pass "after another command (type) names complete as well" || fail "type My<Tab>\\no<Tab>: '$(got o10.txt)'"
[ "$(got o11.txt)" = zz ] && pass "a word nothing matches stays as it was" || fail "no match: '$(got o11.txt)'"
[ "$(got o12.txt)" = 'deep\inner' ] && pass "a folder's names complete after its backslash" || fail "deep\\i<Tab>: '$(got o12.txt)'"
if grep -qP '\t' "$D"/o*.txt 2>/dev/null; then fail "a tab character got into a line"; else pass "no tab character in any line"; fi

# CompletionChar 0x40 (as Windows sets in HKLM to turn it off) and PathCompletionChar 0x40: off
"$WINE" reg add 'HKCU\Software\Microsoft\Command Processor' /v CompletionChar /t REG_DWORD /d 0x40 /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Microsoft\Command Processor' /v PathCompletionChar /t REG_DWORD /d 0x40 /f >/dev/null 2>&1
rm -f "$D"/o*.txt
run 'echo al{T}X>o1.txt' >/dev/null
case "$(got o1.txt)" in alpha.txt*) fail "CompletionChar 0x40 still completes" ;;
    al*X) pass "CompletionChar 0x40: no completion" ;; *) fail "CompletionChar 0x40: '$(got o1.txt)'" ;; esac
"$WINESERVER" -k 2>/dev/null

if [ "$fails" -eq 0 ]; then echo "RESULT: PASS"; exit 0; fi
echo "RESULT: FAIL ($fails)"; exit 1
