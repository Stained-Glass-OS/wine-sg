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

## `patches/sg/0200`-`0204`: MSIX beyond one package; winget installs MSIX

What 0060-0063 left out, and what winget needed to install Store/MSIX
packages. `make test-msix` (`test/msix-gate.sh`, `msix-probe.c`, fixtures
written by `test/mkmsix.py` -- our makeappx equivalent -- and signed with a
certificate generated for the run). 42 checks with `WINGET_DIR`; stock
10.0-43 fails 30 of them (the other 12 are set-up or vacuous).

- **0200 iertutil**: `Windows.Foundation.Uri("C:\\x.msix")` is
  `file:///C:/x.msix`, as on Windows. winget hands PackageManager its
  downloaded MSIX that way; refusing it was winget's "0x80070057 Invalid
  parameter" on every MSIX install.
- **0201 appxpackaging**: the manifest's PackageDependency and Resource
  elements, and a bundle payload's qualified resources (language, scale).
- **0202 aliases** (`include/wine/appexeclink.h` -- registered in
  `include/Makefile.in`): Windows' alias is an `APPEXECLINK` reparse point
  in `%LOCALAPPDATA%\Microsoft\WindowsApps`; ours is a small UTF-16 text
  file (family, AUMID, target). kernelbase's CreateProcess runs the target
  in its place **only if the target is inside the install location of a
  deployed package of that family**; shell32's `SHGFI_EXETYPE` reports the
  target's type, or cmd.exe does not wait for a console program started
  through an alias (errorlevel 0 instead of its exit code).
- **0203 appxdeploymentclient**: dependency URIs are deployed first; every
  PackageDependency must be met or nothing installs (0x80073CF3, text naming
  the framework like Windows'); record value `Dependencies` (REG_MULTI_SZ of
  full names) -- **ntdll and kernelbase read it: the record is an
  interface**. A framework in use cannot be removed (0x80073CFA). Newer main
  package replaces older; older refused (0x80073D06) unless
  ForceUpdateFromAnyVersion; frameworks side by side. Bundles install the
  resource packages for the user's languages and the scale nearest 100
  (records with `ResourcePackage`=1, listed in the app's `ResourcePackages`,
  removed with it). Aliases (record `Aliases`) and the WindowsApps folder on
  the user's `HKCU\Environment\Path`. http(s) locations are downloaded.
  IPackageManager6 (**winget calls `RequestAddPackageAsync`**),
  RemovePackageWithOptions, typed queries (untyped = main + framework),
  IPackage4 (**winget's installed list QIs for it**; SignatureKind
  Enterprise, Developer for dev-mode). winget lists an MSIX as
  `MSIX\<full name>`; uninstall it by that id.
- **0204 package graph**: ntdll, before imports are resolved, puts the
  frameworks' install locations at the front of a packaged process's PATH
  (the process is packaged when its image is inside a record's
  InstallLocation, the same rule as 0061). `GetCurrentPackageInfo` (was a
  stub) returns the package and its frameworks; `appmodel.h` gains
  `PACKAGE_INFO` and the flags. A process started directly from Unix
  (`wine app.exe`) gets it too, since it is ntdll's.
- Not yet: optional/related packages, `.appinstaller` files, staging,
  per-user vs machine provisioning, merging a resource package's PRI into
  the app's ResourceLoader, package volumes, DirectX-level resource packages.
- Real-world check still to do: Microsoft's App Installer bundle (winget's
  own package) with its VCLibs/WindowsAppRuntime frameworks.

## `patches/sg/0209`-`0210`: real Store packages -- timestamps and signed payloads

Found by deploying Microsoft's own App Installer bundle (winget's package,
a local test copy, never shipped).

- **0209 wintrust: RFC 3161 timestamps.** Store signing certificates are
  valid for about **three days**; the signature is dated by an RFC 3161
  token (`1.3.6.1.4.1.311.3.3.1`, a SignedData with a TSTInfo). Wine only
  knew the PKCS #9 countersignature, so every Store package failed with
  CERT_E_EXPIRED three days after signing. The token counts only if its
  imprint is the hash of this signature's encrypted digest, its signature
  verifies, and its signer chains (at that time) to a trusted root with
  the time-stamping usage; otherwise the current time is used. **Open the
  token's certificates one by one** (`CERT_STORE_PROV_MSG` failed with
  ASN1_BADTAG on Microsoft's token -- one undecodable item lost them all).
  Wine's CMSG_CONTENT_PARAM returns the eContent still wrapped in its OCTET
  STRING; the parser takes either.
- **0210: bundles whose block map lists only the bundle manifest.** Newer
  bundles sign each payload package on its own (and carry
  `AppxMetadata\Stub\*` stub packages and `CodeIntegrity.cat`). The reader
  accepts an unmapped entry only if the bundle manifest names it at that
  size and it contains a signature; deployment copies such a payload to a
  temp `.msix` (**the SIP is chosen by extension**), requires it trusted,
  and requires every payload to be the bundle's (name, publisher).
- Result: App Installer is verified and read and stops only at its
  framework dependency (Microsoft.WindowsAppRuntime.1.8), as on Windows;
  VCLibs (aka.ms/Microsoft.VCLibs.x64.14.00.Desktop.appx) deploys.
- Gate: `make test-msix` gained timestamps (expired signer + trusted TSA:
  trusted; untrusted TSA, time after expiry, no timestamp: expired) and a
  bundle of self-signed payloads (unsigned, foreign and good). 49 checks without WINGET_DIR;
  the 10.0-46 build fails the 7 new ones.

## `patches/sg/0205`: a domain account's real SID

A domain user had a SID of this machine's (`S-1-5-21-0-0-0-<1000+uid>`,
0002): tokens, files, HKCU and ACLs named an account no other machine knows.
Now **sg-session's `sg-domain-logon` (root, at sign-in, before the first
Windows program) writes `/run/stained-glass/domain-sids/<uid>`** from
winbind -- `user <SID> DOMAIN\name`, `group <SID> DOMAIN\group` (the first
is the primary group), `name <SID> DOMAIN\group` (the domain's well-known
groups the user is *not* in, for ACLs). **That file is an interface: change
both sides together.** The server believes it only when it and its
directory are root's and not group/world-writable; it never calls winbind.

- The uid's SID becomes the domain SID (tokens, file owners, HKCU path --
  the hive file is per uid, so settings carry over). **A uid whose hive is
  loaded keeps its SID until the server restarts** (`settled`): switching
  mid-life would lose HKCU. A domain SID, once read, is kept.
- Tokens hold the domain groups (primary group = first); local groups stay;
  **no Administrators** for Domain Admins -- elevation is the broker's.
- **No protocol change**: `sg_lookup_account` with rid 0 and a SID string
  answers `DOMAIN\name`; with a name, `<SID>\t<DOMAIN>`; reply rid
  0x80000001 user / 0x80000002 group. advapi32 LookupAccountSid/Name, the
  current user's domain, secur32 `NameSamCompatible` use it.
- File ACLs still map to Unix modes (upstream Wine keeps no file SDs), so
  an ACE for a domain group persists only on objects that keep descriptors
  (registry keys, kernel objects) -- which is what the gate checks.
