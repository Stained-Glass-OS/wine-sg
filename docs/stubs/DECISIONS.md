# Decisions on "escalate" items (coordinator, delegated by David 2026-10-13)
Principle (David): "max compat with Windows software as best we can".
Implement what a Windows program can observe, the way Windows does it,
gated the way Windows gates it (administrator / privilege checks), and routed
to the real Linux mechanism where one exists. Never fake success for an
operation that did not happen unless Windows itself reports success there.

1. cldapi (Cloud Files API, OneDrive-style sync providers): IMPLEMENT the
   user-mode API: CfRegisterSyncRoot/Unregister/GetSyncRootInfo* (kept where
   Windows keeps them: the SyncRootManager registry key), CfConnectSyncRoot/
   Disconnect with the callback table kept per connection, placeholders
   (CfCreatePlaceholders, CfConvertToPlaceholder, CfSetInSyncState,
   CfGetPlaceholderInfo/StateFromAttributeTag, CfUpdatePlaceholder,
   CfSetPinState, CfDehydrate/Hydrate) with the placeholder state kept as file
   metadata (attributes + an alternate store in the prefix), and
   CfExecute transfers. Hydration-on-open by a filter driver is out of reach:
   document it; CfHydratePlaceholder calls the provider's FETCH_DATA callback
   itself. Gate with a fake provider in a scratch folder.
2. dcomp CreateSurfaceFromHandle / CreateSurfaceFromHwnd (browsers' video
   overlays): IMPLEMENT on the existing dcomp surface objects
   (DCompositionCreateSurfaceHandle handles are ours, so resolve them;
   hwnd surfaces wrap the window's content as dcomp visuals do). Browser
   check at integration.
3. winspool AddPrintProvidor / UploadPrinterDriverPackage: IMPLEMENT as
   Windows does: administrator (SeLoadDriverPrivilege) required, else
   ERROR_ACCESS_DENIED; UploadPrinterDriverPackage copies the package into
   the spooler's driver store in the prefix and returns its path, so a
   following AddPrinterDriverEx works; AddPrintProvidor registers the
   provider under the Print\Providers key (a provider that cannot load is
   reported the way Windows reports a failing provider). Office note.
4. wiaservc event callbacks: IMPLEMENT registration (RegisterEventCallback*
   keep the callbacks, unregister on release; no events fire without a WIA
   device). Device creation/state changes and the WIA dialogs keep answering
   "no device". Do NOT touch the TWAIN/sane path (David's Ambir scanner).
5. shell32 SetAppAsDefault (and SetAppAsDefaultAll, start-menu client, MIME):
   DUPLICATE WINDOWS 10/11 exactly per Microsoft's docs (since Windows 8 these
   no longer change defaults silently; check the documented return), and
   keep our Default apps settings untouched.
6. Network-state changes (IpReleaseAddress, IpRenewAddress, SetIfEntry,
   SetIpForwardEntry and similar): IMPLEMENT through sg-netctl (already used
   by ipconfig /release /renew), administrator only (ERROR_ACCESS_DENIED for a
   standard user, as Windows), never in a test against the real host: gate
   with a stand-in sg-netctl.
7. Local accounts and groups (NetUserAdd/Del/SetInfo, NetLocalGroupAdd/
   Del/AddMembers/DelMembers): IMPLEMENT through sg-admind's existing verbs
   (user-add, user-remove, user-type, user-password), administrator only;
   Administrators <-> administrator type, Users <-> standard. Other local
   groups: kept in the prefix's registry (created, listed, membership) and
   reported as Windows does, without changing Linux groups -- say so in the
   notes. Gate with a stand-in sg-admind spool; never create real accounts.
8. RAS phonebook edits (RasSetEntryProperties, RasDeleteEntry,
   RasRenameEntry, RasSetEntryDialParams...): IMPLEMENT on the phonebook file
   where Windows keeps it (%APPDATA%\Microsoft\Network\Connections\Pbk\
   rasphone.pbk and the all-users one), so enumeration and properties round-
   trip. Dialing stays unsupported (the documented error).
9. Security-model items (tokens, ACLs, logon, credentials, crypto policy) in
   any queue: still escalate -- they go to the integrator with the security
   review; but write each with a concrete Windows-matching proposal.
