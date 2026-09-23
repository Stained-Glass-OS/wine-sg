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

## `patches/sg/0006-seed-new-user-hives-from-userdef.patch`

Patch 0002 creates a per-user HKCU hive on demand and creates it **empty**.
Windows does not: a new profile's hive is seeded from Default User. This patch
seeds ours from `userdef.reg`, which Wine already loads as
`\Registry\User\.Default` and which exists in every prefix.

**The empty hive is the most expensive bug this project has hit so far**, and
worth understanding, because nothing about the symptom points at the registry.
HKCU is where Wine keeps its *own* per-user defaults, including the display and
desktop settings under `Software\Wine\Explorer`. A user with an empty hive
gets none of them, so the shell starts believing the screen is Wine's built-in
1024x768 and lays itself out for that. On a 1280x800 desktop the visible result
is a 1024-wide, 4-pixel-high taskbar at y=764 — which reads as a rendering or
theming bug, in a component a long way from where the fault is.

Nothing errors. The session comes up, `explorer` runs, the desktop window is
the right size, and only the taskbar is wrong.

**The lesson is about method, not about Wine.** Five image rebuilds went on
guesses — the machine-level wineserver, token DACLs, startup ordering, stray
shells — each plausible, each about 25 minutes, none of them tested against
anything that could have said *no*. What settled it in one run was the control
that should have come first: `make test` in `sg-session` reproduces this gate on
the dev box in about two minutes. **Reproduce on the dev box before rebuilding
an image.** If a hypothesis cannot be stated as something `make test` would
falsify, it is not yet a hypothesis.

The tell, once you know it: `user-<uid>.reg` was ~134 bytes. A seeded hive is
~36KB and around 55 keys. `wc -c` on the hive would have found this immediately.

## `patches/sg/0007-session-0-has-no-interactive-shell.patch`

A shared system prefix runs a machine-level wineserver before anyone logs in.
On Windows that is session 0: a non-interactive window station, and no shell.
Wine already implements the rule — `get_desktop_window()` checks for
`__wineservice_winstation` and has the server create the desktop window instead
of starting `explorer`.

**It was unreachable from here.** A process's station comes from
`STARTUPINFO.lpDesktop`, set by its parent, and our Windows system is started by
a shell script. `services.exe` sets it for the services it launches but never
for itself, because on Windows its parent does that. So every session-0 process
landed on the interactive `WinSta0` and Wine started an explorer for it — and
the logged-in user's explorer then shared one wineserver with it. Two shells in
one prefix, and the taskbar laid out for whichever screen size arrived first.

`SG_WINSTATION` supplies the string `lpDesktop` would. It is consulted only
when the process parameter is absent, so anything started by a Windows parent
is unaffected. `sg-session` sets it in `sg-wineserver` and `sg-services-start`,
and **explicitly unsets it in `sg-run-explorer`** — the interactive shell must
be on `WinSta0`, and must not inherit session 0 from whatever started it.

**The second hunk is a safety property.** Upstream reaches the explorer path
only when the server declined to create the desktop window, which for a service
means the station or desktop name is wrong — and starting explorer then is
actively harmful, because that explorer inherits the same environment, hits the
same failure, and starts another. This is not hypothetical: `\\` inside POSIX
single quotes is *two* backslashes, so `__wineservice_winstation\\Default`
became a desktop named `\Default`, which cannot be created. The box went to
load 72 in seconds. A service has no shell to lose, so declining is both
correct and safe.

**Two explorers is the tell.** `pgrep -a explorer.exe` should show exactly one,
the session's `/desktop=shell,WxH`. A bare `/desktop` beside it is session 0
growing a shell, and it will fight the real one for the display mode.

## `patches/sg/0008-default-dacl-names-the-token-s-own-user.patch`

Patch 0002 gave every token the HKLM arrangement as its default DACL — Local
System and Administrators full control, Domain Users read — and left the
token's own user out. That is the right shape for HKLM and the wrong shape for
a token default, because it names everyone except the identity that matters.

**An object created by an ordinary user got a descriptor its own creator could
read and not write.** The user then could not create a subkey inside a key it
had just created. That is not a strict permission model, it is a contradiction:
no ordering of operations satisfies it. Windows names Local System *and* the
user for exactly this reason. Adding the user back keeps what patch 0002
needed — users at large still only read, administrators still write.

**The symptom was nowhere near the cause,** and it is the best example in this
repo of why a trace beats a guess. Wine's display setup creates
`HKLM\System\CurrentControlSet\Control\Video\{guid}\0000` from the session
process. Refused at the last component, it generates a *fresh guid* and tries
again — forever. What you see is `explorer.exe` at 100% of one core, a black
screen, a desktop window created but never named, and no error message
anywhere. Four plausible theories died before a `WINEDEBUG=+relay` trace showed
`RegCreateKeyExW` returning 5 in a loop over changing guids.

**If a Wine process spins at 100% with no syscalls, take a relay trace.**
`WINEDEBUG=+relay wine <prog> 2>&1 | tail -40` shows the repeating call pattern
and names the loop in one run. `/proc/<pid>/syscall` reading `running` is the
tell that it is a userspace loop and that no kernel-level tool will help.

## `patches/sg/0009-machine-wide-dll-overrides.patch`

