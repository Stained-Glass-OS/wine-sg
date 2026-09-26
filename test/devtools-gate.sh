#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What developer tools need (patches/sg/0320 on), against the installed tree.
#
#   conscp  0320: a program started from a Unix shell with its output
#           redirected (no terminal: a build script, CI, our own tools) can
#           read and set the console code pages, as a Windows program whose
#           output is redirected can. The dotnet CLI sets
#           Console.OutputEncoding first thing, and .NET throws when
#           SetConsoleOutputCP fails -- `dotnet build > log` died at once.
#
#   msienv  0321: an MSI's Environment table writes a value that refers to
#           another variable (%USERPROFILE%\go -- Go's installer) as
#           REG_EXPAND_SZ, as Windows Installer does; it was REG_SZ, so
#           programs saw the literal "%USERPROFILE%\go" and `go` refused to
#           run ("GOPATH entry is relative").
#
#   complex 0322: ucrtbase's (and msvcr120's) C99 complex functions --
#           cabs, csqrt, cexp, clog, cpow, the trigonometric ones and their
#           float/long double forms, _FCbuild, _Cmulcc -- were stubs; numpy
#           calls crealf when it is imported and the process aborted. 64- and
#           32-bit, as MSVC calls them (a 16-byte result through a hidden
#           pointer, an 8-byte one in registers).
#
#   credenum 0323: CredEnumerate takes CRED_ENUMERATE_ALL_CREDENTIALS (every
#           credential, no filter). Git Credential Manager enumerates so;
#           Wine refused any flag, so `git credential-manager get` failed
#           ("Failed to enumerate credentials. [0x3ec] Invalid flags") and
#           Git asked for the password on every push.
#
#   conpty  0324: a pseudo console made on named pipes that nobody has
#           connected to yet (node-pty -- VS Code's integrated terminal --
#           creates it first and connects afterwards) still reads its input:
#           conhost's input thread read STATUS_PIPE_LISTENING, gave up, and
#           nothing typed ever reached the shell.
#
#   WINE=/opt/wine-sg/bin/wine test/devtools-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-devtools.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;$WINEDLLOVERRIDES" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/devtools-probe.exe" "$HERE/devtools-probe.c" -lmsi -ladvapi32 || { fail "probe did not build"; exit 1; }
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
"$MINGW32" -O2 -o "$T/devtools-probe32.exe" "$HERE/devtools-probe.c" -lmsi -ladvapi32 || { fail "32-bit probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
# a crash (an unimplemented function, on the old build) ends the program
# rather than waiting in winedbg
"$WINE" reg add 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\AeDebug' /v Debugger /d false /f >/dev/null 2>&1
"$WINESERVER" -w

# run PROBE-ARGS: output to a file (no terminal), stdin from /dev/null
run() { timeout -s KILL 120 "$WINE" "$T/devtools-probe.exe" "$@" > "$T/out" 2>/dev/null < /dev/null; tr -d '\r' < "$T/out"; }
run32() { timeout -s KILL 120 "$WINE" "$T/devtools-probe32.exe" "$@" > "$T/out" 2>/dev/null < /dev/null; tr -d '\r' < "$T/out"; }
# expect OUTPUT KEY VALUE WHAT
expect() {
    got=$(printf '%s\n' "$1" | sed -n "s/^$2 //p" | head -n1)
    [ "$got" = "$3" ] && pass "$4" || fail "$4 (expected $2 '$3', got '$got')"
}

# ---- conscp (0320) -----------------------------------------------------------
out=$(run conscp)
printf '%s\n' "$out" | sed 's/^/      /'
oem=$(printf '%s\n' "$out" | sed -n 's/^initial-output-cp //p')
[ -n "$oem" ] && [ "$oem" != 0 ] && pass "the output code page starts as the OEM one ($oem)" || fail "initial output code page is '$oem'"
expect "$out" set-output-cp 1 "SetConsoleOutputCP(65001) succeeds with output redirected"
expect "$out" output-cp 65001 "and GetConsoleOutputCP returns it"
expect "$out" set-input-cp 1 "SetConsoleCP(1252) succeeds"
expect "$out" input-cp 1252 "and GetConsoleCP returns it"
expect "$out" set-bogus-cp "0 error 87" "a code page that does not exist is refused (ERROR_INVALID_PARAMETER)"
expect "$out" output-cp-after-bogus 65001 "and does not change it"

# ---- msienv (0321) -----------------------------------------------------------
out=$(run msienv 'C:\envtest.msi')
printf '%s\n' "$out" | sed 's/^/      /'
expect "$out" install 0 "an MSI with an Environment table installs"
expect "$out" home-type REG_EXPAND_SZ "a value with %USERPROFILE% is written REG_EXPAND_SZ"
expect "$out" plain-type REG_SZ "a plain value stays REG_SZ"
"$WINESERVER" -w
out=$(run showenv SGTESTHOME)
if printf '%s\n' "$out" | grep -qi '^env C:\\users\\[^%]*\\sgtest$'; then
    pass "a new program sees it expanded ($out)"
else
    fail "a new program sees '$out'"
fi
out=$(run msiremove 'C:\envtest.msi')
expect "$out" remove 0 "and it uninstalls"

# ---- complex (0322) ----------------------------------------------------------
for bits in 64 32; do
    if [ $bits = 64 ]; then out=$(run complex); else out=$(run32 complex); fi
    printf '%s\n' "$out" | tr '\n' ' ' | sed 's/^/      /'; echo
    for fn in cabs cimag csqrt cexp clog casin ctan cpow fcbuild crealf conjf cpowf; do
        expect "$out" $fn 1 "$bits-bit: $fn"
    done
done

# ---- credenum (0323) ---------------------------------------------------------
out=$(run credenum)
printf '%s\n' "$out" | sed 's/^/      /'
expect "$out" write 1 "a generic credential is written"
expect "$out" enum-all "1 error 0" "CredEnumerate(NULL, CRED_ENUMERATE_ALL_CREDENTIALS) succeeds"
expect "$out" enum-all-found 1 "and returns it"
expect "$out" enum-all-filter "0 error 87" "with a filter too: ERROR_INVALID_PARAMETER"
expect "$out" enum-bad-flag "0 error 1004" "an unknown flag: ERROR_INVALID_FLAGS"
expect "$out" enum-filter "1 count 1" "a filter alone still works"

# ---- conpty (0324) -----------------------------------------------------------
out=$(run conpty)
printf '%s\n' "$out" | sed 's/^/      /'
expect "$out" create 0 "CreatePseudoConsole on unconnected named pipes"
expect "$out" wrote 17 "input written after the pipes are connected is read"
expect "$out" echoed 1 "and reaches the shell (cmd echoes it)"

[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
