# Drafter 6 (core): what differs from RULES.md

Drafter 6 runs in a separate Claude Code session on its own account, possibly
on another computer, so /var/tmp/stubs-common and the coordinator's files may
not exist there. Everything you need is in this repo under docs/stubs/.

## Your job (David 2026-10-13: "close the real gap first")
- Queue: docs/stubs/gap-CORE.json -- 1186 REAL gaps (fixme-stub, notimpl,
  fake-success) in the CORE DLLs, from an audit of main + all drafter branches,
  sorted by score: ntdll 143, ntoskrnl.exe 113, ole32 101, advapi32 89,
  gdi32 73, kernel32 72, win32u 67, rpcrt4 65, kernelbase 61, crypt32 57,
  user32 50, oleaut32 40, combase 36, bcrypt 31, wintrust 29, wow64 21,
  secur32 20, wtsapi32 15, netio.sys 15, and the WinRT authentication /
  cryptowinrt / pstorec items.
- Each item ends IMPLEMENTED (a patch) or TRIAGED (docs/stubs/RULES.md,
  "Closing the real gap" and "NO BULK TRIAGE"): append to
  docs/stubs/triage-6.tsv IN YOUR BRANCH, one examined item per line, with
  evidence (a Wine test line that runs on Windows, or a Microsoft docs URL).
- Branch stubs-6, draft PR to main; patch numbers 5000-5499.
- Extra: an ntdll bug drafter 5 found -- a symbolic link whose relative target
  points into the link's own folder reads back as just the file name, and a
  directory symlink cannot be deleted (msvcp140 filesystem tests). Fix it.

## CAREFUL MODE, always (these DLLs are loaded by every Windows program)
- Small patches, one area each. A table-driven gate per patch, sourcing
  test/scratch-home.sh on line 2, with at least one mutant per behaviour
  family that you SHOW fails the gate.
- Before every push: the FULL conformance suites of every DLL you touched,
  64- and 32-bit, compared with the same suites on the series-applied base
  without your patches: zero new failures; remove todo_wine marks that now
  pass.
- Every notes file (patches/notes/NNNN.txt) starts with "CORE: integrator
  must review". Say what is unverified.
- Read our existing patches for a DLL before touching it
  (git log origin/main -- patches/sg/ | grep -i DLLNAME); never change
  behaviour they set. Security model items (tokens, ACLs, logon, sessions,
  credentials, crypto policy) -> triage as "escalate" with the reason, do not
  implement.
- A fix to a patch already on main is a NEW patch.

## Setup
1. Wine source: the wine-10.0 tarball from upstream (URL and SHA-256 in this
   repo's build.sh / wine-version); verify the SHA-256. Apply origin/main's
   full patches/series. Wine's build dependencies (Debian: apt-get build-dep
   wine, or debian/control Build-Depends; also mingw-w64, Xvfb, xdotool).
2. Build once with make -j4 (or what the machine allows; leave headroom),
   then only the DLLs you touch.
3. gh: if available, open the draft PR stubs-6 -> main; otherwise push the
   branch and the coordinator opens it.
4. Progress and escalations IN YOUR BRANCH: docs/stubs/progress-6.md (every
   few batches: patch numbers, DLLs, items implemented/triaged, CI state) and
   docs/stubs/escalate-6.md.

## Hard rules (same as RULES.md)
- Every wine/wineserver/wineboot run with a scratch HOME and WINEPREFIX under
  /var/tmp and WINEDLLOVERRIDES=winemenubuilder.exe=d. Never touch the
  machine owner's real HOME or ~/.wine.
- No VMs, no release.sh, never push to main, never send email.
- Clean room: never copy or disassemble Microsoft binaries; GUIDs, layouts and
  signatures only from Wine's headers/IDL, Microsoft's published docs pages,
  or freely licensed headers (mingw-w64).
- Never rm with globs over shared names; kill processes by PID.
- Commit trailer: Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
- Do not edit debian/changelog (the integrator writes it).
