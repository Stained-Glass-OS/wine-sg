#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Credential Manager keeps credentials in the person's keyring
# (patches/sg/1380).
#
# CredWrite stored them in the user's registry hive (Software\Wine\Credential
# Manager), only RC4-scrambled with a key kept beside them; anyone who could
# read the hive could read every saved password. Now a program's credential
# goes to the keyring of the Unix account it runs as (gnome-keyring's Secret
# Service on the session bus, encrypted with the sign-in password). In a
# session-like bus with an open keyring:
#
#   - CredWrite: secret-tool finds the item (by the folded target name and the
#     type), its secret the one written; nothing in the registry; the
#     keyring file on disk does not hold it in the clear
#   - CredRead round-trips it (the target in another case, as Windows
#     matches), from a 64- and a 32-bit program; CredEnumerate (a filter, and
#     CRED_ENUMERATE_ALL_CREDENTIALS) lists it, aligned
#   - writing again replaces it (one item); CRED_PRESERVE_CREDENTIAL_BLOB
#     keeps the secret; CredDelete removes it from the keyring
#   - migration: what an earlier version (or a time without a keyring) left
#     in the registry moves to the keyring at the next use, and leaves the hive
#   - fallback: no session bus, or a locked keyring (no prompt, no hang), and
#     the registry keeps it, with a warning; a logon-session credential
#     (CRED_PERSIST_SESSION) stays in the registry's volatile key
#
# Mutants (dlls/advapi32/cred.c): SG_MUTANT_CRED_KEYRING (never the keyring),
# SG_MUTANT_CRED_MIGRATE (no migration) -- each fails the gate.
#
#   WINE=... test/credkeyring-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
MINGW32="${MINGW32:-i686-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" "$MINGW32" gnome-keyring-daemon secret-tool dbus-run-session gdbus iconv; do
    command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-credkeyring.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
