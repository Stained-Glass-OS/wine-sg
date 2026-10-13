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
