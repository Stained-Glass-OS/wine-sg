' wshom batch (patches/sg/3010), run by test/fastwshom-gate.sh
Dim fails, v
fails = 0
Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        fails = fails + 1
    End If
End Sub

Set sh = CreateObject("WScript.Shell")

' environments
Set pe = sh.Environment("Process")
pe("FASTWSHOM") = "hello"
v = (pe("FASTWSHOM") = "hello")
Check v, "process environment put/get"
v = (sh.ExpandEnvironmentStrings("%FASTWSHOM%") = "hello")
Check v, "process variable reaches the script process"
n = 0
For Each e In pe
    If Left(e, 10) = "FASTWSHOM=" Then n = n + 1
Next
Check n = 1, "process environment enumerates NAME=value"
v = (pe.Count > 0)
Check v, "process environment count"
pe.Remove "FASTWSHOM"
v = (pe("FASTWSHOM") = "")
Check v, "process environment remove"

Set ue = sh.Environment("User")
ue("FASTWSHOM") = "user value"
v = (sh.Environment("User")("FASTWSHOM") = "user value")
Check v, "user environment persists in the registry"
v = (sh.RegRead("HKCU\Environment\FASTWSHOM") = "user value")
Check v, "user environment is HKCU\Environment"
ue.Remove "FASTWSHOM"
v = (sh.Environment("User")("FASTWSHOM") = "")
Check v, "user environment remove"

' special folders
Set sf = sh.SpecialFolders
v = (Len(sf.Item("Desktop")) > 0 And Len(sf.Item("Programs")) > 0 And Len(sf.Item("Startup")) > 0)
Check v, "special folder paths"
v = (Len(sf.Item("AllUsersDesktop")) > 0 And Len(sf.Item("Fonts")) > 0 And Len(sf.Item("MyDocuments")) > 0)
Check v, "more special folders"
v = (IsEmpty(sf.Item("NoSuchFolder")))
Check v, "unknown special folder is empty"
v = (sf.Count = 16)
Check v, "special folders count"
n = 0
For Each p In sf
    n = n + 1
Next
Check n = 16, "special folders enumerate"

' registry
sh.RegWrite "HKCU\Software\FastWshom\a", "1"
sh.RegWrite "HKCU\Software\FastWshom\Sub\b", 2, "REG_DWORD"
sh.RegDelete "HKCU\Software\FastWshom\a"
On Error Resume Next
sh.RegRead "HKCU\Software\FastWshom\a"
v = (Err.Number <> 0)
Err.Clear
On Error GoTo 0
Check v, "RegDelete removes a value"
sh.RegDelete "HKCU\Software\FastWshom\"
On Error Resume Next
sh.RegRead "HKCU\Software\FastWshom\Sub\b"
v = (Err.Number <> 0)
Err.Clear
On Error GoTo 0
Check v, "RegDelete removes a key and its subkeys"

' shortcut
Set fso = CreateObject("Scripting.FileSystemObject")
lnk = fso.GetSpecialFolder(2).Path & "\fastwshom.lnk"
Set sc = sh.CreateShortcut(lnk)
v = (sc.FullName = lnk)
Check v, "shortcut FullName"
sc.TargetPath = "C:\windows\system32\cmd.exe"
sc.Description = "a description"
sc.Hotkey = "CTRL+ALT+F"
sc.Save
Set sc2 = sh.CreateShortcut(lnk)
v = (sc2.TargetPath = "C:\windows\system32\cmd.exe")
Check v, "shortcut TargetPath round trips"
v = (sc2.Description = "a description")
Check v, "shortcut Description round trips"
WScript.Echo "# hotkey: " & sc2.Hotkey
v = (InStr(sc2.Hotkey, "F") > 0 And InStr(sc2.Hotkey, "Ctrl") > 0 And InStr(sc2.Hotkey, "Alt") > 0)
Check v, "shortcut Hotkey round trips"
fso.DeleteFile lnk

' Exec
Set ex = sh.Exec("cmd.exe /c echo piped& exit 3")
Do While ex.Status = 0
    WScript.Sleep 50
Loop
v = (ex.ExitCode = 3)
Check v, "Exec ExitCode"
line = ex.StdOut.ReadLine
v = (line = "piped")
Check v, "Exec StdOut.ReadLine"
Check ex.StdOut.AtEndOfStream, "Exec StdOut ends when the process exits"

' network
Set net = CreateObject("WScript.Network")
v = (Len(net.UserProfile) > 0)
Check v, "network UserProfile"
v = (net.EnumNetworkDrives.Count >= 0)
Check v, "EnumNetworkDrives"
v = (net.EnumPrinterConnections.Count >= 0)
Check v, "EnumPrinterConnections"

If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL (" & fails & ")"
