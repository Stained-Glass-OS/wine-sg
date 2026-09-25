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

## `patches/sg/0031-recolour-light-theme-to-stained-glass-purple.patch`

Wine's Light visual style accents themed controls, selection and window buttons
in Windows-10 blue; this recolours its source SVGs to the project's purple
(#3096fa->#7b2fbe, #2979ff->#5f2496, #e3f2fd->#f0e9fa). Greys, the red Close and
all non-accent colours are untouched.

**The theme packs .bmp/.cur/.ico, not the SVGs.** A normal (non maintainer-mode)
build uses the pre-rendered images shipped in the tarball, so editing the SVGs
alone builds blue. `build.sh`'s `regenerate_theme_images` re-renders them from
the patched SVGs after applying the series, exactly as `tools/buildimage` does
in maintainer mode -- it needs `rsvg-convert`, `icotool` and ImageMagick
(`make deps`, and Build-Depends for the .deb). If those are missing the build
warns and stays blue.

**tools/buildimage only adds rsvg-convert's `-o` when RSVG is exactly
`rsvg-convert`.** Pass the bare name, never a full path, or every render fails
with "Multiple SVG files are only allowed for PDF" and the theme silently stays
blue. Verify with `grep -c 7b2fbe` on a built `light.msstyles` (should be
non-zero) or on a rendered `.bmp`.

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

## `patches/sg/0014-clients-open-their-own-files.patch`

**Clients open their own files** (Stained Glass ADR 0013). Upstream Wine's
server open()s files for its clients; with the shared machine-level server
running as SYSTEM that gave every user SYSTEM's file rights (debt D14). Now
`open_unix_file()` in ntdll opens -- and creates -- the file as the client's own
user and sends the fd with `create_file`; the server adopts it, and refuses to
open by name for any client that is not its own Unix account.

- **The client mirrors the server's pre-open logic exactly**: flags per
  disposition, access to O_RDWR/O_WRONLY/O_RDONLY, the read-only retry for
  directories, and the *server's* errno-to-status table (ntdll's differs, and
  NtCreateFile's callers see these statuses). Change one side, change both.
- **New files start 0600** and get the server's computed mode (security
  descriptor or default) under the process umask after the reply, so they are
  never briefly readable by others.
- **The server takes the fd's path from `/proc/self/fd`**, never from the
  request: it may be unable to traverse the path, and must not trust it, since
  delete-on-close and rename act on that name.
- **Deletes, renames, reopening and permissions** followed in 0015 and 0016.
  Device nodes (`server/device.c`) still open as the server.
- **Conformance**: kernel32 file/directory/path/loader/module/process and ntdll
  file/directory/om/info -- 810,000+ tests -- show no regressions, and four
  `todo_wine` tests in kernel32:file now pass (delete-on-close on a read-only
  file under FILE_OPEN_IF returns STATUS_CANNOT_DELETE, as on Windows).
  Reproduce with a second object tree configured with tests
  (`build/obj-tests`) and compare against the previous package.

## `patches/sg/0015` and `0016`: the rest of a user's file operations

ADR 0013 stage 2. **Nothing path-based runs with the server's rights for a
client that is not the server's own Unix account** -- the server still makes
every decision, the client performs it.

- **Deletion**: the server still decides *when* (the last handle closing) and
  records *who asked* (`closed_fd.disp_uid`). For another user, the
  `close_handle` reply that makes it due lists the file (dev, ino, path) and
  that client unlinks it -- only if it runs as the user who asked. Anyone
  else's last close (SYSTEM, another user, process exit) leaves the file: no
  one's rights are borrowed. The list is built in `unlink_closed_fd` only
  during a `close_handle` request; anything queued elsewhere is discarded.
- **Rename and link**: `set_fd_name_info` has stages. 1 runs all the server's
  checks and performs it only for the server's own account, otherwise replies
  `client_performs`; the client (`sg_client_rename`) does it, mirroring
  `set_fd_name()` (target removal, `RENAME_NOREPLACE`, .exe/.com bits); 2
  commits the new names, read back from the fd. Stage 0 (one-shot) is refused
  to other users.
- **Reopening** (ReOpenFile; an empty name relative to a file handle): the
  client opens `/proc/self/fd/N` -- the kernel checks its rights on the inode
  -- and sends the fd with `open_file_object`; the server checks dev/ino match
  and adopts it.
- **Permissions** (0016): `set_security_object` returns the mode a DACL maps
  to (`unix_mode`) instead of applying it; the client `fchmod`s its own fd.
- **The server's view of paths must be the client's**: the unlink list carries
  the server's `/proc/self/fd` path. `sg-wineserver.service` must not get a
  private mount namespace (`PrivateTmp=`, `ProtectHome=`, ...).
- **Testing needs a second Unix user**, or none of this code runs: the same
  suites as a different user against a shared persistent server (`wineserver
  -p` as the prefix owner, group-shared prefix). What still fails there and
  why is recorded in ADR 0013; the only non-file item is that the server
  cannot signal another user's threads (debt D16).

## `patches/sg/0017-name-the-accounts-behind-per-user-sids.patch`

Debt D12. `sg_lookup_account` maps RID (1000 + uid) to the passwd name and
back, in the server, which assigned the SIDs. advapi32 asks for local SIDs with
a RID above 1000 it does not know, and for names it cannot otherwise resolve.
SYSTEM (prefix owner, root) is never answered -- it has its well-known name.

## `patches/sg/0018-define-userprofile-before-the-user-environment.patch`

Debt D15. ntdll now sets `USERPROFILE` (ProfilesDirectory + user name) before
reading `HKCU\Environment`, as Windows does, so the shared Default User
template can say `TEMP=%USERPROFILE%\AppData\Local\Temp`. sg-session's
`sg-prefix-init` writes the template that way; without this patch that TEMP is
left unexpanded.

## `patches/sg/0019-only-system-mints-administrators-tokens.patch`

Debt D17. **The server's token requests are a privilege boundary in a shared
prefix**, and upstream checks nothing: `create_token` (no
SeCreateTokenPrivilege check), `grant_process_admin_token`,
`create_linked_token` (returns `token_create_admin()` -- whose user, here, is
SYSTEM) and assigning a primary token. Each let a standard user become SYSTEM;
a `requireAdministrator` manifest alone did it, through the loader's
`elevate_token()`. Now a standard user's token is elevation type Default with
no linked token, and minting/assigning others' tokens is for the prefix owner
or root (`sg_requester_is_admin()`).

