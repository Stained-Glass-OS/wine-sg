#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# command-line tools (patch 3519): whoami /groups /priv /all /fo /nh /upn /fqdn, where /r /q /f /t $env:, icacls and cacls
# (display, grant, deny, remove, findsid, save), certutil (-hashfile -encode -decode -encodehex -store -addstore -delstore),
# systeminfo /fo, sc create argument checks and dependency cycles.
#
#   WINE=/opt/wine-sg/bin/wine test/fastb-tools-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
T=$(mktemp -d /var/tmp/sg-fastb-tools.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
pass() { printf "      PASS  %s\n" "$1"; }
fail() { printf "      FAIL  %s\n" "$1"; RC=1; }
check() { if [ "$2" = 0 ]; then pass "$1"; else fail "$1"; fi; }

mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"

# run one command line through cmd.exe; output in $T/o (CRs removed), exit code in $T/rc
run() {
    printf '@echo off\r\n%s\r\necho RC=%%errorlevel%%\r\n' "$1" > "$C/t.bat"
    timeout -s KILL 240 env DISPLAY= "$WINE" cmd /c 'c:\t.bat' 2>&1 </dev/null | tr -d '\r' > "$T/o"
    sed -n 's/^RC=//p' "$T/o" | tail -1 > "$T/rc"
}
rc() { cat "$T/rc"; }
has() { grep -q -- "$1" "$T/o"; }

echo "== whoami"
run 'whoami /groups'
check "groups: heading and three columns" "$(has '^GROUP INFORMATION' && has '^Group Name  *Type  *SID  *Attributes'; echo $?)"
check "groups: Everyone is a well-known group" "$(has '^Everyone  *Well-known group  *S-1-1-0 '; echo $?)"
run 'whoami /groups /fo csv /nh'
check "groups csv: quoted, no header" "$(has '^"Everyone","Well-known group","S-1-1-0","' && ! has 'Group Name'; echo $?)"
run 'whoami /priv'
check "priv: table with state" "$(has '^PRIVILEGES INFORMATION' && has '^SeChangeNotifyPrivilege .*Enabled'; echo $?)"
run 'whoami /priv /fo list'
check "priv list: Privilege Name:" "$(has '^Privilege Name: Se'; echo $?)"
run 'whoami /all'
check "all: user, groups, privileges, claims" "$(has '^USER INFORMATION' && has '^GROUP INFORMATION' && has '^PRIVILEGES INFORMATION' && has '^USER CLAIMS INFORMATION'; echo $?)"
run 'whoami /upn'
check "upn: a local account has none" "$(has 'not a domain user' && [ "$(rc)" = 1 ]; echo $?)"
run 'whoami /fqdn'
check "fqdn: a local account has none" "$(has 'not a domain user' && [ "$(rc)" = 1 ]; echo $?)"
run 'whoami /bogus'
check "bad option" "$(has 'Invalid argument/option' && [ "$(rc)" = 1 ]; echo $?)"
run 'whoami /user /fo list'
check "user list" "$(has '^User Name: ' && has '^SID: S-1-5-'; echo $?)"

echo "== where"
run 'where cmd.exe'
check "finds cmd" "$(has 'system32.cmd.exe' && [ "$(rc)" = 0 ]; echo $?)"
run 'where /q cmd.exe'
check "/q: silent, exit 0" "$(! has 'cmd.exe' && [ "$(rc)" = 0 ]; echo $?)"
run 'where /q nosuchprogram'
check "/q: not found is exit 1" "$([ "$(rc)" = 1 ]; echo $?)"
run 'where nosuchprogram'
check "not found message" "$(has 'INFO: Could not find files' && [ "$(rc)" = 1 ]; echo $?)"
run 'where /f cmd'
check "/f: quoted" "$(has '^".*cmd.exe"$'; echo $?)"
run 'where /t cmd.exe'
check "/t: size, date, path" "$(has '^ *[0-9][0-9]*  [0-9/]*  *[0-9:]* [AP]M  .*cmd.exe'; echo $?)"
run 'where /r c:\windows\system32 notepad'
check "/r: searches below" "$(has 'notepad.exe'; echo $?)"
run 'where $windir:system32'
check "\$env: pattern with no match" "$([ "$(rc)" = 1 ]; echo $?)"
run 'where /z x'
check "bad option is exit 2" "$(has 'Invalid argument or option' && [ "$(rc)" = 2 ]; echo $?)"
run 'where'
check "no pattern is exit 2" "$([ "$(rc)" = 2 ]; echo $?)"

echo "== icacls, cacls"
mkdir -p "$C/acl"
printf hi > "$C/acl/a.txt"
run 'icacls c:\acl\a.txt'
check "display: path, owner ACE, summary" "$(has '^c:.acl.a.txt .*:(F)' && has 'Successfully processed 1 files; Failed processing 0 files' && [ "$(rc)" = 0 ]; echo $?)"
check "display: continuation lines are indented" "$(grep -q '^          .*:(' "$T/o"; echo $?)"
run 'icacls c:\acl\nosuch'
check "missing file is exit 2" "$(has 'cannot find the file' && [ "$(rc)" = 2 ]; echo $?)"
run 'icacls c:\acl\a.txt /grant:r *S-1-1-0:(R)'
check "grant:r an SID" "$(has 'Successfully processed 1 files' && [ "$(rc)" = 0 ]; echo $?)"
run 'icacls c:\acl\a.txt'
check "Everyone shows with its right" "$(has 'Everyone:(R'; echo $?)"
run 'icacls c:\acl\a.txt /grant nosuchuser:(F)'
check "an unknown account is refused" "$(has 'No mapping between account names' && [ "$(rc)" = 5 ]; echo $?)"
run 'icacls c:\acl\a.txt /grant Everyone:(BOGUS)'
check "an unknown right is refused" "$(has 'Invalid parameter' && [ "$(rc)" = 5 ]; echo $?)"
run 'icacls c:\acl /T /Q /findsid Everyone'
check "findsid lists the file" "$(has 'a.txt'; echo $?)"
run 'icacls c:\acl\a.txt /save c:\acl\saved.txt'
check "save writes an SDDL file" "$([ -s "$C/acl/saved.txt" ] && iconv -f UTF-16 -t UTF-8 "$C/acl/saved.txt" 2>/dev/null | grep -q 'O:'; echo $?)"
run 'icacls'
check "no arguments: usage, exit 1" "$(has 'ICACLS name /save' && [ "$(rc)" = 1 ]; echo $?)"
run 'cacls c:\acl\a.txt'
check "cacls displays" "$(has '^c:.acl.a.txt .*:[FRCW]' ; echo $?)"
run 'cacls c:\acl\a.txt /E /P Everyone:W'
check "cacls /E /P" "$(has 'processed file: c:.acl.a.txt' && [ "$(rc)" = 0 ]; echo $?)"
run 'cacls c:\acl\nosuch'
check "cacls missing file" "$(has 'cannot find the file' && [ "$(rc)" = 1 ]; echo $?)"
run 'cacls c:\acl\a.txt /E /G nosuchuser:F'
check "cacls unknown account" "$(has 'No mapping' && [ "$(rc)" = 1 ]; echo $?)"

echo "== certutil"
printf hello > "$C/h.txt"
run 'certutil -hashfile c:\h.txt'
check "SHA256 of hello" "$(has '^2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824$' && has 'completed successfully' && [ "$(rc)" = 0 ]; echo $?)"
run 'certutil -hashfile c:\h.txt MD5'
check "MD5 of hello" "$(has '^5d41402abc4b2a76b9719d911017c592$'; echo $?)"
run 'certutil -hashfile c:\h.txt SHA1'
check "SHA1 of hello" "$(has '^aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d$'; echo $?)"
run 'certutil -hashfile c:\missing.txt'
check "missing file fails" "$(has 'command FAILED' && [ "$(rc)" != 0 ]; echo $?)"
run 'certutil -hashfile c:\h.txt BOGUS'
check "unknown algorithm fails" "$(has 'command FAILED'; echo $?)"
run 'certutil -encode c:\h.txt c:\h.b64'
check "encode" "$(has 'Input Length = 5' && grep -q '^aGVsbG8=' "$C/h.b64" && grep -q 'BEGIN CERTIFICATE' "$C/h.b64"; echo $?)"
run 'certutil -decode c:\h.b64 c:\h.dec'
check "decode brings the file back" "$(cmp -s "$C/h.txt" "$C/h.dec"; echo $?)"
run 'certutil -encodehex c:\h.txt c:\h.hex'
check "encodehex" "$(grep -q '68 65 6c 6c 6f' "$C/h.hex"; echo $?)"
run 'certutil -decodehex c:\h.hex c:\h.dec2'
check "decodehex brings the file back" "$(cmp -s "$C/h.txt" "$C/h.dec2"; echo $?)"
run 'certutil -user -store Root'
check "store lists certificates" "$(has '^================ Certificate 0 ================' && has '^Cert Hash(sha1): ' && has '^Subject: '; echo $?)"
run 'certutil -frobnicate'
check "an unknown verb is refused" "$(has 'not supported' && [ "$(rc)" != 0 ]; echo $?)"

echo "== sc create argument checks"
run 'sc create fbt1 binpath= c:\x.exe type= bogus'
check "type= bogus is 1639" "$([ "$(rc)" = 1639 ]; echo $?)"
run 'sc create fbt2 binpath= c:\x.exe start= bogus'
check "start= bogus is 1639" "$([ "$(rc)" = 1639 ]; echo $?)"
run 'sc create fbt3 binpath= c:\x.exe error= bogus'
check "error= bogus is 1639" "$([ "$(rc)" = 1639 ]; echo $?)"
run 'sc create fbt4 binpath= c:\x.exe type= interact'
check "interact alone is 87" "$([ "$(rc)" = 87 ]; echo $?)"
run 'sc create fbt5 binpath= c:\x.exe tag= yes'
check "tag= yes for a program is 87" "$([ "$(rc)" = 87 ]; echo $?)"
run 'sc create fbt6 binpath= c:\x.exe depend= fbt6'
check "depending on itself is 1059" "$([ "$(rc)" = 1059 ]; echo $?)"
run 'sc create fbt7 binpath= c:\x.exe type= interact type= own
sc query fbt7
sc delete fbt7'
check "interact with own is created" "$(has 'SERVICE_NAME: fbt7'; echo $?)"
run 'sc create fbt8 binpath= c:\x.exe depend= fbt9
sc create fbt9 binpath= c:\x.exe depend= fbt8
sc delete fbt8
sc delete fbt9'
check "a dependency cycle through another service is refused" "$(grep -q 'CreateService FAILED 1059' "$T/o"; echo $?)"

echo "== findstr"
mkdir -p "$C/fsd/sub"
printf 'alpha one\r\nBeta two\r\nGamma 3\r\nalphabet soup\r\n' > "$C/fs1.txt"
printf 'zeta\r\nalpha z\r\n' > "$C/fs2.txt"
printf 'sub alpha\r\n' > "$C/fsd/sub/x.txt"
run 'findstr /n alpha c:\fs1.txt'
check "/n numbers the lines" "$(has '^1:alpha one' && has '^4:alphabet soup' && [ "$(rc)" = 0 ]; echo $?)"
run 'findstr /r /c:"^[A-Z]" c:\fs1.txt'
check "[A-Z] at the start" "$(has '^Beta two' && has '^Gamma 3' && ! has 'alpha'; echo $?)"
run 'findstr /r "[0-9]$" c:\fs1.txt'
check "a digit at the end" "$(has '^Gamma 3' && ! has 'Beta'; echo $?)"
run 'findstr /r /c:"[^a-z ]" c:\fs1.txt'
check "[^a-z ] finds the capitals and digits" "$(has 'Beta two' && has 'Gamma 3' && ! has 'alpha one'; echo $?)"
run 'findstr /r "\<alpha\>" c:\fs1.txt'
check "word boundaries: alpha but not alphabet" "$(has '^alpha one' && ! has 'alphabet'; echo $?)"
run 'findstr /v alpha c:\fs1.txt'
check "/v prints the other lines" "$(has '^Beta two' && has '^Gamma 3' && ! has 'alpha'; echo $?)"
run 'findstr /i /b beta c:\fs1.txt'
check "/b with /i" "$(has '^Beta two'; echo $?)"
run 'findstr /e soup c:\fs1.txt'
check "/e" "$(has 'alphabet soup' && ! has 'alpha one'; echo $?)"
run 'findstr /x "Gamma 3" c:\fs1.txt'
check "/x with a space-separated word list matches whole lines" "$([ "$(rc)" = 1 ]; echo $?)"
run 'findstr /x /c:"Gamma 3" c:\fs1.txt'
check "/x /c exact line" "$(has '^Gamma 3'; echo $?)"
run 'findstr /m alpha c:\fs1.txt c:\fs2.txt'
check "/m names the files" "$(has '^c:.fs1.txt' && has '^c:.fs2.txt'; echo $?)"
run 'findstr alpha c:\fs1.txt c:\fs2.txt'
check "several files prefix the names" "$(has '^c:.fs1.txt:alpha one' && has '^c:.fs2.txt:alpha z'; echo $?)"
run 'findstr /s alpha c:\fsd\*.txt'
check "/s goes below the directory" "$(has 'sub.x.txt:sub alpha'; echo $?)"
run 'findstr /o Beta c:\fs1.txt'
check "/o gives the offset" "$(has '^11:Beta two'; echo $?)"
run 'findstr nosuchtext c:\fs1.txt'
check "no match is exit 1" "$([ "$(rc)" = 1 ]; echo $?)"
run 'findstr alpha c:\nosuchfile.txt'
check "a missing file is exit 2" "$(has 'Cannot open' && [ "$(rc)" = 2 ]; echo $?)"
printf 'beta\r\n' > "$C/gs.txt"
run 'findstr /i /g:c:\gs.txt c:\fs1.txt'
check "/g reads the strings from a file" "$(has '^Beta two'; echo $?)"
run 'findstr /l "[0-9]" c:\fs1.txt'
check "/l is literal" "$([ "$(rc)" = 1 ]; echo $?)"

echo "== schtasks"
run 'schtasks /create /tn fbdaily /tr "c:\windows\system32\cmd.exe /c echo hi" /sc daily /st 09:30'
check "create a daily task" "$(has 'SUCCESS: The scheduled task "fbdaily" has successfully been created' && [ "$(rc)" = 0 ]; echo $?)"
run 'schtasks /create /tn fbdaily /tr c:\x.exe /sc daily'
check "creating it again is refused" "$(has 'ERROR: ' && [ "$(rc)" = 1 ]; echo $?)"
run 'schtasks /create /tn fbdaily /tr c:\y.exe /sc weekly /d MON,FRI /st 08:00 /mo 2 /f'
check "/f replaces it" "$(has 'successfully been created' && [ "$(rc)" = 0 ]; echo $?)"
run 'schtasks /create /tn fbonce /tr c:\x.exe /sc once'
check "ONCE needs a start time" "$(has '/ST is required' && [ "$(rc)" = 1 ]; echo $?)"
run 'schtasks /create /tn fbbad /tr c:\x.exe /sc fortnightly'
check "an unknown schedule is refused" "$(has 'Invalid value for /SC' && [ "$(rc)" = 1 ]; echo $?)"
run 'schtasks /create /tn fbmin /tr c:\x.exe /sc minute /mo 5000'
check "a minute interval beyond a day is refused" "$(has 'Invalid value for /MO'; echo $?)"
run 'schtasks /create /tn fbmon /tr c:\x.exe /sc monthly /mo second /d TUE'
check "second Tuesday of the month" "$(has 'successfully been created'; echo $?)"
run 'schtasks /query /fo csv /nh'
check "csv query lists both tasks without a header" "$(has '^"[^"]*","\\fbdaily",' && has '^"[^"]*","\\fbmon",' && ! has 'HostName'; echo $?)"
run 'schtasks /query /fo list /v /tn fbdaily'
check "verbose list: task to run and author" "$(has '^Task To Run: *c:.y.exe' && has '^Author: ' && has '^TaskName: *.fbdaily'; echo $?)"
run 'schtasks /query /tn fbdaily'
check "table query" "$(has '^TaskName  *Next Run Time  *Status' && has '^fbdaily  '; echo $?)"
run 'schtasks /query /xml /tn fbdaily'
check "xml query shows weekly days" "$(has '<Monday/>' && has '<Friday/>' && has '<WeeksInterval>2<'; echo $?)"
run 'schtasks /change /tn fbdaily /disable'
run 'schtasks /query /fo csv /nh /tn fbdaily'
check "disabled shows Disabled" "$(has '"Disabled"'; echo $?)"
run 'schtasks /change /tn fbdaily /tr "c:\z.exe -a"'
run 'schtasks /query /fo list /v /tn fbdaily'
check "the program was changed" "$(has '^Task To Run: *c:.z.exe -a'; echo $?)"
run 'schtasks /query /tn nosuchtask'
check "querying a missing task is exit 1" "$(has 'cannot find the file' && [ "$(rc)" = 1 ]; echo $?)"
run 'schtasks /delete /tn fbdaily /f'
check "delete" "$(has 'successfully deleted' && [ "$(rc)" = 0 ]; echo $?)"
run 'schtasks /delete /tn fbmon /f'
run 'schtasks /delete /tn fbdaily /f'
check "deleting a missing task is exit 1" "$([ "$(rc)" = 1 ]; echo $?)"
run 'schtasks /bogus'
check "unknown command" "$(has 'Invalid syntax' && [ "$(rc)" = 1 ]; echo $?)"

echo "== net, tasklist"
run 'net helpmsg 5'
check "net helpmsg shows the system text" "$(has 'Access is denied' && [ "$(rc)" = 0 ]; echo $?)"
run 'net helpmsg abc'
check "net helpmsg needs a number" "$(has 'NET HELPMSG message#' && [ "$(rc)" = 1 ]; echo $?)"
run 'net share'
check "net share lists (nothing shared here)" "$(has 'There are no entries in the list' || has 'Share name'; echo $?)"
run 'net share nosuchshare'
check "an unknown share is an error" "$(has 'network name cannot be found' && [ "$(rc)" = 2 ]; echo $?)"
run 'net'
# (3528 put LOCALGROUP between them in the list: both names, not their neighbourhood)
check "net names helpmsg and share" "$(has 'HELPMSG, ' && has ' SHARE, '; echo $?)"
run 'tasklist /fo csv /nh /fi "imagename eq services.exe"'
check "tasklist csv" "$(has '^"services.exe","[0-9]*"'; echo $?)"
run 'tasklist /svc /fo csv /fi "imagename eq services.exe"'
check "tasklist /svc has a Services column" "$(has '^"Image Name","PID","Services"'; echo $?)"
run 'tasklist /v /fo csv /fi "imagename eq services.exe"'
check "tasklist /v has status, user and CPU columns" "$(has '"Status","User Name","CPU Time","Window Title"' && has '^"services.exe",.*"Running",'; echo $?)"

echo "== systeminfo"
run 'systeminfo /fo csv'
check "csv: header row and one value row" "$([ "$(sed -n 1p "$T/o" | grep -c '^"Host Name","OS Name"')" = 1 ] && sed -n 2p "$T/o" | grep -q '^"[^"]*","Microsoft'; echo $?)"
run 'systeminfo /fo bogus'
check "bad format refused" "$(has 'Invalid value given for the /FO' && [ "$(rc)" = 1 ]; echo $?)"

exit $RC