- Gates: `make test-domainsid` (shared prefix, second Unix user, a made-up
  domain record; 28 checks, 15 fail on 10.0-46), and sg-image's
  `make domain-test` against a real Samba DC (her SID from dc1's objectSid).

## `patches/sg/0206`: `dir \\server\share` in cmd

cmd treated an argument starting with `\` as relative to the current drive:
`dir \\dc1\shared` listed `Z:\dc1`. A UNC path is complete as given; the
volume header/trailer are per root (`X:\` or `\\server\share\`), and a
share without volume information is listed without a header. Gate: `make
test-uncdir` (a tmpfs share mounted where sg-netmountd mounts shares; stock
fails 6 of 6).

## `patches/sg/0207`-`0208`: winex11's BadWindow and the desktop painting over windows

- **0207: "a second program's overlapped window dies on an X BadWindow"**
  (the GUI gates' old note) was a cross-display bug. The cursor clip window
  is made by the desktop's owner on *its* display and every process takes
  its id from the desktop window's property. The gates ran `wineboot` with
  the developer's `DISPLAY` (:0) and then programs under Xvfb: explorer was
  still up on :0, the Xvfb programs used a :0 window id, and the X error on
  the first `XUnmapWindow` (cursor clip or its release on a focus change)
  was fatal. `init_clip_window` now checks the id on its own display (an
  override-redirect InputOnly window, error trapped) and makes its own clip
  window when it is not there. **Gates: still run `wineboot` without a real
  `DISPLAY`** -- it puts an explorer on the developer's desktop.
- **0208: the desktop painted over every window** (0069's note: a new
  wallpaper covered the windows and the taskbar). Not the surface flush --
  in a virtual desktop the desktop draws directly (`whole_window ==
  root_window`) and `X11DRV_GetDC` already gives its GC `ClipByChildren` --
  but **XRender's destination picture was always `IncludeInferiors`**, and
  the wallpaper is blitted through XRender. The picture now takes the DC's
  mode (`X11DRV_PDEVICE.subwindow_mode`). Explorer's 250 ms repaint (0069)
  stays, harmless.
- **Gate: `make test-display`** (`test/display-gate.sh`,
  `display-probe.c`): two Xvfb servers -- the desktop on A, a program on B
  that starts a second one which clips and releases the cursor; then the
  shell's virtual desktop with windows painted once (so a repaint cannot
  hide anything), a desktop repaint and a new tiled wallpaper, checked on
  the X server's pixels. 10 checks; 10.0-48 fails 5.

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

## `patches/sg/0130`: Win+I and the Win+X menu open Settings

`shellkeys.c`'s `run_settings()`: Win+I and Win+X > Settings run
`ms-settings:`, Win+X > Apps and Features `ms-settings:appsfeatures` and
Win+X > System `ms-settings:about`, through ShellExecute (sg-shell's
Settings registers the protocol, `defaults/65-sg-settings.reg`). With no
ms-settings: handler they fall back to what 0071 opened (the Control Panel
and its pages). Win+X > Control Panel stays the Control Panel. Gates:
`make test-shellkeys` (a stand-in registered for ms-settings: records Win+I;
stock fails it) and sg-shell's `test/settings-check.sh` with `WINI=1`.

## `patches/sg/0131`-`0133`: pseudo consoles that terminals can use; wt.exe

What sg-shell's Terminal (and any ConPTY host: Windows Terminal, VS Code,
Git's terminals) needed from Wine's pseudo consoles:

- **0131 (kernelbase, conhost)**: a program started on a pseudo console
  (`PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE`) is on a new console: CreateProcess
  drops the parent's standard handles (unless `STARTF_USESTDHANDLES`) and
  sets a Wine-private `ConsoleFlags` bit (`CONSOLE_FLAGS_PSEUDO_CONSOLE`,
  kernelbase.h), and the child's `init_console` opens the console's handles
  for a console program, then clears the bit so its own children do not
  inherit it. **Without it cmd and PowerShell wrote nowhere.** Scoping
  matters: the first version opened handles for any console program with
  none, and kernel32:console's `with_console_tests[18]` (an inherited
  console, NULL handles: Windows gives none) failed -- run that test after
  touching this. `ResizePseudoConsole` (was E_NOTIMPL) writes the resize
  signal (8, width, height) on the signal pipe; conhost resizes the buffer
  and its window, notifies, and repaints the tty.
- **0132 (conhost)**: with `ENABLE_VIRTUAL_TERMINAL_PROCESSING`, what a
  program writes is interpreted as VT (the list is in the patch): cursor
  movement, erasing, insert/delete, scrolling, SGR (256 and RGB to the
  nearest of the buffer's 16), save/restore, ?25, ?7, the alternate screen,
  DSR/DA replies (as input), RIS, OSC 0/2. Before, the sequences were stored
  as characters -- PowerShell 7's PSReadLine showed escape codes, split at
  line ends. `set_tty_attr` keeps a background across `\e[m` and passes
  underline/reverse on. kernel32:console: 0 failures with 0131+0132 (19987
  tests, same as stock).
- **0133**: `wt.exe` in system32/syswow64 (0120's launcher, `PARENTSRC =
  ../calc`) starts App Paths' `wt.exe`; explorer's Win+X has Terminal and
  Terminal (Admin) where it had Windows PowerShell (wt.exe, else pwsh, else
  cmd). New module: configure and configure.ac are patched (a clean
  configure was run on the series).

Gates: sg-shell's `test/terminal-check.sh` (needs these; stock shows no
prompt) and `make test-shellkeys` (Win+X, T with a stand-in wt.exe).

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
- **The main window** is the largest *captioned* new window that stays 4 s
  and is at least 400x300 (a splash has no caption; a first-run "please
  wait" dialog comes and goes); smaller ones are taken only at the 240 s
  timeout. Closing dismisses any application dialog in front first (updaters
  are other processes). `compat-probe list` dumps every top-level window,
  `LAUNCH_DEBUG=` traces just the launch, `KEEP_PREFIX=1` keeps prefixes in
  ARTIFACTS for poking at.
- **Close also reports whether the process ended** within 30 s
  (`exited=`; the note "still running 30 s after closing" -- Thunderbird's
  first-run and qBittorrent, which keeps running in the tray, show it).
- **Results (2026-09-25, 10.0-54 + 0170-0177): all 24 launch and show their
  main window**, 23 close (Greenshot is a tray program). Added then:
  Thunderbird, SumatraPDF, Inkscape (its smoke output is long: the runner
  keeps 4000 bytes), Pinta 3.1 (.NET 9 + GTK 4; its text needed 0177 and
  0222), qBittorrent (`--confirm-legal-notice`), ShareX 17 (.NET;
  `/NORUN`, else the installer starts it and the suite's launch only hands
  off to that one), HxD, foobar2000, Steam (its CEF sign-in window).
  Paint.NET 5.1 is left out: 0174-0176 got it past its first three walls,
  then Direct2D effects stop it.
- **Results (2026-09-24, 10.0-25 + 0080, with sg-shell's defaults):** 13 of
  14 launch and show their main window; 12 of 13 close. Open: Git for
  Windows' **mintty** dies silently inside `EnumFontFamiliesExW`'s callback
  when Lucida Console resolves (our font Replacements) -- the MSYS runtime
  runs out of stack there (gdi32's 32-entry, 14.5 KB stack buffer overflowed
  first; moving it to the heap removed that fault but not the death, so it
  was not shipped); git.exe itself works. **Firefox** closed its window only
  after >30 s and left its processes running -- fixed by 0170 (a vsync
  flood). Its content processes' `CreateWindow` error 1411 is noise.

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
  old binary. **And list it in `include/Makefile.in`** (0094): otherwise a
  clean configure cannot generate the Makefiles at all -- an incremental
  tree hides it; prove a patch with a clean tarball+series `configure`.

Gate: `make test-netsh` (a stand-in sg-netctl via `SG_NETCTL`; 14 checks,
13 fail on stock). The real thing: sg-image's net gate.

## `patches/sg/0090`: the shell's desktop icons

explorer's desktop draws This PC, the user's files, Network, Recycle Bin and
Control Panel as Windows does, by `HKCU\...\Explorer\HideDesktopIcons\NewStartPanel`
(`{CLSID}` DWORD 0 = shown; default: Recycle Bin only; sg-shell's
`theme/54-sg-desktop-icons.reg` adds This PC and the user's folder). Icons
fill **columns** (Windows) not rows; titles are shadowed. `explorer.exe
::{CLSID}` now opens the folder -- `GetFullPathNameW` had turned it into
`::\{...}`. Gate: `make test-desktop-icons` (xdotool double-click; the
desktop is drawn only once a window exists -- the gate opens one away from
the icons).

**Patch numbers:** the compatibility work uses 0080-0089, the main session
0090 on.

## `patches/sg/0081`, `0082`: what Git for Windows' terminal needed

Found by the compatibility suite: mintty (Git Bash's terminal) died, then
every program in it segfaulted. Two upstream Wine gaps, both in things the
Cygwin/MSYS runtime does exactly as Windows allows:

- **0081 (ntdll): a stack grows into read-write pages.** Cygwin reserves its
  stacks `PAGE_NOACCESS` and commits the top read-write itself;
  `grow_thread_stack` kept the reservation's protection, so the grown page
  was committed with no access, the retry faulted again, and the process was
  **killed by SIGSEGV -- which Windows code (and our probe) sees as exit code
  0.** A silent "exit 0" with no NtTerminateProcess is this, not a clean exit.
  Gate: `make test-stackgrow` (the probe builds a Cygwin-style stack and
  switches onto it with a few lines of asm; its verdict is the child's
  printed marker, never its exit code).
- **0082 (server): SD-less objects have their creator as owner and group.**
  `NtQuerySecurityObject` returned an empty descriptor for pipes, events,
  mutexes, semaphores, sections created without one; Cygwin's
  `cygpsid::get_id` read the pty pipe's NULL owner. `create_object` /
  the creators of pipes, events, mutexes, semaphores, timers and sections now
  record the effective token's owner and primary group -- **no DACL**, so
  access is unchanged (`token_access_check` grants everything when no DACL
  is present, as it did for no descriptor). **Only those kinds:** 10.0-33 did
  it in `create_named_object` for everything, and registry keys, window
  stations, desktops, directories and links have defaults of their own that
  an owner-only descriptor overrode -- keys became open to everyone (saved in
  the registry) and the session's display links broke: the VM boot gate's
  taskbar came up 1024x40 (proven by an A/B image with only 0082 removed).
  10.0-34 narrows it; the gate checks a key keeps its DACL. Gate:
  `make test-objowner`.

- **0083 (server): a pipe end reports its WriteQuotaAvailable.** It was
  always 0 (a FIXME). Cygwin polls it (1 ms timers -- genuine, not a stuck
  object: the "spinning wait" was `fhandler_pty_slave::write` ->
  `process_opost_output` waiting for room) before every pty write, so
  nothing printed. Now the peer's `buffer_size` less what is queued there.
  Gate: `make test-pipequota`. With 0081-0083 Git Bash comes up at its
  `MINGW64` prompt and passes every compat stage.

How it was found, for next time: a temporary ERR in `NtWaitForMultipleObjects`
naming each handle's type (`NtQueryObject`) when a thread repeats a wait;
`winedbg` `bt all` on the spinning process; and msys-2.0.dll's own symbols
(`x86_64-w64-mingw32-nm -C`) to name the frames.

**Firefox shutdown: fixed by 0170 (below).** It was never the pipes: the
IPC probe of Firefox's exact channel pattern (overlapped named pipe + IOCP +
posted-message wake) works on stock Wine. `WINEDEBUG=+server` (the trace is
the *wineserver's*, so it lands in whatever started the server) showed the
parent's I/O thread writing ~100k messages in 30 s to the GPU process: its
vsync thread loops on `DwmFlush()`, which was a stub returning at once.

**Mozilla symbols for triage:** read `xul.dll`'s CodeView debug ID (RSDS
record: GUID + age), fetch
`https://symbols.mozilla.org/xul.pdb/<ID>/xul.sym` (~600 MB, Breakpad text;
public, fine to use), and map `xul (+0x...)` offsets from `winedbg` to the
`FUNC` lines (some carry an `m` flag before the address).

`winedbg`: pipe `attach 0x<pid>` / `bt all` / `detach` on stdin (the pid
from `wine tasklist` is decimal).

## `patches/sg/0170`: DwmFlush waits for the next vertical blank

Firefox's vsync thread (`D3DVsyncSource::VBlankLoop`) is `NotifyVsync();
DwmFlush();` in a loop. Wine's `DwmFlush` returned at once, so it notified
thousands of times a second; with the GPU process (the default) each
notification is an IPC message to it. The GPU process's I/O thread never
caught up, synchronous requests (`BrowserParent::InitRendering`) timed out
("Killing GPU process due to IPC reply timeout"), the parent's queued
messages grew to 20+ GB (it swapped the build machine), pages did not load
and closing the window left everything running. Without the GPU process
the same loop kept the main thread busy and shutdown took minutes. Stock
Debian Wine is the same. `DwmFlush` now sleeps to the next refresh boundary
on the grid `DwmGetCompositionTimingInfo` already reports.

- Firefox 128.6 ESR with its GPU process: the page loads, closing the window
  ends every process (browser, gpu, socket, rdd, utility, tabs) in ~1 s.
- **Run Firefox experiments in a memory-capped scope**
  (`systemd-run --user --scope -p MemoryMax=6G -p MemorySwapMax=0`): a
  broken build eats all RAM within a minute.
- How it was found: `winedbg` `bt all` on the GPU process (IO thread in
  `ReadFile`, not stuck but busy), Mozilla's `xul.sym` for names, then
  `WINEDEBUG=+server` counting requests per thread id.
- **0173:** `IDXGIOutput::WaitForVBlank` (was `E_NOTIMPL`; Firefox tries it
  before `DwmFlush`, Chromium's vsync thread uses it) waits the same way,
  for the output mode's refresh rate.

Gates: `make test-vsync` (thirty flushes take thirty frames, each on the
vblank grid, and Firefox's vsync-over-pipe+IOCP pattern runs at the refresh
rate, and 30 `WaitForVBlank` calls take 30 frames; stock fails 3 of 6) and `make test-firefox` (the pinned installer:
page title on the window, a GPU process, window closes, every process gone
within 30 s; stock is OOM-killed in its 6 GB scope, fails 4 of 5).

## `patches/sg/0171`: characters beyond the BMP (emoji) in GDI text

A surrogate pair (emoji, 𝐀-style maths, CJK Extension B) drew as two
missing-glyph boxes, even from a font that has the character: every GDI
text path looked up each UTF-16 half on its own.

- **win32u:** `text_char_at()` (ntgdi_private.h) turns `str[i]` into the
  character to draw: the whole code point at a pair's first half, `~0u`
  (nothing, no advance) at its second. Used by the DIB engine's
  `render_string` (window surfaces and memory DCs -- what our sessions draw
  through), the null driver and paths; `font_GetTextExtentExPoint` puts the
  pair's width on its first half. The code point reaches the font code with
  the internal `WINE_GGO_FULL_CHAR` format bit (wingdi.h, `__WINESRC__`):
  **`GetGlyphOutlineW` must keep ignoring the high word** (gdi32:font tests
  `0x10000 + 'A'` == `'A'`), so only win32u's own callers pass it. Glyphs
  beyond the BMP bypass the DIB engine's 16-bit-page glyph cache (drawn,
  then freed). winex11's XRender text path is unchanged (our sessions do not
  draw through it).
- **Fallback:** after a font's links, every font falls back to Segoe UI
  Emoji, Segoe UI Symbol (Windows' links end with those), Noto Emoji,
  Symbola, Noto Sans Symbols2, Noto Sans Symbols -- whichever are installed.
  sg-image ships Symbola.
- **Uniscribe:** its Surrogates script is complex, so edit controls
  (`SSA_LINK|SSA_FALLBACK|SSA_GLYPHS`) shape it by glyph index in the
  control's font and had no fallback font for it (an empty name): it now
  takes the first of the same fonts whose cmap has the run.
- freetype prefers a font's full-Unicode cmap (3,10), not its BMP one.
- Monochrome only: Windows' GDI draws emoji in one colour too; colour is
  DirectWrite's. No font on the build machine has CJK Extension B, so those
  are still boxes there.

**Gate: `make test-astral`** (`test/astral-gate.sh`, `astral-probe.c`): pixels
of ExtTextOutW into a DIB (two emoji differ from each other and from boxes),
extents, fallback from Liberation Sans to Symbola's glyph, a path, Uniscribe's
ScriptStringOut, and GetGlyphOutlineW's high word. Stock fails 8 of 11; a
mutant without the symbol fallback list fails 1 (another linked font had a
different emoji). gdi32:font/dib/path/metafile and usp10: no new failures.

## `patches/sg/0172`: dynamic time zone conversions

`SystemTimeToTzSpecificLocalTimeEx` / `TzSpecificLocalTimeToSystemTimeEx`
were stubs and `SetDynamicTimeZoneInformation` was not even exported (an
importing program would not load). The conversions take the rules of the
date's year from the zone's `Time Zones\<key>\Dynamic DST` table
(`GetTimeZoneInformationForYear`), re-checking with the local year near New
Year; a zone without a key name (or dynamic rules disabled and no key) uses
its own fields; NULL is the current zone. `GetTimeZoneInformationForYear`
now uses the table's first year before it and its last year after it
(Windows' rule; Wine used today's `TZI` before it).
`SetDynamicTimeZoneInformation` = `SetTimeZoneInformation`
(`ERROR_PRIVILEGE_NOT_HELD`: the zone is the host's). **Setting the zone
for real** would go through sg-shell's `sg-admind` (`timezone AREA/CITY`,
SYSTEM-only spool) and needs a Windows-key-to-IANA table -- not done.

Gate: `make test-tzex` (US Pacific 2006 vs 2008 rules both ways, before and
after the table, disabled, key-less, NULL). Stock fails all 12 (no
exports); a mutant without the first-year rule fails 1. kernel32:time and
locale: 0 failures.

## `patches/sg/0174`-`0176`: what Paint.NET 5 needed first

Found trying Paint.NET 5.1 (.NET 9, self-contained) for the compat suite:

- **0175: Windows 10 is 22H2, build 19045** (Wine said 19043 = 21H1, out of
  support; Paint.NET: "Windows 10 (version 21H2) ... or newer is
  required"). ntdll's version table, kernelbase's manifest table, winecfg,
  and wine.inf's `CurrentBuild(Number)` / `DisplayVersion` 22H2 /
  `ReleaseId` 2009 / `UBR` 6456 -- **without** no-clobber, because ntdll
  reports the registry's `CurrentBuildNumber` when no version is configured:
  an old prefix moves on at `wineboot -u`.
- **0176: `DXGIDeclareAdapterRemovalSupport`** (S_OK, then
  `DXGI_ERROR_ALREADY_EXISTS`).
- **0174: Windows.System.DispatcherQueue** (coremessaging): was `E_NOTIMPL`.
  Priority queue of work run by one thread's message loop -- the caller's
  or a dedicated thread (optionally STA); a message-only window per queue
  carries enqueue, timers (`DispatcherQueueTimer` = `WM_TIMER`) and
  shutdown (`ShutdownStarting`, drain, `ShutdownCompleted`, then the
  `IAsyncAction` completes). `GetForCurrentThread`,
  `CreateOnDedicatedThread` (activation factories, registered by
  `classes.idl`), `HasThreadAccess`. Deferrals are not waited for.
  WinUI 3 / Windows App SDK programs need it too.
- **Where Paint.NET stops now:** `ID2D1Factory7::GetEffectProperties` ->
  `ERROR_NOT_FOUND` for Direct2D's built-in effects (it builds its UI and
  rendering on D2D effects and custom effects) -- Wine's d2d1 effect support
  is the next wall, a big one; its installer also still exits 2. Not in the
  compat suite.

Gates: `make test-dispatcherq` (12 checks; stock fails all; a
priority-order mutant fails 1), `make test-winver` (6; stock fails all,
including the `wineboot -u` upgrade of a 19043 prefix).

## `patches/sg/0177`: DirectWrite finds the font GDI uses for a substituted name

`IDWriteGdiInterop::CreateFontFromLOGFONT` looked the face name up only as
a family, so "Segoe UI" / "MS Shell Dlg" (FontSubstitutes -> Inter on our
machines) were `DWRITE_E_NOFONT`: DirectWrite shapers (WPF, Chromium,
Pango in GTK 4 apps like Pinta) lost the UI font. Now, if GDI knows the
name (its selected face is that name, not its default), the collection's
font for GDI's file -- else same family/weight/stretch/style -- is used.
Gate: `make test-dwlogfont` (stock fails 2 of 4). dwrite:font/layout/
analyzer unchanged (layout's 3 failures are stock's too).

Pinta's misshapen text was 0222's.

## `patches/sg/0222`: DirectWrite hints a glyph at the size it is drawn at

cairo (GTK 4: Pinta, Inkscape 1.x, GIMP 3) draws each glyph with
`IDWriteFactory3::CreateGlyphRunAnalysis` at **em size 1, the size in the
transform**, a glyph offset putting the ink box at 0,0, and asks for exactly
that box (never `GetAlphaTextureBounds`). Wine's FreeType glue sizes in whole
pixels (`freetype_set_face_size(FT_UInt)`), so it hinted at 1 ppem and scaled
the result: the box snapped to whole ems and the glyph was drawn above the
texture -- lower-case tops cut ("Background" -> "Backyiuuliu"). `font.c`'s
`fold_transform_scale` moves sqrt(|det|) of the transform into the em size for
the glyph box and bitmap (cache keyed on it). Found by `CreateGlyphRunAnalysis`
/`CreateAlphaTexture` in a `+dwrite` trace of Pinta and cairo 1.18.4's
`cairo-dwrite-font.cpp`. Gate: `make test-dwscale` (cairo's call vs drawing at
16 px: same box and pixels for 5 glyphs; stock 1/5 boxes, 0/5 pixels).
dwrite:font/layout/analyzer unchanged.

## `patches/sg/0178`-`0179`: HKEY_CLASSES_ROOT is the merged view; the user's choices

Found by the PDF work: Wine's HKCR was `HKLM\Software\Classes` alone, so a
per-user install's handlers and **Settings > Default apps' choices never took
effect**. Now Windows' merged view:

- **0178 kernelbase** (`registry.c`, the block before RegCreateKeyExW): a key
  opened through HKCR -- or through such a key -- is the user's key
  (`HKCU\Software\Classes\<path>`) when it exists, else the machine's. Such
  handles are **tagged `| 2`** (Windows' HKCR mark; the server ignores the low
  bits) and remembered with their path (`classes_keys`), so relative opens
  resolve the whole path again. Values: `RegQueryValueEx*`, `RegSetValueEx*`,
  `RegDeleteValue*`, `RegEnumValue*` are wrappers at the end of the file
  around the original code (`query_value_ex_w` ...): the user's value first,
  writes to the user's key when it exists, decided per call; a handle opened
  on the user's key stays with it (`user_bound`: ERROR_KEY_DELETED after it
  is deleted, as Windows). Subkeys (sorted) and values (user's, then the
  machine's others) enumerate merged; RegQueryInfoKey counts both. A new key
  goes to the machine's classes, or -- when denied, a standard user on the
  multi-user machine -- to the user's (UAC-virtualization-like). Per-user
  hives (0002) keep users' classes apart.
- **0179 shell32**: `SHELL_GetUserChoice` -- `HKCU\...\Explorer\FileExts\
  .ext\UserChoice\ProgId` and `...\Shell\Associations\UrlAssociations\
  <proto>\UserChoice\ProgId` (a registered ProgId only; **Windows' hash is
  not checked**) -- in IQueryAssociations::Init and its command lookup
  (AssocQueryString), ShellExecute/FindExecutable for files and URLs, the
  context menu's class, QueryCurrentDefault.
- **Conformance**: advapi32:registry's HKCR tests (skipped on stock) now run,
  0 failures 64- and 32-bit (their todo_wine removed); shell32 assoc/shlexec/
  shlfileop, shlwapi assoc, ole32 compobj/marshal unchanged.

**Gate: `make test-hkcr`** (`test/hkcr-gate.sh`, `hkcr-probe.c`): on a shared
prefix the owner registers machine classes, `sgconf` their own classes and
UserChoices; 21 checks (merged values/subkeys/enumeration/counts, writes,
creates, AssocQueryString, QueryCurrentDefault, ShellExecute of a file and
a URL, and the owner seeing none of sgconf's). Stock fails 10.

## `patches/sg/0220`: CJK Extensions B-G fall back to SimSun-ExtB, HanaMinB, BabelStone Han

0171's last-resort fallback list (GDI and Uniscribe) ends with the fonts that
have CJK beyond the BMP; sg-image installs `fonts-hanazono` (HanaMinA/B).
Gate: `make test-astral` (U+20000 from Liberation Sans is HanaMinB's glyph,
when HanaMinB is installed).

## `patches/sg/0221`: the time zone and the clock through sg-admind

`SetDynamicTimeZoneInformation` (key -> IANA, a CLDR `windowsZones.xml`
table in locale.c), `SetTimeZoneInformation` (by standard name),
`SetLocalTime`/`SetSystemTime` file requests in **sg-shell's sg-admind spool**
(`sg_admin_request`, locale.c; the same format sg-control uses: `<id>.req`,
UTF-8 lines verb/arg, written as `.<id>` and renamed; `replies/<id>.rep`
`OK`/`FAILED ...`). Only SYSTEM (elevated) can write the spool; others get
ERROR_PRIVILEGE_NOT_HELD, as on Windows. **The spool format and verbs
(`timezone`, `time`) are an interface with sg-admind -- change both
together.** Gate: `make test-tzset` (the real sg-admind, test mode, stand-in
timedatectl; 9 checks).

## `patches/sg/0125`: startup items disabled in Task Manager do not start

sg-taskmgr's Startup tab writes Windows' `Explorer\StartupApproved\{Run,
Run32,StartupFolder}` values (12 bytes, first byte odd = disabled). wineboot
now skips those Run entries and Startup folder items; RunOnce is unaffected.
Note wineboot runs **HKLM** RunOnce only (HKCU RunOnce is explorer's on
Windows). Gate: `make test-startup` (`test/startup-gate.sh`), 9 checks; stock
wine-sg starts all four disabled items.

## `patches/sg/0120`-`0124`: the Windows program names reach sg-shell's apps

Windows has `calc.exe`, `mspaint.exe` and `snippingtool.exe` in system32, and
programs start them with **CreateProcess, which searches system32 and PATH
but never App Paths** -- so an App Paths entry alone (what sg-shell's
`defaults/*.reg` register) only serves ShellExecute and the Run box.

- **0120** adds them as Wine programs (`programs/calc/handoff.c`, shared by
  `mspaint` and `snippingtool` through `PARENTSRC`), so wineboot puts them in
  system32 and syswow64. Each reads App Paths for its **own file name** and
  starts that program with the same arguments, then exits (Windows 10's
  calc.exe is a launcher too). With nothing registered it says the program
  is not installed. New modules: configure and configure.ac are patched.
- **0121** does the same inside Wine's `taskmgr.exe` and `wmplayer.exe` (like
  0072 for control.exe). With no App Paths entry Wine's Task Manager runs.
- **0122**: explorer's Win+Shift+S and Print Screen run `snippingtool.exe
  /clip` (Print Screen unless `HKCU\Control Panel\Keyboard
  PrintScreenKeyForSnippingEnabled` = 0).
- **0124**: `charmap.exe`, the same launcher (sg-shell's Character Map).
- **0123**: Ctrl+Shift+Esc runs `taskmgr.exe` (so sg-taskmgr, through App
  Paths).

Things that bit:
- **`RegGetValue(RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ)` without
  `RRF_NOEXPAND` fails with ERROR_INVALID_PARAMETER** (as on Windows) and
  never reads the value. `RRF_RT_REG_SZ` alone takes and expands
  REG_EXPAND_SZ. (0072's control.exe hand-off does not hit it: it asks for
  REG_SZ only.)
- **The 32-bit copies must open App Paths with `KEY_WOW64_64KEY`**: it is a
  redirected key, and the .reg files are imported by 64-bit reg.exe.
- **wmplayer.exe is in `Program Files\Windows Media Player`**, not system32,
  as on Windows.
- **Loops**: a target that is the launcher itself or sits in a system
  directory is refused. The gate's mutant with both checks removed chains
  launchers forever and never shows the refusal.

Gate: `make test-handoff` (`test/handoff-gate.sh`): the six system files, a
64-bit and a 32-bit caller's CreateProcess of each name with awkward
arguments reaching a stand-in with its command line intact, Win+Shift+S and
Print Screen and Ctrl+Shift+Esc typed on the X keyboard, the loop refusal,
and Wine's Task Manager without App Paths. 22 checks; stock wine-sg fails
all of them; mutants (no loop guards; 0122 without 0123) turn it red.

## `patches/sg/0183`: fontview.exe, the Fonts folder, per-user fonts, shell: URLs

sg-shell's `sg-fontview` is Windows' font viewer and the Fonts folder (App
Paths `fontview.exe`); this is what Wine itself needed for them.

- **`programs/fontview`**: `fontview.exe` in system32 and syswow64 is calc's
  launcher (0120's `handoff.c` via `PARENTSRC`) for App Paths' `fontview.exe`.
- **wine.inf**: `.ttf`/`.otf`/`.ttc`/`.fon` -> `ttffile`/`otffile`/`ttcfile`/
  `fonfile`, opened with `fontview.exe "%1"`, `print` = `/p`, Windows'
  `install` ("Install") and `installallusers` ("Install for all users",
  `HasLUAShield`) verbs = `/install [/allusers]`. No-clobber (flag 2).
- **win32u: a user's own fonts load.** Windows 10 1809 installs a font per
  user without an administrator: the file under `%LOCALAPPDATA%\Microsoft\
  Windows\Fonts` and `HKCU\Software\Microsoft\Windows NT\CurrentVersion\
  Fonts` "Name (TrueType)" = its full path. Wine's `load_registry_fonts`
  read only HKLM's key, so such a font (installed by sg-fontview or by any
  Windows installer that does this) was invisible. The loop is now
  `load_fonts_from_key` over HKLM's key, then HKCU's. It runs in every
  process (not only the one that builds the session's font cache), so a
  value added or removed shows in the next new process.
- **explorer**: `explorer shell:fonts`, its CLSID
  (`::{BD84B380-8CA2-1069-AB1D-08000948F534}`) or `%WINDIR%\Fonts` open
  `App Paths fontview.exe /folder` (the plain folder when nothing is
  registered). `shell:<known folder>` (shell:downloads, shell:startup ...)
  opens that folder through `IKnownFolderManager::GetFolderByName`;
  `shell:::{CLSID}` through the desktop folder's parser. **explorer.c no
  longer ShellExecutes a `shell:` root** -- with wine.inf now registering the
  `shell` URL scheme to explorer (Wine had none: the Run box's `shell:fonts`
  failed with "no association"), that would loop forever.
- **Gate: `make test-fonts`** (`test/fonts-gate.sh`, `test/fonts-probe.c`):
  the two launchers from a 64- and a 32-bit caller with the file argument
  intact, the four associations and Install verbs, ShellExecute of a .ttf
  and its `install` verb, `explorer shell:fonts`, `explorer C:\windows\Fonts`
  and ShellExecute(`shell:fonts`) reaching `/folder`, ShellExecute
  (`shell:downloads`) opening a window titled Downloads with the explorer
  count steady, and a fontTools-made font named only in HKCU (in a folder
  fontconfig does not scan, `HOME` redirected) enumerated by a new process,
  in a new session, and gone when its value is deleted. 16 checks. Stock
  wine-sg fails 14 (2 vacuous); a build without explorer.c's `shell:` guard
  fails 4 (the loop).

## `patches/sg/0100`-`0101`: Notepad is a real editor

David: "the wine notepad kinda sucks ... something a lot more like Kate."
`programs/notepad` is replaced wholesale (0100) and stays `notepad.exe` in
system32 **and** syswow64 -- which is why it is a Wine patch and not an
sg-shell program: CreateProcess("notepad.exe") searches system32 first and
never App Paths, a 32-bit caller gets syswow64's, and wineboot keeps both
current in every prefix with nothing to install.

- **Files:** `textbuf.c` (gap buffer + line index; line ends are kept in the
  text exactly -- CRLF, LF, CR, mixed -- and the starts around an edit are
  re-scanned, since an edit can join or split a CRLF), `editor.c` (the
  control, class `SgNotepadEditor`), `syntax.c` (per-line lexers with a
  carried state; add a language to `langs[]`), `regex.c` (backtracking VM,
  explicit stack, 50M-step budget), `find.c`, `fileio.c` (detection and
  exact save), `print.c`, `main.c` (tabs, find bar, status bar, commands,
  settings, command line).
- **Layout never asks GDI where a character landed:** advances come from a
  per-font cache and are passed to `ExtTextOutW` as `lpDx`, so caret and
  pixels agree. Glyphs the font lacks are drawn from installed fallback
  fonts (`fallback_faces[]`) -- Wine links fonts only for a few UI faces, so
  a monospace font showed boxes for CJK. Characters beyond the BMP
  (emoji) draw since 0171, from the image's Symbola (fonts-symbola).
- **Compatibility is deliberate** (see main.c's header): one process per
  invocation living until its window closes (git's `core.editor`), Windows
  Notepad's command line (`/p` prints and exits without a window, `/pt`,
  `/a`, `/w`, unquoted paths with spaces, `.txt` tried, "create it?"), window
  class `Notepad`, title `<name> - Notepad`, settings in
  `HKCU\Software\Microsoft\Notepad`, `.LOG`. Several *quoted* paths open as
  tabs (our extension). No file ever goes to another process's window.
- **The EDIT messages are answered** (WM_GETTEXT/SETTEXT, EM_GETSEL/SETSEL,
  EM_REPLACESEL, EM_LINEINDEX, ...), for programs that drive Notepad's text,
  but the class is not `Edit` -- as on Windows 11.
- **The theme** (`HKCU\...\Themes\Personalize AppsUseLightTheme` or View >
  Theme) covers the tab strip, find bar, status bar, editor **and the menu
  bar**: dark, Notepad makes the bar's items owner-drawn (their text kept in
  `menubar_text[]`, Alt+letter answered in `WM_MENUCHAR`) and gives the bar a
  dark MIM_BACKGROUND brush, which win32u paints since 0188. The popups stay
  the system's (COLOR_MENU): a system-wide dark scheme darkens them.
- **Page Setup** is the common dialog grown by a hook with Header and Footer
  boxes (Windows Notepad's), kept at once in `szHeader`/`szTrailer`. `&l`,
  `&c`, `&r` place what follows left/centre/right (no code: centred); `&f`
  `&p` `&d` `&t` `&&`. Wine's Page Setup and Print dialogs need a printer:
  none installed means "No default printer defined".
  `NOTEPAD_PRINT_EMF=<dir>` makes printing write `page<N>.emf` there
  (letter size, 96 dpi) -- the print gate reads their text records.
- **0101 (wine.inf):** Wine had `txtfile` but no `.txt` -> `txtfile`, so
  ShellExecute of any .txt failed ("no application associated"). `.txt`,
  `.text`, `.log` now map to it (plus content type and ShellNew), `.inf`
  opens in Notepad. Flag 2 (no-clobber) on purpose here: it creates the key
  on `wineboot -u` when missing and never overrides a user's choice.
- New strings are English only; the `.po` translations still cover the
  strings Wine's Notepad had.

- **The minimap** (Kate's; View > Minimap; `sgMinimap` 2 = code files, the
  default, 1 always, 0 never -- the editor option `EDOPTS.minimap`): a strip
  inside the editor control, right of the text (`e->cw` is the text's
  width, `e->full_w` the client's). A document that fits gets `mm_row()` px
  a line (2 at 96 dpi); a longer one is compressed to the strip's height
  (`mm_line_at`/`mm_y_of`), a pixel row per n/h lines, drawn from the line
  it stands for. Pixels go straight into a 32-bit DIB (`mm_paint`), only
  for the rows shown; a compressed strip never lexes past `hl_valid` (lines
  beyond are coloured from the start state), so opening a 300 000-line file
  does not lex it all. The band (the lines on screen, at least 2 px) is
  shaded; a click centres the editor on the line under it, dragging
  follows (captured). Colours are mixes of the theme's bg/fg/styles, so
  dark mode needs nothing. `SGE_GETMINIMAP`, `SGE_GETMMLINE`,
  `SGE_GETTOPLINE`, `SGE_GETVISROWS` are for the gate.

**Gate: `make test-notepad-minimap`** (`test/notepad-minimap-gate.sh`), 15
checks: a C file's strip in the lexer's colours, the whole 10 000-line
document in it, the band at the top; a click on row 300 centring the
editor on that row's line (the band follows), a drag, a short file at 2 px
a line; a 300 000-line file: a click at the foot reaches the end, quickly;
the dark theme; a .txt with none until View > Minimap, and the choice kept.
Mutants `SG_MUTANT_MMSCALE` (no compression) and `SG_MUTANT_MMCLICK`
(clicks ignored) fail 4 and 6; stock has no minimap.

**Gate: `make test-notepad-print`** (`test/notepad-print-gate.sh`), 13
checks: the dark bar's pixels and its light item text, Alt+F on the
owner-drawn bar, the light theme's bar back to light; Page Setup's boxes
typed into (xclip paste: xdotool loses Shift for `&`) and kept; the printed
EMF pages' header parts left/centre/right with `&f`/`&p` expanded, above the
text, the footer on every page below it. A private unprivileged cupsd (its
own `cupsd.conf`, `CUPS_SERVER` a socket in the gate's directory, a raw
`file:` queue) gives Wine a default printer without touching the machine's
CUPS. Stock 10.0-43 fails 6 (the dark bar, the boxes, the printed pages and
their header/footer); the series without 0188 fails the dark bar.

**Gate: `make test-notepad`** (`test/notepad-gate.sh`, `test/notepad-probe.c`),
28 checks in a shell session under xvfb: cmd, a 64-bit and a 32-bit
CreateProcess and ShellExecute of a .txt all start ours; UTF-8 / UTF-16 LE /
UTF-16 BE (CR, no final line end) show exactly and save back byte for byte;
find as you type, replace all plain and regex with a group; tabs (Ctrl+O,
command line, Ctrl+N, Ctrl+Tab, Ctrl+W); the lexer's styles and the keyword's
purple pixels. Stock Wine's Notepad fails 25 (3 vacuous); a build that swaps
UTF-16 BE bytes or loses keywords fails those checks. To run it against a
build tree: `WINE=build/<tree>/obj/wine WINESERVER=build/<tree>/obj/server/wineserver`.

## `patches/sg/0180`, `0184`, `0240`: WordPad

- **0180**: Wine's `wordpad.exe` (Program Files\Windows NT\Accessories --
  where system32's `write.exe` and the `rtffile`/`wrifile` associations go)
  hands off to App Paths' `wordpad.exe` (sg-shell's sg-wordpad), as 0121
  does for taskmgr.exe; nothing registered, Wine's WordPad runs.
- **0184 riched20** -- found building sg-wordpad; every RichEdit program
  benefits:
  - **EM_FORMATRANGE** was an unsupported stub (Wine's own WordPad printed
    blank pages). `editor_format_range()` (paint.c) re-wraps every paragraph
    for the target DC (its dpi, the page's width: `rcFormat`, no zoom),
    walks the rows from `cpMin` (`row_from_cursor`, `row_next_all_paras`;
    a row's top is `para->pt.y + row->pt.y`) until one would pass the
    page, draws those paragraphs with `draw_paragraph` clipped to the
    rows that fit (`bHideSelection`, transparent background; with a target
    of another resolution, `MM_ANISOTROPIC` so the drawing is in the
    target's units), then re-wraps for the screen with
    `wrap_marked_paras_dc(..., FALSE)`. Returns the next page's first
    character, or the length + 1 when the rest fitted, sets `rc.bottom` to
    where the text ended, and with no FORMATRANGE returns the length -- as
    Windows; `cpMax` stops it early. riched20:editor's EM_FORMATRANGE tests
    pass (their todo_wine removed); richole/txtsrv unchanged.
  - `\pngblip`/`\jpegblip` read (OleLoadPicture -> bitmap);
    `\dibitmap`'s bits were taken after `sizeof(BITMAPINFO)` (4 bytes too
    far: colours shifted); a bitmap-cached picture is written back as
    `\dibitmap0` (only EMFs were).
  - The writer had `\li`/`\fi` wrong (dxOffset / dxStartIndent); RTF's
    `\li` is `dxStartIndent + dxOffset`, `\fi` is `-dxOffset`, as the
    reader always took them.
  - A style's `script_cache` (Uniscribe's advances) is freed when its font
    height changes (`script_cache_height`, editstr.h): after EM_SETZOOM the
    old size's advances made letters overlap -- and a printer's resolution
    would have done the same to printed text.
- **Gate:** sg-shell's `test/wordpad-check.sh` (58 checks with this Wine;
  stock 10.0-43 fails 16: the hand-offs, the saved indent, the zoomed
  advance, pictures' colours and saving, printing and preview).
- **0240 riched20 tables** (4.1 mode) -- found adding Insert > Table:
  - writer.c `stream_out_para_props` skipped a paragraph's properties equal
    to the previous paragraph's; a row's first cell shares its ROWSTART's
    format, so its `\intbl` was never written and a reopened table fell
    apart. No skip after a ROWSTART/ROWEND. `\trowd` is written as
    `\pard\trowd` so a row doesn't inherit the paragraph before's spacing.
  - editor.c's `\cell` (4.1) applies and resets `info->fmt` as `\par`
    does (a cell kept the pre-table paragraph's format).
  - wrap.c: a ROWSTART/ROWEND paragraph's space before/after is not laid
    out (they are delimiters), so rows touch.
  - Gate: wordpad-check.sh's inserted table -> RTF -> reopen checks (a grid
    of 3 evenly spaced lines; the .docx saved from it has 2 rows). Without
    0240 both fail. riched20 editor/richole/txtsrv tests: no new failures.

## `patches/sg/0188`: a menu bar is painted with its MIM_BACKGROUND brush

`draw_menu_bar` (win32u/menu.c) fills the bar -- and the line under it --
with `menu->hbrBack` when SetMenuInfo gave one, as Windows does; otherwise
COLOR_MENU/COLOR_MENUBAR and the 3D-face line as before. Notepad's dark
theme uses it (0100). Gate: `make test-notepad-print`.

## `patches/sg/0140`: a pipe server impersonates its client

`ImpersonateNamedPipeClient` (and `RpcImpersonateClient` over ncacn_np) gave
the server thread a copy of the **server's own** token (an upstream FIXME).
In a shared prefix every service runs as SYSTEM, so any service that decides
by impersonating its caller -- the SCM's access checks, the event log's --
saw SYSTEM for every standard user. The server now captures the client's
token (thread impersonation token, else process token) when the client opens
the pipe and hands a duplicate to `FSCTL_PIPE_IMPERSONATE`; dropped on
disconnect. Server only, no protocol change. Gate: `make test-pipeimp`
(`test/pipeimp-gate.sh`: a server run by the prefix owner impersonates a
client run by `sgconf` and must see sgconf's SID and no Administrators);
stock wine-sg sees S-1-5-18 with ADMIN 1 and fails 2 of 4.

## `patches/sg/0143`-`0144`: a real event log

Every event log function was a stub (ReportEvent only printed to the debug
log; nothing could be read), so there was no Application, System or Security
log for programs, administrators or Event Viewer.

- **0143 wevtsvc: the EventLog service owns the logs** (svchost, auto-start,
  as wine.inf already registered it). A log is a subkey of
  `HKLM\System\CurrentControlSet\Services\EventLog`; its file is
  `%SystemRoot%\System32\winevt\Logs\<log>.sgevt` -- our own format: a
  16-byte header (`SGEVTLOG`, version 1, next record number) then the
  records in ReadEventLogW's `EVENTLOGRECORD` layout, oldest first. Backups
  are the same format. Kept in memory while the service runs; appended to,
  and rewritten only when `MaxSize` (default 20 MB, min 64 KB) forces the
  oldest out (down to 9/10 of it). Record numbers carry on after a clear.
  The service writes 6005/6006 (start/stop, 24 bytes of data) and 104 ("The
  %1 log file was cleared.", with the clearer's SID) to System from source
  `EventLog`, whose `EventMessageFile` is wevtsvc.dll's message table
  (`wevtsvc.mc`).
- **0144 advapi32 is the client**, over `\\.\pipe\wine_eventlog`
  (message mode, one request per connection, `include/wine/eventlog.h`:
  OPEN, REPORT, READ, INFO, CLEAR, BACKUP, WAIT). **The protocol header is
  shared: change both sides.** A handle is advapi32's own (log, source,
  read position); backups are read into memory and served locally.
  Unreachable service: RPC_S_SERVER_UNAVAILABLE after <= 3 s (then at once
  for 30 s), so ReportEvent never hangs a program; it tries StartService
  once per process.
- **Who may do what is the service's decision, from the caller's token**
  (ImpersonateNamedPipeClient -- real only since **0140**; without it every
  caller is SYSTEM): Application/System/other logs -- everyone reads and
  writes; **Security -- read only by Administrators or SYSTEM, written only by
  SYSTEM** (ERROR_ACCESS_DENIED at OpenEventLog/RegisterEventSource);
  clear -- Administrators/SYSTEM (ERROR_ACCESS_DENIED); backup --
  Administrators/SYSTEM (ERROR_PRIVILEGE_NOT_HELD). The pipe's DACL leaves
  out FILE_CREATE_PIPE_INSTANCE, so no one else can serve the name.
- **Not a Unix boundary**: the log files map to mode 0770 in the prefix's
  group (the shared prefix's files all are -- see D13); the Security log is
  protected from Windows programs, not from a user reading the prefix
  through `Z:`/`\\?\unix`. Security events come from the Linux side's audit spool (0187).
- Formatting an event's text is the viewer's job, as on Windows:
  `EventMessageFile` (REG_EXPAND_SZ, `;`-separated) under
  `EventLog\<log>\<source>`, `FormatMessage(FORMAT_MESSAGE_FROM_HMODULE |
  FORMAT_MESSAGE_ARGUMENT_ARRAY, ..., EventID, ...)` with the record's
  strings; `ParameterMessageFile`/`CategoryMessageFile` likewise.
- **Not done**: the Vista+ Evt* API (wevtapi) and `.evtx`, `wevtutil`, remote
  logs, `CustomSD`, `Retention` (always "overwrite as needed"), the
  `Sources` value.
- **Conformance**: advapi32:eventlog 506 tests, 0 failures (stock: 290, 115
  todo); every test that now passes lost its `todo_wine`, the two that need
  ten System records are `todo_wine_if` fewer, and 14 ETW todos remain.

**Gate: `make test-eventlog`** (`test/eventlog-gate.sh`,
`test/eventlog-probe.c`), 17 checks on a shared prefix with the second Unix
user `sgconf` as the standard user: the probe's 39 functional checks in 64-
and 32-bit, 6005 in System, a standard user writing and reading Application
but refused Security (read and write), clear and backup, SYSTEM reading and
writing Security, persistence across a wineserver restart, MaxSize
wraparound, NotifyChangeEventLog. Stock wine-sg fails 15 (2 vacuous: its
ReportEvent always "succeeds"); a mutant without the Security read check
fails 2.

## `patches/sg/0141`: the SCM checks who is asking

Wine's SCM put whatever access was asked for into every handle, so in a
shared prefix any standard user could stop, reconfigure, delete or create
services. `OpenSCManager`/`OpenService` now `RpcImpersonateClient` (0140
makes that the caller) and `AccessCheck` against Windows' defaults: SCM --
Authenticated Users connect/enumerate/query lock, SYSTEM and Administrators
all; a service -- Authenticated Users query/interrogate/user-defined
controls, SYSTEM and Administrators all **plus Wine's private
`SERVICE_SET_STATUS` (0x8000)**: without it every service process
(`sechost`'s dispatcher opens its own service with it) failed and every
service start returned 1053. Deliberate difference: a service whose program
is in system32 may be **started** by anyone, because Wine starts RpcSs,
MSIServer and COM servers' services from the calling program. Per-service
descriptors (`sc sdset`) are still not implemented.

- **Gate: `make test-scm-access`** (`test/scm-access-gate.sh`,
  `scm-probe.c`, `scm-svc.c` -- a real start/stop/pause service also used
  by sg-shell's Services gate). 25 checks; stock fails 17.
- **A shared-prefix gate must keep one services.exe alive**: after a
  `wineboot -i` that exits, the next program started a second services.exe
  beside the exiting one, each with its own database, and every other call
  failed with 1060 -- stock too. The gate starts a sleeping probe first (it
  initialises the prefix) and keeps it for the run, as sg-services-start
  keeps the machine's services.

## `patches/sg/0185`: a service's own security descriptor (`sc sdset`)

0141 checked callers against Windows' *default* descriptors only; now a
service can have its own. services.exe keeps it where Windows does -- the
REG_BINARY value `Security` of `Services\<name>\Security`, self-relative --
loads it with the configuration, and OpenService's `AccessCheck` uses it
(else the default). `QueryServiceObjectSecurity` needs READ_CONTROL and returns
the parts asked for; `SetServiceObjectSecurity` needs WRITE_DAC (DACL) /
WRITE_OWNER (owner, group), replaces those parts and keeps the rest. SACLs:
ERROR_ACCESS_DENIED. sechost's two functions now call the SCM; `sc sdshow`
and `sc sdset` print Windows' `[SC] ...` lines, and sc opens the SCM with only
SC_MANAGER_CONNECT (a standard user's `sc query`/`sdshow` failed with 5 under
0141 because sc asked for SC_MANAGER_ALL_ACCESS).

- **SERVICE_SET_STATUS (0x8000) is Wine's own right**: no SDDL written for
  Windows grants it, and without it a service's own process (SYSTEM) cannot
  report its state -- every start after an `sc sdset` returned 1053. A SYSTEM
  caller now always gets it, whatever the descriptor says.
- **Gate: `make test-scm-sd`** (`test/scm-sd-gate.sh`, shared prefix, `sgconf`
  the standard user, `scm-probe`/`scm-svc` from 0141's gate): the default
  DACL shown, readable by a standard user who may not change it, `sdset`
  letting everyone start/stop (the standard user starts it; it reaches
  RUNNING), the registry value, still in force after a wineserver restart,
  then a DACL that shuts the standard user out (query 5, READ_CONTROL 5).
  22 checks; the tree without 0185 fails 14; a mutant without the
  SERVICE_SET_STATUS rule fails 7 (1053). advapi32:service 0 failures.

## `patches/sg/0186`: `lusrmgr.msc`, `fsmgmt.msc`; `msinfo32 /report` waits

wineboot's `create_msc_files` (0142) also writes `lusrmgr.msc` and
`fsmgmt.msc`. Wine's msinfo32.exe hand-off waits for the App Paths program
when given `/report` or `/nfo` and returns its exit code (Windows' msinfo32
returns once the file is written). sg-shell's sg-msinfo waits for its own
bridged copy the same way (an event; a native parent cannot be waited on --
a Windows process's handle to a Unix child is not waitable). Gate: `make
test-admintools` (+4 checks; the stand-in writes the report 2 s late and
exits 3). The tree without 0186 fails exactly those 5.

## `patches/sg/0187`: audit events from the Linux side reach the Security log

Logons, logoffs and elevation happen in PAM and sg-session's broker, not in
Windows. They write one small file per event into the **audit spool** (a
Unix directory, `HKLM\...\EventLog\Security` `AuditSpool`, default
`/var/lib/stained-glass-audit`, 0700 sgsystem -- only root and SYSTEM's
account can write it, so no user can forge an audit event); the Event Log
service imports them into Security once a second in name order and deletes
them (`.name` files are still being written; non-events are dropped). File
format: `ID`, `TYPE success|failure`, `CATEGORY`, `TIME`, `SID`, `SOURCE`,
`STRING`... lines (see the comment in wevtsvc.c; sg-session's `sg-audit`
writes them). wevtsvc.mc words 4624/4625/4634/4648/4672 and the categories
12544-12548 (our own wording); wine.inf registers wevtsvc.dll as
`Microsoft-Windows-Security-Auditing`'s message and category file.

- **The Linux-level protection of the log files is sg-session's**: Wine maps a
  DACL naming SYSTEM to *user + group* bits when the process holds that SID
  (`sd_to_mode`), so wevtsvc cannot make its files 0600 itself;
  sg-services-start makes `winevt/Logs` 0700 (the service is the only reader).
- **Gate: `make test-audit`** (`test/audit-gate.sh`, `audit-probe.c`, shared
  prefix): the spool emptied but a dot-file, 4624/4672/4625 with their
  formatted messages, categories and types, the spool's time, exactly three
  records in name order, a standard user refused, an event written while
  running. 10 checks; the tree without 0187 fails 9. advapi32:eventlog
  unchanged.

## `patches/sg/0142`, `0145`: the administrative tools' Windows names

- **0142**: `mmc.exe`, `eventvwr.exe`, `resmon.exe`, `cleanmgr.exe` are
  launchers (calc's `handoff.c`, 0120) for what App Paths registers --
  sg-shell's console host and tools; Wine's `msinfo32.exe` hands off the same
  way. **wineboot writes `services.msc`, `eventvwr.msc`, `devmgmt.msc`,
  `diskmgmt.msc`, `compmgmt.msc`** into system32/syswow64 (an
  `MMC_ConsoleFile` with `<StainedGlass Console="services" .../>`; a file
  that is not ours is left alone), `.msc` is `MSCFile` opened with
  `"mmc.exe" "%1" %*`, and wine.inf's `ProfileItems` make the Start menu's
  `Administrative Tools` shortcuts (setupapi's ProfileItems takes only a
  path, no arguments -- so .msc shortcuts point at the .msc files).
- **0145**: Win+X gains Event Viewer (V), Device Manager (M), Disk
  Management (I), Computer Management (G).
- **Gate: `make test-admintools`** (`test/admintools-gate.sh`): the files,
  .msc contents, shortcuts, association; each name from a 64- and 32-bit
  CreateProcess, ShellExecute of `services.msc`/`devmgmt.msc` and of the
  Services shortcut, and Win+X typed on the X keyboard, all reaching a
  stand-in registered in App Paths. 39 checks; stock passes only the two
  that Wine's own msinfo32.exe exists.

## `patches/sg/0110`-`0112`: File Explorer

- **0110 explorer:** the folder window is ours (`programs/explorer/fileexplorer.c`;
  explorer.c keeps only the command line) around shell32's ExplorerBrowser:
  command bar, back/forward (our own history -- ebrowser's travel log has no
  public query), up, breadcrumb/editable address bar, refresh, search
  (a thread; results in our own list view, not a shell folder), navigation
  pane (a tree we draw entirely in custom draw; its chevrons are ours, so
  clicks left of the icon expand), status bar, This PC page (capacity bars).
  No arguments opens This PC. Selection changes arrive through an
  `ICommDlgBrowser` site (`SID_SExplorerBrowserFrame`) that ebrowser forwards.
  State: `HKCU\Software\Stained Glass\Explorer` (window size, nav pane,
  `FolderViews\<parsing name>` = view mode).
- **Alt+key accelerators must be passed to DefWindowProc** after we handle
  them, or Alt's release opens the window menu and swallows the next keys.
- **0111 shell32:** columns Name/Date modified/Type/Size (shfldr_fs and
  shfldr_desktop share the header layout; attributes are not on by default and
  DefView stops at the first such column); sort by column *property*; header
  arrows; SetSortColumns/GetSortColumns; selection counts; rename selects the
  base name (posted EM_SETSEL: the list view selects all after
  LVN_BEGINLABELEDIT); light selection tint via custom draw (clear
  CDIS_SELECTED or comctl32 paints the highlight); This PC / Recycle Bin /
  Network / "Local Disk (C:)". Column order changes can move shell32 winetests.
- **0112:** SHFileOperation's progress window (`dlls/shell32/fileopdlg.c`,
  its own thread, shown after 500 ms, paused while a question is up) and
  "Replace or Skip Files" (replace / skip / keep both, for all). A nested
  SHFileOperation (folder contents) reuses the running progress through TLS.
  kernelbase CopyFileEx/CopyFile2 call their progress routines and honour
  cancel (a cancelled copy's partial file is deleted).

**Gate: `make test-explorer`** (18 checks, xdotool + `explorer-probe`: title
changes prove navigation; This PC's accent pixels; search trace; files on
disk; progress/conflict window classes `SGFileOperation`/`SGFileConflict`;
in-process columns/sort/selection). Stock fails all 18. The gate uses a temp
`HOME` and replaces the prefix's user-folder symlinks -- **never let a test
prefix's Documents point at the developer's home.**

Round 2 (0150-0157, below) did the next steps round 1 listed.

## `patches/sg/0150`-`0157`: File Explorer, round 2

- **0150 comctl32: list view groups and the tile view.** Group view is a
  *positioned* layout: `LISTVIEW_ArrangeGroups` puts every item's position in
  `hdpaPosX/Y` (details included -- `GetItemOrigin`, the frame iterator and
  the vertical scroll range read them when `is_group_view()`), an item in no
  shown group gets `LV_HIDDEN_POS` and is never drawn or hit, and
  `displayOrder` is the items in group order (Home/End). Group view is on only
  once there are groups (`is_group_view`), never in List or owner data.
  **Anything new that places items must go through `is_positioned()` /
  `is_icon_view()`** (ICON, SMALLICON, TILE), not the old ICON||SMALLICON
  test. Headers: the highlight colour darkened (not COLOR_HOTLIGHT, which the
  Stained Glass colours set to a light grey), a line, a chevron for
  LVGS_COLLAPSIBLE (click folds; a click elsewhere on a header selects the
  group). Tiles: `himlNormal`, the name then `LVTILEINFO` columns in grey;
  default 2 lines, width from `LVM_SETTILEVIEWINFO` or icon + ~20 chars.
- **0151 comctl32:** a selected icon is blended (ILD_SELECTED) only if custom
  draw left `CDIS_SELECTED` set -- DefView clears it for its light tint, so
  File Explorer's icons no longer turn purple. Icons and tiles are drawn with
  `CLR_NONE` so a custom-draw fill behind them shows.
- **0152 shell32: type names** -- `HCR_GetFileTypeNameW` (classes.c):
  FriendlyTypeName (indirect strings loaded), ProgID default, else "EXT
  File"; "File folder". wine.inf gives common types names with flag 2
  (no-clobber). **A ProgID shared by many extensions must not name itself**
  ("Image"), or every picture says so: sg-shell's photos/media ProgIDs have
  an empty default so .png reads "PNG File", as on Windows 10.
- **0153 shell32: thumbnails** -- `thumbnail.c`: our WIC provider
  `{8b9284a6-ab7a-41df-bcfa-c80995441db9}` (registered in
  shell32_classes.idl; wine.inf's `ShellEx\{e357fccd-...}` for the image
  extensions, flag 0 so `wineboot -u` repairs), box-filtered, EXIF
  orientation, never enlarged; `SHELL_GetThumbnail` finds a type's
  IThumbnailProvider or IExtractImage handler as Windows does;
  `IShellItemImageFactory::GetImage` is real. shobjidl.idl gained
  IInitializeWithItem, IExtractImage, IExtractImage2. **WIC/thumbcache GUIDs
  are not in libuuid**: thumbnail.c includes initguid.h before those headers.
- **0154 shell32: DefView** -- `ShellView_ApplyViewMode` is the one place a
  mode becomes a list view view + image list: sizes above 32 get the view's
  own ILC_COLOR32 list (`sys_to_own` maps system indices; icons from
  SHIL_EXTRALARGE/JUMBO), thumbnails come from a per-view thread
  (`thumb_queue`, refcounted because it may outlive the view; newest request
  first; results posted as `WM_SV_THUMBNAIL`, matched by child pidl and
  generation). Tiles/Content are LV_VIEW_TILE; Content's right column is
  drawn in item post-paint. Grouping (`group_pid`) recomputes all groups on a
  posted `WM_SV_REGROUP` after changes. Recent documents on open; Shift+F10
  menu at the focused item.
- **0155 shell32: the FS drop target is real** (was a stub that only logged):
  HDROP or ID list, Windows' key rules and same-volume move, SHFileOperation.
  A folder item's `GetUIObjectOf(IID_IDropTarget)` binds to the item.
- **0156 shell32: Send to and pins** -- `sendto.c`. SendTo items by
  extension: `.lnk` (folder: copy; program: run with the files), folder,
  program, `.DeskLink`, `.mydocs`, else the type's `shell\sendto\command`
  (`%*`): sg-shell's zip reg registers `.ZFSendToTarget` that way and
  sg-session plants the empty SendTo files once per profile. Pins:
  `HKCU\Software\Stained Glass\Explorer\QuickAccess` `Pinned`
  (REG_MULTI_SZ; absent = Desktop, Downloads, Documents, Pictures) -- **the
  same key fileexplorer.c reads; change both together.**
- **0157 explorer:** views table (`views[]`, saved as mode | size << 8 in
  `FolderViews`), `FolderGroups`, the pane (`PANE_DETAILS`/`PANE_PREVIEW`,
  Alt+Shift+P / Alt+P, `Pane` value), Quick access as `CONTENT_QUICK` -- a
  page like This PC, reached through a pidl of Windows' Home CLSID
  (`fe.quick`, never given to the browser; `navigate()` diverts it) --
  frequent folders (`QuickAccess\Frequent`, visits >= 2), recent files from
  the Recent folder; the loop waits on `RegNotifyChangeKeyValue` so a pin
  from any menu shows at once. The navigation pane is a drop target that
  forwards to the hovered folder's.

**Gate: `make test-explorer2`** (`test/explorer2-gate.sh`, 29 checks; with
`SGZIP=` pointing at sg-shell's sg-zip64.exe for the zip check): thumbnail
pixels of a known PNG and JPEG at Large and Extra large icons, the same
pixels when selected (0151), a drag onto the navigation pane, Group by Type
typed through the Sort menu (header line pixels) and in process, Tiles and
Content in process, a sideways photo's EXIF orientation, no thumbnail for a
text file, preview pane text, details pane, Send to (zip and desktop
shortcut), pin and unpin, Quick access page, frequent folder, recent files
(and a name outside the code page), type names. The build before 0150 failed
23 of the first 25 (it passes "opens Pictures" and .txt's "Text Document"
from 0101); 10.0-16 fails 24 of them; a mutant without orientation, with
GetImage ignoring SIIGBF_THUMBNAILONLY, without the Unicode recent link and
without unpin fails exactly those 4. **Editing `patches/series` in the shared
checkout puts your patches into other agents' builds** (build.sh follows the
series): cut and test in your own tree first.

Not done: editable properties in the details pane, collapsing groups from
the keyboard (video/PDF thumbnails, the thumbnail cache, image dimensions,
Remove from Quick access and pin reordering: 0244-0246 below).

## `patches/sg/0244`-`0246`: File Explorer, round 3

- **0244 shell32: video and PDF thumbnails** -- two more providers in
  `thumbnail.c` (`struct image_thumbnail` has a `kind`): video
  `{a3d8b0f2-6c1e-4d57-9b8a-2f4e7c9d1a60}`, PDF
  `{c51f3e8a-2b7d-4e90-8f6a-9d3c1b5e7a24}`, wine.inf ShellEx lines for
  their extensions after .jxr's (**a prefix needs `wineboot -u`** for them).
  They take a path (IInitializeWithItem/File), not a stream -- the WIC one
  alone exposes IInitializeWithStream. Video: `IMFSourceReader` with
  `MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING`, RGB32, the frame at 1/10 of
  the length (3/10 if dark); works for H.264, VP8, MPEG-4 in STA and MTA.
  **Wine's `MF_MT_DEFAULT_STRIDE` is in pixels, not bytes** (320 for a
  320-wide frame): `read_frame` keeps only the sign of a stride smaller than
  a row. **MF GUIDs come from `mfuuid` and the MF headers must come before
  `initguid.h`** (otherwise IID_IMFSample etc. are defined twice). PDF: a
  Windows program has no pipe to a native one, so shell32 starts
  `\\?\unix/usr/bin/sg-pdf --thumbnail PDF SIZE OUT.png` (sg-session;
  `SG_PDF` names another) on unix paths from `wine_get_unix_file_name`, and
  polls up to 20 s for OUT.png (written then renamed) or OUT.png.err.
- **0245 shell32: the thumbnail cache** --
  `%LOCALAPPDATA%\Microsoft\Windows\Explorer\sg-thumbcache\HASH_SIZE.thumb`,
  HASH = FNV-1a 64 of the lower-cased path's UTF-16. `struct tc_header` (40
  bytes: magic "SGTC", version 1, width, height, the source's last write
  time, its size, path length) then the path then BGRA rows; used only if
  time, size and path match; a hit touches the entry's time, which is the
  LRU order. `ThumbnailCacheKB` (HKCU `Software\Stained Glass\Explorer`,
  default 102400, min 16) -- over it, oldest go down to 3/4; checked at a
  process's first write and whenever the running total passes the limit.
  Policies `NoThumbnailCache` / `DisableThumbnailCache` turn it off. Bump
  `TC_VERSION` if the layout changes.
- **0246 explorer:** `fileprops.c` -- `image_properties` (WIC size and
  `IWICPixelFormatInfo` bits per pixel) and `video_properties` (MF frame size,
  `MF_PD_DURATION`); `media_lines` adds them to the details pane after "Date
  created". Quick access: `QuickAccess\Excluded` (REG_MULTI_SZ) --
  "Remove from Quick access" (`CMD_QA_REMOVE`, first in a frequent folder's
  menu, in the tree and on the page) deletes its `Frequent` value and lists
  it there; `qa_visit`/`qa_frequent` skip excluded folders. Pin reordering is
  done by hand in `tree_proc` (**it eats WM_LBUTTONDOWN, so TVN_BEGINDRAG
  never comes**): press on a pin + move past the drag threshold = drag, an
  insert mark (TVM_SETINSERTMARK), drop = `move_pin` rewrites `Pinned`;
  Escape / lost capture cancel. `trace_quick_rows` traces each Quick access
  row's centre in window coordinates after every `layout()` (the gate clicks
  them).

**Gate: `make test-explorer3`** (`test/explorer3-gate.sh`, 19 checks, Xvfb
:153 or `EXPLORER3_DPY`; needs ffmpeg and python3 with PIL and cairo;
`SG_PDF_HELPER=` a checkout's `bin/sg-pdf` if sg-session's is not
installed): MP4/WebM/AVI thumbnail colours and the Videos view's pixels,
the PDF's first page, the cache entry, a doctored entry read back (magenta),
a touched file regenerated, the 16 KB limit with the newest kept, the
details pane lines for a PNG and an MP4, Remove from Quick access typed
through the tree's context menu (trace, registry, not back after visits),
a pin dragged above Desktop with xdotool (registry order, the tree).
explorer-probe gained `close-all`. The build before (10.0-57) failed 12 of
the first 13. Mutants: no cache (cache always off) fails the 4 cache
checks; width and height swapped in `media_lines` fails dimensions; the
menu command not calling `qa_remove_frequent` fails the 3 remove checks.
Also run `make test-explorer2` (29/29 with these).

## `patches/sg/0168`: the user's regional format is their choice

Found by the first-run setup (sg-session's OOBE): Wine took the user's locale
from the Unix locale the process started with (`LC_MESSAGES`, in
`ntdll/unix/env.c`) and at **every process start** kernelbase rewrote
`HKCU\Control Panel\International` to match it -- so a format chosen in
Settings > Region, or at first run, was undone by the very next program, and
`Geo\Nation` was reset with it. Now, as on Windows, a valid `LocaleName` there
decides the user's format (`GetUserDefaultLCID`, `LOCALE_USER_DEFAULT`); the
Unix locale still decides the display language (ntdll's UI language is
untouched). The other format values are regenerated once per new choice --
`sg-FormatLocale` names the locale they were last generated for -- and a
country chosen apart from the format is kept. A hive written before the patch
has its values regenerated once for its own LocaleName (a hand-edited
`sShortDate` is reset that once).

**Gate: `make test-region`** (`test/region-gate.sh`, `test/region-probe.c`,
LANG=C.UTF-8): en-GB chosen gives dd/MM/yyyy for the next two programs and in
the registry; de-DE with country 94 keeps 94 and a decimal comma; a bogus
LocaleName is ignored. Stock Wine fails 5 of 6 (the unchosen check passes).

## `patches/sg/0181`-`0182`: Magnifier, the On-Screen Keyboard, appbars

- **0181**: `magnify.exe` and `osk.exe` in system32/syswow64 are launchers
  (calc's `handoff.c`, 0120; new modules, so configure and configure.ac are
  patched) for what App Paths registers -- sg-shell's Magnifier and
  On-Screen Keyboard. Explorer's keys (`shellkeys.c`): Win+Plus (`=` or the
  keypad's +) runs `magnify.exe`, or, when a window of class `SgMagnifier`
  is open, posts it `WM_COMMAND` 0x101 (zoom in); Win+Minus 0x102; Win+Esc
  `WM_CLOSE`; with no Magnifier open those two do nothing. Win+Ctrl+O runs
  `osk.exe`, or closes `OSKMainClass` when it is open. **That window class
  and those command ids are the interface** with sg-shell's `src/magnify`
  -- change both together.
- **0182: an appbar's space comes off the work area.** Explorer's appbar
  code kept the rectangles and never touched the work area; now every
  ABM_SETPOS/ABM_REMOVE sets it (0074's global SPI_SETWORKAREA) to the
  screen less the taskbar (`Shell_TrayWnd` at the bottom edge) and every
  appbar's space, dropping appbars whose window is gone. The docked
  Magnifier (top) and docked keyboard (bottom) are appbars. **win32u cached
  SPI_GETWORKAREA for the life of a process** (upstream's `spi_loaded`), so
  after 0074 made the work area global a running program still saw its
  first answer forever -- found when a program's own appbar removal never
  showed. It now asks the monitors each time (re-read only on a new serial).
- **Gate: `make test-a11y`** (`test/a11y-gate.sh`, `test/a11y-probe.c` as
  probe and as stand-ins that log their command lines and messages): the
  four files; 64-bit `magnify.exe /lens /zoom:300` and 32-bit `osk.exe`
  reaching App Paths with their arguments; Win+Minus/Win+Esc starting
  nothing, Win+Plus starting Magnifier, Win+Plus / Win+keypad-Plus /
  Win+Minus / Win+Esc as 0x101 / 0x101 / 0x102 / WM_CLOSE to the open one,
  Win+Ctrl+O on and off (all typed with xdotool); the work area with a top
  appbar and back after ABM_REMOVE in the same process, a 32-bit program's
  left appbar, and an appbar whose program exited giving its space back.
  19 checks; a wine-sg without them fails 18 (1 vacuous); 0182 without the
  win32u change fails "removing it gives the space back". sg-shell's
  `magnify-check.sh` and `osk-check.sh` drive the real programs on it.

## `patches/sg/0250`-`0251`: the X keyboard layout's keys

Found making sg-shell's On-Screen Keyboard label its keys from the layout
(`MapVirtualKeyEx` from a scan code, `ToUnicodeEx`). Wine's keyboard is the
X server's keymap: winex11 builds `keyc2vkey`/`keyc2scan` by matching it
against its own layout tables (`main_key_tab`); **the HKL is only the
locale's** (`GetKeyboardLayout` says 0409 whatever the X layout is), and
`ActivateKeyboardLayout` is a stub -- a layout is switched on the X server
(setxkbmap; the compositor's keymap under Xwayland).

- **0250:** in a UTF-8 locale `X11DRV_InitKeyboard` matched no key whose
  keysym is not ASCII (its fallback for XkbTranslateKeySym was 0; the tables
  are ISO-8859 bytes, DetectLayout's fallback the keysym's low byte): German
  ß ü ö ä, French é è à ... had spare virtual keys and scan codes 0x60+, so
  nothing could reach them by scan code. Dead keys never reached the
  matching, and the German table lacked the ´ key. **And a program is not
  always given a MappingNotify on a layout switch** (Xlib reads the new XKB
  map by itself), or gets it before the XKB map is new: its tables stayed
  the old layout's while its keysyms were new -- German Z typed Y.
  `check_keyboard_mapping()` compares the keysyms the tables were built from
  (`keyc2sym`) with the current ones before every use (KeyEvent,
  ToUnicodeEx, MapVirtualKeyEx, VkKeyScanEx, GetKeyNameText) and rebuilds.
- **0251:** `ToUnicodeEx` with Ctrl+Alt in the key state gives the key's
  third level (XKB level 3/4 in the current group, looked up with the
  ISO_Level3_Shift or Mode_switch modifier instead of Control) -- Windows'
  AltGr, what an on-screen keyboard sends; a key without one gives nothing,
  as before (US Ctrl+Alt+Q).
- **Gate: `make test-kbdlayout`** (`test/kbdlayout-gate.sh`,
  `kbdlayout-probe.c`; setxkbmap us/de/fr under Xvfb): German ß ü ö ä ´ at
  their scan codes, QWERTZ, AltGr @ and |, French a/é/AltGr #, US Ctrl+Alt+Q
  nothing, a program started on US following a switch to German and typing
  z ü @ by scan code. 8 checks; the tree without them fails 6 (the
  following check is timing-dependent there: sg-shell's
  `osk-layout-check.sh` catches the stale tables every time -- a build
  without the rebuild fails its typing checks). user32:input 0 failures.
- Not done: the HKL still does not name the X layout (Wine's FIXME: Winword
  reads it as a code page), `ActivateKeyboardLayout`/`LoadKeyboardLayout`
  cannot switch the X layout, no `WM_INPUTLANGCHANGE` reaches programs on a
  switch, and a dead key sent by scan code types its accent at once (no
  composition for injected input).

## `patches/sg/0190`-`0191`: WebView2 apps draw

Apps built on Microsoft Edge WebView2 (the runtime is the user's: the
Evergreen installer from Microsoft, never shipped; apps bring
`WebView2Loader.dll`) worked except for the one thing a user sees: the
Evergreen standalone installer runs silently (`/silent /install`, ~80 s),
registers `HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}`
`pv`/`location` and installs `EdgeWebView\Application\<ver>\msedgewebview2.exe`;
the loader finds it, the environment and controller are created, pages load
and `ExecuteScript` answers -- and nothing is ever painted. WebView2's GPU
process calls `DCompositionCreateDevice(NULL, IID_IDCompositionDevice)`
(Chromium's software output device on the swap-chain path), Wine answered
`E_NOTIMPL`, the GPU process CHECKs and dies (`ProcessFailed=6`), restarts,
gives up. No browser argument avoids the path (`--disable-gpu`,
`--disable-direct-composition`, swiftshader... all tried).

- **0190 dxgi:** `IDXGIFactory2::CreateSwapChainForComposition` for Direct3D
  11 devices (`dlls/dxgi/composition.c`). The back buffer is a texture that
  persists across presents; `Present` reads it back and draws it into the
  target window with GDI -- slow next to a compositor, but simple. A private
  interface, `IWineDXGICompositionSwapChain::set_target(hwnd, x, y)`
  (`include/wine/winedxgi.idl`), is how DirectComposition tells it where.
  The host window (in the app's process) can paint over the frame at any
  time, so the last frame is drawn again 100 ms after each present and
  every 500 ms after that: 1 run in 5 showed the page without, 5 of 5 with.
- **0191 dcomp:** the version-1 device -- `CreateTargetForHwnd`,
  `CreateVisual`, `Commit`, `IDCompositionTarget::SetRoot`, visuals with
  offsets, content and children; `Commit` hands each composition swap chain
  in a target's tree its window and offset. Version 2/3 devices still fail
  on purpose (Chromium probes them to choose DirectComposition for GPU
  compositing and keeps its plain window path without). Surfaces,
  transforms, clips, effects and animations are not done.
  **`include/dcomp.idl` had `IDCompositionVisual`'s overloads in the wrong
  vtable order**: MSVC lays an overload set out in reverse declaration
  order (mingw-w64's public `dcomp.h` encodes the same), so
  `SetOffsetX(IDCompositionAnimation*)` comes before `SetOffsetX(float)`
  (and SetOffsetY, SetTransform, SetClip). Other interfaces with overloads
  (transforms) have the same problem, left alone: nothing implements them.
- **Browser arguments at High integrity:** a prefix owner is an
  administrator, and then the loader honours only the HKLM policy
  `HKLM\Software\Policies\Microsoft\Edge\WebView2\AdditionalBrowserArguments`
  (value `*` or `<exe>`); `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS` and the
  HKCU policy are ignored. Useful for `--enable-logging --v=1` when
  debugging; nothing needs it now.
- **Gate: `make test-webview2`** (`test/webview2-gate.sh`,
  `test/wv2test.c` -- our own app against the public SDK header, BSD-3,
  fetched at test time): `WV2_INSTALLER=` the runtime installer and
  `WV2_SDK=` the unpacked `Microsoft.Web.WebView2` package, or `NETWORK=1`
  to fetch both into `~/.cache/stained-glass/webview2/`. Installs the
  runtime into a fresh prefix, checks its registration, runs the app (a
  page whose script sets the title, `ExecuteScript`) and requires the page's
  green on the screen. Stock wine-sg fails only that last check. Not
  verified: dxgi conformance tests, input into the WebView, a real GPU,
  whether full Edge now takes the swap-chain path (`make test-edge`).
  Pages appear 15-40 s after launch on a loaded machine (llvmpipe).
## `patches/sg/0160`-`0163`: dark mode, system-wide and live

Settings > Personalization > Colors writes Windows' two modes,
`HKCU\...\Themes\Personalize` `AppsUseLightTheme` (programs) and
`SystemUsesLightTheme` (the shell), and broadcasts `WM_SETTINGCHANGE
"ImmersiveColorSet"`. What follows:

- **0160 light.msstyles gets a "Dark" colour scheme** beside "Blue" (Stained
  Glass Light), as a Windows style carries several (COLORNAMES,
  FILERESNAMES, display names). **Dark is generated, never drawn:**
  `theme/dark.py` turns BLUE_INI into DARK_INI (images renamed, colours
  remapped, the system colours replaced by its `DARK_SYS` table, check box and
  radio text colours added -- Light leaves them to the program's DC, which is
  black) and every rendered `blue_*.bmp` into `dark_*.bmp` (greys turned over
  onto a dark ramp, pale accent tints to dark ones, the accent kept; 24- and
  32-bit bitmaps). `build.sh` runs it after the series and the image render
  (`generate_dark_scheme`), and the series fingerprint and CI's cache key
  include `theme/`, so a script change refreshes the tree. `light.rc`
  `#include`s the generated `dark.rc`; makedep tracks it and the images. Blue's
  [SysMetrics] colours are now sg-shell's palette (50-sg-colors.reg), so
  switching back lands where a new profile starts.
- **0161 uxtheme/win32u:** `RefreshImmersiveColorPolicyState` (ordinal 104)
  picks Dark while AppsUseLightTheme is 0 (only for a style that has a Dark
  scheme), writes ColorName, applies the scheme's **system colours only**
  (never its fonts/sizes -- `MSSTYLES_GetThemeSysColors`), saves them in
  `Control Panel\Colors`, broadcasts WM_THEMECHANGED and repaints shown,
  framed windows' frames. **Every process follows**: uxtheme watches the
  ThemeManager key (RegNotifyChangeKeyValue) and reloads the style in
  OpenThemeData/IsThemeActive; win32u re-reads its cached system colours
  when another process's WM_SYSCOLORCHANGE arrives (`reload_sys_colors`,
  in peek_message's sent-message path) -- stock Wine cached them per process
  forever. A changed colour gets a new brush; the old one is left alive.
- **0162 dwmapi/win32u: dark title bars.** `DWMWA_USE_IMMERSIVE_DARK_MODE`
  (20, and 19) and `DWMWA_CAPTION_COLOR`/`DWMWA_TEXT_COLOR` become window
  properties (`__wine_dark_caption`, `__wine_caption_color`/`_text`, colour +
  1) that defwnd.c's caption painter reads (`caption_color`); own windows
  only.
- **0163 explorer:** the taskbar's palette follows SystemUsesLightTheme (a
  light #eeeeee bar), and the shell calls RefreshImmersiveColorPolicyState at
  start and on ImmersiveColorSet, then repaints the desktop **and every
  window after it** (the desktop surface is flushed over its children, 0069 --
  repainting only the desktop drew its icons over windows).
- **Things that bit:** a frame repaint of *every* top-level window (hidden
  and transparent helpers included) erased the desktop's icons -- only shown,
  framed windows now. A test prefix keeps the `light.msstyles` it was created
  with (wineboot copies it): make a new prefix after rebuilding the style.

**Gate: `make test-darkmode`** (21 checks, pixels + the programs' own
reports): a DWM-dark title bar in light mode, then the app mode dark -- another
process's title bar, window background, menu bar and themed button go dark
and it sees the Dark scheme and colours itself; a new process starts dark;
the taskbar follows the Windows mode both ways; back to light everything but
the DWM-dark title bar is light and the registry keeps the light colours.
Against 10.0-43 (no 0160-0163) it fails 14. sg-shell's Settings, Start and
network flyout follow the modes with `src/sg-mode.h`.

## `patches/sg/0164`: the taskbar honours Settings

`programs/explorer/systray.c` reads Settings > Personalization > Taskbar
(`sg_load_settings`) at start and on `WM_SETTINGCHANGE "TraySettings"`:
position (`HKCU\Software\Stained Glass\Taskbar` `Position`, ABE_*; vertical
bars 62/48 px with square buttons, clock and tray icons at the foot),
`AutoHide` (tucks to 2 px after 600 ms away, back at the edge, topmost, no
work area; repaints what it uncovers), `TaskbarSmallIcons` (30 px),
`TaskbarGlomLevel` (0/1/2; default 2 = never, as before -- combined buttons
are one icon per executable, a stacked edge, a menu to choose),
`TaskbarAl` (centre), `ShowTaskViewButton`, `...\Search SearchboxTaskbarMode`
(icon or box; opens Start with wparam 1). SHAppBarMessage answers from it
(ABM_GETSTATE, ABM_GETTASKBARPOS, ABM_GETAUTOHIDEBAR) and ABM_SETSTATE sets
auto-hide (`ABM_SETSTATE` added to shellapi.h).

- **The bar follows WinEvents** (show/hide/foreground/name change of top-level
  windows, debounced 50 ms): before, a window shown after its creation
  notification had no button until something else happened.
- **Button icons come from the executable** (`ExtractIconEx` of
  `QueryFullProcessImageName`): Wine's HICONs are per process, so another
  program's `WM_GETICON` handle draws nothing here.
- Not done: dragging the bar to an edge, resizing it, per-monitor bars,
  jump lists, badges, peek.

**Gate: `make test-taskbar`** (23 checks: the bar's rectangle, work area,
SHAppBarMessage, its buttons and pixels for each option, auto-hide by the
pointer, ABM_SETSTATE). 10.0-16 (no 0164) fails 21.

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
