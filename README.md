# wine-sg

Wine built for [Stained Glass OS](https://github.com/Stained-Glass-OS/stained-glass),
with one property the packaged builds do not have:

> **A single amd64 installation runs 32-bit Windows applications, with no i386
> Linux libraries and no multiarch.**

That is Wine's "new WoW64" mode, enabled by building with
`--enable-archs=i386,x86_64`. Neither Debian's nor WineHQ's Wine is built that
way — both ship the WoW64 thunk DLLs in their amd64 package while keeping the
32-bit PE set in a separate i386 package, so an amd64-only install appears
capable and then cannot run a 32-bit binary.

Verified, not assumed:

```
PASS  no i386-unix tree (new WoW64, no 32-bit Linux side)
PASS  syswow64 populated (834 files)
PASS  syswow64/cmd.exe is PE32 i386
PASS  32-bit Windows binary executed
PASS  no i386 Linux libraries mapped while running 32-bit code
```

The last line is the one that matters, and it is measured from
`/proc/<pid>/maps` of the live 32-bit process.

## Build

```sh
make deps && make build && sudo make install && make test
```

Installs to `/opt/wine-sg`, alongside a distribution Wine rather than over it.

## It is also smaller than the Wine it replaces

| | installed |
|---|---|
| Debian `wine`, amd64 tree | 717 MB |
| Debian `wine`, i386 tree (needed for 32-bit) | 601 MB |
| **`wine-sg`, both architectures** | **454 MB** |

Debian does not strip its Wine; `build.sh` does, using the matching mingw
`strip` per architecture. Unstripped this is 1.5 GB, roughly 1.1 GB of it DWARF.
The cost is symbolised `winedbg` backtraces — build with `STRIP=0` when chasing
a crash inside Wine.

## Not a fork

Every patch here is carried from Debian's Wine packaging with its original
author attribution intact. `patches/fixes/binutils2.44.patch` is load-bearing:
without it, Wine built on Debian trixie faults during module load and cannot
start a process at all (WineHQ bug 57819).

Patches of our own will live in `patches/sg/` when they exist, kept separate so
that what we owe upstream stays obvious.

## License

LGPL-2.1+, Wine's license. The Wine source is fetched at build time and is not
redistributed here.