RT="$T/run"; mkdir -p "$RT"; chmod 700 "$RT"
cleanup() {
    : > "$T/s.stop"; sleep 1
    "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/credkeyring-probe.c" -ladvapi32 &&
"$MINGW32" -municode -O2 -o "$T/probe32.exe" "$HERE/credkeyring-probe.c" -ladvapi32 || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
env -u DBUS_SESSION_BUS_ADDRESS timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/probe64.exe" "$T/probe32.exe" "$WINEPREFIX/drive_c/"

# a session: its bus, and a keyring daemon opened with the sign-in password
cat > "$T/session.sh" <<'EOF'
printf '%s' 'Gate#Pass1' | gnome-keyring-daemon --unlock --components=secrets >/dev/null 2>&1
echo "$DBUS_SESSION_BUS_ADDRESS" > "$2.tmp" && mv "$2.tmp" "$2"
while [ ! -e "$1" ]; do sleep 0.3; done
for p in $(pgrep -u "$(id -u)" -x gnome-keyring-d); do
    tr '\0' '\n' < "/proc/$p/environ" 2>/dev/null | grep -qx "XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR" && kill "$p"
done
EOF
env -i PATH=/usr/bin:/bin HOME="$HOME" XDG_RUNTIME_DIR="$RT" dbus-run-session -- sh "$T/session.sh" "$T/s.stop" "$T/s.bus" >/dev/null 2>&1 &
_w=0; while [ ! -s "$T/s.bus" ] && [ $_w -lt 100 ]; do sleep 0.1; _w=$((_w + 1)); done
BUS=$(cat "$T/s.bus" 2>/dev/null)
[ -n "$BUS" ] || { fail "no session bus came up"; exit 1; }
sleep 0.5
st() { env -i PATH=/usr/bin:/bin HOME="$HOME" XDG_RUNTIME_DIR="$RT" DBUS_SESSION_BUS_ADDRESS="$BUS" timeout 20 "$@"; }
# a probe in the session (or, with NOBUS=1, outside any)
probe() { # 64|32 ARGS...
    _b=$1; shift
    if [ "${NOBUS:-0}" = 1 ]; then
        env -u DBUS_SESSION_BUS_ADDRESS XDG_RUNTIME_DIR="$T/nobus" timeout -s KILL 60 "$WINE" "C:\\probe$_b.exe" "$@" 2>"$T/probe.err" | tr -d '\r'
    else
        DBUS_SESSION_BUS_ADDRESS="$BUS" XDG_RUNTIME_DIR="$RT" timeout -s KILL 60 "$WINE" "C:\\probe$_b.exe" "$@" 2>"$T/probe.err" | tr -d '\r'
    fi
}
kr_secret() { # KEY TYPE: the item's secret (UTF-16) as UTF-8
    st secret-tool lookup key "$1" type "$2" 2>/dev/null | iconv -f UTF-16LE -t UTF-8 2>/dev/null
}
kr_count() { st secret-tool search --all key "$1" 2>/dev/null | grep -c '^\[/'; }
REGKEY='HKCU\Software\Wine\Credential Manager'
in_registry() { "$WINE" reg query "$REGKEY\\$1" >/dev/null 2>&1; }
mkdir -p "$T/nobus"; chmod 700 "$T/nobus"

# --- CredWrite goes to the keyring ------------------------------------------------
SECRET='Gh-token-7f3a9Zq'
r=$(probe 64 write 'git:https://github.com' alice "$SECRET")
[ "$r" = OK ] && pass "CredWrite succeeds ($r)" || fail "CredWrite: '$r' $(head -c 300 "$T/probe.err")"
got=$(kr_secret 'git:https://github.com' 1)
[ "$got" = "$SECRET" ] && pass "secret-tool finds it in the keyring, the secret the one written" \
    || fail "secret-tool lookup: '$got'"
if in_registry 'Generic: git:https://github.com'; then fail "the registry has a copy"; else pass "the registry has no copy"; fi
KF="$HOME/.local/share/keyrings/login.keyring"
if [ -f "$KF" ] && ! grep -q "$SECRET" "$KF" && ! iconv -f UTF-8 -t UTF-16LE <<EOF | grep -qF -f - "$KF" 2>/dev/null
$SECRET
EOF
then pass "the keyring file on disk does not hold it in the clear"
else fail "no keyring file, or the secret is in it in the clear"; fi

# --- CredRead, CredEnumerate --------------------------------------------------------
r=$(probe 64 read 'GIT:https://GitHub.com')
case "$r" in "OK target=git:https://github.com user=alice secret=$SECRET persist=2 type=1 comment=yes written=yes") pass "CredRead round-trips it, whatever the case of the target ($r)" ;;
    *) fail "CredRead: '$r'" ;; esac
r=$(probe 32 read 'git:https://github.com')
case "$r" in "OK target=git:https://github.com user=alice secret=$SECRET"*) pass "... and from a 32-bit program" ;;
    *) fail "32-bit CredRead: '$r' $(head -c 300 "$T/probe.err")" ;; esac
probe 64 write 'web:example.org' bob 'Web#Pass2' >/dev/null
r=$(probe 64 enum 'git:*')
case "$r" in "COUNT 1
git:https://github.com|alice|$SECRET") pass "CredEnumerate with a filter lists it" ;; *) fail "enum git:*: '$r'" ;; esac
r=$(probe 64 enumall)
case "$r" in *MISALIGNED*) fail "enumerated credentials misaligned: $r" ;;
    "COUNT 2"*"git:https://github.com|alice|$SECRET"*) case "$r" in *"web:example.org|bob|Web#Pass2"*) pass "CRED_ENUMERATE_ALL_CREDENTIALS lists both, aligned" ;; *) fail "enumall: '$r'" ;; esac ;;
    *) fail "enumall: '$r'" ;; esac
r=$(probe 32 enumall)
case "$r" in "COUNT 2"*"web:example.org|bob|Web#Pass2"*) pass "... and from a 32-bit program" ;; *) fail "32-bit enumall: '$r'" ;; esac

