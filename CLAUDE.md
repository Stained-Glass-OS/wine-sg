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

**This is not a fork.** It is upstream plus a patch series, rebased on upstream
releases. `patches/fixes/` is carried from Debian's packaging with original
authorship intact; `patches/sg/` is ours.

**We do not submit anything upstream** — Wine prohibits LLM-generated code and
David's decision is that we simply do not upstream
([ADR 0006](https://github.com/Stained-Glass-OS/stained-glass/blob/main/docs/decisions/0006-wine-llm-contribution-policy.md)).
Patches are still kept small and one-concern, because that is what makes a
downstream series survive rebasing. Two consequences: we own these forever, and
`sg-testlab`'s winetest baseline is the only safety net left, since upstream
review is not coming.

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

## `patches/sg/0001-shared-system-prefix.patch`

Lets one prefix be shared by several Unix users — the first half of S2. Opt-in,
marked by a `.sg-system-prefix` file in the prefix; without it every path
behaves exactly as before.

What made this more work than the source analysis suggested:

- **The ownership assumption is not one check, it is four.** The prefix
  directory, the server directory, the socket *and* the lock file are all
  owner-only, and each has to be handled. Fixing three of them and leaving the
  lock gives the second user
  `error creating .../lock: Permission denied` from a directory they
  demonstrably can write to — because it is the existing file's mode refusing
  them, not the directory's.
- **Anything created in shared mode needs the prefix's group**, not the primary
  group of whoever started the server first. Otherwise the first login silently
  locks everyone else out.
- **The rule is implemented twice**, in `server/request.c` and in
  `dlls/ntdll/unix/server.c`, because the client works out where the server
  lives on its own — it may be the process that starts it. Patch one side only
  and you get `chdir to /tmp/.wine-<uid>/server-<dev>-<ino>: No such file or
  directory`, which names neither the cause nor the file to fix.

The `SO_PEERCRED` check is not separable from the relaxations: alone it is dead
code, and without it the relaxations remove a security control and put nothing
in its place. Admitted are root, the server directory's owner, and members of
its group — so **who may use a shared prefix is decided by group membership**,
with ordinary Unix tools rather than a policy file of ours.

## `patches/sg/0002-per-user-identity.patch`

Gives each user of a shared prefix its own SID, its own token and its own HKCU
hive — the second half of S2's identity work. Takes the gate from 2 of 5 clauses
to **3 of 5**.

Four things that are not independent, and break each other if done alone:

- **A SID per uid**, keeping `local_user_sid`'s shape
  (`S-1-5-21-0-0-0-<1000+uid>`). Upstream's mapping is binary — you, or
  Anonymous Logon.
- **Per-user tokens.** `token_create_admin()` is upstream's only path for a new
  process, which is why "non-admin cannot write `HKLM\Software\Policies`"
  cannot be satisfied by any descriptor: there is no non-admin. Administrators
  are root and the prefix's owner.
- **Per-user HKCU hives, loaded on demand.** *Required* by the SID change, not
  optional: with a SID per uid, every user but the first finds
  `\Registry\User\<their SID>` missing and every HKCU operation fails with
  `OBJECT_NAME_NOT_FOUND`. `MAX_SAVE_BRANCH_INFO` was exactly 3 — system,
  userdef, user — so there was no room for even one more hive.
- **Security descriptors on new keys.** `check_object_access()` returns TRUE for
  any object *without* a descriptor, and objects get none unless a caller
  supplies one. The registry is therefore not under-checked so much as
  un-checkable — no descriptor written to a key protects it while the check that
  would read it is never reached.

**The default DACL is the trap.** Naming the creating user is the obvious first
guess and is wrong: it protects HKLM keys by making them invisible to everyone
else, which breaks HKLM as shared machine state. The gate caught it precisely —
clause 3 started passing and clause 2 started failing in the same run. The
arrangement that works is the one Windows uses for HKLM: system and
administrators write, everyone else reads. Per-user privacy comes from HKCU
being a separate hive.

## `patches/sg/0003-check-create-access-on-the-container.patch`

Moves the create-permission check from `registry.c` into
`create_named_object()`, which is the only place that knows which container an
object actually lands in. Patch 0002 checked the *handle's* parent, so with a
path like `Policies\Foo\Bar` from an HKLM handle everything below the first
component went unchecked.

**Registry security in Wine is in-memory only.** `save_subkeys()` writes a key's
name, timestamp, class, symlink flag and values — and no security descriptor.
The `.reg` format has no field for one. So every descriptor is lost when the
server restarts and reloads `system.reg`, and any protection applied by an
earlier session evaporates.

That is why the S2 gate's clause 3 still fails while the mechanism demonstrably
works. Within a single live server session:

```
admin:     reg add HKLM\Software\LiveTest /v A   -> success
non-admin: reg add HKLM\Software\LiveTest /v B   -> Unable to access or create
                                                     the specified registry key
```

Restart the server and the denial is gone with the descriptor. Making clause 3
pass needs a decision about **persisting descriptors** — extending the `.reg`
format, storing them beside it, or reapplying a policy at every server start.
That is design work, not a patch, and is tracked on issue #2.

## `patches/sg/0004` and `0005`: SYSTEM, and who gets to be it

The machine-level wineserver has to exist before anyone logs in and outlive
every logout, and the services it hosts must be SYSTEM's — so whoever runs it
has to *be* SYSTEM, not merely be an administrator, or a boot-time service looks
wrong to the SCM and to every default DACL, all of which name Local System.

`0004` made that root. `0005` makes it **the prefix's owner**, which is the
right answer: an ordinary unprivileged account.

**Hosting the Windows system needs no Unix root**, and it is worth being precise
about why, because the opposite is an easy assumption:

- `winebus.sys` reaches devices through **udev**, not privileged syscalls.
- `winedevice` has no uid or capability checks at all.
- Printing goes through **CUPS**, which is a socket and a group.
- There is no `getuid() == 0` anywhere in `winspool`, `wineps` or `winebus`.

Driver work needs two things and neither is root: permission to touch the device
(udev rules plus group membership — `sg-session` ships both) and **NT
administrator** to install into HKLM, which this code decides from prefix
ownership rather than from the kernel.

Running it as root would put a root process on a socket every desktop user can
reach and buy no capability an ordinary account lacks.

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

`debian/copyright` lists **every patch separately with its own author**, because
they do not share one: some are ours, some come from Debian's wine packaging,
and one is Wine's maintainer's. A blanket "everything here is ours" would be
wrong about most of `patches/`. **When you add a patch, add its stanza** — the
`.deb` is what carries this downstream, and it is the only place a recipient
sees who wrote what.

This matters beyond bookkeeping. Per
[ADR 0004](https://github.com/Stained-Glass-OS/stained-glass/blob/main/docs/decisions/0004-licensing.md),
the rest of Stained Glass is AGPL-3.0-or-later, and **AGPL-3.0 code cannot be
incorporated into Wine.** That is precisely why Wine-bound work lives in this
repo rather than in an AGPL one: a patch written elsewhere in the project could
never be submitted upstream. Keep it that way.
