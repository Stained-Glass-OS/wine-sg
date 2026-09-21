# wine-sg — Wine for Stained Glass OS

Wine built with `--enable-archs=i386,x86_64` ("new WoW64"), so **one amd64
install runs both 64-bit and 32-bit Windows applications with no i386 Linux
libraries and no multiarch**.

Project brief: [`stained-glass/docs/BRIEF.md`](https://github.com/Stained-Glass-OS/stained-glass/blob/main/docs/BRIEF.md).

## Why this repo exists

Neither Debian's nor WineHQ's packaged Wine is built for new WoW64. Both ship
the WoW64 *thunk* DLLs in their amd64 package while keeping the 32-bit PE set in
a separate i386 package — so an amd64-only install looks capable and then cannot
run a 32-bit Windows binary. See
[ADR 0002](https://github.com/Stained-Glass-OS/stained-glass/blob/main/docs/decisions/0002-wow64-and-image-architecture.md)
and
[ADR 0005](https://github.com/Stained-Glass-OS/stained-glass/blob/main/docs/decisions/0005-building-wine-ourselves.md).

**This is not yet a fork.** Every patch here is carried from Debian's packaging
and keeps its original author attribution. When we write Wine patches of our
own — S2 will — they go in `patches/sg/`, so "carried from elsewhere" and "ours
to upstream" stay distinguishable at a glance.

## Build and gate

```sh
make deps      # build-dep wine + both mingw cross compilers + libsane-dev
make build     # ~12 minutes on 12 cores
sudo make install
make test      # the gate
make deb
```

`make test` runs against the **installed** tree, not the build tree, because
what we ship is what matters. Install before testing.

## The gate

`test/wow64-gate.sh` proves the claim rather than the build:

1. The install has the new-WoW64 *shape* — `i386-windows` and `x86_64-windows`
   PE trees, an `x86_64-unix` tree, and **no `i386-unix`**. A build that quietly
   fell back to old WoW64 would still run 32-bit code, so the absence of a
   32-bit Unix side is asserted explicitly.
2. A prefix bootstraps with a populated `syswow64`.
3. `syswow64/cmd.exe` really is `PE32 … i386`.
4. It executes.
5. **While it is executing, nothing 32-bit-ELF is mapped.** The gate reads
   `/proc/<pid>/maps` of the live process. The claim is not "32-bit works" but
   "32-bit works on a pure amd64 system", and only the live process can falsify
   that.

It runs under `env -i` with the system Wine off `PATH`, so nothing can satisfy
the test using the distribution's build by accident.

## Things that will bite you

- **`patches/fixes/binutils2.44.patch` is not optional on Debian trixie.**
  Without it, `winebuild` emits import address tables into read-only sections,
  and every process dies during module load with a write fault at an address
  inside `ntdll`'s `.text`. Upstream Wine 10.0 built on trixie without this
  patch produces a Wine that cannot start `cmd`. Debian carries it; upstream
  tarballs do not. WineHQ bug 57819.
- **Don't reconfigure into a dirty object tree.** `build.sh` skips `configure`
  when `build/obj/Makefile` exists. If you change configure flags, `make clean`.
- **Bumping `wine-version` is a deliberate act.** It invalidates `sg-testlab`'s
  winetest baseline and every patch has to be re-checked against the new tree.
- **The gate needs `/proc` and the ability to `pgrep` your own processes.**
  A container with a hidden `/proc` will fail step 5 with "could not find the
  running process", which is a false negative rather than a real failure.

## License

**LGPL-2.1+ — Wine's license, not ours to choose.**

This matters beyond bookkeeping. Per
[ADR 0004](https://github.com/Stained-Glass-OS/stained-glass/blob/main/docs/decisions/0004-licensing.md),
the rest of Stained Glass is AGPL-3.0-or-later, and **AGPL-3.0 code cannot be
incorporated into Wine.** That is precisely why Wine-bound work lives in this
repo rather than in an AGPL one: a patch written elsewhere in the project could
never be submitted upstream. Keep it that way.
