#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# subst (patch 3293): add / list / remove a virtual drive, and the error answers, run inside one cmd
# session of a scratch prefix (the mapping lives in the wineserver, as it does until logoff on Windows).
#
#   WINE=/opt/wine-sg/bin/wine test/fastsubst-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (programs/subst): SG_MUTANT_SUBST_* (see work/muts-subst.sh).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastsubst.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$WINEPREFIX/drive_c/sdir/sub"
echo hello > "$WINEPREFIX/drive_c/sdir/f.txt"
echo nested > "$WINEPREFIX/drive_c/sdir/sub/g.txt"
cat > "$WINEPREFIX/drive_c/t.bat" <<'EOB'
@echo off
echo ---list0
subst
echo ---add
subst X: C:\sdir
echo rc=%errorlevel%
echo ---list1
subst
echo ---read
type X:\f.txt
echo ---again
subst X: C:\sdir
echo rc=%errorlevel%
echo ---nopath
subst Y: C:\nonexistent
echo rc=%errorlevel%
echo ---notdrive
subst Y C:\sdir
echo rc=%errorlevel%
echo ---real
subst C: C:\sdir
echo rc=%errorlevel%
echo ---onsubst
subst W: X:\sub
echo rc=%errorlevel%
subst
echo ---nested
type W:\g.txt
dir /b X:\
echo ---deleteunknown
subst V: /D
echo rc=%errorlevel%
echo ---deletereal
subst C: /D
echo rc=%errorlevel%
echo ---delete
subst X: /D
echo rc=%errorlevel%
subst W: /d
echo rc=%errorlevel%
echo ---list2
subst
echo ---gone
type X:\f.txt
echo ---help
subst /?
echo ---end
EOB
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 120 "$WINE" cmd /c 'C:\\t.bat' 2>/dev/null </dev/null | tr -d '\r' > "$T/out.txt"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
fails=0
# section NAME: text between ---NAME and the next ---
sec() { awk -v n="---$1" '$0==n{f=1;next} /^---/{f=0} f' "$T/out.txt"; }
check() { # name "section" pattern
    if sec "$2" | grep -q -- "$3"; then echo "PASS  $1"; else echo "FAIL  $1"; fails=$((fails+1)); sec "$2" | sed 's/^/      /'; fi
}
nocheck() {
    if sec "$2" | grep -q -- "$3"; then echo "FAIL  $1"; fails=$((fails+1)); sec "$2" | sed 's/^/      /'; else echo "PASS  $1"; fi
}
nocheck "no substitution listed at start" list0 ':\\: =>'
check "subst X: C:\\sdir succeeds" add 'rc=0'
check "listing shows X:\\: => C:\\sdir" list1 '^X:\\: => C:\\sdir$'
nocheck "mounted drives are not listed as substitutions" list1 '^[CZ]:\\: =>'
check "the drive reads the files" read 'hello'
check "second subst of the same letter" again 'rc=1'
check "second subst says already substed" again 'Drive already SUBSTed'
check "missing path" nopath 'Path not found - C:\\nonexistent'
check "missing path fails" nopath 'rc=1'
check "bad drive spec" notdrive 'Invalid parameter - Y'
check "mounted drive cannot be substituted" real 'Invalid parameter - C:'
check "subst of a path on a substituted drive" onsubst 'rc=0'
check "listing resolves it to the real path" onsubst '^W:\\: => C:\\sdir\\sub$'
check "a drive made on a substituted drive reads the files" nested 'nested'
check "directory listing of the drive" nested '^sub$'
check "deleting an unknown drive" deleteunknown 'Invalid parameter - V:'
check "deleting an unknown drive fails" deleteunknown 'rc=1'
check "deleting a mounted drive is refused" deletereal 'Invalid parameter - C:'
check "subst X: /D succeeds" delete 'rc=0'
check "lower case /d works" delete 'rc=0'
nocheck "nothing listed after delete" list2 ':\\: =>'
nocheck "drive is gone" gone 'hello'
check "help text" help 'SUBST drive1: /D'
check "help keeps its indentation" help '^  /D             Deletes'
if [ $fails = 0 ]; then echo "RESULT: PASS"; exit 0; fi
echo "RESULT: FAIL"; cat "$T/out.txt"; exit 1
