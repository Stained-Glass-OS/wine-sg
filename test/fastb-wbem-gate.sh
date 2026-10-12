#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wbemprox classes with host values (patch 3518): Win32_OperatingSystem, Win32_ComputerSystem, Win32_Processor,
# Win32_BIOS, Win32_DiskDrive, Win32_LogicalDisk, Win32_Volume and root\SecurityCenter2, read through wmic.
#
#   WINE=/opt/wine-sg/bin/wine test/fastb-wbem-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree); SG_XVFB_N is not needed (no display)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
T=$(mktemp -d /var/tmp/sg-fastb-wbem.XXXXXX)
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

# value NAME-of-property : the value of one property out of the last "/value" listing
wm() {
    printf '@echo off\r\n%s\r\nwmic %s\r\n' "${2:-rem}" "$1" > "$C/wm.bat"
    timeout -s KILL 180 env DISPLAY= "$WINE" cmd /c 'c:\wm.bat' 2>/dev/null </dev/null | tr -d '\r' > "$T/o"
}
val() { sed -n "s/^$1=//p" "$T/o" | head -1; }
num() { case "$1" in ''|*[!0-9]*) return 1;; esac; return 0; }

echo "== Win32_OperatingSystem"
wm 'os get /value'
os_tz=$(val CurrentTimeZone); os_boot=$(val LastBootUpTime); os_local=$(val LocalDateTime)
check "CreationClassName" "$([ "$(val CreationClassName)" = Win32_OperatingSystem ]; echo $?)"
check "CSCreationClassName" "$([ "$(val CSCreationClassName)" = Win32_ComputerSystem ]; echo $?)"
check "NumberOfProcesses counts the processes" "$(n=$(val NumberOfProcesses); num "$n" && [ "$n" -ge 2 ]; echo $?)"
check "MaxNumberOfProcesses is unsigned" "$([ "$(val MaxNumberOfProcesses)" = 4294967295 ]; echo $?)"
check "MaxProcessMemorySize is the address space in KB" "$(n=$(val MaxProcessMemorySize); num "$n" && [ "$n" -ge 2097152 ]; echo $?)"
check "paging file numbers are consistent" "$(a=$(val SizeStoredInPagingFiles); b=$(val FreeSpaceInPagingFiles); num "$a" && num "$b" && [ "$b" -le "$a" ]; echo $?)"
check "virtual memory is the commit limit (beyond RAM)" "$(v=$(val TotalVirtualMemorySize); p=$(val TotalVisibleMemorySize); num "$v" && num "$p" && [ "$v" -ge "$p" ]; echo $?)"
check "free virtual memory is within the total" "$(f=$(val FreeVirtualMemory); v=$(val TotalVirtualMemorySize); num "$f" && [ "$f" -le "$v" ]; echo $?)"
check "LastBootUpTime is local time with the zone offset" "$(echo "$os_boot" | grep -q '^[0-9]\{14\}\.[0-9]\{6\}[-+][0-9]\{3\}$'; echo $?)"
check "LastBootUpTime and LocalDateTime use the same offset" "$([ "${os_boot#??????????????.??????}" = "${os_local#??????????????.??????}" ]; echo $?)"
check "LastBootUpTime is not in the future" "$([ "$(echo "$os_boot" | cut -c1-14)" \< "$(echo "$os_local" | cut -c1-14)" ] || [ "$(echo "$os_boot" | cut -c1-14)" = "$(echo "$os_local" | cut -c1-14)" ]; echo $?)"

echo "== Win32_ComputerSystem"
wm 'computersystem get /value'
check "Caption is the name" "$([ -n "$(val Name)" ] && [ "$(val Caption)" = "$(val Name)" ]; echo $?)"
check "CreationClassName" "$([ "$(val CreationClassName)" = Win32_ComputerSystem ]; echo $?)"
check "CurrentTimeZone agrees with the OS class" "$([ "$(val CurrentTimeZone)" = "$os_tz" ]; echo $?)"
check "DNSHostName" "$([ -n "$(val DNSHostName)" ]; echo $?)"
check "PCSystemType is desktop or mobile" "$(case "$(val PCSystemType)" in 1|2) true;; *) false;; esac; echo $?)"
check "PartOfDomain is false, Workgroup named" "$([ "$(val PartOfDomain)" = FALSE ] && [ -n "$(val Workgroup)" ]; echo $?)"
check "Status OK, BootupState" "$([ "$(val Status)" = OK ] && [ "$(val BootupState)" = "Normal boot" ]; echo $?)"

echo "== Win32_Processor"
wm 'cpu get /value'
check "ThreadCount equals the logical processors" "$([ -n "$(val ThreadCount)" ] && [ "$(val ThreadCount)" = "$(val NumberOfLogicalProcessors)" ]; echo $?)"
check "NumberOfEnabledCore" "$(n=$(val NumberOfEnabledCore); num "$n" && [ "$n" -ge 1 ]; echo $?)"
check "cache sizes are numbers" "$(num "$(val L2CacheSize)" && num "$(val L3CacheSize)"; echo $?)"
check "Status, SystemName, class names" "$([ "$(val Status)" = OK ] && [ -n "$(val SystemName)" ] && [ "$(val CreationClassName)" = Win32_Processor ]; echo $?)"
check "CpuStatus is 1 for an enabled processor" "$([ "$(val CpuStatus)" = 1 ]; echo $?)"

echo "== Win32_BIOS"
wm 'bios get /value'
check "Name, Caption and Description are the BIOS version" "$([ -n "$(val Name)" ] && [ "$(val Name)" = "$(val Caption)" ] && [ "$(val Name)" = "$(val Description)" ] && [ "$(val Name)" = "$(val SMBIOSBIOSVersion)" ]; echo $?)"
check "no invented serial number or version" "$([ "$(val SerialNumber)" != "Serial number" ] && [ "$(val Version)" != "WINE   - 1" ]; echo $?)"
check "Version reads VENDOR - n" "$(val Version | grep -q ' - '; echo $?)"
check "PrimaryBIOS" "$([ "$(val PrimaryBIOS)" = TRUE ]; echo $?)"

echo "== Win32_DiskDrive"
wm 'diskdrive get /value'
check "Caption is the model" "$([ -n "$(val Model)" ] && [ "$(val Caption)" = "$(val Model)" ]; echo $?)"
check "Name is the device" "$([ "$(val Name)" = "$(val DeviceID)" ]; echo $?)"
check "BytesPerSector, Status" "$([ "$(val BytesPerSector)" = 512 ] && [ "$(val Status)" = OK ]; echo $?)"

echo "== Win32_LogicalDisk, Win32_Volume"
wm 'logicaldisk where deviceid="C:" get /value'
size=$(val Size); free=$(val FreeSpace)
check "C: has its real size" "$(num "$size" && [ "$size" -gt 1073741824 ]; echo $?)"
check "free space within the size" "$(num "$free" && [ "$free" -le "$size" ]; echo $?)"
check "Description, MediaType, Access" "$([ "$(val Description)" = "Local Fixed Disk" ] && [ "$(val MediaType)" = 12 ] && [ "$(val Access)" = 3 ]; echo $?)"
check "MaximumComponentLength and system name" "$([ "$(val MaximumComponentLength)" = 255 ] && [ -n "$(val SystemName)" ]; echo $?)"
wm 'volume where driveletter="C:" get /value'
check "the volume's capacity is the logical disk's size" "$([ "$(val Capacity)" = "$size" ]; echo $?)"
check "the volume is a fixed drive, the boot volume" "$([ "$(val DriveType)" = 3 ] && [ "$(val BootVolume)" = TRUE ] && [ "$(val Name)" = "C:" ]; echo $?)"

echo "== root\\SecurityCenter2"
D="$T/defender"; mkdir -p "$D"
now_engine=$(date -u -d '2 days ago' '+%a %b %e %H:%M:%S %Y')
old_engine=$(date -u -d '40 days ago' '+%a %b %e %H:%M:%S %Y')
setdir="set SG_DEFENDER_DIR=Z:$(echo "$D" | tr / '\\')"
q='/namespace:\\root\SecurityCenter2 path AntiVirusProduct get displayname,productstate,instanceguid /value'
printf '{\n "enabled": true,\n "engine": "ClamAV 1.4.3/27780/%s",\n "found": 0,\n "scanned": 3\n}\n' "$now_engine" > "$D/status.json"
wm "$q" "$setdir"
check "defender on and up to date: 266240" "$([ "$(val displayName)" = "SG Defender" ] && [ "$(val productState)" = 266240 ]; echo $?)"
check "an instance GUID" "$(val instanceGuid | grep -q '^{[0-9A-Fa-f-]*}$'; echo $?)"
printf '{\n "enabled": true,\n "engine": "ClamAV 1.4.3/27780/%s"\n}\n' "$old_engine" > "$D/status.json"
wm "$q" "$setdir"
check "old definitions: 266256" "$([ "$(val productState)" = 266256 ]; echo $?)"
printf '{\n "enabled": false,\n "engine": ""\n}\n' > "$D/status.json"
wm "$q" "$setdir"
check "off: 262144" "$([ "$(val productState)" = 262144 ]; echo $?)"
rm -f "$D/status.json"
wm "$q" "$setdir"
check "no defender, no product" "$([ -z "$(val displayName)" ]; echo $?)"
wm '/namespace:\\root\SecurityCenter2 path FirewallProduct get displayname,productstate /value'
check "firewall product" "$([ "$(val displayName)" = "SG Firewall" ] && num "$(val productState)"; echo $?)"
wm '/namespace:\\root\SecurityCenter2 path AntiSpywareProduct get displayname /value' "$setdir"
check "the anti-spyware class exists" "$([ $? = 0 ]; echo $?)"

exit $RC
