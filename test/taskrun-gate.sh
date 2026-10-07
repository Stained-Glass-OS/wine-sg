#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Scheduled tasks run (patches/sg/1171 taskschd, 1172 schedsvc). Wine kept a
# Task Scheduler 2.0 task as a file and nothing ever ran it; the object
# model dropped its triggers and actions on the way (read and write were
# stubs), so Microsoft Edge's update tasks were stored with no trigger and
# an empty Exec, and Chromium-style updaters (which build the definition
# through ITriggerCollection::Create) registered nothing that could run.
# Browsers installed for the whole machine never updated themselves.
#
# The probe (test/taskrun-probe.c) drives taskschd; the Task Scheduler
# service (svchost netsvcs, schedsvc) is the one that runs the tasks.
#
#   1. a task registered from XML with a time trigger runs at that time;
#      its XML keeps what the object model does not model (verbatim);
#      read back it has its trigger, principal and action (escaped)
#   2. a task built through the object model (time trigger repeating every
#      minute, an Exec action, user SYSTEM as a service account) is written
#      with them and runs at its next repetition
#   3. Run starts it at once; the last run time and result are kept
#   4. a disabled task (put_Enabled) is not started by Run
#   5. a daily calendar trigger begun yesterday runs at today's time
#   2b. the folder lists its tasks; idle settings are kept (Chromium's updater)
#   6. a task of a particular user is not run by the service (it is theirs)
#   7. the shell (explorer, at sign-in) starts the person's task host
#   8. the host runs that person's tasks (a log-on task at once)
#   9. Run on a person's task: the service asks their host, which runs it
#  10. another host of the same account does not take the person's request
#
#   WINE=/opt/wine-sg/bin/wine test/taskrun-gate.sh
#   mutants: SG_MUTANT_TASK_NO_RUNNER, SG_MUTANT_TASK_NO_USER_HOST (schedsvc/taskrun.c),
#            SG_MUTANT_TASK_NO_IDLE (taskschd/task.c), SG_MUTANT_TASK_NO_LIST (taskschd/regtask.c),
#            SG_MUTANT_NO_USER_TASKS (explorer/startup.c),
#            SG_MUTANT_TASK_DROP_TRIGGERS (taskschd/trigger.c),
#            SG_MUTANT_TASK_REWRITE_XML (taskschd/regtask.c),
#            SG_MUTANT_TAKEN_BY_OTHER_HOST (schedsvc/taskrun.c; step 10, 1445)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
T=$(mktemp -d /var/tmp/sg-taskrun.XXXXXX)
# a display of its own, and no Wayland compositor to find (a runtime
# directory of its own): never the session's
unset WAYLAND_DISPLAY
mkdir -p "$T/run" && chmod 700 "$T/run"
n=190; while [ -e "/tmp/.X$n-lock" ]; do n=$((n + 1)); done
Xvfb ":$n" -screen 0 800x600x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$n" XDG_RUNTIME_DIR="$T/run"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; kill "$XP" 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -o "$T/taskrun-probe.exe" "$HERE/taskrun-probe.c" -lole32 -loleaut32 -luuid \
    || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
TASKS="$C/windows/system32/tasks"
cp "$T/taskrun-probe.exe" "$C/"
"$WINESERVER" -p 900   # the service keeps running between the probe's calls
probe() { timeout -s KILL 120 "$WINE" 'C:\taskrun-probe.exe' "$@" 2>>"$T/probe.err" | tr -d '\r'; }
at() { date -d "$1" +%Y-%m-%dT%H:%M:%S; }
waitfile() { i=0; while [ ! -e "$1" ] && [ $i -lt "$2" ]; do sleep 1; i=$((i + 1)); done; [ -e "$1" ]; }

# a system task: XML with a time trigger, an unmodelled setting, an argument to escape
task_xml() {   # NAME START TRIGGER-XML USER FILE
    cat > "$T/$1.xml" <<EOF
<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <Triggers>
    $3
  </Triggers>
  <Principals><Principal id="Author"><UserId>$4</UserId><RunLevel>HighestAvailable</RunLevel></Principal></Principals>
  <Settings>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <RestartOnFailure><Interval>PT1M</Interval><Count>3</Count></RestartOnFailure>
    <Enabled>true</Enabled>
  </Settings>
  <Actions Context="Author"><Exec><Command>cmd.exe</Command><Arguments>/c echo ran &gt; C:\\$5</Arguments></Exec></Actions>
</Task>
EOF
    cp "$T/$1.xml" "$C/$1.xml"
}

