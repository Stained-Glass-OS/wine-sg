' msado15 batch 2 (patch 3203), run by test/fastado2-gate.sh
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
Function Err3(code)
    Err3 = (Err.Number = code)
End Function

Set rs = CreateObject("ADODB.Recordset")
rs.Fields.Append "id", 3
rs.Fields.Append "name", 200, 20
rs.Fields.Append "note", 203, 100
rs.Open
Check rs.Supports(&H200) = True, "Supports(adMovePrevious)"
Check rs.Supports(&H400000) = False, "Supports(adSeek) is false"
Check rs.Supports(&H1000400) = False, "Supports(adAddNew) is false for a read-only lock"
Set rs = CreateObject("ADODB.Recordset")
rs.Fields.Append "id", 3
rs.Fields.Append "name", 200, 20
rs.Fields.Append "note", 203, 100
rs.LockType = 4
rs.Open
Check rs.Supports(&H1000400) = True, "Supports(adAddNew) with a batch lock"
Check rs.Supports(&H10000) = True, "Supports(adUpdateBatch) with a batch lock"

' AddNew and Update with field lists
rs.AddNew Array("id", "name"), Array(1, "one")
Check rs.Fields("id").Value = 1 And rs.Fields("name").Value = "one", "AddNew with a field list"
rs.Update
rs.AddNew
rs.Fields("id").Value = 2
rs.Fields("name").Value = "two"
rs.Update
rs.MoveFirst
rs.Update Array("name", "note"), Array("uno", "memo")
Check rs.Fields("name").Value = "uno" And rs.Fields("note").Value = "memo", "Update with arrays of fields and values"
On Error Resume Next
rs.Update Array("name", "note"), Array("x")
Check Err3(3001), "Update with arrays of different length is an invalid argument"
Err.Clear
On Error GoTo 0

' OriginalValue, UnderlyingValue, CancelUpdate
rs.MoveFirst
rs.Fields("name").Value = "changed"
Check rs.Fields("name").OriginalValue = "uno", "OriginalValue during an edit"
Check rs.Fields("name").UnderlyingValue = "uno", "UnderlyingValue during an edit"
Check rs.Fields("name").Value = "changed", "the new value is visible"
rs.CancelUpdate
Check rs.Fields("name").Value = "uno", "CancelUpdate restores the row"
rs.AddNew
rs.Fields("id").Value = 99
rs.CancelUpdate
Check rs.RecordCount = 2, "CancelUpdate drops the row AddNew appended"

' chunks
rs.MoveFirst
rs.Fields("note").Value = ""
rs.Fields("note").AppendChunk "abc"
rs.Fields("note").AppendChunk "def"
Check rs.Fields("note").Value = "abcdef", "AppendChunk appends"
c1 = rs.Fields("note").GetChunk(4)
c2 = rs.Fields("note").GetChunk(4)
c3 = rs.Fields("note").GetChunk(4)
Check c1 = "abcd", "GetChunk returns the first piece"
Check c2 = "ef", "GetChunk returns the rest"
Check IsNull(c3), "GetChunk returns Null at the end"
rs.Update

' precision and scale
rs.Fields("id").Precision = 10
rs.Fields("id").NumericScale = 2
Check rs.Fields("id").Precision = 10 And rs.Fields("id").NumericScale = 2, "Precision and NumericScale can be set"

' properties
Check rs.Fields("id").Properties.Count = 0, "a field has no counted properties"
Set p = rs.Fields("id").Properties("Optimize")
Check p.Name = "Optimize", "Property.Name"
Check p.Type = 11, "Property.Type"
p.Value = True
Check p.Value = True, "Property.Value can be set"
Check rs.Properties.Count = 0, "Recordset.Properties is empty here"
Set cn = CreateObject("ADODB.Connection")
Check cn.Properties.Count = 0, "Connection.Properties is empty before Open"
Set cm = CreateObject("ADODB.Command")
Check cm.Properties.Count = 0, "Command.Properties is empty"

' seek, index, requery, resync, active command, data source
On Error Resume Next
rs.Seek 1
Check Err3(3251), "Seek is not available (3251)"
Err.Clear
x = rs.Index
Check Err3(3251), "Index is not available (3251)"
Err.Clear
rs.Requery
Check Err3(3709), "Requery of a recordset without a connection (3709)"
Err.Clear
rs.Resync
Check Err3(3709), "Resync of a recordset without a connection (3709)"
Err.Clear
On Error GoTo 0
Check rs.ActiveCommand Is Nothing, "ActiveCommand is Nothing"
rs.Cancel
cn.Cancel
cm.Cancel
Check True, "Cancel on objects with nothing pending"

On Error Resume Next
Set cm.ActiveConnection = Nothing
cm.CommandText = "select 1"
cm.Execute
Check Err.Number <> 0, "Command.Execute without a connection fails"
Err.Clear
On Error GoTo 0

' save as XML
Set fso = CreateObject("Scripting.FileSystemObject")
fname = fso.GetSpecialFolder(2) & "\fastado2.xml"
If fso.FileExists(fname) Then fso.DeleteFile fname
rs.Save fname, 1
Set f = fso.OpenTextFile(fname, 1)
txt = f.ReadAll
f.Close
Check InStr(txt, "<s:AttributeType name='id' rs:number='1'") > 0, "the XML has the schema"
Check InStr(txt, "dt:type='int'") > 0 And InStr(txt, "dt:type='string'") > 0, "the XML has the data types"
Check InStr(txt, "<z:row id='1' name='uno' note='abcdef'/>") > 0, "the XML has the first row"
Check InStr(txt, "<z:row id='2' name='two'/>") > 0, "the XML leaves out null values"
On Error Resume Next
rs.Save fname, 1
Check Err.Number <> 0, "Save does not overwrite an existing file"
Err.Clear
On Error GoTo 0
fso.DeleteFile fname

If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL"
