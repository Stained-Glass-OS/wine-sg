On Error Resume Next
Dim fails
Sub Check(name, cond)
  If cond Then
    WScript.Echo "PASS " & name
  Else
    WScript.Echo "FAIL " & name & " (err " & Err.Number & " " & Err.Description & ")"
    fails = fails + 1
  End If
  Err.Clear
End Sub

Set svc = GetObject("winmgmts:\\.\root\cimv2")
Set cls = svc.Get("Win32_Process")
Check "get class", Err.Number = 0

' properties: Name, CIMType, IsArray, IsLocal, Origin, enumeration
Set p = cls.Properties_("Handle")
Check "property Name", p.Name = "Handle"
Check "property CIMType string", p.CIMType = 8
Check "property IsArray", p.IsArray = False
Check "property Origin", Len(p.Origin) > 0
Check "property Qualifiers_", p.Qualifiers_.Count >= 1
n = 0
For Each x In cls.Properties_
  n = n + 1
Next
Check "properties enumerate", n > 5

' methods
Check "methods count", cls.Methods_.Count >= 1
k = 0
For Each m In cls.Methods_
  If m.Name = "Create" Then k = 1
Next
Check "methods enumerate", k = 1
Check "method Origin", cls.Methods_("Create").Origin = "Win32_Process"
Check "method OutParameters", Not cls.Methods_("Create").OutParameters Is Nothing

' object operations
Set c = cls.Clone_
Check "Clone_", Err.Number = 0 And cls.CompareTo_(c)
Set inst = cls.SpawnInstance_
inst.Properties_.Item("Name").Value = "x"
Check "property put via Properties_", inst.Name = "x"
d = cls.Derivation_
Check "Derivation_", UBound(d) >= 0
Check "Path_ Class", Err.Number = 0
Check "Qualifiers_", cls.Qualifiers_.Count >= 1
Set q = cls.Qualifiers_("dynamic")
Check "Qualifier Name", Err.Number = 0 And q.Name = "dynamic"
Check "Security_", Not cls.Security_ Is Nothing

' instances and paths
For Each o In svc.ExecQuery("select * from Win32_OperatingSystem")
  Set pa = o.Path_
  Check "object path ns", UCase(pa.Namespace) = "ROOT\CIMV2"
  Check "object path rel", pa.RelPath = "Win32_OperatingSystem"
  Check "object path class", pa.Class = "Win32_OperatingSystem"
  Check "object path display", Left(pa.DisplayName, 9) = "winmgmts:"
  Check "object set Item", Not svc.InstancesOf("Win32_OperatingSystem").Count < 1
Next
Check "InstancesOf", svc.InstancesOf("Win32_Process").Count >= 1

' method call with out parameters (positional, ByRef)
Set reg = GetObject("winmgmts:\\.\root\default:StdRegProv")
rc = reg.GetStringValue(&H80000002, "SOFTWARE\Microsoft\Windows NT\CurrentVersion", "ProductName", val)
Check "method call out param", rc = 0 And Len(val) > 0

' security and privileges
Set sec = svc.Security_
sec.ImpersonationLevel = 3
Check "ImpersonationLevel", sec.ImpersonationLevel = 3
sec.Privileges.AddAsString "SeDebugPrivilege", True
Check "privileges count", sec.Privileges.Count = 1
For Each pv In sec.Privileges
  Check "privilege name", pv.Name = "SeDebugPrivilege" And pv.Identifier = 19 And pv.IsEnabled
Next
sec.Privileges.Remove 19
Check "privilege remove", sec.Privileges.Count = 0
Set loc = CreateObject("WbemScripting.SWbemLocator")
Check "locator ImpersonationLevel default", loc.Security_.ImpersonationLevel = 3
loc.Security_.AuthenticationLevel = 6
Check "locator AuthenticationLevel", loc.Security_.AuthenticationLevel = 6

' named values
Set nv = CreateObject("WbemScripting.SWbemNamedValueSet")
nv.Add "a", 1
nv.Add "b", "two"
Check "namedvalueset Count", nv.Count = 2
s = ""
For Each x In nv
  s = s & x.Name & "=" & x.Value & ";"
Next
Check "namedvalueset enumerate", s = "a=1;b=two;" Or s = "b=two;a=1;"
Set nv2 = nv.Clone
nv2.Remove "a"
Check "namedvalueset Clone", nv2.Count = 1 And nv.Count = 2
nv.Item("a").Value = 7
Check "namedvalue put", nv.Item("a").Value = 7

If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