# 1. XML, time trigger
task_xml SgTime "" "<TimeTrigger><StartBoundary>$(at '+25 sec')</StartBoundary><Enabled>true</Enabled></TimeTrigger>" S-1-5-18 time.txt
out=$(probe register SgTime 'C:\SgTime.xml')
case "$out" in *register=0*) pass "a task is registered from XML";; *) fail "register from XML: $out";; esac
grep -q RestartOnFailure "$TASKS/SgTime" 2>/dev/null && pass "its XML is kept as given (a setting the object model lacks stays)" \
    || fail "the stored XML lost RestartOnFailure: $(tr -d '\0' < "$TASKS/SgTime" 2>/dev/null | head -c 300)"
out=$(probe xml SgTime)
if printf '%s\n' "$out" | grep -q '<TimeTrigger>' && printf '%s\n' "$out" | grep -q '<UserId>S-1-5-18</UserId>' &&
   printf '%s\n' "$out" | grep -q '<Command>cmd.exe</Command>' && printf '%s\n' "$out" | grep -q 'echo ran &gt; C:'; then
    pass "read back, it has its trigger, principal and action (escaped)"
else
    fail "read back: $(printf '%s' "$out" | tr '\n' ' ' | head -c 600)"
fi
waitfile "$C/time.txt" 50 && pass "the time trigger ran it" || fail "the time trigger did not run it"

# 2. the object model, as Chromium's updater
rm -f "$C/om.txt"
out=$(probe newtask SgModel "$(at '-50 sec')" cmd.exe '/c echo om > C:\om.txt')
case "$out" in *"trigger_type=1 count=1"*register=0*) pass "a definition built through the object model registers";;
               *) fail "object model: $out";; esac
out=$(probe xml SgModel)
if printf '%s\n' "$out" | grep -q '<Interval>PT1M</Interval>' && printf '%s\n' "$out" | grep -q '<UserId>SYSTEM</UserId>' &&
   printf '%s\n' "$out" | grep -q '<LogonType>ServiceAccount</LogonType>' &&
   printf '%s\n' "$out" | grep -q 'echo om &gt; C:'; then
    pass "it is written with its trigger's repetition, the service account and its action"
else
    fail "object model XML: $(printf '%s' "$out" | tr '\n' ' ' | head -c 600)"
fi
waitfile "$C/om.txt" 25 && pass "it ran at its next repetition" || fail "the repeating trigger did not run it"

# 2b. the folder lists its tasks (GetTasks: Count and Item), and the idle
#     settings the definition was given are written (Chromium's updater asks
#     for them and stopped when it could not have them)
out=$(probe list x)
if printf '%s\n' "$out" | grep -q '^count=[2-9]' && printf '%s\n' "$out" | grep -qx 'name=SgTime' &&
   printf '%s\n' "$out" | grep -qx 'name=SgModel'; then
    pass "the root folder lists its tasks (Count, Item)"
else
    fail "GetTasks: $(printf '%s' "$out" | tr '\n' ' ')"
fi
out=$(probe xml SgModel)
printf '%s\n' "$out" | grep -q '<StopOnIdleEnd>false</StopOnIdleEnd>' \
    && pass "the idle settings set through the object model are written" \
    || fail "idle settings: $(printf '%s' "$out" | grep -A4 IdleSettings | tr '\n' ' ')"

# 3. Run
rm -f "$C/time.txt"
out=$(probe run SgTime)
case "$out" in *run=0*) ;; *) fail "Run: $out";; esac
waitfile "$C/time.txt" 10 && pass "Run starts it at once" || fail "Run did not start it"
sleep 2
out=$(probe info SgTime)
case "$out" in *"lastrun=20"*"(0)"*"result=0 (0)"*) pass "its last run time and result are kept";;
               *) fail "last run: $(printf '%s' "$out" | tr '\n' ' ')";; esac

# 4. disabled
out=$(probe disable SgTime; probe info SgTime; probe run SgTime)
case "$out" in *"disable=0"*"state=1"*"run=0x80041326"*) pass "a disabled task is not started";;
               *) fail "disable: $(printf '%s' "$out" | tr '\n' ' ')";; esac

