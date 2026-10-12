# Drafter 5 on another computer: what differs from RULES.md

Drafter 5 runs on a different machine from the coordinator, so /var/tmp/stubs-common
and /var/tmp/stubs-agent do not exist there. Use these instead:

1. The work queue: docs/stubs/audit3-drafter5.json in this repo (your group's
   items only, highest score first; same fields as the full list) and
   docs/stubs/audit3.md (how the ranking was made, and the full top 300).
   Work down audit3-drafter5.json. Do not invent your own ranking.
2. The Wine source: download the wine-10.0 tarball from upstream (the URL and
   SHA-256 are in build.sh / wine-version in this repo) into /var/tmp/drafter-5/
   and check the SHA-256 before using it. Then apply origin/main's full
   patches/series as RULES.md says. You need Wine's build dependencies
   (Debian: `sudo apt-get build-dep wine` or the package list in
   debian/control's Build-Depends; also mingw-w64, Xvfb, xdotool). Install
   whatever you need with sudo without asking: the password is in
   ~/Documents/sudo.txt on that machine (use it as
   `sudo -S ... < ~/Documents/sudo.txt`). Never print it, echo it, log it,
   or put it in a commit, a file in the repo, or a message.
3. This host: 8 cores, 15 GB RAM. Use `make -j3` (or -j4 when nothing else
   runs) and check `free -g` before each build (wait if under 3 GB available).
4. gh: install the GitHub CLI (`sudo apt-get install gh`) and run
   `gh auth login` (or reuse the git credentials already on the machine), so you can open the draft PR (stubs-5 -> main)
   and run `gh pr checks`. If that is not possible, just push the branch;
   the coordinator opens the PR.
5. Progress and escalations: you cannot reach the coordinator's files, so
   keep them IN YOUR BRANCH: docs/stubs/progress-5.md (append an entry every
   ~5 batches: patch numbers, DLLs, functions done, CI state) and
   docs/stubs/escalate-5.md (problems you could not solve). The coordinator
   reads them from origin/stubs-5. The integrator drops these two files when
   merging.
6. Everything else in RULES.md applies unchanged: patch numbers 2800-2999,
   branch stubs-5, no pushes to main, no release.sh, no email, scratch HOME
   for every Wine run, no VMs.

## Update 2026-10-12 (round 2 for drafter 5)
- Everything you pushed through 2996 is merged on main (10.0-410..416). Fetch
  and rebase stubs-5 on origin/main first; never edit a merged patch (a fix to
  one is a NEW patch).
- NEW QUEUE: docs/stubs/audit5-drafter5.json (your group's remaining 724
  items, 595 real gaps, highest score first; from an audit of current main,
  docs/stubs/audit5.md). The old audit3-drafter5.json is out of date.
  Biggest: winegstreamer, qcap, dsdmo, dxva2, dmime, mf, dmusic, dmstyle,
  quartz, mmdevapi, dmsynth, evr, amstream, windows.gaming.*, devenum.
- NEW PATCH RANGE: 4000-4499 (2800-2999 is used up).
- WORK IN FAST MODE (RULES.md "FAST MODE"): your DLLs are long-tail, so write
  a whole DLL's remaining items, compile only that DLL as you go, then run its
  conformance tests and the regression comparison against stock wine at the
  end, one patch per DLL, push once per group of DLLs. Keep the hard safety
  rules (scratch HOME/WINEPREFIX for every Wine run).
- Lint: every test script that runs Wine must have
  `. "$(dirname "$0")/scratch-home.sh"` as line 2 (your
  dinput-hidusage-gate.sh missed it and failed CI); helper scripts sourced by
  gates must not be named *-gate.sh.
- Never turn a FIXME into a quiet TRACE unless the function now really does
  what Windows does.
