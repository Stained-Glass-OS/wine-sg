#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SG Defender's real-time protection on execute (patch 9456): before a program
# starts or a DLL loads (NtCreateSection of an image), before msi opens a
# package, before cmd and cscript run a script, and in AmsiScanBuffer, wine-sg
# hands the file -- an open descriptor, never a path -- to the sg-defender
# service (sg-session) and does not run what it blocks: ERROR_VIRUS_INFECTED.
# A stand-in service (defender-exec-service.py, the same protocol) finds the
# EICAR test string; no real malware.
#   1. blocked: program, DLL, batch file, script, package, AMSI text (64- and
#      32-bit); the program file is removed; the descriptor is read-only and
#      the caller's own, the path only named
#   2. the service gone: everything runs
#   3. the service stuck: everything runs, within the time limit
#   WINE=/opt/wine-sg/bin/wine test/defender-exec-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
M64="${MINGW:-x86_64-w64-mingw32-gcc}" M32=i686-w64-mingw32-gcc
DPY="${DEFENDER_EXEC_DPY:-530}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP="" SP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
for need in Xvfb xdpyinfo python3 "$M64" "$M32"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY XAUTHORITY

T=$(mktemp -d /var/tmp/sg-defender-exec.XXXXXX)
cleanup() {
    [ -n "$SP" ] && kill "$SP" 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
RC=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }

EICAR='X5O!P%@AP[4\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*'
printf 'int main(void) { return 42; }\n' > "$T/clean.c"
printf '#include <windows.h>\nBOOL WINAPI DllMain(HINSTANCE h, DWORD r, void *p) { return TRUE; }\n__declspec(dllexport) int sg_probe(void) { return 1; }\n' > "$T/cleandll.c"
for a in 64 32; do
    cc=$M64; [ $a = 32 ] && cc=$M32
    TMPDIR=/var/tmp $cc -O2 -municode -o "$T/probe$a.exe" "$HERE/defender-exec-probe.c" 2>"$T/cc.log" \
        && TMPDIR=/var/tmp $cc -O2 -o "$T/clean$a.exe" "$T/clean.c" && TMPDIR=/var/tmp $cc -O2 -shared -o "$T/clean$a.dll" "$T/cleandll.c" \
        || { echo "FAIL  the probe did not build: $(head -5 "$T/cc.log")"; exit 1; }
done
fixtures() {   # DIR BITS: the clean and infected files (infected: the clean one with the test string in it)
    mkdir -p "$1"
    cp "$T/clean$2.exe" "$1/clean.exe"; { cat "$T/clean$2.exe"; printf '%s' "$EICAR"; } > "$1/infected.exe"
    cp "$T/clean$2.dll" "$1/clean.dll"; { cat "$T/clean$2.dll"; printf '%s' "$EICAR"; } > "$1/infected.dll"
    printf '@exit /b 7\r\n' > "$1/clean.bat"; printf '@rem %s\r\n@exit /b 7\r\n' "$EICAR" > "$1/infected.bat"
    printf 'WScript.Quit 5\r\n' > "$1/clean.vbs"; printf "' %s\r\nWScript.Quit 5\r\n" "$EICAR" > "$1/infected.vbs"
    printf 'not a package' > "$1/clean.msi"; printf '%s' "$EICAR" > "$1/infected.msi"
}

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

service() {   # [DELAY]
    [ -n "$SP" ] && kill "$SP" 2>/dev/null; rm -f "$T/sock"
    python3 "$HERE/defender-exec-service.py" "$T/sock" "$T/log" "${1:-0}" & SP=$!
    i=0; while [ ! -S "$T/sock" ] && [ $i -lt 40 ]; do sleep 0.1; i=$((i + 1)); done
}
probe() {   # BITS DIR [allowed]: run the probe, its lines in $T/out
    winedir=$(SG_DEFENDER_SOCKET=/nonexistent "$WINE" winepath -w "$2" 2>/dev/null | tr -d '\r')
    timeout -s KILL 240 "$WINE" "$T/probe$1.exe" "$winedir" ${3:-} >"$T/out.raw" 2>/dev/null </dev/null
    tr -d '\r' <"$T/out.raw" >"$T/out"
    [ -n "${GATE_KEEP:-}" ] && cp "$T/out" "$T/log" "$GATE_KEEP/" 2>/dev/null
}

