' vbscript IsNull/IsEmpty/IsDate/VarType take an object's default property (patches/sg/9610), run by
' test/vbsnull-gate.sh: a real ADODB.Recordset with a NULL column is tested the way ASP pages and
' admin scripts do it -- IsNull(rs("name")) -- and objects without a usable default stay objects.
Option Explicit
Dim failures, rs, d, x
failures = 0

Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Class NoDefault
    Public v
End Class

Class WithDefault
    Public v
    Public Default Property Get Item
        Item = v
    End Property
End Class

Set rs = CreateObject("ADODB.Recordset")
rs.Fields.Append "id", 3
rs.Fields.Append "name", 202, 20, &H20   ' adFldIsNullable
rs.Fields.Append "born", 7, , &H20       ' adDate
rs.Fields.Append "note", 202, 20, &H20
rs.Open
rs.AddNew
rs("id") = 1
rs("name") = Null
rs("born") = #1/2/2000#
rs.Update
rs.AddNew
rs("id") = 2
rs("name") = "beta"
rs("born") = Null
rs.Update
rs.MoveFirst

Check IsNull(rs("name").Value), "IsNull(rs(""name"").Value) is True (sanity)"
Check IsNull(rs("name")), "IsNull(rs(""name"")) is True for a NULL column"
Check IsNull(rs.Fields("name")), "IsNull(rs.Fields(""name"")) is True"
Check IsNull(rs(1)), "IsNull(rs(1)) is True"
Check Not IsNull(rs("id")), "IsNull(rs(""id"")) is False for a value"
Check VarType(rs("name")) = vbNull, "VarType(rs(""name"")) is vbNull (got " & VarType(rs("name")) & ")"
Check VarType(rs("id")) = vbLong, "VarType(rs(""id"")) is vbLong (got " & VarType(rs("id")) & ")"
Check IsDate(rs("born")), "IsDate(rs(""born"")) is True for a date column"
Check TypeName(rs("name")) = "Field", "TypeName(rs(""name"")) stays Field (got " & TypeName(rs("name")) & ")"
Check IsObject(rs("name")), "IsObject(rs(""name"")) stays True"
Set x = rs("name")
Check IsNull(x), "a Field held in a variable: IsNull(x) is True"
If IsNull(rs("name")) Then x = "(none)" Else x = rs("name")
Check x = "(none)", "If IsNull(rs(""name"")) Then ... takes the Null branch"
rs.MoveNext
Check Not IsNull(rs("name")), "second row: IsNull(rs(""name"")) is False"
Check VarType(rs("name")) = vbString, "second row: VarType(rs(""name"")) is vbString (got " & VarType(rs("name")) & ")"
Check IsNull(rs("born")), "second row: IsNull(rs(""born"")) is True"
Check Not IsDate(rs("born")), "second row: IsDate(rs(""born"")) is False"

' script classes
Set x = New NoDefault
Check Not IsNull(x) And Not IsEmpty(x) And VarType(x) = vbObject, "a class without Default stays an object"
Set x = New WithDefault
x.v = Null
Check IsNull(x), "a class whose Default property is Null: IsNull is True"
x.v = Empty
Check IsEmpty(x), "a class whose Default property is Empty: IsEmpty is True"
x.v = "s"
Check VarType(x) = vbString, "a class whose Default property is a String: VarType is vbString (got " & VarType(x) & ")"

' a Dictionary's default (Item) needs a key: it stays an object, and no error is left behind
Set d = CreateObject("Scripting.Dictionary")
Err.Clear
Check VarType(d) = vbObject, "VarType(Dictionary) is vbObject (got " & VarType(d) & ")"
Check Not IsNull(d) And Not IsEmpty(d), "a Dictionary is neither Null nor Empty"
Check Err.Number = 0, "no error is left behind (Err " & Err.Number & ")"
Check Not IsNull(Nothing) And VarType(Nothing) = vbObject, "Nothing stays an object"

If failures = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & failures & ")"
End If
