# Escalations (drafter 7)

## apphelp: the .sdb database reader (SdbCreateDatabase, SdbOpenDatabase, SdbInitDatabase, SdbCloseDatabase, SdbGetFirstChild, and the rest of the Sdb* family)
- **What:** Microsoft documents the API (learn.microsoft.com/en-us/windows/win32/devnotes/application-compatibility-database and one page per function) but not the file: the overview defines TAG, TAGID ("a pointer to an entry in a particular database") and TAG_TYPE_STRINGREF, not the header, the tag stream layout or the numeric tag values. None of the allowed clean-room sources (Wine's headers/IDL, Microsoft's docs, mingw-w64) carries them, so a reader written here would be guesswork, and answering "no children" for a database that has some would be a false result.
- **Why it matters:** programs that read the compatibility database (sdbinst, compat tooling, installers that ask whether a shim applies) get NULL or ERROR_CALL_NOT_IMPLEMENTED today. Wine ships no sysmain.sdb, so ordinary programs are not affected.
- **Proposal (decision needed):** either (a) allow the .sdb reader to be written from an LGPL-2.1+ independent implementation of the format (ReactOS' apphelp by Mark Jansen is licensed that way; it is outside RULES.md's source list, so this is a policy call) -- then implement open/close/init/create and the tag-reading functions together as one batch with a gate that writes a database with SdbCreateDatabase/SdbWriteStringTag and reads it back; or (b) leave the stubs and document that Stained Glass has no application compatibility database.

## newdev: InstallNewDevice (the Add Hardware wizard)
- **What:** `InstallNewDevice(hwndParent, ClassGuid, pReboot)` is the wizard of hdwwiz.cpl (Microsoft: the user "starts at the detection choice page" or, for GUID_NULL / GUID_DEVCLASS_UNKNOWN, the class selection page; pReboot receives DI_NEEDRESTART or DI_NEEDREBOOT). Today it logs a FIXME and returns TRUE with nothing installed and *pReboot untouched.
- **Why it matters:** a program that calls it to let the user add a device is told it succeeded.
- **Proposal (decision needed):** make it hand off to the shell's Add Hardware page the way calc/control/mmc do (an App Paths entry `hdwwiz.cpl`/sg-shell Settings > Devices, started through `\\?\unix`, waited on), return TRUE with *pReboot = 0 only after the user finished it, and FALSE with ERROR_CANCELLED when none is registered. That needs sg-shell to expose the wizard; until then the honest behaviour is FALSE + ERROR_CANCELLED.

