' msi batch (patches/sg/3010), run by test/fastmsi-gate.sh
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

Set inst = CreateObject("WindowsInstaller.Installer")

inst.Environment("FASTMSI") = "value one"
v = (inst.Environment("FASTMSI") = "value one")
Check v, "Environment round trips"
v = (inst.Environment("FASTMSI_UNSET") = "")
Check v, "unset Environment is empty"

Set fso = CreateObject("Scripting.FileSystemObject")
f = fso.GetSpecialFolder(2).Path & "\fastmsi.txt"
Set t = fso.CreateTextFile(f, True)
t.Write "12345"
t.Close
v = (inst.FileSize(f) = 5)
Check v, "FileSize"
v = ((inst.FileAttributes(f) And 32) = 32)
Check v, "FileAttributes (archive)"
v = (inst.FileSize(Left(f, 3)) >= 0)
fso.DeleteFile f

sys = fso.GetSpecialFolder(1).Path & "\kernel32.dll"
v = (Len(inst.FileVersion(sys)) > 0)
Check v, "FileVersion of a system dll"

v = (inst.LastErrorRecord Is Nothing)
Check v, "LastErrorRecord is Nothing without an error"

inst.EnableLog "iwearucmpv", fso.GetSpecialFolder(2).Path & "\fastmsi.log"

v = (inst.ProductState("{00000000-0000-0000-0000-000000000000}") = -1)
Check v, "ProductState of an unknown product"
v = (inst.FeatureState("{00000000-0000-0000-0000-000000000000}", "Nothing") <> 3)
Check v, "FeatureState of an unknown product is not local"
v = (inst.ComponentPath("{00000000-0000-0000-0000-000000000000}", "{00000000-0000-0000-0000-000000000001}") = "")
Check v, "ComponentPath of an unknown component is empty"

Set comps = inst.Components
v = (comps.Count >= 0)
Check v, "Components list"
Set feats = inst.Features("{00000000-0000-0000-0000-000000000000}")
v = (feats.Count = 0)
Check v, "Features of an unknown product is empty"
Set clients = inst.ComponentClients("{00000000-0000-0000-0000-000000000001}")
v = (clients.Count = 0)
Check v, "ComponentClients of an unknown component is empty"
Set patches = inst.Patches("{00000000-0000-0000-0000-000000000000}")
v = (patches.Count = 0)
Check v, "Patches of an unknown product is empty"

On Error Resume Next
x = inst.FeatureParent("{00000000-0000-0000-0000-000000000000}", "Nothing")
v = (Err.Number <> 0 Or Err.Description <> "")
Err.Clear
On Error GoTo 0
Check v, "FeatureParent of an unknown product raises"

On Error Resume Next
inst.OpenProduct "{00000000-0000-0000-0000-000000000000}"
v = (Err.Number <> 0 Or Err.Description <> "")
Err.Clear
On Error GoTo 0
Check v, "OpenProduct of an unknown product raises"

If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL (" & fails & ")"