# --- rewrite, preserve, delete ---------------------------------------------------------
probe 64 write 'git:https://github.com' carol 'New#Token3' >/dev/null
n=$(kr_count 'git:https://github.com')
r=$(probe 64 read 'git:https://github.com')
case "$r:$n" in "OK target=git:https://github.com user=carol secret=New#Token3"*":1") pass "writing it again replaces it (one item)" ;;
    *) fail "rewrite: '$r', $n items" ;; esac
r=$(probe 64 keep 'git:https://github.com' dave)
r2=$(probe 64 read 'git:https://github.com')
case "$r2" in "OK target=git:https://github.com user=dave secret=New#Token3"*) pass "CRED_PRESERVE_CREDENTIAL_BLOB keeps the secret ($r)" ;;
    *) fail "preserve: '$r' then '$r2'" ;; esac
r=$(probe 64 delete 'git:https://github.com')
r2=$(probe 64 read 'git:https://github.com')
got=$(kr_secret 'git:https://github.com' 1)
case "$r:$r2:$got" in "OK:ERR 1168:") pass "CredDelete removes it from the keyring" ;; *) fail "delete: '$r', then '$r2', keyring '$got'" ;; esac

# --- fallback: no session bus ---------------------------------------------------------------
r=$(NOBUS=1 WINEDEBUG=-all,err+cred probe 64 write 'legacy:server' eve 'Old#Pass4' 2)
in_registry 'DomPasswd: legacy:server' && reg=yes || reg=no
if [ "$r" = OK ] && [ "$reg" = yes ] && [ -z "$(kr_secret 'legacy:server' 2)" ]; then
    pass "with no session bus the registry keeps it"
else fail "no bus: '$r', in the registry: $reg"; fi
grep -q 'no keyring to keep credentials in' "$T/probe.err" && pass "... with a warning" || fail "no warning: $(head -c 300 "$T/probe.err")"
r=$(NOBUS=1 probe 64 read 'legacy:server' 2)
case "$r" in "OK target=legacy:server user=eve secret=Old#Pass4"*) pass "... and reads it back from there" ;; *) fail "no-bus read: '$r'" ;; esac

# --- migration -------------------------------------------------------------------------------
r=$(probe 64 read 'LEGACY:server' 2)
got=$(kr_secret 'legacy:server' 2)
in_registry 'DomPasswd: legacy:server' && reg=yes || reg=no
if [ "$got" = 'Old#Pass4' ] && [ "$reg" = no ]; then pass "migration: at the next use with a keyring it moves there and leaves the registry"
else fail "migration: keyring '$got', still in the registry: $reg"; fi
case "$r" in "OK target=legacy:server user=eve secret=Old#Pass4"*) pass "... and reads the same" ;; *) fail "migrated read: '$r'" ;; esac

# --- a logon-session credential stays in the registry ---------------------------------------
r=$(probe 64 write 'session:only' frank 'Sess#5' 1 1)
[ "$r" = OK ] && [ -z "$(kr_secret 'session:only' 1)" ] && in_registry 'Generic: session:only' \
    && pass "a logon-session credential (CRED_PERSIST_SESSION) stays in the registry's volatile key" \
    || fail "session credential: '$r', keyring '$(kr_secret 'session:only' 1)'"

# --- fallback: a locked keyring, no prompt ------------------------------------------------
st gdbus call --session --dest org.freedesktop.secrets --object-path /org/freedesktop/secrets \
    --method org.freedesktop.Secret.Service.Lock "['/org/freedesktop/secrets/collection/login']" >/dev/null 2>&1
start=$(date +%s)
r=$(probe 64 write 'locked:target' gina 'Lock#6')
took=$(( $(date +%s) - start ))
in_registry 'Generic: locked:target' && reg=yes || reg=no
if [ "$r" = OK ] && [ "$reg" = yes ] && [ "$took" -lt 30 ]; then pass "a locked keyring: the registry keeps it, at once, no prompt (${took}s)"
else fail "locked keyring: '$r', in the registry: $reg, ${took}s"; fi

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
