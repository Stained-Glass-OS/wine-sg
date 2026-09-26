#!/usr/bin/env bash
. "$(dirname "$0")/scratch-home.sh"
# The wine-sg gate.
#
# Proves the one thing this repo exists to deliver: a single amd64 Wine that
# runs 32-bit Windows applications with no i386 Linux libraries involved.
#
# "It built" is not the claim. "A PE32 i386 binary executed, and nothing
# 32-bit-ELF was mapped while it did" is the claim, and that is what this
# checks.
set -euo pipefail

PREFIX="${PREFIX:-/opt/wine-sg}"
WINE="$PREFIX/bin/wine"
WINESERVER="$PREFIX/bin/wineserver"
WORK="${WORK:-$(mktemp -d "${TMPDIR:-/tmp}/wine-sg-gate.XXXXXX")}"
PFX="$WORK/prefix"
FAILED=0

pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; FAILED=1; }
info() { echo "info  $*"; }

cleanup() {
    local rc=$?
    WINEPREFIX="$PFX" "$WINESERVER" -k 2>/dev/null || true
    [[ -n "${KEEP_WORK:-}" ]] || rm -rf "$WORK"
    exit $rc
}
trap cleanup EXIT INT TERM

# A deliberately minimal environment: no DISPLAY, no inherited WINE* settings,
# and PATH without the system Wine, so nothing can quietly satisfy this test
# using the distribution's build instead of ours.
wine_env() {
    env -i HOME="$WORK" PATH="$PREFIX/bin:/usr/bin:/bin" \
        WINEPREFIX="$PFX" WINEDEBUG=-all \
        WINEDLLOVERRIDES='mscoree,mshtml=' \
        XDG_RUNTIME_DIR="$WORK/run" "$@"
}

[[ -x "$WINE" ]] || { fail "no wine at $WINE -- build and install first"; exit 2; }
mkdir -p "$WORK/run" && chmod 700 "$WORK/run"

# --- 1. the install has the new-WoW64 shape -------------------------------
# Old WoW64 needs a 32-bit Unix side (i386-unix, 32-bit ELF .so files). New
# WoW64 has only an x86_64 Unix side and thunks to 32-bit PE code. The absence
# of i386-unix is the structural signature, and it is worth asserting because a
# build that silently fell back would still pass a naive "does it run" test.
info "wine: $("$WINE" --version)"
for tree in i386-windows x86_64-windows x86_64-unix; do
    if [[ -d "$PREFIX/lib/wine/$tree" ]]; then
        pass "$tree present ($(ls "$PREFIX/lib/wine/$tree" | wc -l) files)"
    else
        fail "$tree missing"
    fi
done
if [[ -d "$PREFIX/lib/wine/i386-unix" ]]; then
    fail "i386-unix present -- this is an old-WoW64 build, not new WoW64"
else
    pass "no i386-unix tree (new WoW64, no 32-bit Linux side)"
fi

# --- 2. a prefix builds, with a populated syswow64 -------------------------
info "creating a prefix (takes a minute)"
wine_env "$WINE" wineboot --init >"$WORK/boot.log" 2>&1 || true
wine_env "$WINESERVER" -w

count_of() { ls "$PFX/drive_c/windows/$1" 2>/dev/null | wc -l; }
if [[ "$(count_of system32)" -gt 100 ]]; then
    pass "system32 populated ($(count_of system32) files)"
else
    fail "system32 has only $(count_of system32) files -- the prefix did not bootstrap"
    sed 's/^/      /' "$WORK/boot.log" | tail -20
    echo; echo "RESULT: FAIL"; exit 1
fi
if [[ "$(count_of syswow64)" -gt 100 ]]; then
    pass "syswow64 populated ($(count_of syswow64) files)"
else
    fail "syswow64 has only $(count_of syswow64) files -- no 32-bit side"
fi

# --- 3. the 32-bit binary really is 32-bit --------------------------------
WOW_CMD="$PFX/drive_c/windows/syswow64/cmd.exe"
if [[ -f "$WOW_CMD" ]] && file -b "$WOW_CMD" | grep -q 'PE32 executable.*i386'; then
    pass "syswow64/cmd.exe is PE32 i386: $(file -b "$WOW_CMD" | sed 's/, for MS Windows.*//')"
else
    fail "syswow64/cmd.exe is not a 32-bit PE: $(file -b "$WOW_CMD" 2>/dev/null || echo missing)"
    echo; echo "RESULT: FAIL"; exit 1
fi

# --- 4. it executes -------------------------------------------------------
OUT=$(wine_env "$WINE" 'C:\windows\syswow64\cmd.exe' /c "echo SG_WOW64_OK" 2>/dev/null | tr -d '\r')
if grep -q SG_WOW64_OK <<<"$OUT"; then
    pass "32-bit Windows binary executed"
else
    fail "32-bit binary produced no output"
    info "got: $OUT"
fi

# --- 5. and it did so without any i386 Linux library ----------------------
# The claim this repo makes is not "32-bit works" but "32-bit works on a pure
# amd64 system". Anything mapped from i386-linux-gnu, or any 32-bit ELF, would
# falsify that -- so look at the live process rather than trusting the build.
wine_env "$WINE" 'C:\windows\syswow64\cmd.exe' /c "ping -n 30 127.0.0.1" >/dev/null 2>&1 &
BG=$!
for _ in $(seq 1 30); do
    PID=$(pgrep -f 'syswow64\\cmd.exe' | head -1) && [[ -n "$PID" ]] && break
    sleep 1
done

if [[ -n "${PID:-}" ]] && [[ -r "/proc/$PID/maps" ]]; then
    MAPPED=$(awk '{print $6}' "/proc/$PID/maps" | grep -E '^/' | sort -u)
    I386_COUNT=$(grep -c 'i386-linux-gnu' <<<"$MAPPED" || true)
    ELF32_COUNT=0
    while read -r f; do
        [[ -f "$f" ]] || continue
        file -b "$f" 2>/dev/null | grep -q 'ELF 32-bit' && ELF32_COUNT=$((ELF32_COUNT + 1))
    done <<<"$MAPPED"

    if [[ "$I386_COUNT" -eq 0 && "$ELF32_COUNT" -eq 0 ]]; then
        pass "no i386 Linux libraries mapped while running 32-bit code"
        info "ELF objects mapped: $(grep -c '\.so' <<<"$MAPPED" || true), all 64-bit"
    else
        fail "32-bit Linux objects were mapped: $I386_COUNT from i386-linux-gnu, $ELF32_COUNT 32-bit ELF"
        grep 'i386-linux-gnu' <<<"$MAPPED" | sed 's/^/      /' | head
    fi
else
    fail "could not find the running 32-bit process to inspect"
fi
kill "$BG" 2>/dev/null || true
pkill -f 'syswow64\\cmd.exe' 2>/dev/null || true

echo
if [[ "$FAILED" -eq 0 ]]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$FAILED"
