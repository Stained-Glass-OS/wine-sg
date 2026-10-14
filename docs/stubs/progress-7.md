# Drafter 7 progress

## Environment report (2026-10-13)
- Host: Debian 13 trixie, 12 cores, 31 GB RAM, ~400 GB free; sudo, network and `gh` push rights available. Not an isolated container: the checkout and the build both live under /var/tmp/d7 (outside any other agent's directory).
- Build dependencies: all Build-Depends of debian/control present (plus xvfb, xauth, bison, flex, mingw-w64).
- Tarball: wine-10.0.tar.xz, SHA-256 c5e0b3f5...ac3601 verified against wine-version; origin/main's full series (1168 entries) applied with no rejects, plus the theme generation steps from build.sh (Light/Dark/Rounded/eras images). Base tree committed to a local git repo.
- Build: configure --enable-archs=i386,x86_64 --with-mingw (tests enabled, unlike build.sh) and a full make succeeded.
- Conformance run under private Xvfb, scratch HOME/WINEPREFIX, winemenubuilder disabled: uxtheme_test passes 64- and 32-bit (0 failures); version_test exits 1 (no failure lines, base behaviour, not investigated).
- Finding on the base: dwmapi_test.exe crashes at start with EXCEPTION_INT_DIVIDE_BY_ZERO (both archs); dwmapi is in the queue, so this will be examined in its batch.

## Environment report, second run (2026-10-13, Claude Code cloud container)
Verdict: **the container can build Wine and run it; work can proceed.**
- Host: Ubuntu 24.04, 4 cores, 15 GB RAM, ~26 GB free disk after the build. Build and trees live in /var/tmp/d7 (base/ = series-applied tree, a git repo; obj/ = build tree). The full build with tests took roughly 1h on 4 cores.
- Egress is a policy allowlist: dl.winehq.org had to be allowed (tarball, SHA-256 verified); apt works. Push to the assigned branch `claude/wine-sg-stub-drafter-7-cbbvkg` works once the Claude GitHub App is installed on the repo.
- Packages: gcc-mingw-w64-{i686,x86-64}, mingw-w64-tools, flex, bison, xvfb, xauth, xdotool, librsvg2-bin, icoutils, imagemagick, libxml-libxml-perl and the -dev packages of debian/control's Build-Depends, all from Ubuntu noble.
- Series: all 1168 entries of patches/series applied to the wine-10.0 tarball with no rejects, plus build.sh's theme generation (icons, Light/Dark/Rounded/eras images).
- Configure: --enable-archs=i386,x86_64 --with-mingw --with-sane --without-oss --without-netapi (tests ON). `make -j4` succeeded.
- **Prefix bootstrap needs `WINEDLLOVERRIDES=mscoree,mshtml=;winemenubuilder.exe=d`.** Without it `wineboot -i` blocks 300 s in `control.exe appwiz.cpl install_mono` (no network for Mono/Gecko, nobody to answer the dialog) and the prefix is left with an empty syswow64. With it: 17 s, syswow64 has 870 files.
- WoW64 check: `syswow64/cmd.exe` is `PE32 ... Intel 80386`; 64-bit and 32-bit `cmd /c echo` print their text; a running 32-bit cmd.exe is a static-pie **x86-64** ELF with zero 32-bit ELF mappings in /proc/<pid>/maps. No i386-unix tree exists in the build.
- Conformance: uxtheme_test 64- and 32-bit: 22241 tests, 0 failures, 14 todo.
- **Findings on the base (not yet examined):**
  1. dwmapi_test.exe exits 148 silently at start on both architectures (the first run saw EXCEPTION_INT_DIVIDE_BY_ZERO). In the dwmapi batch.
  2. After `wineboot -i`, winedevice.exe (thread hosting hid/winebus, right after `hid:fdo_pnp_dispatch Unhandled minor function 0x13`) takes an unhandled write page fault at ntoskrnl.exe+0x132e2 (a Unix stack address ending ...FE80), same offset every run. Seen only with WINEDEBUG=-all/err; vanishes under +seh tracing. winebus.sys is in the queue; to be examined there.
- Hygiene: run `wineserver -k` per prefix before reusing a display; never `pkill -f wineserver` from the shell running the command.

## Queue status (2026-10-14)
All 467 items of gap-CLOUD.json have a row in triage-7.tsv with evidence: 102 implemented (patches 5500-5534, each with a gate and a mutant shown failing, built from a clean series-applied tree), 51 windows-matches, 96 no-ground-truth, 69 not-applicable, 149 escalate (grouped in escalate-7.md).
- Last batches: 5530/5531 (COM surrogate bitness, dllhost GetMarshalSizeMax; `make test-dllhost`), 5532 (rpcss ept_lookup + rpcrt4 RpcMgmtEpEltInq*; rpcrt4 `test_endpoint_inquiry`), 5533 (loadperf lodctr/unlodctr; `make test-loadperf`), 5534 (fontsub CreateFontPackage subsetting; `make test-fontsub`).
- Not done inside the implemented items (each stated in its patches/notes/NNNN.txt): taskschd's own trigger objects for monthly triggers; package formats and the name-language filter in fontsub; loadperf's trusted-library checksum and error codes (unverified).
- A gate that starts a COM surrogate or any detached process must write the probe's output to a file, not a pipe: the surrogate inherits the pipe and keeps `$(...)` from ever returning.

## Round 2

Task 2 (report only, no code): docs/stubs/ngt-recheck.tsv, one line per no-ground-truth item of fast-a (381), fast-b (287, last line per dll+func wins), fast-a-cloud (30), triage-5 (64), triage-6 (188) and triage-7 (96): 1046 lines.
Method: ten agents each re-checked about 105 items against the MicrosoftDocs repositories (sdk-api, win32, winrt-api, windows-driver-docs-ddi, cpp-docs), learn.microsoft.com pages (including the IE platform-apis table of contents, which names pages the raw repos lack) and the Wine tests in the series-applied tree. Two chunks whose first pass was shallow (mshtml/mpr/msasn1 and urlmon/user32/win32u, and the last one) were redone in halves; the ntoskrnl rows were redone with the correct WDK docs branch.
Result: 170 documented, 12 tested, 48 windows-matches, 816 still-no-ground-truth. Integrator-pass downgrades (marked in the evidence): "tested" by analogy with another factory class, a test inside if(0), Windows CE pages, "Not supported" pages without an HRESULT, and windows-matches that were not confirmed against the code.
Caveats for whoever takes the "documented" lines: some rest on a generic interface contract rather than the DLL's own answer (the evidence says which); learn.microsoft.com rate-limited (429) several agents, so a "still" can hide a page they could not reach; the input file:line values were stale for many items and agents located stubs by name; a few items belong to other branches' code that is not in the tree they read (clusapi, bluetooth, some apphelp/combase/crypt32). Worth a first look: wininet CreateUrlCacheContainer/DeleteUrlCacheContainer (documented under sdk-api winineti/, Wine's stubs take wrong parameter lists), CoInternetQueryInfo (docs say E_FAIL for an unknown option, Wine returns E_NOTIMPL), and the IBrowserService/IDocObjectService/IShellUIHelper methods in ieframe/shdocvw.
