#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# An out-of-context WinEvent hook on another process's thread, without a module (wine-sg 8403),
# as Windows allows: wineserver refused it ("module is optional only if hook is in current
# process"), which is right only for hooks that run in the hooked thread. mpv embedded in SMPlayer
# (--wid) follows the size of SMPlayer's video widget through EVENT_OBJECT_LOCATIONCHANGE from
# SMPlayer's thread; without the hook its picture kept the widget's first size (491 of 654 pixels
# wide, the rest black).
# test/xwinevent-probe.c, 64- and 32-bit: a second process makes a window; the probe hooks that
# thread out of context with no module, the other process resizes its child window three times,
# the probe's thread receives the events; an in-context hook into it without a module still fails.
#   WINE=/opt/wine-sg/bin/wine test/xwinevent-gate.sh
# Mutant: -DSG_MUTANT_NO_XPROC_WINEVENT (server/hook.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xwinevent.XXXXXX)
XP=
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill $XP 2>/dev/null; rm -rf "$T"' EXIT INT TERM
for a in x86_64 i686; do
    TMPDIR=/var/tmp $a-w64-mingw32-gcc -O1 -o "$T/probe-$a.exe" "$HERE/xwinevent-probe.c" -luser32 || { fail "$a probe did not build"; exit 1; }
done
# windows need a display: an X server of the gate's own
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for a in x86_64 i686; do
    cp "$T/probe-$a.exe" "$WINEPREFIX/drive_c/"
    out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" "C:\\probe-$a.exe" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $a: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^$1=1"; then pass "$a: $2"
        else fail "$a: $2 ($(printf '%s\n' "$out" | grep "^$1=" | head -1 || echo none))"; fi
    }
    check start      "the other process and its window"
    check hook       "SetWinEventHook out of context on another process's thread, no module"
    check incontext  "in context without a module into another process is still refused"
    check delivered  "its EVENT_OBJECT_LOCATIONCHANGEs reach the hooking thread"
    check done        "the probe ran to the end"
done

