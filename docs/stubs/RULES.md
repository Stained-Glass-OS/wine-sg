# Stub drafters: shared rules (Stained Glass OS, wine-sg)

You are one of four Sonnet "drafters" implementing Wine stubs the way Windows
behaves. An Opus "integrator" merges your branch into main; you never push to main.

## The project
- Stained Glass OS: Debian trixie + our Wine fork "wine-sg" (patch series on top
  of the wine-10.0 tarball). Repo: /home/david/Stained-Glass-OS/wine-sg (do NOT
  work in that checkout; others use it). Read its CLAUDE.md first.
- Patches: patches/sg/NNNN-short-name.patch, listed in patches/series. Gates:
  test/NAME-gate.sh (+ test/NAME-probe.c), sourcing test/scratch-home.sh, like
  the existing ones. Look at recent stubs patches 1690-1701 and their gates as
  models (git log origin/main).
- The ranked work list: /var/tmp/stubs-common/audit3.md (+ audit3-items.json).
  Only take items in YOUR DLL group (see your prompt). Items marked
  [DAVID: ...] or [skip: ...] are not yours unless your prompt says so.

## How to work (long-tail batching)
- One patch per DLL area per batch, covering 10-50 related functions: implement
  them as Windows does (documented behaviour, Wine's conformance tests, MS docs).
- One table-driven gate per batch calling every function in it, checking real
  results (not just "returns success"). At least one mutant per behaviour family
  (#ifdef SG_MUTANT_NAME in the patched code) that you SHOW fails the gate.
- Also run Wine's own conformance tests for each DLL you touch (build them
  standalone if needed) and note any todo_wine that now passes (remove the
  todo_wine mark in the same patch if it does) and confirm no new failures.
- When you make a dead path live, check what real callers do next and implement
  the neighbouring calls too (a past patch made Word crash that way).

## Your tree and builds (the host has 12 cores / 31 GB shared by everyone)
- Your own directory /var/tmp/drafter-N/ (N = your number). Set it up once:
  extract /var/tmp/stubs-agent/wine-10.0.tar.xz, apply origin/main's full series
  (git -C /home/david/Stained-Glass-OS/wine-sg show origin/main:patches/...), make it
  a git repo (commit the base) so you can diff, configure and build once with
  `nice -n 10 make -j3`. After that build ONLY the DLLs/programs you touch
  (make -j3 dlls/foo programs/bar). Never more than -j3. No full rebuilds unless
  a header change forces it.
- Gates run against your build tree (WINE=/var/tmp/drafter-N/obj/wine or as the
  gates expect), on a private Xvfb (pick a display number 200+N*10 .. +9), with a
  scratch HOME (test/scratch-home.sh) and WINEDLLOVERRIDES including
  winemenubuilder.exe=d. NEVER touch /home/david's real HOME or ~/.wine.
- No VMs. Do not run release/release.sh. Never send email.
- Memory: if `free -g` shows under 4 GB available, wait before building.

## Build slots (MANDATORY since 2026-10-09: the host ran out of memory)
- Run EVERY build (make, configure, full or partial) through
  /var/tmp/stubs-common/buildslot.sh, e.g.
  `/var/tmp/stubs-common/buildslot.sh make -j3 dlls/foo`. It allows at most 3
  builds at once across all agents and waits for 5 GB of free memory.
  Never run make directly, never above -j3 (integrator full builds: -j6).
- Run Wine tests/gates one at a time per agent (no parallel test runs).
- Gates must be light: probes run one at a time (at most 2 at once), each with
  a timeout, small data (a few MB at most), and the gate's trap kills its
  wineserver (`wineserver -k`) and anything it started, on every exit path. A
  gate that ran ~40 probes of 64 MB at once took the host to 0 GB free
  (2026-10-09). Never leave Xvfb, dbus-daemon, udevadm or wine processes behind.

## Ground truth first (from drafter 2, 2026-10-09)
- Before drafting a batch, look for todo_wine blocks in Wine's conformance tests
  (dlls/*/tests/*.c) for your functions: they record Windows behaviour that
  Wine's tests have verified on real Windows. Run that test first, implement
  until the todo_wine checks pass, then remove those todo_wine marks in the same
  patch. Prefer this over guessing from documentation, and don't "fix" what the
  tests show Windows really does (e.g. an E_NOTIMPL that is correct).
- When Windows requires a privilege for a call, enforce it
  (ERROR_PRIVILEGE_NOT_HELD), don't leave it open.

## Regressions first (from drafter 2, 2026-10-10)
- Our patches can make Wine worse. Run your DLLs' conformance tests under both
  our build and stock wine (Debian's wine64, or the tarball built plain) and fix
  anything that passes on stock but fails on ours, before new stubs. Bisect our
  series to name the culprit patch; a culprit outside your group goes to
  ESCALATE.md. (Drafter 2 found a spurious file-change notification this way.)

## Hard safety rules (2026-10-10, after two incidents)
- Never run any wine/wineserver/wineboot binary bare, not even a quick one-off:
  always a scratch HOME and WINEPREFIX under /var/tmp and
  WINEDLLOVERRIDES=winemenubuilder.exe=d. A bare run touched David's real
  ~/.wine. David's real HOME (Desktop, Documents, ~/.wine) must never be read
  or written by a test.
- Never delete anything outside your own /var/tmp/drafter-N or a temp directory
  your own running command created. No rm with globs over shared /var/tmp names:
  other agents' gates use them while running.

## FAST MODE (long-tail DLLs only; David 2026-10-12)
For DLLs listed as fast-mode in your prompt (never core, Office-facing, browser,
security or installer DLLs), the per-batch rules above are replaced by:
1. Write ALL the remaining stubs/todo items of one DLL in one go (ranked list
   first, then everything else that has documentation or Wine test ground truth).
2. After each file or two, compile ONLY that DLL (and its tests):
   `buildslot.sh make -j3 dlls/NAME dlls/NAME/tests` -- seconds to minutes. Fix
   compile errors and warnings immediately. No full builds, no hand-written gate
   per batch, no mutants, no push, no CI while writing.
3. When the DLL is done: run Wine's own conformance tests for it (64- and 32-bit)
   and the regression comparison against stock wine; fix everything that passes
   on stock but fails on ours, remove todo_wine marks that now pass. Write ONE
   patch for the DLL (or a few by area), with a notes file listing what was
   implemented and what is unverified. A small smoke gate per DLL is enough
   (load the DLL, call a handful of the new functions) -- it must source
   scratch-home.sh on line 2.
4. Push once per group of DLLs (every few hours), not per batch; the integrator
   does the full build, the broad gates and the Office/browser checks.
Hard safety rules, scratch HOME, build slots and "fixes to merged patches are new
patches" still apply in fast mode.

## Your branch
- A worktree of wine-sg on your own branch: `git -C /home/david/Stained-Glass-OS/wine-sg
  worktree add /var/tmp/drafter-N/wsg -b stubs-N origin/main` (first time), then push
  with `git push -u origin stubs-N`. Rebase it on origin/main before each push
  (git fetch; git rebase origin/main) so the integrator can merge cleanly.
- Each batch = one commit on stubs-N containing: the patch file, its series line
  appended to patches/series, the gate + probe, and a note file
  patches/notes/NNNN.txt (3-6 lines: what it implements, the gate, the mutants
  shown caught, conformance results). Do NOT edit debian/changelog (the
  integrator writes the changelog when merging).
- Before every push: apply the WHOLE series from your branch to a clean tarball
  tree (no rejects) and build the DLLs your patch touches from THAT tree (so a
  file missing from the patch is caught). Patches must be generated from a diff
  against the series-applied base, never from an untracked file.
- Commit trailer:
  Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_01XZFS4SPPzeH5d2c1MuzqCS
- After pushing, open or update a draft PR from stubs-N to main
  (`gh pr create --draft --base main --head stubs-N ...` once; later pushes
  update it). CI runs a full build on the PR. Check `gh pr checks` later; if red,
  fix before the next batch.

## Review lessons (read before every batch)
- State another process can read: many Windows APIs are called from a DIFFERENT
  process than the one that set the state (e.g. ShutdownBlockReasonQuery from the
  logoff UI, window properties, shared handles). Never hand out raw pointers via
  window properties or shared memory; keep such state server-side or in a
  cross-process store, and make the gate test it from a second process.
- Lifetime: anything you allocate per window/object must be freed when that
  window/object goes away (WM_NCDESTROY / final Release), and the gate should
  show it does.
- Check every allocation (NetApiBufferAllocate, HeapAlloc...) and return the
  error Windows returns on failure.
- Build your touched DLLs from the clean series-applied tree before pushing, not
  just compare sources (CI is the backstop, not the check).

- Once a patch of yours is merged on main, never edit it on your branch: the
  integrator only takes patches that are NEW to main's series. A fix to a merged
  patch is a NEW patch (next number) that changes the code on top of it.

## Licensing and safety
- Our code, written from documentation and observed behaviour. Never copy or
  disassemble Microsoft binaries or code. Never call our product "Windows ...".
- If something is beyond you (a crash you can't explain, a cross-DLL design
  question, a security-model call), write it in /var/tmp/drafter-N/ESCALATE.md
  with details and move on to the next item; the integrator handles those.

## Reporting
- Every ~5 batches send a short progress message to "main" with SendMessage:
  patches done (numbers, DLLs, function counts), CI state, escalations. Keep
  working after reporting; stop only when your group's items are exhausted.
- Sibling DLLs: many DLLs build from another's sources (grep PARENTSRC in
  dlls/*/Makefile.in, e.g. d3dx11_42 from d3dx11_43, d3dx9_24..43, msvcr*).
  When you add exports or source files to a parent, build every child from the
  clean tree too (CI failed on d3dx11_42 after 3540, 2026-10-12).
- No 16-bit (Win16) work: wine-sg is a new-style WoW64 build, which cannot run
  16-bit programs at all, so *.dll16 / *.exe16 / krnl386 code never executes
  for our users (2026-10-12). Skip those audit items.
- Clean room for metadata too: never use a Microsoft SDK .winmd, .lib, .pdb
  or header file as the source of GUIDs, vtable order or signatures. Use
  Wine's include/*.idl, Microsoft's published API documentation pages, or
  freely licensed open-source headers (mingw-w64). (2026-10-12: a drafter
  dumped interfaces from an SDK .winmd; don't.)

## Closing the real gap (David 2026-10-13: "close the real gap first")
The goal is now the audit's REAL gaps (fixme-stub, notimpl, fake-success in
/var/tmp/audit7/audit7-items.json, scanned on main + all drafter branches).
Every real item ends in exactly one of these states:
1. IMPLEMENTED as Windows does (a patch, careful or fast as your prompt says).
2. TRIAGED, written to /var/tmp/stubs-common/triage/<you>.tsv, one line per
   item, tab-separated: dll, func, kind, file:line, verdict, evidence.
   Verdicts:
   - windows-matches: Windows really behaves this way (E_NOTIMPL, a failure
     code, a no-op). Evidence = a Wine test line that runs on Windows, or a
     Microsoft docs URL. Then also turn a FIXME into a WARN/TRACE ONLY IF the
     behaviour is complete, never to hide a gap.
   - no-ground-truth: undocumented and untested; say what is missing.
   - not-applicable: 16-bit (WoW64 has no Win16), ARM-only, or a Wine-
     internal debug message that is not a gap.
   - escalate: needs a design or security decision; also put it in ESCALATE.md.
The audit will count TRIAGED windows-matches/not-applicable items as closed.
Never triage an item as windows-matches without evidence.
