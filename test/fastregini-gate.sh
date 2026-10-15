#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# regini (patch 3294): apply script files (ANSI and UTF-16) in a scratch prefix and read the result back
# with reg.exe: every value type, quoting, continuation, indentation (subkeys), one-line "key name = data",
# HKEY_ names, DELETE of a value and of a key, and the exit status for bad input.
#
#   WINE=/opt/wine-sg/bin/wine test/fastregini-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (programs/regini): SG_MUTANT_REGINI_* (see work/muts-regini.sh).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
command -v iconv >/dev/null || { echo "SKIP: needs iconv"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastregini.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cat > "$T/a.ini" <<'EOA'
; a comment
\Registry\Machine\Software\SgRegini
    Plain = REG_SZ hello world
    Quoted = REG_SZ "  keep  "
    Num = REG_DWORD 0x10
    Dec = REG_DWORD 42
    Big = REG_QWORD 0x100000000
    Exp = REG_EXPAND_SZ %SystemRoot%\sub
    Multi = REG_MULTI_SZ "one" "two words"
    Bin = REG_BINARY 5 0x04030201 0x05
    "Quoted Name" = REG_SZ qn
    Cont = REG_SZ first \
           second
    NoType = bare text
    Gone = REG_SZ x
    Gone = DELETE
    Sub
        SubVal = REG_DWORD 7
        Deeper
            DV = REG_SZ d
    Sibling
        SV = REG_SZ s
\Registry\Machine\Software\SgRegini2 OneLine = REG_DWORD 3
HKEY_LOCAL_MACHINE\Software\SgRegini3
  X = REG_SZ hk
\Registry\Machine\Software\SgReginiDel
  V = REG_SZ 1
\Registry\Machine\Software\SgReginiDel DELETE
EOA
sed 's/SgRegini/SgReginiU/g; s/hello world/unicode ok/' "$T/a.ini" | iconv -f UTF-8 -t UTF-16LE > "$T/u0.ini"
printf '\377\376' > "$T/u.ini"; cat "$T/u0.ini" >> "$T/u.ini"
printf '\\Registry\\Machine\\Software\\SgReginiBad\n  Good = REG_SZ ok\n  Bad = REG_DWORD notanumber\n  After = REG_SZ still\n' > "$T/bad.ini"
printf '  Orphan = REG_SZ x\n' > "$T/rel.ini"
cp "$T/a.ini" "$T/u.ini" "$T/bad.ini" "$T/rel.ini" "$C/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$C"
{
echo "---run"; timeout -s KILL 120 "$WINE" regini a.ini 2>&1; echo "rc=\$?"
echo "---unicode"; timeout -s KILL 120 "$WINE" regini u.ini >u.out 2>&1; echo "rc=\$?"; tail -3 u.out
echo "---bad"; timeout -s KILL 120 "$WINE" regini bad.ini 2>&1; echo "rc=\$?"
echo "---missing"; timeout -s KILL 120 "$WINE" regini nofile.ini 2>&1; echo "rc=\$?"
echo "---orphan"; timeout -s KILL 120 "$WINE" regini rel.ini 2>&1; echo "rc=\$?"
echo "---noargs"; timeout -s KILL 120 "$WINE" regini 2>&1; echo "rc=\$?"
echo "---q"; timeout -s KILL 120 "$WINE" reg query 'HKLM\\Software\\SgRegini' /s 2>&1
echo "---qx"; timeout -s KILL 120 "$WINE" reg export 'HKLM\\Software\\SgRegini' 'C:\\exp.reg' /y >/dev/null 2>&1; iconv -f UTF-16 -t UTF-8 "$C/exp.reg" 2>/dev/null
echo "---q2"; timeout -s KILL 120 "$WINE" reg query 'HKLM\\Software\\SgRegini2' 2>&1
echo "---q3"; timeout -s KILL 120 "$WINE" reg query 'HKLM\\Software\\SgRegini3' 2>&1
echo "---qdel"; timeout -s KILL 120 "$WINE" reg query 'HKLM\\Software\\SgReginiDel' 2>&1; echo "rc=\$?"
echo "---qu"; timeout -s KILL 120 "$WINE" reg query 'HKLM\\Software\\SgReginiU' 2>&1
echo "---qbad"; timeout -s KILL 120 "$WINE" reg query 'HKLM\\Software\\SgReginiBad' 2>&1
} | tr -d '\r' > "$T/out.txt"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 600 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
fails=0
sec() { awk -v n="---$1" '$0==n{f=1;next} /^---/{f=0} f' "$T/out.txt"; }
check() { if sec "$2" | grep -q -- "$3"; then echo "PASS  $1"; else echo "FAIL  $1"; fails=$((fails+1)); sec "$2" | sed 's/^/      /' | head -20; fi; }
nocheck() { if sec "$2" | grep -q -- "$3"; then echo "FAIL  $1"; fails=$((fails+1)); sec "$2" | sed 's/^/      /' | head -20; else echo "PASS  $1"; fi; }
check "regini a.ini succeeds" run 'rc=0'
check "the script is echoed" run 'Plain = REG_SZ hello world'
check "REG_SZ" q 'Plain *REG_SZ *hello world$'
check "quoted REG_SZ keeps its blanks" q 'Quoted *REG_SZ *  keep  *$'
check "REG_DWORD hex" q 'Num *REG_DWORD *0x10$'
check "REG_DWORD decimal" q 'Dec *REG_DWORD *0x2a$'
check "REG_QWORD" q 'Big *REG_QWORD'
check "REG_QWORD value" qx '"Big"=hex(b):00,00,00,00,01,00,00,00'
check "REG_EXPAND_SZ" q 'Exp *REG_EXPAND_SZ *%SystemRoot%.sub$'
check "REG_MULTI_SZ" q 'Multi *REG_MULTI_SZ *one.0two words$'
check "REG_BINARY: count and words" q 'Bin *REG_BINARY *0102030405$'
check "quoted value name" q 'Quoted Name *REG_SZ *qn$'
check "continuation joins the lines with one blank" q 'Cont *REG_SZ *first second$'
check "no type means a string" q 'NoType *REG_SZ *bare text$'
nocheck "value deleted" q 'Gone'
check "subkey by indentation" q 'SgRegini.Sub$'
check "subkey value" q 'SubVal *REG_DWORD *0x7$'
check "second level subkey" q 'SgRegini.Sub.Deeper$'
check "deep value" q 'DV *REG_SZ *d$'
check "sibling returns to the right parent" q 'SgRegini.Sibling$'
nocheck "sibling is not below Deeper" q 'Deeper.Sibling'
check "key and value on one line" q2 'OneLine *REG_DWORD *0x3$'
check "HKEY_LOCAL_MACHINE spelling" q3 'X *REG_SZ *hk$'
check "key DELETE removes the key" qdel 'rc=1'
check "UTF-16 script with a byte order mark" qu 'Plain *REG_SZ *unicode ok$'
check "UTF-16 exit status" unicode 'rc=0'
check "bad data is reported" bad 'invalid data for .Bad.'
check "bad data gives a failure status" bad 'rc=1'
check "lines before and after the bad one are applied" qbad 'Good *REG_SZ *ok$'
check "lines after the bad one are applied" qbad 'After *REG_SZ *still$'
check "missing file fails" missing 'rc=1'
check "orphan relative key fails" orphan 'rc=1'
check "no arguments prints usage" noargs 'REGINI'
if [ $fails = 0 ]; then echo "RESULT: PASS"; exit 0; fi
echo "RESULT: FAIL"; cat "$T/out.txt"; exit 1
