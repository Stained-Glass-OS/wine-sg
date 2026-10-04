#!/usr/bin/python3
"""bashtty-drive.py WINE -- cmd in a pseudo-terminal (as over ssh), then bash
typed into interactively, then back to cmd (test/bashtty-gate.sh). Prints
PASS/FAIL lines."""
import os, pty, select, sys, time

wine = sys.argv[1]
pid, fd = pty.fork()
if pid == 0:
    os.environ["PS1"] = "SGBASH$ "
    if os.environ.get("SG_DRIVE_STDERR"):   # Wine's traces, out of the terminal
        f = os.open(os.environ["SG_DRIVE_STDERR"], os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
        os.dup2(f, 2)
    os.execvp(wine, [wine, "cmd"])

buf = b""
alltext = b""
def alltext_add(d):
    global alltext
    alltext += d
def read_until(pat, timeout):
    global buf
    end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.2)
        if r:
            try:
                data = os.read(fd, 4096)
            except OSError:
                break
            buf += data
            alltext_add(data)
            if pat.encode() in buf:
                return True
    return False

def type_slow(text):
    for ch in text:
        os.write(fd, ch.encode())
        time.sleep(0.03)

ok = True
def check(cond, what):
    global ok
    print(("PASS  " if cond else "FAIL  ") + what)
    if not cond:
        ok = False

check(read_until(">", 60), "cmd's prompt in the terminal")
buf = b""
type_slow("bash --norc --noprofile\r")
check(read_until("SGBASH$ ", 30), "bash starts, interactive (its prompt)")
buf = b""
type_slow("echo inner $((6*7)) $$\r")
got = read_until("inner 42", 15)
check(got, "what is typed reaches bash: 'echo inner $((6*7))' -> inner 42")
if not got:
    print("      terminal: %r" % buf[-300:])
buf = b""
type_slow("sleep 30\r")
time.sleep(1.5)
os.write(fd, b"\x03")           # Ctrl+C: bash's command, not the console's programs
check(read_until("SGBASH$ ", 10), "Ctrl+C stops bash's command and bash goes on (its prompt)")
buf = b""
type_slow("exit\r")
check(read_until(">", 20), "exit: back at cmd's prompt")
buf = b""
type_slow("echo back-in-cmd\r")
got = read_until("back-in-cmd", 15)   # contiguous only as cmd's output (its echo of typing is drawn key by key)
check(got, "and what is typed reaches cmd again")
if not got:
    print("      terminal: %r" % buf[-300:])
if os.environ.get("SG_DRIVE_DUMP"):
    open(os.environ["SG_DRIVE_DUMP"], "wb").write(alltext)
os.write(fd, b"exit\r")
time.sleep(1)
try:
    os.kill(pid, 9)
except OSError:
    pass
sys.exit(0 if ok else 1)
