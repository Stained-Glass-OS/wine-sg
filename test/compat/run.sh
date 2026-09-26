#!/bin/sh
# The application compatibility suite (test/compat/apps.list).
#
# For each application, in a fresh prefix joined to a shell desktop under
# xvfb (as a session has it): download (cached, hash-checked), install
# silently, check the program is there, run a smoke command, launch it and
# wait for its main window, screenshot it, close it. Writes results.md and
# results.tsv (and screenshots, logs) to ARTIFACTS.
#
#   WINE=/opt/wine-sg/bin/wine test/compat/run.sh [NAME-SUBSTRING...]
#   ARTIFACTS=DIR (default build/compat-results)  CACHE=DIR (~/.cache/sg-compat)
#   OFFLINE=1: use only what is cached
#   LAUNCH_DEBUG=channels: WINEDEBUG for the launch only (into the log)
#   SG_DEFAULTS=DIR: import these .reg defaults first (sg-shell's theme/), as
#   an installed system has them; KEEP_PREFIX=1 keeps each prefix in ARTIFACTS
#   DXVK_TGZ=FILE: the DXVK release the image stages in /opt/sg-d3d (default:
#   sg-image's cached one, else /opt/sg-d3d/dxvk), for entries with "dxvk"
#   among their registry values -- installed as sg-install-d3d does
#
# Not part of `make test`: it needs the network and takes a while. Exit
# status is the number of applications with a failing stage.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
CACHE="${CACHE:-$HOME/.cache/sg-compat}"
ARTIFACTS="${ARTIFACTS:-$HERE/../../build/compat-results}"
export WINESERVER
for t in "$MINGW" xvfb-run import sha256sum curl unzip; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
mkdir -p "$CACHE" "$ARTIFACTS"
ARTIFACTS=$(cd "$ARTIFACTS" && pwd)
T=$(mktemp -d /var/tmp/sg-compat.XXXXXX)
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -municode -O2 -o "$T/compat-probe.exe" "$HERE/compat-probe.c" || { echo "probe did not build"; exit 1; }
# MSIX packages are added through the Store's PackageManager (msix-probe add)
"$MINGW" -O2 -o "$T/msix-probe.exe" "$HERE/../msix-probe.c" -lruntimeobject -lshlwapi 2>/dev/null || true
DXVK_TGZ="${DXVK_TGZ:-$HERE/../../../sg-image/build/d3d-cache/dxvk-3.1.1.tar.gz}"
[ -f "$DXVK_TGZ" ] || DXVK_TGZ="$HOME/.cache/sg-compat/dxvk-3.1.1.tar.gz"