Upstream reads DLL overrides from **HKCU only**. For a single-user prefix that
is the whole story; for a system-wide installation it leaves machine policy
with nowhere to live. In a shared prefix HKCU is per user, so an administrator
installing DXVK and VKD3D-Proton would have to write the overrides into every
existing user's hive *and* every hive created afterwards.

The failure is silent, which is what makes it worth knowing: the DLLs sit in
`system32` and `syswow64` looking installed, Wine loads its own builtins, and
Direct3D works — just not through the translation layer that was installed for
it. Nothing errors. You find out from frame rates, or from a probe that asks
which implementation answered.

HKLM is consulted **last**, after the environment variable, the per-application
key and the user's own key, so anything a user sets still wins. It is opened
**read-only**: HKLM is machine state an ordinary user must not rewrite, and
asking for `KEY_ALL_ACCESS` would fail for every non-admin — which, since patch
0002, is every interactive user.

## `patches/sg/0010-lockworkstation-asks-the-compositor.patch`

`LockWorkStation()` was a stub, so every Windows program and RMM tool that
locks the workstation silently did nothing. It now starts the native helper
`/usr/libexec/stained-glass/sg-lockctl LOCK` (from `sg-session`), which asks
`sg-compositor` over its control socket — Windows code cannot open that socket
itself, since Wine has no `AF_UNIX`. The helper has no authority; the
compositor checks peer credentials.

- **`\\?\unix\` path, not `Z:`.** Administrators often remove the drive mapped
  to `/`; locking must survive that.
- **It returns TRUE once the request is launched**, which is exactly Windows'
  documented contract ("initiated", not "succeeded"). It is also all it can
  know: Wine gives a native child a process handle that `WaitForSingleObject`
  rejects and that has no exit code. The outcome is in the compositor's audit
  log.

## `patches/sg/0011-display-config-keys-are-user-writable.patch`

`HKLM\...\Control\Video` and `\Control\GraphicsDrivers` hold the adapters,
monitors and modes Wine's display setup finds. On Windows the per-session
display driver owns this scratch; here every interactive user's win32u writes
it during display init and every app reads it back to find a graphics driver.

Under the shared-prefix HKLM policy (admins write, users read) a **non-admin
session** comes up with a working shell -- the driver host needs no registry --
and then **no app can launch**: `update_display_cache` reads an empty display
config, logs `Failed to read display config`, and the app dies with
`no driver could be loaded`. It hides completely on a developer box, where the
interactive user owns the prefix and so is an administrator.

Keys under those two paths get a DACL granting Local System, administrators
**and ordinary users** full access; the rest of HKLM is unchanged. The
containers must also exist for a user to create beneath them, and a non-admin
cannot create them under the admin-owned `Control` key -- so `sg-prefix-init`
creates `Control\Video` and `Control\GraphicsDrivers` once, as the owner, and
this patch gives them the writable DACL.

**The tell:** a session whose desktop and taskbar are correct but where every
app launch fails with "no driver could be loaded", only in the multi-user
image. `Failed to read display config` in the journal is the smoking gun.

## `patches/sg/0012-windows-10-taskbar.patch`

Explorer's taskbar (`Shell_TrayWnd`, `systray.c`) given a Windows 10 look **in
place** -- 40px dark bar, flat task buttons with the app icon and a purple
active-underline, the project's stained-glass diamond as the Start button, and
a clock. No protocol path changes: the tray, the taskbar buttons, the position
and `ITaskbarList` are exactly as upstream, so applications see the same shell.
This is ADR 0007's "upgrade the bar, don't overlay or hide it".

- **The Windows-10 metrics/colours are `#define SG_*` at the top of the file**,
  before `do_show_systray`. When one referenced `SG_CLOCK_TIMER` defined lower
  down, `systray.o` failed to compile and `make` fell back to linking the
  previous explorer -- so the taskbar looked unchanged and nothing said why.
  If a taskbar change does not show, check that `systray.o` actually rebuilt.
- The height is a one-line change to `tray_height`; the window repositions
  itself and winex11 follows with the work-area reservation.
- New UI that explorer does *not* have -- a Start menu, a notification centre --
  is not this patch; it is separate AGPL programs in `sg-shell`.

## `patches/sg/0013-flat-caption-buttons.patch`

The minimize, maximize/restore and close buttons drawn flat, Windows 10 style,
in `dlls/win32u/defwnd.c`: the caption colour behind a thin line glyph, a solid
fill while pressed (red for Close). Wine's visual-style support never reaches
the title bar, so no `.msstyles` could do this; the caption *colours* come from
`HKCU\Control Panel\Colors`, which sg-shell ships (`theme/50-sg-colors.reg`).

- **Only the window's own caption buttons change.** win32u's
  `draw_frame_caption` is used solely by the non-client painters; the public
  `DrawFrameControl(DFC_CAPTION)` is a separate copy in `user32/uitools.c` and
  stays classic for applications that call it.
- `defwnd.c` is on win32u's **Unix** side: the change lands in `win32u.so`, not
  in the PE `win32u.dll`s. Copying only the DLLs to test it shows no change.

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
- **An empty HKCU is silent.** Wine keeps its own per-user defaults there, so a
  user with an empty hive gets a working session laid out for the wrong screen
  rather than an error. Check `wc -c` on `user-<uid>.reg`: ~134 bytes means
  empty, ~36KB means seeded. See patch 0006.
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