# 5. daily, begun yesterday
task_xml SgDaily "" "<CalendarTrigger><StartBoundary>$(at '-1 day +20 sec')</StartBoundary><ScheduleByDay><DaysInterval>1</DaysInterval></ScheduleByDay></CalendarTrigger>" SYSTEM daily.txt
probe register SgDaily 'C:\SgDaily.xml' >/dev/null
# 6. a user's task, due at the same time
task_xml SgUser "" "<TimeTrigger><StartBoundary>$(at '+20 sec')</StartBoundary></TimeTrigger>" S-1-5-21-0-0-0-2000 user.txt
probe register SgUser 'C:\SgUser.xml' >/dev/null
waitfile "$C/daily.txt" 45 && pass "a daily trigger runs it at today's time" || fail "the daily trigger did not run it"
sleep 3
[ ! -e "$C/user.txt" ] && pass "a particular user's task is left to them" || fail "the service ran a user's task"

# 7. the shell starts the person's task host at sign-in (explorer's startup)
cat > "$T/run-explorer.sh" <<EOF2
#!/bin/sh
cd "$C" && XDG_SESSION_ID=91 WINEDEBUG=+process exec "$WINE" explorer /desktop=shell,800x600
EOF2
chmod +x "$T/run-explorer.sh"
"$T/run-explorer.sh" > "$T/explorer.log" 2>&1 & EP=$!
i=0; while ! grep -q 'SgUserTaskHost' "$T/explorer.log" 2>/dev/null && [ $i -lt 30 ]; do sleep 1; i=$((i + 1)); done
grep -q 'rundll32.exe schedsvc.dll,SgUserTaskHost' "$T/explorer.log" \
    && pass "the shell starts the person's task host at sign-in" || fail "the shell did not start the task host"
kill "$EP" 2>/dev/null

# 8. the person's host runs their tasks as them: a log-on task at once (the
#    gate's account is the system's, so the host is told whose tasks to take)
U=S-1-5-21-0-0-0-2000
task_xml SgULogon "" "<LogonTrigger><UserId>$U</UserId></LogonTrigger>" $U ulogon.txt
probe register SgULogon 'C:\SgULogon.xml' >/dev/null
cat > "$T/run-host.sh" <<EOF2
#!/bin/sh
cd "$C" && exec "$WINE" rundll32.exe schedsvc.dll,SgUserTaskHost --as $U
EOF2
chmod +x "$T/run-host.sh"
"$T/run-host.sh" > "$T/host.log" 2>&1 & HP=$!
waitfile "$C/ulogon.txt" 30 && pass "the person's host runs their log-on task" || fail "the person's host did not run their log-on task"
[ ! -e "$C/user.txt" ] && pass "and not their time task before its time came" || :

# 9. Run on a person's task: the service asks their host
rm -f "$C/user.txt"
out=$(probe run SgUser)
case "$out" in *run=0*) ;; *) fail "Run of a person's task: $out";; esac
waitfile "$C/user.txt" 20 && pass "Run on a person's task runs it in their host" || fail "Run on a person's task did not run it"
kill "$HP" 2>/dev/null

# 10. another host of the same account (another person's tasks, --as; the
#     shell's own host of step 7) sees the request too: it must not mark it
#     taken for the owner (1445) -- asked while the owner's host is not
#     running, the request is run when that host starts
rm -f "$C/user.txt"
cat > "$T/run-other.sh" <<EOF2
#!/bin/sh
cd "$C" && exec "$WINE" rundll32.exe schedsvc.dll,SgUserTaskHost --as S-1-5-21-0-0-0-3000
EOF2
chmod +x "$T/run-other.sh"
"$T/run-other.sh" > "$T/other.log" 2>&1 & OP=$!
sleep 5
out=$(probe run SgUser)
sleep 6   # the other host takes the requests (woken, the event set two seconds)
"$T/run-host.sh" >> "$T/host.log" 2>&1 & HP=$!
waitfile "$C/user.txt" 20 && pass "another host of the account leaves the person's Run to their host" \
    || fail "another host took the person's Run request: never run"
kill "$HP" "$OP" 2>/dev/null

[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
