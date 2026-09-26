#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A custom action that raises an exception (patches/sg/0304).
#
# Foxit PDF Reader's silent install never returned: a C++ exception escaped
# one of its DLL custom actions, Wine's custom action server caught only page
# faults, and the unhandled exception started the debugger, whose crash
# dialog waited for a click nobody could give. Any exception is now caught,
# logged and the install goes on, as a page fault already did.
#
# The gate builds a package (test/msica/) whose ThrowCA raises 0xe06d7363
# and whose MarkCA, sequenced after it, writes C:\ca-marker.txt; `msiexec /i
# /qn` must finish (rc 0) with the marker written. Under xvfb, with the
# crash dialog allowed, as a user's session has it.
#
#   WINE=/opt/wine-sg/bin/wine test/msica-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in xvfb-run "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-msica.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" \
    XDG_DESKTOP_DIR="$T/home/Desktop"
mkdir -p "$HOME/Desktop"
export WINEDLLOVERRIDES="winemenubuilder.exe=d${WINEDLLOVERRIDES:+;$WINEDLLOVERRIDES}"

"$MINGW" -O2 -shared -o "$T/ca.dll" "$HERE/msica/ca.c" "$HERE/msica/ca.def" -lmsi &&
    "$MINGW" -O2 -o "$T/build.exe" "$HERE/msica/build.c" -lmsi || { echo "FAIL  probes did not build"; exit 1; }

export WINEPREFIX="$T/prefix" WINEDEBUG=-all
"$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/ca.dll" "$T/build.exe" "$WINEPREFIX/drive_c/"

cat > "$T/session.sh" <<EOF
#!/bin/sh
case "\$DISPLAY" in :0|:0.*) echo "refusing DISPLAY :0"; exit 1 ;; esac
cd "$WINEPREFIX/drive_c"
"$WINE" build.exe 'C:\\ca.msi' 'C:\\ca.dll' > "$T/build.out" 2>&1
timeout -s KILL 180 "$WINE" msiexec /i 'C:\\ca.msi' /qn > "$T/msi.out" 2>&1
echo "rc=\$?" >> "$T/msi.out"
EOF
chmod +x "$T/session.sh"
timeout -s KILL 400 xvfb-run -a -s '-screen 0 1024x768x24' "$T/session.sh" >/dev/null 2>&1

grep -q built "$T/build.out" 2>/dev/null && pass "the package builds" || fail "package: $(cat "$T/build.out" 2>/dev/null)"
rc=$(sed -n 's/^rc=//p' "$T/msi.out" 2>/dev/null)
[ "$rc" = 0 ] && pass "msiexec /qn finishes (rc $rc) past a custom action that raised an exception" ||
    fail "msiexec rc=${rc:-none} (137: killed after 180 s -- the install hung)"
[ "$(cat "$WINEPREFIX/drive_c/ca-marker.txt" 2>/dev/null)" = marked ] &&
    pass "the custom action after it ran" || fail "the custom action after the exception never ran"

[ $RC = 0 ] && echo "msica-gate: all passed" || echo "msica-gate: FAILURES"
exit $RC