# 1. blocked, 64-bit and 32-bit
service
export SG_DEFENDER_SOCKET="$T/sock"
for a in 64 32; do
    fixtures "$T/f$a" $a; : > "$T/log"
    probe $a "$T/f$a"
    if grep -qx 'RESULT: PASS' "$T/out"; then pass "$a-bit: program, DLL, batch file, script, package and AMSI text with the test string are stopped; clean ones run"
    else fail "$a-bit probe:"; grep '^FAIL' "$T/out" | sed 's/^/      /'; grep -q '^RESULT' "$T/out" || tail -n 5 "$T/out"; fi
    for k in image script msi amsi; do grep -q "^$k " "$T/log" || { fail "$a-bit: no '$k' request reached the service"; }; done
    grep -q "^image .*hint=$T/f$a/infected.exe\$" "$T/log" && pass "$a-bit: the service is told where the program is (a name only)" || fail "$a-bit: no image request names infected.exe: $(head -3 "$T/log")"
    if grep '^image \|^script \|^msi ' "$T/log" | grep -v ' mode=r ' | grep -q .; then fail "$a-bit: a descriptor not read-only: $(grep -v ' mode=r ' "$T/log" | head -2)"
    else pass "$a-bit: every descriptor is read-only"; fi
    if grep '^image \|^script \|^msi ' "$T/log" | awk '{ split($5, a, "="); split($6, b, "="); if (a[2] != b[2]) print }' | grep -q .; then
        fail "$a-bit: a descriptor that is not the named file: $(grep '^image' "$T/log" | head -2)"
    else pass "$a-bit: each descriptor is the file it names (the caller's own open)"; fi
    grep -q "^amsi .*fd=/memfd:sg-defender-amsi" "$T/log" && pass "$a-bit: AMSI's text goes in a memfd" || fail "$a-bit: AMSI not by memfd: $(grep '^amsi' "$T/log" | head -2)"
    grep -q "uid=$(id -u) " "$T/log" && ! grep -v "uid=$(id -u) " "$T/log" | grep -q . && pass "$a-bit: the service knows the caller (SO_PEERCRED)" || fail "$a-bit: peer uids: $(cut -d' ' -f3 "$T/log" | sort -u)"
    n=$(grep -c "fd=$T/f$a/clean.exe" "$T/log")
    [ "$n" -ge 1 ] && pass "$a-bit: a clean program's start is asked about ($n)" || fail "$a-bit: the clean program was not asked about"
    grep -qi "fd=$WINEPREFIX/drive_c/windows/sys" "$T/log" && fail "$a-bit: the prefix's fake DLLs were asked about: $(grep -i "fd=$WINEPREFIX/drive_c/windows/sys" "$T/log" | head -2)" \
        || pass "$a-bit: the prefix's fake DLLs (never mapped) are not asked about"
    grep -q "fd=$(dirname "$WINE")" "$T/log" && fail "$a-bit: Wine's own builtin files were asked about: $(grep "fd=$(dirname "$WINE")" "$T/log" | head -2)" \
        || pass "$a-bit: Wine's own installed DLLs are not asked about"
done
# 2. the service gone
kill "$SP"; SP=""; fixtures "$T/g" 64
SG_DEFENDER_SOCKET="$T/gone" probe 64 "$T/g" allowed
grep -qx 'RESULT: PASS' "$T/out" && pass "SG Defender not running: everything runs" || { fail "with no service:"; grep '^FAIL' "$T/out" | head -5; }
# 3. the service stuck: allowed after the time limit
service 6; fixtures "$T/s" 64
start=$(date +%s)
SG_DEFENDER_TIMEOUT_MS=1500 probe 64 "$T/s" allowed
took=$(( $(date +%s) - start ))
grep -qx 'RESULT: PASS' "$T/out" && pass "a stuck scan: everything runs, after the time limit ($took s)" || { fail "with a stuck service:"; grep '^FAIL' "$T/out" | head -5; }
"$WINESERVER" -k 2>/dev/null
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
