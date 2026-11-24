' msado15 Field.Value takes the field's type (patches/sg/9614), run by test/adocoerce-gate.sh:
' a page assigns form strings to numeric columns and adds them up; Windows' cursor engine stores
' the column's type, so the sum is a number, not "53".
Option Explicit
Dim failures, rs, e
failures = 0

Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        failures = failures + 1
    End If
End Sub

Set rs = CreateObject("ADODB.Recordset")
rs.Fields.Append "a", 3                ' adInteger
rs.Fields.Append "b", 3
rs.Fields.Append "price", 6            ' adCurrency
rs.Fields.Append "when", 135, , &H20   ' adDBTimeStamp, nullable
rs.Fields.Append "name", 202, 20, &H20 ' adVarWChar
rs.Fields.Append "flag", 11            ' adBoolean
rs.Fields.Append "any", 12             ' adVariant
rs.Open
rs.AddNew
rs("a") = "5"
rs("b") = "3"
rs("price") = "2.5"
rs("when") = "2001-02-03 04:05:06"
rs("name") = 42
rs("flag") = 1
rs("any") = "7"
rs.Update
rs.MoveFirst

Check VarType(rs("a").Value) = vbLong, "a string given to an adInteger column reads back a Long (got " & VarType(rs("a").Value) & ")"
Check rs("a").Value + rs("b").Value = 8, "the two columns add up to 8 (got " & (rs("a").Value + rs("b").Value) & ")"
Check VarType(rs("price").Value) = vbCurrency, "adCurrency reads back a Currency (got " & VarType(rs("price").Value) & ")"
Check VarType(rs("when").Value) = vbDate, "adDBTimeStamp reads back a Date (got " & VarType(rs("when").Value) & ")"
Check Year(rs("when").Value) = 2001 And Second(rs("when").Value) = 6, "the date is the one given"
Check VarType(rs("name").Value) = vbString And rs("name").Value = "42", "a number given to adVarWChar reads back the String ""42"""
Check VarType(rs("flag").Value) = vbBoolean And rs("flag").Value = True, "1 given to adBoolean reads back True"
Check VarType(rs("any").Value) = vbString, "adVariant keeps what it is given"
rs("when") = Null
Check IsNull(rs("when").Value), "Null stays Null"
On Error Resume Next
rs("a") = "five"
e = Err.Number
On Error GoTo 0
Check e = &H80040E21, "a value that is no number is refused with ""Multiple-step operation generated errors"" (Err " & Hex(e) & ")"
Check rs("a").Value = 5, "the refused value leaves the old one"

If failures = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & failures & ")"
End If