# --- 8404: another user's, session's or desktop's thread is out of reach -----------------------
# A prefix several Unix users share (as Stained Glass OS's): SYSTEM (SG_SYSTEM) owns it, SG_USER1
# shares session 1 with SYSTEM, SG_USER2 gets session 2. Hooks on another process's thread (out of
# context with no module, and in context with this module) only on the hooker's own desktop, in its
# session, for its own user. Mutants (server/hook.c): -DSG_MUTANT_HOOK_ANY_USER,
# -DSG_MUTANT_HOOK_ANY_DESKTOP (the "another desktop" case), -DSG_MUTANT_HOOK_ANY_SESSION (caught
# together with ANY_DESKTOP: each session has window stations of its own, so a thread of another
# session is on another desktop too), -DSG_MUTANT_HOOK_ANY_USER (the "sysuser" case: SYSTEM in the
# target's session and desktop, which UIPI does not stop).
SG_SYSTEM=${SG_SYSTEM:-sgsystem}; SG_USER1=${SG_USER1:-sgtest1}; SG_USER2=${SG_USER2:-sgtest2}; SG_GROUP=${SG_GROUP:-sgwine}
xok=1
for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do id "$u" >/dev/null 2>&1 && sudo -n -u "$u" true 2>/dev/null || xok=0; done
if [ $xok = 0 ]; then echo "SKIP  cross-user cases: no $SG_SYSTEM/$SG_USER1/$SG_USER2 or no sudo"
else
    X=$(mktemp -d /var/tmp/sg-xwinevent-x.XXXXXX); chmod 755 "$X"
    XPFX=$X/prefix
    Xvfb -displayfd 3 -ac -screen 0 1024x768x24 -nolisten tcp 3>"$X/display" >/dev/null 2>&1 & XP2=$!
    i=0; while [ ! -s "$X/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    XD=":$(cat "$X/display")"
    for u in "$SG_SYSTEM" "$SG_USER1" "$SG_USER2"; do sudo -n install -d -o "$u" -m 700 "$X/home-$u"; done
    sudo -n install -d -o "$SG_SYSTEM" -g "$SG_GROUP" -m 2770 "$XPFX"
    sudo -n -u "$SG_SYSTEM" touch "$XPFX/.sg-system-prefix"
    mkdir -m 1777 "$X/ids"; cp "$T/probe-x86_64.exe" "$X/probe.exe"; chmod 755 "$X/probe.exe"
    xas() {   # USER [ENV=...] ARGS...: run wine as USER in the shared prefix
        local u=$1; shift
        sudo -n -u "$u" env DISPLAY="$XD" WINEPREFIX="$XPFX" WINESERVER="$WINESERVER" WINEDEBUG=-all \
            WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" HOME="$X/home-$u" XDG_RUNTIME_DIR="$X/home-$u" "$@"
    }
    xas "$SG_SYSTEM" "$WINE" wineboot -i >/dev/null 2>&1; xas "$SG_SYSTEM" "$WINESERVER" -w
    # each account's first program sets up its profile (a while): before the timed cases
    for u in "$SG_USER1" "$SG_USER2"; do xas "$u" timeout -s KILL 120 "$WINE" cmd /c exit >/dev/null 2>&1; done
    zx() { printf 'Z:%s' "$(printf '%s' "$1" | sed 's#/#\\#g')"; }
    P=$(zx "$X/probe.exe")
    xcase() {   # NAME TARGETUSER TARGETENV TARGETDESKTOP HOOKUSER [HOOKDESKTOP] : the hooker's KEY=V line
        local f="$X/ids/$1" out
        rm -f "$f"
        xas "$2" $3 timeout -s KILL 40 "$WINE" "$P" xtarget "$(zx "$f")" ${4:-} >/dev/null 2>&1 & tp=$!
        i=0; while [ ! -s "$f" ] && [ $i -lt 120 ]; do sleep 0.25; i=$((i + 1)); done
        out=$(xas "$5" ${XHOOKENV:-} timeout -s KILL 40 "$WINE" "$P" xhook "$(zx "$f")" ${6:-} 2>/dev/null | tr -d '\r' | tr '\n' ' ')
        wait $tp
        printf '%s\n' "$out"
    }
    xv() { printf '%s' "$1" | grep -o "$2=[0-9]*" | cut -d= -f2; }
    # both on a desktop of their own making (one with no shell on it, as the gate has none)
    o=$(xcase same "$SG_USER1" "" sg-xwinevent-same "$SG_USER1" sg-xwinevent-same)
    [ "$(xv "$o" xhook)$(xv "$o" xmodhook)$(xv "$o" xevents)" = 111 ] \
        && pass "the same user's other program on the same desktop: both hooks, its events arrive (as before)" \
        || fail "same user: '$o'"
    o=$(xcase user "$SG_SYSTEM" "" "" "$SG_USER1")
    [ "$(xv "$o" xhook)$(xv "$o" xmodhook)$(xv "$o" xevents)" = 000 ] \
        && pass "another user's thread in the same session and desktop (SYSTEM's): both hooks refused, no events" \
        || fail "another user, same session/desktop: '$o'"
    o=$(xcase desktop "$SG_USER1" "" sg-xwinevent-other "$SG_USER1")
    [ "$(xv "$o" xhook)$(xv "$o" xmodhook)$(xv "$o" xevents)" = 000 ] \
        && pass "the same user's thread on another desktop: both hooks refused, no events" \
        || fail "another desktop: '$o'"
    o=$(xcase session "$SG_USER2" "" "" "$SG_USER1")
    [ "$(xv "$o" xhook)$(xv "$o" xmodhook)$(xv "$o" xevents)" = 000 ] \
        && pass "another user's thread in another session: both hooks refused, no events" \
        || fail "another user, another session: '$o'"
    o=$(xcase sysession "$SG_SYSTEM" "SG_SESSION_UID=$(id -u "$SG_USER2")" "" "$SG_SYSTEM")
    [ "$(xv "$o" xhook)$(xv "$o" xmodhook)$(xv "$o" xevents)" = 000 ] \
        && pass "the same user's (SYSTEM's) thread in another session: both hooks refused" \
        || fail "same user, another session: '$o'"
    # SYSTEM started in SG_USER2's session (SG_SESSION_UID, as sg-elevated-run starts programs):
    # same session and desktop, above the target's integrity (UIPI lets it), another user
    o=$(XHOOKENV="SG_SESSION_UID=$(id -u "$SG_USER2")" xcase sysuser "$SG_USER2" "" "" "$SG_SYSTEM")
    [ "$(xv "$o" xhook)$(xv "$o" xmodhook)$(xv "$o" xevents)" = 000 ] \
        && pass "another user (SYSTEM) in the target's session and desktop, UIPI allowing: both hooks refused, no events" \
        || fail "another user, same session/desktop, UIPI allowing: '$o'"
    xas "$SG_SYSTEM" "$WINESERVER" -k 2>/dev/null; sleep 1
    kill $XP2 2>/dev/null
    sudo -n rm -rf "$X"
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
