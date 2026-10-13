# Drafter 7 progress

## Environment report (2026-10-13)
- Host: Debian 13 trixie, 12 cores, 31 GB RAM, ~400 GB free; sudo, network and `gh` push rights available. Not an isolated container: the checkout and the build both live under /var/tmp/d7 (outside any other agent's directory).
- Build dependencies: all Build-Depends of debian/control present (plus xvfb, xauth, bison, flex, mingw-w64).
- Tarball: wine-10.0.tar.xz, SHA-256 c5e0b3f5...ac3601 verified against wine-version; origin/main's full series (1168 entries) applied with no rejects, plus the theme generation steps from build.sh (Light/Dark/Rounded/eras images). Base tree committed to a local git repo.
- Build: configure --enable-archs=i386,x86_64 --with-mingw (tests enabled, unlike build.sh) and a full make succeeded.
- Conformance run under private Xvfb, scratch HOME/WINEPREFIX, winemenubuilder disabled: uxtheme_test passes 64- and 32-bit (0 failures); version_test exits 1 (no failure lines, base behaviour, not investigated).
- Finding on the base: dwmapi_test.exe crashes at start with EXCEPTION_INT_DIVIDE_BY_ZERO (both archs); dwmapi is in the queue, so this will be examined in its batch.
