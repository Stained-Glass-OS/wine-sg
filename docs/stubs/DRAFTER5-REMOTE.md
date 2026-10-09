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
   debian/control's Build-Depends; also mingw-w64, Xvfb, xdotool). Installing
   build dependencies with sudo is fine.
3. This host: 8 cores, 15 GB RAM. Use `make -j3` (or -j4 when nothing else
   runs) and check `free -g` before each build (wait if under 3 GB available).
4. gh: installing the GitHub CLI (`sudo apt-get install gh`) and
   `gh auth login` is fine, so you can open the draft PR (stubs-5 -> main)
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
