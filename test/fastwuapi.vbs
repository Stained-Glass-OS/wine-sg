' wuapi batch (patches/sg/3006), run by test/fastwuapi-gate.sh
Dim fails
fails = 0
Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        fails = fails + 1
    End If
End Sub

Set s = CreateObject("Microsoft.Update.Session")
s.ClientApplicationID = "probe"
Check s.ClientApplicationID = "probe", "session ClientApplicationID round trips"
Check s.ReadOnly = False, "session is not read only"

Set u = s.CreateUpdateSearcher
Check u.Online = True, "searcher is online by default"
u.Online = False
Check u.Online = False, "searcher Online can be cleared"
u.ClientApplicationID = "searcher"
Check u.ClientApplicationID = "searcher", "searcher ClientApplicationID"
u.ServerSelection = 2
Check u.ServerSelection = 2, "searcher ServerSelection"
u.IncludePotentiallySupersededUpdates = True
Check u.IncludePotentiallySupersededUpdates = True, "searcher IncludePotentiallySupersededUpdates"
Check u.EscapeString("it's a 'test'") = "it''s a ''test''", "EscapeString doubles quotes"
Check u.EscapeString("plain") = "plain", "EscapeString leaves plain text"

Set r = u.Search("IsInstalled=0 and Type='Software'")
Check r.ResultCode = 2, "search succeeds"
Check r.Updates.Count = 0, "search finds no updates"
Check r.RootCategories.Count = 0, "search has no categories"
Check r.Warnings.Count = 0, "search has no warnings"
n = 0
For Each x In r.Updates
    n = n + 1
Next
Check n = 0, "enumerating no updates"
Check u.GetTotalHistoryCount = 0, "no update history"
Set h = u.QueryHistory(0, 10)
Check h.Count = 0, "history collection is empty"

Set d = s.CreateUpdateDownloader
Check d.Priority = 2, "downloader priority defaults to normal"
d.Priority = 3
Check d.Priority = 3, "downloader priority set"
d.IsForced = True
Check d.IsForced = True, "downloader IsForced"
d.Updates = r.Updates
Check d.Updates.Count = 0, "downloader Updates"

Set i = s.CreateUpdateInstaller
i.ClientApplicationID = "inst"
Check i.ClientApplicationID = "inst", "installer ClientApplicationID"
Check i.IsBusy = False, "installer is not busy"
Check i.RebootRequiredBeforeInstallation = False, "no reboot required"
i.AllowSourcePrompts = False
Check i.AllowSourcePrompts = False, "installer AllowSourcePrompts"
Check i.Updates.Count = 0, "installer Updates starts empty"

Set a = CreateObject("Microsoft.Update.AutoUpdate")
a.DetectNow
Check a.ServiceEnabled = False, "no update service"

Set si = CreateObject("Microsoft.Update.SystemInfo")
Check si.RebootRequired = False, "system info reboot flag"
Check VarType(si.OemHardwareSupportLink) = 8, "OEM link is a string"

If fails = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL " & fails
End If