## difxapi / newdev: the signature of a driver package
- **What:** DriverPackagePreinstall/Install and DiInstallDriver install a driver package "only if the driver package has a WHQL signature or a Microsoft Authenticode signature" (DIFx pages), with the signing policy of the Windows version, and return CERT_E_EXPIRED, CERT_E_UNTRUSTEDROOT, CERT_E_WRONG_USAGE, ERROR_INVALID_CATALOG_DATA, TRUST_E_NOSIGNATURE, ERROR_SIGNATURE_OSATTRIBUTE_MISMATCH. 5503/5504 check that a catalog is named, present and well formed (setupapi's CryptCATAdminAddCatalog parses it) and give TRUST_E_NOSIGNATURE / CRYPT_E_FILE_ERROR for the unsigned and missing cases; they do **not** verify the catalog's signature or that it lists the INF's hash, and neither does setupapi (SetupVerifyInfFile is not exported at all).
- **Why it matters:** a catalog forged by anyone is accepted, so a driver package from any source installs. It is a trust-policy decision (which roots and publishers may sign drivers), not a code gap: with the verification on, every real vendor package would fail until the vendor's root is trusted.
- **Proposal (decision needed, security review):** verify with WinVerifyTrust(WTD_CHOICE_CATALOG, DRIVER_ACTION_VERIFY) on the catalog and the INF's hash (CryptCATAdminCalcHashFromFileHandle + CryptCATAdminEnumCatalogFromHash), against the machine's Root and TrustedPublisher stores, mapping its errors to the codes above; provide the policy as a registry switch under HKLM\Software\Policies (default: enforce for DIFx / DiInstallDriver, as Windows on x64) so an administrator can add a publisher or turn it off; the 0039/0040 fixes make chain checks real, so it now has something to stand on. The gate would then sign its test catalogs with a run-time CA, as test-appx does.

## appxpackaging: the interfaces Wine's IDL does not define
- **What:** ten of appxpackaging's eleven audit items return E_NOTIMPL or log a FIXME because the interface they return or implement is not defined: IAppxPackageWriter, IAppxBundleWriter, IAppxBlockMapReader / IAppxBlockMapFile / IAppxBlockMapBlock (+ enumerators), IAppxManifestDeviceCapabilitiesEnumerator, and the newer IAppxFactory2-4 (all documented under learn.microsoft.com/en-us/windows/win32/api/appxpackaging/). Wine's include/appxpackaging.idl only forward-declares most of them. Microsoft's pages give method names alphabetically, no IIDs and not the vtable order; mingw-w64 in this container has no appxpackaging.h. (The eleventh, CreateBundleManifestReader, only needed an interface Wine had: done in 5505.)
- **Why it matters:** package tools (MSIX Packaging Tool, makeappx-like programs, Visual Studio's packager) cannot create a package or bundle; code that reads a package's block map or asks for its device capabilities fails. The reader side of the work exists (zip.c, the block-map parser, the manifest parser).
- **Proposal (decision needed):** patch 0063 took the interface IIDs from the MIT-licensed `windows` crate's published metadata; RULES.md (2026-10-12) now says not to use an SDK .winmd. Please rule whether the `windows` crate's generated source (MIT, an independent open-source artifact) is an allowed source for IIDs and vtable order. If yes: define the interfaces in appxpackaging.idl and implement, in one batch, the block-map reader (reusing package.c's parser; CreateValidatedBlockMapReader checks the AXBM digest against AppxSignature.p7x with the 0037 code), GetBlockMap on both readers, GetDeviceCapabilities, the package writer (a ZIP writer with stored or deflated parts, the block map with SHA-256 per 64 KiB, [Content_Types].xml, the manifest last) and the bundle writer, with a gate that writes a package with the writer and reads it back with the reader and makepri-free tools (test/mkappx.py as the oracle).

## winebus.sys: sdl_bus_init / sdl_bus_wait / sdl_bus_stop

These are the stubs compiled when libsdl2 is absent (`SONAME_LIBSDL2` unset). They are the intended
'bus not built' answer, not a gap. Making them real means building with libsdl2-dev, a new build
dependency the owner has to approve; and the code path cannot be exercised here without an SDL
joystick. No patch proposed.

## Found while working winebus (not in the queue)

At wineboot shutdown winedevice can fault: hidclass's read thread (`hid_device_thread`) leaves its
IOCTL_HID_READ_REPORT IRP pending in winebus when `halt_event` fires and exits, freeing the stack
IO_STATUS_BLOCK; `IRP_MN_REMOVE_DEVICE` then reaches winebus `remove_pending_irps`, which completes the
IRP (ntoskrnl.c:2093 writes `*irp->UserIosb`) into that dead stack. Fix sketch: winebus sets a cancel
routine on the parked read IRP; hidclass calls IoCancelIrp and waits for the IRP event before leaving the
thread. Touches upstream-owned paths in two drivers, so left for a decision.

## hal: IRQL and port I/O

- `KfRaiseIrql`, `KfLowerIrql`, `KeGetCurrentIrql` (hal, i386/arm) and the x86_64 `KfRaiseIrql`/`KeLowerIrql`
  in ntoskrnl are all logging stubs. A real implementation keeps one IRQL per thread (a thread-local in
  ntoskrnl, exported to hal), raises in `KeAcquireSpinLock*` and the DPC/work-item runners, and lowers in the
  release routines. Proposal: do it once in ntoskrnl, with hal forwarding. It changes what every driver sees
  from `KeGetCurrentIrql()` (always PASSIVE_LEVEL today), so it wants a decision and a broad driver test run.
- `READ_PORT_*`/`WRITE_PORT_*` on x86 need `iopl`/`ioperm`, i.e. Unix root (or CAP_SYS_RAWIO). The machine
  server is deliberately unprivileged. Proposal: leave them as they are (return 0 / ignore) unless a
  privileged helper process is wanted for specific legacy hardware.

## wineusb.sys: unhandled URB functions, flags and ioctls

Three diagnostics for URB functions/flags/internal ioctls the driver does not map to libusb. Each is
documented, but none can be verified without a USB device; this kernel has no `dummy_hcd` or `usbip`.
Proposal: a drafter with USB hardware (or a usbip loopback) implements them as a set: GET_STATUS_FROM_*,
SET/CLEAR_FEATURE_*, RESET_PIPE, ISOCH_TRANSFER, the `USBD_TRANSFER_DIRECTION`/short-transfer flags, and
`IOCTL_INTERNAL_USB_GET_PORT_STATUS`/`RESET_PORT`.

## ndis.sys: NdisRegisterProtocol

Protocol drivers (firewalls, packet capture such as WinPcap's npf.sys) call it from `DriverEntry`, then
expect `BindAdapterHandler` for each adapter and the send/receive path. Wine has only an ioctl view of
the host's interfaces. A binding model (an NDIS adapter per host interface, packets through AF_PACKET via a
privileged helper) is a design and a security decision; failing the registration remains the safe answer.