- **Any new request that creates or swaps a token must check
  `sg_requester_is_admin()`.** Review upstream rebases for new ones.
- `requireAdministrator` programs now run unelevated as the user. The real
  path is the elevation broker (ADR 0012); until then installers that need
  HKLM or Program Files fail as a standard user would on Windows with UAC
  declined.
- Gate: sg-session's `sg-token-check` (image: `make token-test`).

## `patches/sg/0020-owner-can-open-own-processes-and-threads.patch`

Debt D18. A process's default SD (`process_get_sd`) named Administrators only,
and threads had none -- so in a shared prefix a standard user could not open,
suspend, read the context of or debug **their own** processes. Each process
and thread now gets `sg_create_owner_sd(user)` -- the owning token's user,
Local System and Administrators, full access -- in a shared prefix only.

- **Any new server object a user should reach across processes** may need the
  same. The pattern is `sg_create_owner_sd()` on the object at creation.
- Not covered: attaching a debugger to your own process (`DebugActiveProcess`)
  still fails for a non-server user -- a further debug-object gap, tracked in
  the debt list.

## `patches/sg/0021-delegate-cross-uid-process-operations.patch`

Debt D16/D19 (ADR 0014). The shared server runs as SYSTEM, so the kernel
refuses it `ptrace`/`tgkill`/`sched_setaffinity` on an **ordinary user's**
processes — `ReadProcessMemory`/`WriteProcessMemory` across processes,
`DebugActiveProcess` (PEB write), async-APC thread signals, and thread
affinity all failed with `ERROR_ACCESS_DENIED`. The server now delegates each
to a per-user agent (`sg-procagent`, in sg-session) that runs as the target's
user and does the operation on that user's own processes. Same-uid operations
(the server's own account — including programs elevated to SYSTEM) are still
done directly; the whole thing is inert on a non-system prefix.

- **Delegation points**, all guarded by `sg_delegate_process()` (system prefix
  and `peer_uid != getuid()`): `read_process_memory`, `write_process_memory`,
  `send_thread_signal` (server/ptrace.c) and `set_thread_affinity`
  (server/thread.c, via `sg_agent_set_affinity`).
- **Rendezvous:** the server connects to `<prefix>/.sg-procagent.<uid>`,
  requires it to be a socket owned by that uid, and verifies `SO_PEERCRED`.
  `request.c` keeps the prefix path for `get_config_dir()`.
- **Wire protocol (must match sg-procagent):** request `{int op; int sig;
  uint pid; uint tid; uint64 addr; uint len;}` then `len` bytes for a write;
  reply `{int status; uint len;}` then `len` bytes for a read. Ops:
  READ=1, WRITE=2, SIGNAL=3, GETDR=4, SETDR=5, SETAFF=6. GETDR/SETDR (debug
  registers / hardware breakpoints) are reserved, not yet delegated —
  software breakpoints go through WRITE and work.
- **No new privilege anywhere:** the agent can only touch its own user's
  processes, exactly as that user could; a dead/absent agent means the op
  fails as it does today. Any new cross-uid ptrace/signal the server grows
  must delegate the same way.
- Gate: sg-session's `sg-procagent-check` (image: `make procagent-test`).

## `patches/sg/0022-route-runas-to-the-elevation-broker.patch`

ADR 0012. Since patch 0019 a standard user has no administrator token, so
ShellExecute's `runas` verb ("Run as administrator") ran the program
unelevated. Now, when `runas` has no token to elevate with, `SHELL_ExecuteW`
launches `sg-elevate` (the broker's client, in sg-session) through
`\\?\unix\`, with `--wine` so the broker runs the command under Wine as the
SYSTEM account after consent on the secure surface. Guarded: only the `runas`
verb with no usable token, only when `sg-elevate` is installed, and never
inside a program the broker started (`SG_IN_BROKER`, so no loop). Absent the
client, the old path runs unchanged. The broker itself is sg-session's
`sg-brokerd`; see ADR 0012.

## `patches/sg/0023-explorer-reads-the-graphics-driver-from-hklm.patch`

Debt D4. explorer read `HKCU\Software\Wine\Drivers\Graphics` only, so a
machine-level setting was written per user. It now reads HKLM first (SYSTEM
sets it in sg-prefix-init), HKCU still overrides -- same layering as
DllOverrides (0009).

## `patches/sg/0024-enforce-a-keys-dacl-when-creating-subkeys.patch`

S2 clause 3. Wine checked a key's DACL only on open; the recursive create path
never checked `KEY_CREATE_SUB_KEY` on the parent, so any user could create a
subkey under an admin-owned key (HKLM\Software\Policies). Now, in a system
prefix, creating a key checks the immediate parent's descriptor, and the SYSTEM
account may stamp the default descriptor onto an existing descriptor-less key
(system.reg carries none). Any new registry request that creates keys must keep
this property.

## `patches/sg/0025-shrestricted-reads-machine-policy-from-hklm.patch`

"Apply policies" (Group Policy). Windows evaluates a shell restriction from HKLM
(machine policy) as well as HKCU, machine winning. shell32's `SHRestricted` read
only HKCU, so a machine-wide policy could not be expressed. It now reads HKLM
first; with the HKLM policy branch administrator-owned (0024), an admin's policy
binds every user and a user cannot plant or override one. sg-session applies
`/etc/stained-glass/policy.d/*.reg` at boot; sg-shell's sg-start honours
NoClose/StartMenuLogOff.

## `patches/sg/0032-windows-applicationmodel-resourceloader-and-pri-reader.patch`

MRT string resources. `Windows.ApplicationModel.Resources.ResourceLoader`
(in windows.applicationmodel.dll) is built on `pri.c`, a reader for the
Package Resource Index format (`resources.pri`). Without it, every MRT-localised
program printed resource keys: winget's help read "ToolDescription".

- **The index is `resources.pri` beside the executable**, else `<exe>.pri`.
  That is the package root of an unpacked package, which is how packaged apps
  run here. A loader names a map below the root: `Resources` by default, or its
  name (`winget`). A key that starts with `/` is taken from the root. A missing
  resource is an empty string, not an error, as on Windows.
- **The format is undocumented.** pri.c is written from the description in
  independent readers (chausner/PriTools, Apache-2.0), not from any Microsoft
  code. It is **bounds-checked throughout** because an application ships the
  file. It was fuzzed with 20000 mutated indexes under ASan with no findings;
  redo that if you change the parser (build pri.c on the host with
  `-DPRI_HOST_TEST`).
- **Language choice**: the user's preferred UI languages
  (`GetUserPreferredUILanguages`), then the index's own fallback language.
  Other qualifiers (scale, contrast) prefer the default.
- **Gate:** `make test-resources` (`test/resources-gate.sh`). It uses a
  generated fixture (`test/mkpri.py`, so no Microsoft tool is needed) and
  checks default and named maps, a root path, data items, UTF-8, a missing
  key, `ms-resource:` URIs and German versus English candidates. With
  `WINGET_DIR` set it also reads winget's real, makepri-written index. The
  installed 10.0-7 fails it, because the class is not there.

## `patches/sg/0033-iertutil-parse-windows-foundation-uri-with-urlmon.patch`

`Windows.Foundation.Uri` used to store the raw string and nothing else.
Every property except RawUri was `E_NOTIMPL`, and AbsoluteUri echoed its input.
It now parses with urlmon's RFC 3986 `IUri` and answers each property from the
matching `Uri_PROPERTY`. Equals compares canonical forms, and
CombineUri/CreateWithRelativeUri use `CoInternetCombineUrlEx`. A non-absolute
string is `E_INVALIDARG`. QueryParsed is still unimplemented. The gate is the
Uri half of `make test-resources`.

## `patches/sg/0034`-`0042`: AppX/MSIX packages, and signature trust

This is what winget's community source needs, a signed MSIX. The work
exposed real trust bugs in Wine along the way. `make test-appx` and
`make test-compress` are the gates. `NETWORK=1` adds Microsoft's real
source package.

- **0034 kernelbase**: the package name functions
  (`PackageFamilyNameFromFullName` and the rest). The publisher ID is the
  first 64 bits of the SHA-256 of the UTF-16LE publisher, with a zero bit
  appended, as 13 Crockford base-32 digits. The known answer is
  `8wekyb3d8bbwe`.
- **0035 appxpackaging.dll** (new DLL): a ZIP/ZIP64 reader, the manifest
  reader (xmllite with DTDs prohibited) and package identity. **The block map
  is enforced**, because the signature covers only the block map. The archive
  and block map must agree when the package is opened, and every file is
  checked block by block when it is read. Bundles are detected
  (`APPX_E_MISSING_REQUIRED_FILE` for a package) but not yet read.
- **0036 cabinet: Compression API** (MSZIP, buffer mode and `COMPRESS_RAW`).
  The header is 24 bytes: `0a 51 e5 c0`, size `0x18`, 0, **checksum = low
  byte of the CRC-32 of the other 23 bytes** (found against real buffers),
  algorithm, uncompressed size, chunk size. Then come length-prefixed chunks
  of `CK` blocks: raw deflate, at most 32 KiB each, with the previous block's
  window as dictionary. XPRESS and LZMS are not implemented yet.
- **0037 the AppX SIP** `{0ac5df4b-ce07-4de2-b76e-23c839a09fd1}`. The digest
  is a record: `APPX`, then `AXPC`, `AXCD`, `AXCT`, `AXBM` and optionally
  `AXCI`. AXPC is the archive up to the signature's local header. AXCD is the
  central directory without the signature's record, plus the ZIP64 end record
  and locator rewritten without it, plus the end record. **This layout was
  established against Microsoft's own signed package and cross-checked against
  osslsigncode, an independent signer.** The signature must be the last entry.
- **0038**: wintrust opens verified files with share delete. winget never
  closes its `WTD_STATEACTION_VERIFY` state and then renames the file.
- **0039 SECURITY**: `SoftpubAuthenticode` fetched the signature hash into
  a 20-byte buffer and shared the result with the chain check. For every
  SHA-256 certificate the fetch failed, the chain policy was **skipped**, and
  WinVerifyTrust said trusted, **whatever signed it**. Now the chain policy
  always runs.
- **0040 SECURITY**: the base policy ignored `CERT_TRUST_IS_PARTIAL_CHAIN`, so
  a signer from an unknown issuer passed. It now fails with
  `CERT_E_CHAINING`.
- **0041**: Microsoft's Marketplace CAs mark application policies
  (`1.3.6.1.4.1.311.21.10`) critical. Once chains were really checked, Wine
  failed every such chain with `CERT_E_CRITICAL`. It is now supported, and
  enforced like an extended key usage.
- **0042**: crypt32 treated `HKLM\...\Root` as a cache of the host's CA
  bundle and **wiped any root an administrator or Group Policy added** at
  the next program start. It now records its own imports under
  `HKLM\Software\Wine\Crypt32\ImportedRootCerts` and keeps everything
  else.

**How the trust bugs hid:** 0039 made every verification "succeed", so the
real Microsoft package seemed to verify before its chain had ever been
checked. The gate's test certificates come from a CA generated at run time.
"Refused before the root is added, trusted after" is the case that proves
the chain is really consulted. Keep it.

**Adding a module changes `configure`.** `build.sh` now rechecks an existing
object tree when `configure` is newer than `config.status`. Without that, an
incremental build silently drops the new DLL.

## `patches/sg/0043`-`0045`: winget install, list and uninstall

- **0043 PackageCatalog** (windows.applicationmodel). winget's installed
  source opens it and subscribes with C++/WinRT **auto-revoke**, which QIs
  for `IWeakReferenceSource`. Without it winget crashed on a null deref. The
  weak reference resolves only while strong references remain; it takes one
  by compare-and-swap, never from zero. **Any WinRT object whose events
  C++/WinRT code subscribes to needs this**, so copy the pattern.
- **0044 PackageManager queries** return a real, empty `IIterable<Package>`
  rather than `E_NOTIMPL`. Nothing deploys MSIX yet, so empty is the truth.
  **MSIX deployment (AddPackageAsync) is the open item**: Store apps and MSIX
  packages from winget.
- **0045 `.msi` verbs pass `%*`.** Without it, ShellExecute of a .msi dropped
  winget's `/passive /log` and msiexec waited on its full UI forever. The
  entries have no `FLG_ADDREG_NOCLOBBER`, so `wineboot -u` repairs existing
  prefixes. **A wine.inf fix meant for existing machines must not use flag
  2.**

**Acceptance: `test/winget-e2e.sh`**, with `WINGET_DIR` set to a user-supplied
winget (never shipped) and network access. It runs search, show, install of
an MSI and an NSIS package, list and uninstall, in a fresh prefix. **GUI tests
run under `xvfb-run`, never on `$DISPLAY`**: an installer's window must not
land on the developer's desktop.

## `patches/sg/0046-win32u-reg-option-open-link-is-an-attribute.patch`

win32u's own `reg_create_key()` passed `REG_OPTION_OPEN_LINK` to `NtCreateKey`
as an option. The server ignores that; `RegCreateKeyEx` turns it into the
`OBJ_OPENLINK` attribute. As a result, retargeting the display-source links
(`Control\Video\{gpu}\NNNN`) followed each link and wrote into its old target.
With a virtual desktop, whenever the first display update ran before the
virtual source existed, two indices named one source and a monitor was left
with no source. That monitor reported win32u's **1024x768 default** as the
primary screen, and the taskbar was laid out for it (`1024x40+0+728` on a
1280x720 desktop). This is timing-dependent: the session gate passed for a
long time and then failed on every run.

How it was found, for next time: a clean dump of the live registry
(`SG_TEST_HOLD` in sg-session's harness) showed `SymbolicLinkValue` values
written *inside* source keys. A 30-line standalone test then showed that
retargeting works only through an `OBJ_OPENLINK` handle. **Dumping the
registry with `wine reg` perturbs it**, because reg.exe runs its own display
update on the non-virtual desktop. Trust traces from the process under test
more than a dump taken afterwards.

## `patches/sg/0047-server-admit-a-peer-holding-the-prefix-group.patch`

Patch 0001 admits members of the prefix's group by asking NSS
(`getgrouplist`). A process can hold the group with no group file saying so
(pam_group, `sg`/`newgrp`, a unit's `SupplementaryGroups=`), and the kernel
then let it open every file in the prefix while the server refused the
connection. The server now also admits a peer whose **own** supplementary
groups, as the kernel reports them for the connection (`SO_PEERGROUPS`),
include it -- the same membership file access is checked against, so nobody
is admitted who could not already open the prefix. (Found building D1; the
domain join itself writes the membership into the group file, because greetd's
`initgroups()` drops pam_group's additions.)

## `patches/sg/0048-server-display-config-keys-stay-writable-when-stamped.patch`

0011 gives the display-config keys (`Control\Video`, `Control\GraphicsDrivers`)
a user-writable DACL only when a key is **created**; 0024's path that lets
SYSTEM stamp the default descriptor onto an existing descriptor-less key did
not make the exception. Security descriptors are not saved, so a container
re-stamped by SYSTEM after the server started came out "users read", no
session could record its display, and win32u's display setup asserted
(`add_monitor: !list_empty(&sources)`) -- the login screen crashed at every
boot. Seen on a machine with Microsoft Edge installed; it depends on which
process touches the key first. **The tell:** `reg add` under
`HKLM\System\CurrentControlSet\Control\Video` fails for sggreet or a user.

## `patches/sg/0049`-`0054`: what Microsoft Edge needed

Edge is user-installed (never shipped). These are the exports it calls that
Wine lacked; each missing one was a crash, because Chromium's delay-load
failure hook crashes on purpose (the minidump shows the DLL name and 127,
`ERROR_PROC_NOT_FOUND`, on the stack).

- **0049 wofutil `WofSetFileDataLocation`**: Edge's setup compresses what it
  installs through the Windows Overlay Filter. Implemented as Windows does it
  (`FSCTL_SET_EXTERNAL_BACKING`); our file systems decline, nothing crashes.
- **0050 user32** `IsWindowArranged` (FALSE), `GetPointerDevice` and
  `GetPointerPenInfo` (no devices, no pen pointers: `ERROR_INVALID_PARAMETER`).
- **0051 powrprof** `PowerReadACValue` (no schemes: `ERROR_FILE_NOT_FOUND`) and
  the effective-power-mode notifications (always Balanced, callback right after
  registering, on a pool thread).
- **0052 wininet** `InternetGetCookieEx2` / `InternetFreeCookies`.
- **0053 userenv** `DeriveAppContainerSidFromAppContainerName` -- SHA-256 of
  the lower-cased UTF-16LE name; reproduces the published SID of
  `Microsoft.MicrosoftEdge_8wekyb3d8bbwe`.
- **0054 advapi32** `AddConditionalAce` refuses (`ERROR_NOT_SUPPORTED`):
  nothing evaluates conditional ACEs, and an ACE without its condition would be
  wrong.

**Gate:** `make test-edge` (`test/edge-e2e.sh`, `test/edgeapi-probe.c`,
`test/token-probe.c`); with `EDGE_MSI` set it installs Edge silently and checks
a page loads, runs its script and paints -- **with its sandbox off and on**.
A prefix owner is an administrator, so on a dev box Edge says "running
elevated" and relaunches itself de-elevated through the shell
(`--do-not-de-elevate` avoids it).

## `patches/sg/0055-sandbox-restricted-and-appcontainer-tokens-integrity-levels.patch`

Chromium's sandbox, which Edge runs by default. It needs restricted tokens,
integrity levels and, for Edge's renderers, **AppContainer (lowbox)
tokens**. Before this patch the sandboxed tab stayed blank and nothing
logged why. Edge looks up `kernelbase!CreateAppContainerToken`, finds
nothing, and never starts a renderer: no error and no child process.
DevTools shows the tab with `pid 0`. The way in was bisecting Edge features
(`--disable-features=RendererAppContainer` made the page load). A relay
trace of the launch thread then showed the failed `GetProcAddress`.

- **Restricted tokens**: the token keeps its restricting SIDs. An access
  must be granted by the token's own SIDs **and** by the restricting SIDs.
  A write-restricted token is checked twice only for writes, where "writes"
  means the generic write mapping minus `READ_CONTROL|SYNCHRONIZE`: the file
  mapping names both, and without excluding them a write-restricted token
  could not even read. A disabled user SID is deny-only.
  `DISABLE_MAX_PRIVILEGE` keeps only SeChangeNotifyPrivilege.
- **Integrity**: read from the mandatory label; lowering is allowed and
  raising is refused. Wine still does not *enforce* no-write-up.
- **AppContainer**: `NtCreateLowBoxToken`, `CreateAppContainerToken`, and
  `PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES` in CreateProcess. The token
  is Low integrity, and its default DACL also grants the package. An access
  must additionally be granted by the package SID, a capability, or ALL
  APPLICATION PACKAGES (S-1-15-2-1). Not done: a per-package named-object
  directory. LPAC is not distinguished, so ALL APPLICATION PACKAGES grants
  it too.
- The access check's owner+DACL walk is `dacl_access()`, run once per SID
  set (own / restricting / package). **Any new kind of token restriction
  should be another SID set there**, not another copy of the walk.
- Also: TokenSecurityAttributes (class 39) answers an empty list, and
  UpdateProcThreadAttribute accepts the documented policy attributes.
  CreateProcess still ignores the mitigation and child-process policies,
  and logs them as `Unsupported attribute 0x2000e/0x2001a`.
- **This patch adds server requests, so every later request number moves.**
  After changing `protocol.def`, rebuild and install the *whole* tree
  (`make` in `build/obj`, then `sudo make install` there). Installing only
  ntdll.so and wineserver leaves win32u.so and the rest on the old numbers.
  Nothing reports that; things break at random. Here conhost found no font
  and divided by zero, and kernel32:process hung in a winedbg/conhost storm.
- **Conformance**: advapi32:security 3554 tests and kernel32:process 3099
  tests, 0 failures (same as 10.0-13). `test/token-probe.c` checks each
  rule with AccessCheck.

## `patches/sg/0056`-`0057`: network drives, UNC paths, `net use`

A domain user's home drive, a logon script's `NET USE`, and any program that
opens `\\server\share\...`. The mounting is sg-session's `sg-netmountd` (root,
socket-activated): kernel CIFS, `multiuser,sec=krb5,cruid=<requester>`, so
every user reaches a share with their **own** Kerberos ticket and the file
server checks each one. Wine only finds the results.

- **0056 ntdll/kernelbase.** Drive letters are looked up first in
  `/run/stained-glass-net/drives/<uid>/` (root-owned symlinks), then in the
  prefix's `dosdevices`. This is a logon session's own DosDevices, searched
  before Global. `\??\UNC\server\share` resolves under
  `/run/stained-glass-net/unc/` when that path is a real mount point (checked
  with `st_dev`, so an empty directory a failed mount left does not count).
  Otherwise ntdll asks `/run/stained-glass-net/netmount.sock` to mount it and
  caches a failure for 10s. The per-user letter list is rescanned once a
  second, and at once on a miss for a letter the prefix lacks too.
  `GetLogicalDrives` probes the letters missing from `\DosDevices`.
  **ntdll's lookup appends the prefix component ("unc", "h:") to the base
  directory it chose**, so the UNC base is `/run/stained-glass-net/`, not
  `.../unc/`. Getting that wrong gives `.../unc/unc` and "path not found".
- **0057 ntlanman.dll**: the Microsoft Windows Network provider, registered
  in wine.inf (`NetworkProvider\Order` = LanmanWorkstation). mpr already
  dispatches WNetAddConnection2/3 and WNetCancelConnection2 to providers.
  Its Unix side (`ntlanman.so`) talks to sg-netmountd; the params are
  pointer-free, so the same functions serve WoW64. mpr's WNetGetConnection
  asks the providers when the mount manager does not know a remote drive.
  net.exe gained `NET USE X: \\server\share` and `/DELETE`. Explicit
  credentials are not supported: the connection is always the signed-in
  user's.
- **Wire format** (must match sg-session `domain/sg-netmountd.c`): one line,
  fields separated by **tabs** (a share may contain spaces):
  `MOUNT<TAB>server<TAB>share`, `MAP<TAB>letter<TAB>server<TAB>share[<TAB>dir...]`,
  `UNMAP<TAB>letter`. The reply is `OK <path>` or `ERR <errno> <why>`.
- **Gate:** sg-image's `make domain-test`. alice's H: comes from
  homeDirectory; her NETLOGON logon script writes to H: and runs
  `net use S: \\dc1\shared`; a Windows program reads H: and a UNC path; dave
  does not get her letters.

## `patches/sg/0058`, `0059`: what domain logon scripts needed

- **0058 ntdll path.c**: `skip_unc_prefix` counted every separator after
  `\\server\share` as part of the root, which is never collapsed, so
  `\\server\share\\file` stayed as it was and failed with
  `ERROR_INVALID_NAME`. cmd's command search appends `\` + name to a
  directory that already ends in `\`, so every script at a share root
  (`\\DOMAIN\NETLOGON\logon.bat`) was "not recognized". Deeper paths
  worked, which is why a test script in a subdirectory passed. (Still open:
  `dir \\server\share` in cmd prints "Directory of Z:\server".)
- **0059**: `USERNAME` comes from the user's Volatile Environment, which only
  wineboot's own user ever had in a shared prefix. ntdll now sets it from the
  signed-in user. wineboot only fills USERDOMAIN, LOGONSERVER and HOME* when
  they are missing, so sg-domain-logon's values (imported by the session)
  survive any later wineboot.

## `patches/sg/0060`-`0062`: MSIX deployment

- **0060 appxdeploymentclient**: AddPackageAsync/UpdatePackageAsync (file:
  URIs), RegisterPackageAsync (a loose manifest, in development mode) and
  RemovePackageAsync, as `IAsyncOperationWithProgress<DeploymentResult,
  DeploymentProgress>` on the thread pool (`async.c`). `deploy.c` does the
  work. **The signature must be trusted**: WinVerifyTrust through the AppX
  SIP (0037), and a refusal becomes the DeploymentResult's
  ExtendedErrorCode. The package is read with appxpackaging, which checks
  every file against the signed block map, and extracted to
  `%ProgramFiles%\WindowsApps\<full name>`, or
  `%LOCALAPPDATA%\Programs\WindowsApps\...` for a user who may not write
  there. Names that are absolute or contain `..` are refused. The
  registration is `HKCU\Software\Wine\AppModel\Packages\<full name>`
  (identity, InstallLocation, display properties, the Start menu shortcuts
  it made). **That key is an interface**: kernelbase (0061) reads it, so
  change both together. Package objects (`package_obj.c`, `applist.c`):
  IPackage, 2 and 3, IPackageId, a StorageFolder that knows its path, and
  AppListEntry.LaunchAsync.
- **0061 kernelbase**: a process has a package's identity when its
  executable is inside that package's InstallLocation, however it was
  started (GetCurrentPackageFullName/FamilyName/Id/Path;
  GetPackagePathByFullName; GetPackagesByPackageFamily).
- **0062**: ApplicationData.Current is one per process.
- **Conformance**: windows.applicationmodel:model registers, finds,
  launches and removes its package, with 0 failures (174 tests, plus 44 in
  the launched packaged app). The test assumed a system already has
  packages; it now accepts an empty list.
- **Gate:** `make test-appx`. `test/deploy-probe.c` checks that unsigned,
  untrusted and tampered packages are refused, and that a trusted one is
  extracted byte for byte, found, given a shortcut, and removed.
- **0063 bundles**: appxpackaging's bundle reader (the package reader's
  checks via `package_open`; the bundle manifest; payload package info). The
  interface IIDs come from Microsoft's published win32metadata, via the
  MIT-licensed `windows` crate. A bundle's signature names the **bundle
  SIP** `{0f5f58b3-aade-4b9a-a434-95742d92eceb}` over the same digest
  record. wintrust accepts it only for a file with
  `AppxMetadata/AppxBundleManifest.xml`, and the package SIP only for a file
  with `AppxManifest.xml`. Deployment installs the application package for
  this machine's architecture; resource packages are not installed yet.
- Not yet: resource and dependency packages, app execution aliases,
  GetCurrentPackageInfo, deployment for another user.

## `patches/sg/0064`-`0066`: text and scroll bars

David: the fonts looked crappy and the scroll bars too. Two causes, two fixes.

- **0064: the Windows font smoothing setting decides.** win32u took each
  font's anti-aliasing from the host's fontconfig first and only then from
  `HKCU\Control Panel\Desktop` (`FontSmoothing`, `FontSmoothingType`,
  `FontSmoothingOrientation`). Debian's fontconfig says `rgba none` for every
  font, so text could never be ClearType, and neither the Control Panel nor
  `SPI_SETFONTSMOOTHING*` could change it. Now, when the setting exists, it
  wins; a LOGFONT quality still overrides it, as on Windows.
  **winex11 has a second override of its own** (`Xft.antialias`/`Xft.rgba` X
  resources, `get_xft_aa_flags`), used only on its XRender path and only when
  those resources are set; our sessions draw through window surfaces (the DIB
  engine) and set none.
- **0065: Stained Glass scroll bars**, drawn flat -- track `#f0f0f0`, a
  borderless inset thumb `#cdcdcd`/`#a6a6a6` hot/`#606060` pressed, solid
  triangle arrows, no gripper. The SVGs are **generated** by
  `theme/scrollbar.py`; change the script and regenerate, never hand-edit
  them. The grids are Light's: arrows 20 cells (4 directions x
  normal/hot/pressed/disabled, then the 4 hover cells), thumbs and tracks 5.
  Removing the gripper *sections* is what stops the gripper being drawn:
  uxtheme draws it only when `GetThemePartSize` succeeds.
- **0066:** the colours Light's INI names itself (progress bar, Highlight,
  command links, task dialog instructions) were still blue after 0031.

The interface *font* is not here: it is sg-shell's defaults
(`theme/52-sg-fonts.reg`: Segoe UI 9 pt in the window metrics, Segoe UI / MS
Shell Dlg / Tahoma substituted by Inter, metric-compatible Replacements for
Arial, Times New Roman, Courier New, Calibri, Cambria, Consolas).

**Gate: `make test-theme`** (`test/theme-gate.sh`). `theme-gallery --render`
draws text and the scroll bar's parts into a DIB in-process -- pixels that do
not depend on window placement -- under a fontconfig that forces greyscale;
it requires colour fringes for ClearType, grey only for standard smoothing,
two colours for off, and the scroll bar's colours, and that Light's border
grey is gone. Against a Wine without these patches it fails 9 of 11 checks.
`theme-gallery` with no arguments is a window of the common controls to look
at; screenshot it under xvfb when judging the theme.

## `patches/sg/0067`: cloaked windows (virtual desktops' primitive)

Windows' virtual desktops are the shell cloaking windows, and programs cloak
their own with `DwmSetWindowAttribute(DWMWA_CLOAK)`. A cloaked window keeps
`WS_VISIBLE` and gets no messages, but is not drawn, clipped or hit. Wine runs
as one virtual desktop inside our compositor, so this has to live in Wine.

- **server:** `is_shown()` (visible and not cloaked) replaces the raw
  `WS_VISIBLE` tests in everything about drawing, clipping and hit-testing.
  The bits change only through `set_window_pos`'s paint flags
  (`SET_WINPOS_CLOAK`/`UNCLOAK`/`CLOAK_SHELL`), inside the internal
  `set_window_pos`, so exposure is the hide/show code's. Mirrored in the
  `__wine_cloaked` property for other processes. **`cloaked` must be
  initialised in `create_window`**: the server's allocator fills with 0x55,
  and an uninitialised field cloaked every window -- a black screen.
- **win32u:** `set_window_cloak()` re-applies the current rects and surface
  through `apply_window_pos` (no messages). `NtUserSetWindowCloak` from any
  thread (`WM_WINE_SETCLOAK`, sent without waiting); `NtUserGetWindowCloaked`.
- **winex11:** `get_shown_style()` -- a cloaked window is unmapped.
- **dwmapi:** `DWMWA_CLOAK` (own top-level windows), `DWMWA_CLOAKED`.

**Gate: `make test-cloak`** (`test/cloak-gate.sh`, `test/cloak-probe.c`).
The harness must be faithful to a session or it lies: its programs join the
shell's desktop (`HKCU\Software\Wine\Explorer\Desktop=shell`, as
sg-run-explorer sets) -- outside it, even a plain `SW_HIDE` leaves the hidden
window's pixels on the screen, and a second program's overlapped window dies
on an X `BadWindow` (stock Wine too). The probe's steps are synchronised
through files, so screenshots are taken when each step is done.
`ARTIFACTS=DIR` keeps the screenshots. Against a Wine without the patch every
behaviour check fails.

## `patches/sg/0068`: virtual desktops, Task View and Alt+Tab

Explorer's `vdesktop.c`, on 0067's cloaking. An **application window** (what
gets a taskbar button: unowned, not a tool window, not `WS_EX_NOACTIVATE`,
not explorer's own) is on one desktop; owned windows follow their root owner;
everything else is on all desktops. Switching shell-cloaks the other
desktops' windows. Keys: Win+Ctrl+D / F4 / Left / Right,
Win+Ctrl+Shift+Left/Right (carry the active window), Win+Tab (Task View,
also the taskbar button beside Start). Task View: desktops strip on top
(scaled window maps), thumbnails, hover a desktop to see its windows, drag a
window onto a desktop, right-click menu.

- **State is Windows':** `HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VirtualDesktops`
  -- `VirtualDesktopIDs` (GUIDs), `CurrentVirtualDesktop`, `Desktops\{id}\Name`.
  A window's desktop: its root owner's `__sg_vdesk` property (1-based);
  `__sg_vdesk_pinned` = on every desktop.
- **Driving it:** the registered message `SgVirtualDesktopCommand` to
  `Shell_TrayWnd` -- wp 0 query (count | current<<8), 1 switch to lp, 2 new
  (lp: go there), 3 close lp, 4 toggle Task View, 0x100+d move window lp to
  desktop d. shell32's `IVirtualDesktopManager` and `test/vdesk-probe.c` use it.
- **Thumbnails** come from the screen while a window is the foreground one
  (captured every second, and before a switch): Wine has no cross-process
  `PrintWindow` (GDI handles are per process), and a cloaked window has no
  pixels. A window never active shows as an icon card. A real cross-process
  capture (the owner renders into a shared section) is the upgrade.
- **Alt+Tab** (Shift+Alt+Tab back): the current desktop's windows, most
  recently used first, with thumbnails. It commits on Alt's release, watched
  with a `WH_KEYBOARD_LL` hook only while it is up. **Test it with X-level
  keys (xdotool), not SendInput:** injected keys are not down in the X
  server, and winex11 releases them when focus moves -- Alt "lets go" 6 ms
  after the switcher opens. A person's keys (XWayland) are X keys.
- **The gate's explorer must own the desktop:** it waits for "desktop
  message loop starting" before starting programs, or a program starts an
  explorer of its own and the traced one is not the shell.

**Gate: `make test-vdesk`** (`test/vdesk-gate.sh`): 29 checks through the real
hotkeys (SendInput; Alt+Tab with xdotool), the probe's view of cloaking and properties,
`IVirtualDesktopManager`, the taskbar's buttons, the registry and the X
server's pixels. 23 fail on a Wine without the patch.

## `patches/sg/0069`: wallpaper in any format, Windows' styles

`user32/desktop.c` read the wallpaper with `LoadImage(IMAGE_BITMAP)`: BMP
only, centre or tile. Now WIC (JPEG, PNG, BMP, GIF, TIFF) -- windowscodecs is
loaded on demand, **both `WICCreateImagingFactory_Proxy` and
`WICConvertBitmapSource` by `GetProcAddress`** (user32 must not import it; a
direct call links nowhere and the old user32 silently stays in the build
tree -- check the link, not just the compile), with COM initialised for
WIC's decoders. `WallpaperStyle` 10 Fill, 6 Fit, 2 Stretch, 22 Span, 0 Center
/ Tile; the picture is laid out once per desktop size.

**Setting a wallpaper painted over every window and the taskbar** (stock
Wine too): surfaces are flushed with `IncludeInferiors`, so the desktop's
new picture lands on the windows. Explorer now asks every visible window to
repaint 250 ms after a wallpaper change. Proper fix (desktop flush clipped
by children) is still open.

**Gate: `make test-wallpaper`** -- every style from a PNG and a JPEG on the X
server's pixels; a window and the taskbar intact after a change (a build
without the repaint fails those two). The desktop is only drawn once
something is on it: the gate opens a window first, away from the checks.

## `patches/sg/0070`: a standard user's shell folders

Found by the installer work: each user's `Shell Folders\Desktop` was written
**empty** at login, so ShellExecute of any `.lnk` failed ("file not found")
and explorer showed no desktop icons. shell32 opened
`HKLM\...\ProfileList` with `KEY_ALL_ACCESS` just to read it; standard users
cannot write HKLM on the system prefix, the open failed, nothing expanded,
and the empty result was cached. Now read-only fallback, defaults if even
that fails, and an unexpanded path is never cached.

**Gate: `make test-shellfolders`.** A plain prefix lets everyone write HKLM,
so the gate gives ProfileList an explicit DACL (administrators write,
everyone read) and runs the lookup with Administrators disabled
(`CreateRestrictedToken`, patch 0055) -- and first checks that the child
really is denied, or the gate proves nothing. Stock: empty path, empty cache.

## `patches/sg/0071`: the Windows key's shortcuts

`programs/explorer/shellkeys.c`: the Windows key alone opens Start (a
`WH_KEYBOARD_LL` hook: on release, only if nothing else was pressed while it
was down); Win+D (and back), Win+M, Win+Shift+M; Win+Left/Right snap to half
the monitor's work area (and back from the other side), Win+Up maximize,
Win+Down restore/minimize; Win+E File Explorer, Win+R Run (RunFileDlg,
ordinal 61, on its own thread), Win+I and Win+Pause the Control Panel, Win+X
the quick-link menu. Hotkey ids 0x5700.. share the tray's `WM_HOTKEY` with
the virtual desktops' (0x5600..).

**Gate: `make test-shellkeys`** -- 12 checks typed with xdotool (X keys; see
0068 on SendInput). On stock, 4 "put back"/"restore" checks pass vacuously
(nothing moved), after the step before them failed.

## `patches/sg/0072`: control.exe follows App Paths

`system32\control.exe` run directly (CreateProcess, not ShellExecute) now
starts the Control Panel `HKLM\...\App Paths\control.exe` names
(sg-shell's sg-control) with the same arguments. `SG_CONTROL_HANDOFF=1` in
its environment means "do not hand off again", so sg-control can run
control.exe for Wine's own applets. Gate: `make test-control`.

## `patches/sg/0073`-`0075`: snapping by dragging, and the work area

- **0073:** win32u sends `EVENT_SYSTEM_MOVESIZESTART` (after
  `WM_ENTERSIZEMOVE`) and `EVENT_SYSTEM_MOVESIZEEND` (window placed).
- **0074: `SPI_SETWORKAREA` is global.** Stock Wine kept it per process and
  took `rcWork` from the driver (a virtual desktop's whole screen), so
  maximized windows covered the taskbar. Now it writes the monitor's
  registry record, bumps the monitor serial (`set_winstation_monitors`) and
  the video key; and saves **insets** in `HKCU\Software\Wine\WorkArea`,
  re-applied in `add_monitor` -- because explorer sets the desktop size
  (a display change, re-enumerating monitors from the driver) *after* the
  taskbar is up, which silently threw the first attempt away. The taskbar
  sets it when placed.
- **0075:** explorer snaps a dragged window at the left/right edge (half),
  top (maximize), with a region-frame preview `SgSnapPreview` beneath it.

Gate: `make test-shellkeys` (now 16 checks: work area 0,0,1024,660, maximize
above the taskbar, drag preview/snap/top).

## `patches/sg/0076`-`0077`: PrintWindow across processes; real thumbnails

- **0076:** `NtUserPrintWindow` for another process's window creates a
  section, duplicates it into the owner (`NtOpenProcess(PROCESS_DUP_HANDLE)`
  + `NtDuplicateObject`) and sends `WM_WINE_PRINTWINDOW` (wparam: the
  owner's handle; lparam: flags<<30 | width<<15 | height, 15 bits each). The
  owner draws into a DIB on the section -- `WM_PRINT`, or with
  `PW_RENDERFULLCONTENT` (defined now, 0x2) a copy of its surface's
  `color_bitmap`, which survives cloaking -- and **always closes the
  section handle**. Changes `ntuser.h`'s internal message enum and
  `winuser.h`: rebuild everything.
- **0077:** explorer's thumbnails use it (Task View refreshes every window
  on open, Alt+Tab the current desktop's); the screen grab stays as the
  fallback.

Gate: `make test-printwindow` (stock returns black for all three).

## `patches/sg/0080`: WinSta0\Default is the user's desktop

Programs name `WinSta0\Default` in `STARTUPINFO.lpDesktop` (Mozilla-derived
launchers/updaters; LibreOffice's first start relaunches through one). Our
sessions' desktop is "shell", so the child landed on an empty "Default"
desktop with no shell -- "no driver could be loaded", no window. In
`winstation_init` (win32u), Default on WinSta0 now means the configured
desktop (`get_default_desktop`), exactly as when none is given. Gate:
`make test-default-desktop` (a child started with that lpDesktop must land on
"shell" and make a window; stock: "Default", no window).

## The application compatibility suite (`make test-compat`)

`test/compat/apps.list` pins real Windows applications (official URL +
sha256, silent-install arguments, the program, an optional smoke command);
`test/compat/run.sh` installs each into a fresh prefix joined to a shell
desktop under xvfb, checks the files, runs the smoke command, launches the
program and waits for its largest new top-level window (a splash comes first;
`%VAR%` in the path is expanded), screenshots it, and closes it as a person
would (first-run dialogs in front first). Results: `results.md`/`.tsv`, a
screenshot and log per application in `ARTIFACTS` (default
`build/compat-results`). `SG_DEFAULTS=../sg-shell/theme` imports the system's
registry defaults (fonts) first, as an installed machine has them -- the make
target does. Installers are cached in `~/.cache/sg-compat`, hash-checked, and
**never committed or shipped** (users install their own software). Not part
of `make test`: it needs the network and ~1 h. The exit status counts the
applications with a failing stage.

- **Keep the probe's output in a file, not a pipe:** a launched program
  inherits the pipe and holds it open -- the runner hung on 7-Zip that way.
- A tray program (`tray:` prefix) has no window; it must still be running.

## `patches/sg/0078`-`0079`: native stdio; ipconfig and netsh on sg-netctl

- **0078:** `fork_and_exec` (ntdll/unix/process.c, not `spawn_process` --
  both have the same stdio block; edit the right one) passes files it is
  given as a native child's stdin/stdout/**stderr** even when detached (no
  console: GUI programs, `CREATE_NO_WINDOW`); stock closed them. **Wine pipes
  have no Unix fd** and still do not reach a native child -- use a file.
- **A native child's process handle does not wait for it** (fork_and_exec
  double-forks). `include/wine/sgnetctl.h` runs the program under a fixed
  `/bin/sh -c '"$@"; echo "SGNET-EXIT:$?"'` (program and arguments are
  positional parameters) and polls its output file for the marker.
- **0079:** ipconfig `/release` `/renew` (DHCP adapters; `prefix*`) and
  `/flushdns`; netsh `interface ip|ipv4 set address|dns`, `add dns`, `show
  config`, `interface show interface`, `wlan show networks|profiles`,
  `connect`, `disconnect`. Adapter names are sg-netctl's device names. A
  denial prints Windows' elevation message; unknown commands still succeed
  quietly (installers). sg-netctl treats SYSTEM (elevated) as an
  administrator (sg-session).
- **A new header is not in makedep's dependencies** until the Makefile is
  regenerated: touch the including `.c` after changing it, or you test the
  old binary.

Gate: `make test-netsh` (a stand-in sg-netctl via `SG_NETCTL`; 14 checks,
13 fail on stock). The real thing: sg-image's net gate.

## Things that will bite you

- **`patches/fixes/binutils2.44.patch` is not optional on Debian trixie.**
  Without it, `winebuild` emits import address tables into read-only sections,
  and every process dies during module load with a write fault at an address
  inside `ntdll`'s `.text`. Upstream Wine 10.0 built on trixie without this
  patch produces a Wine that cannot start `cmd`. Debian carries it; upstream
  tarballs do not. WineHQ bug 57819.
- **The source tree follows the patch series automatically.** `build.sh`
  fingerprints `wine-version` plus every patch in `series` into
  `build/wine-<ver>/.sg-series`; when that changes it re-patches in a scratch
  tree and copies over only files whose content differs, so the object tree
  rebuilds exactly what changed. Before this, the series was applied only on
  first unpack, and a new patch silently never reached an existing tree. Hand
  edits to `build/wine-<ver>/` are overwritten on the next series change --
  put changes in a patch.
- **Package builds keep the object tree.** `debian/rules` overrides
  `dh_auto_clean`, which used to `rm -rf build/obj` and recompile all of Wine
  on every `.deb`. `make distclean` for a from-scratch build.
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