# itch.io's free downloads: "itch:USER/GAME/UPLOAD-ID" -- the page's CSRF
# token buys a short-lived signed URL for the upload
itch_url() {
    ck="$T/itch.cookies"; rm -f "$ck"
    u=${1#itch:}; user=${u%%/*}; rest=${u#*/}; game=${rest%%/*}; id=${rest#*/}
    tok=$(curl -sL -A Mozilla/5.0 -c "$ck" -b "$ck" "https://$user.itch.io/$game" | grep -oE 'name="csrf_token" value="[^"]+"' | head -1 | sed 's/.*value="//;s/"$//')
    curl -s -A Mozilla/5.0 -c "$ck" -b "$ck" -X POST --data-urlencode "csrf_token=$tok" \
        "https://$user.itch.io/$game/file/$id?source=view_game&as_props=1&after_download_lightbox=true" |
        sed -n 's/^{"url":"\([^"]*\)".*/\1/p' | sed 's,\\/,/,g'
}

TSV="$ARTIFACTS/results.tsv"
printf 'app\tdownload\tinstall\tfiles\tsmoke\tlaunch\tclose\tnotes\n' > "$TSV"
FAILED=0

slug() { printf '%s' "$1" | tr -c 'A-Za-z0-9.' '-' | tr -s '-' | sed 's/-$//'; }

grep -v '^#' "$HERE/apps.list" | grep -v '^$' | while IFS='|' read -r name file sha url kind iargs prog largs smoke expect prep; do
    if [ $# -gt 0 ]; then
        hit=0; for f in "$@"; do case "$name" in *"$f"*) hit=1 ;; esac; done
        [ $hit = 1 ] || continue
    fi
    s=$(slug "$name"); L="$ARTIFACTS/$s.log"; : > "$L"
    dl=- inst=- files=- sm=- la=- cl=- notes=""
    echo "== $name" | tee -a "$L"

    # download; "OUTER.zip!INNER" is an installer shipped inside a zip (the
    # hash is the zip's)
    inner=""; case "$file" in *!*) inner=${file#*!}; file=${file%%!*} ;; esac
    case "$url" in itch:*) [ -s "$CACHE/$file" ] || [ "${OFFLINE:-0}" = 1 ] || url=$(itch_url "$url") ;; esac
    if [ ! -s "$CACHE/$file" ] && [ "${OFFLINE:-0}" != 1 ]; then curl -sL --max-time 1800 -o "$CACHE/$file.part" "$url" && mv "$CACHE/$file.part" "$CACHE/$file"; fi
    if [ -s "$CACHE/$file" ] && [ "$(sha256sum "$CACHE/$file" | cut -d' ' -f1)" = "$sha" ]; then dl=PASS; else dl=FAIL; notes="hash/download"; fi

    if [ $dl = PASS ]; then
        P="$T/$s"; [ "${KEEP_PREFIX:-0}" = 1 ] && P="$ARTIFACTS/prefix-$s"
        rm -rf "$P"; mkdir -p "$P"
        export WINEPREFIX="$P" WINEDEBUG="${COMPAT_DEBUG:--all}"
        timeout -s KILL 600 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
        "$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
        "$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1
        # the system's registry defaults (fonts, theme), as a Stained Glass
        # session has them: SG_DEFAULTS=DIR of .reg files (sg-shell's theme/)
        for r in ${SG_DEFAULTS:+"$SG_DEFAULTS"/*.reg}; do
            [ -f "$r" ] && "$WINE" regedit /S "$("$WINE" winepath -w "$r" 2>/dev/null | tr -d '\r')" >/dev/null 2>&1
        done
        # registry values the application needs before installing (prep);
        # "dxvk": DXVK's PE DLLs and native overrides, as the image has them
        # (each on a line of its own: read skips a last line without a newline,
        # which had every entry's values unapplied)
        printf '%s\n' "$prep" | tr ';' '\n' | while IFS='!' read -r rk rn rt rd; do
            if [ "$rk" = dxvk ]; then
                rm -rf "$T/dxvk"; mkdir -p "$T/dxvk"
                if [ -f "$DXVK_TGZ" ]; then tar -C "$T/dxvk" --strip-components=1 -xzf "$DXVK_TGZ"
                elif [ -d /opt/sg-d3d/dxvk ]; then cp -r /opt/sg-d3d/dxvk/. "$T/dxvk/"; fi
                for d in "$T"/dxvk/x64/*.dll; do [ -f "$d" ] && cp "$d" "$P/drive_c/windows/system32/"; done
                for d in "$T"/dxvk/x32/*.dll; do [ -f "$d" ] && cp "$d" "$P/drive_c/windows/syswow64/"; done
                for d in "$T"/dxvk/x64/*.dll; do
                    [ -f "$d" ] && "$WINE" reg add 'HKLM\Software\Wine\DllOverrides' /v "$(basename "$d" .dll)" /d native /f >/dev/null 2>&1
                done
                # DXVK writes a log per API it serves: which one ran is in the notes
                mkdir -p "$P/drive_c/dxvk-logs"
                continue
            fi
            [ -n "$rk" ] && "$WINE" reg add "$rk" /v "$rn" /t "$rt" /d "$rd" /f >/dev/null 2>&1
        done
        "$WINESERVER" -w
        cp "$T/compat-probe.exe" "$P/drive_c/"
        [ -f "$T/msix-probe.exe" ] && cp "$T/msix-probe.exe" "$P/drive_c/"
        inst_path="$CACHE/$file"
        if [ -n "$inner" ]; then
            rm -rf "$T/unzip"; mkdir -p "$T/unzip"
            unzip -q -o "$CACHE/$file" "$inner" -d "$T/unzip" && inst_path="$T/unzip/$inner"
        fi
        # kind zip: a portable program (most games), unpacked into C:\Games
        if [ "$kind" = zip ]; then
            mkdir -p "$P/drive_c/Games"
            unzip -q -o "$CACHE/$file" -d "$P/drive_c/Games/$s"
        fi
        inst_file=$("$WINE" winepath -w "$inst_path" 2>/dev/null | tr -d '\r')
        tray=0; accept=0
        case "$prog" in accept:*) accept=1; prog=${prog#accept:} ;; esac
        case "$prog" in tray:*) tray=1; prog=${prog#tray:} ;; cli:*) tray=2; prog=${prog#cli:} ;; esac
        cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$P/drive_c"
W() { "$WINE" "\$@" >> "$T/r" 2>>"$L"; }   # a file, not a pipe: the program started holds it open
WINEDEBUG=err+all,trace+explorer "$WINE" explorer /desktop=shell,1280x800 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
if [ "$kind" = msi ]; then
    timeout -s KILL 1500 "$WINE" msiexec /i "$inst_file" /qn $iargs >>"$L" 2>&1; echo "install_rc=\$?" > "$T/r"
elif [ "$kind" = zip ]; then
    echo "install_rc=0" > "$T/r"
elif [ "$kind" = msix ]; then
    timeout -s KILL 1500 "$WINE" msix-probe.exe add "$inst_file" >>"$L" 2>&1; echo "install_rc=\$?" > "$T/r"
else
    timeout -s KILL 1500 "$WINE" "$inst_file" $iargs >>"$L" 2>&1; echo "install_rc=\$?" > "$T/r"
fi
sleep 5
prog_unix=\$("$WINE" cmd /c echo "$prog" 2>/dev/null | tr -d '\r"')
prog_unix=\$("$WINE" winepath -u "\$prog_unix" 2>/dev/null | tr -d '\r')
[ -f "\$prog_unix" ] && echo "files=1" >> "$T/r" || echo "files=0 (\$prog_unix)" >> "$T/r"
if [ -n '$smoke' ]; then
    sp=\$("$WINE" winepath -u '${smoke%%::*}' 2>/dev/null | tr -d '\r')
    timeout -s KILL 120 "$WINE" "\$sp" ${smoke#*::} > "$T/smoke" 2>&1
    echo "smoke_out=\$(tr -d '\r' < "$T/smoke" | tr '\n' ' ' | head -c 4000)" >> "$T/r"
fi
if [ $tray = 2 ]; then
    :   # a command-line program: its smoke command is the test
elif [ $tray = 1 ]; then
    ( "$WINE" "\$prog_unix" >>"$L" 2>&1 & ) ; sleep 25
    W compat-probe.exe alive "\$(basename "\$prog_unix")"
    import -window root "$ARTIFACTS/$s.png"
else
    SG_ACCEPT_DIALOG=$accept DXVK_LOG_PATH='C:\dxvk-logs' WINEDEBUG="${LAUNCH_DEBUG:-\$WINEDEBUG}" W compat-probe.exe launch 240 "$prog" $largs
    import -window root "$ARTIFACTS/$s.png"
    hw=\$(sed -n 's/^window=\(0x[0-9a-f]*\).*/\1/p' "$T/r")
    [ -n "\$hw" ] || W compat-probe.exe list
    [ -n "\$hw" ] && W compat-probe.exe close \$hw
fi
EOF
        chmod +x "$T/session.sh"; : > "$T/r"
        timeout -s KILL 2400 xvfb-run -a -s '-screen 0 1280x800x24' "$T/session.sh" >>"$L" 2>&1
        tr -d '\r' < "$T/r" > "$T/r2"; mv "$T/r2" "$T/r"; cat "$T/r" >> "$L"
        rc=$(sed -n 's/^install_rc=//p' "$T/r")
        case "$rc" in 0|3010) inst=PASS ;; *) inst=FAIL; notes="$notes install rc=$rc" ;; esac
        grep -q '^files=1' "$T/r" && files=PASS || { files=FAIL; notes="$notes $(grep '^files=' "$T/r")"; }
        if [ -n "$smoke" ]; then
            if grep -q "^smoke_out=.*$expect" "$T/r"; then sm=PASS; else sm=FAIL; notes="$notes smoke: $(sed -n 's/^smoke_out=//p' "$T/r" | head -c 100)"; fi
        fi
        if [ $tray = 2 ]; then
            :
        elif [ $tray = 1 ]; then
            grep -q '^alive=1' "$T/r" && la=PASS || { la=FAIL; notes="$notes not running"; }
        elif grep -q '^window=0x' "$T/r"; then
            la=PASS; notes="$notes $(sed -n 's/^window=0x[0-9a-f]* //p' "$T/r")"
            grep -q '^closed=1' "$T/r" && cl=PASS || cl=FAIL
            grep -q '^exited=0' "$T/r" && notes="$notes; still running 30 s after closing"
            if [ -d "$P/drive_c/dxvk-logs" ]; then
                dx=$(ls "$P/drive_c/dxvk-logs" 2>/dev/null | sed -n 's/.*_\(d3d[0-9]*\|dxgi\)\.log$/\1/p' | sort -u | tr '\n' ' ')
                notes="$notes; DXVK: ${dx:-not loaded}"
            fi
        else
            la=FAIL; notes="$notes $(grep -E '^(window|launch)=' "$T/r")"
        fi
        "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
        [ "${KEEP_PREFIX:-0}" = 1 ] || rm -rf "$P"
    fi
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$name" $dl $inst $files $sm $la $cl "$notes" | tee -a "$TSV"
done

# the table
{
    echo "| Application | Download | Install | Files | Smoke | Launch | Close | Notes |"
    echo "|---|---|---|---|---|---|---|---|"
    tail -n +2 "$TSV" | awk -F'\t' '{ printf "| %s | %s | %s | %s | %s | %s | %s | %s |\n", $1, $2, $3, $4, $5, $6, $7, $8 }'
} > "$ARTIFACTS/results.md"
cat "$ARTIFACTS/results.md"
FAILED=$(tail -n +2 "$TSV" | cut -f2-7 | grep -c FAIL)
exit "$FAILED"
