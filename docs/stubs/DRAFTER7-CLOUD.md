# Drafter 7 (cloud session): close the real gap in the unowned long tail

You run in a Claude Code cloud session: a fresh Linux container with this
repository checked out. Nothing from the coordinator's machine exists here;
everything you need is in docs/stubs/.

## Your job (David 2026-10-13: "close the real gap first")
- Queue: docs/stubs/gap-CLOUD.json -- real gaps (fixme-stub, notimpl,
  fake-success) in DLLs no other drafter is working on, sorted by score
  (nsiproxy.sys, apphelp, difxapi, appxpackaging, winebus.sys, hal, sxs,
  iertutil, dwmapi, uxtheme, spoolss, ...).
- Each item ends IMPLEMENTED (a patch) or TRIAGED (docs/stubs/RULES.md,
  sections "Closing the real gap" and "NO BULK TRIAGE"): one examined item
  per line in docs/stubs/triage-7.tsv in your branch, with evidence (a Wine
  test line that runs on Windows, or a Microsoft docs URL).
- Decisions on hard cases: docs/stubs/DECISIONS.md. Security, credential,
  authentication and firewall-policy items (windows.security.*, fwpuclnt,
  authz, ...): triage "escalate" with a concrete Windows-matching proposal.
  Scanner/camera paths (sane.ds, sti, wia, twain): do not touch.
- Branch: stubs-7 (from origin/main), draft PR to main; patch numbers
  5500-5999. Progress in docs/stubs/progress-7.md, problems in
  docs/stubs/escalate-7.md, both in your branch.

## Setup in the container
1. Install Wine's build dependencies (Debian/Ubuntu: the Build-Depends of
   debian/control, plus mingw-w64 (gcc-mingw-w64-i686 and -x86-64), bison,
   flex, xvfb, xauth). Use sudo if needed.
2. Download the wine-10.0 tarball named in wine-version / build.sh, verify
   its SHA-256 (build.sh has it), extract it to a work directory outside the
   checkout, apply origin/main's full patches/series (patches/sg/...), and
   commit that tree to a local git repo as your base so you can diff.
3. configure --enable-archs=i386,x86_64 (as build.sh does), build once with
   all cores, then only the DLLs you touch.
4. Generate each patch as a diff against the series-applied base (never from
   an untracked file), add it to patches/series, write patches/notes/NNNN.txt.

## How to work: BATCH MODE (RULES.md)
- One DLL per batch, all its open documented items at once; at the end: the
  DLL's full Wine conformance suite 64- and 32-bit with 0 new failures against
  the base (remove todo_wine marks that now pass); ONE smoke gate
  test/NAME-gate.sh that sources test/scratch-home.sh on line 2 (look at
  existing gates for the pattern) with at least one mutant per behaviour
  family that you SHOW fails it; ONE notes file listing what is unverified.
- Before every push: apply the WHOLE series of your branch to a clean
  tarball tree (no rejects) and build the touched DLLs from it, 64 and 32.
- Run `make lint` before committing (it must pass) -- this checks gates,
  trademarks and the series.
- Do not edit debian/changelog (the integrator writes it).
- Commit trailer: Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>

## Hard rules
- Every wine/wineserver/wineboot run with a scratch HOME and WINEPREFIX and
  WINEDLLOVERRIDES=winemenubuilder.exe=d (test/scratch-home.sh does this).
- Never push to main. No release scripts. Never send email.
- Clean room: never copy or disassemble Microsoft binaries; GUIDs, layouts and
  signatures only from Wine's headers/IDL, Microsoft's published docs pages,
  or freely licensed headers (mingw-w64).
- Never call our product "Windows ..." (the trademark lint checks).
- Push often (after each batch) so work survives if the session ends.
