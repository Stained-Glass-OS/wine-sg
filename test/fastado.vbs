' msado15 batch (patches/sg/3004), run by test/fastado-gate.sh
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

Set rs = CreateObject("ADODB.Recordset")
rs.Fields.Append "id", 3
rs.Fields.Append "name", 200, 20
rs.Fields.Append "extra", 200, 5
Check rs.Fields.Count = 3, "three fields"
rs.Fields.Delete "extra"
Check rs.Fields.Count = 2, "Fields.Delete on a closed recordset"
n = 0
For Each f In rs.Fields
    n = n + 1
Next
Check n = 2, "enumerating the fields"
rs.Open
rs.AddNew
rs.Fields("id").Value = 1
rs.Fields("name").Value = "one"
rs.Update
rs.AddNew
rs.Fields("id").Value = 2
rs.Fields("name").Value = "two"
rs.Update
rs.MoveFirst
Check rs.Collect("name") = "one", "Collect by name"
rs.Collect(1) = "uno"
Check rs.Fields(1).Value = "uno", "Collect put by index"
Check rs.Fields("name").ActualSize = 3, "ActualSize of a string"
Check rs.Fields("id").ActualSize = 4, "ActualSize of an integer"
Check rs.Fields("id").Status = 0, "field status is OK"
Check rs.Fields("id").Precision = 255, "precision is not available"
Check rs.DataMember = "", "DataMember starts empty"
rs.DataMember = "m"
Check rs.DataMember = "m", "DataMember round trips"
Check rs.MarshalOptions = 0, "MarshalOptions defaults to all"
rs.MarshalOptions = 1
Check rs.MarshalOptions = 1, "MarshalOptions set"
Check rs.StayInSync = True, "StayInSync defaults to true"
rs.StayInSync = False
Check rs.StayInSync = False, "StayInSync set"
b1 = rs.Bookmark
rs.MoveNext
b2 = rs.Bookmark
Check rs.CompareBookmarks(b1, b2) = 0, "first bookmark is less"
Check rs.CompareBookmarks(b2, b1) = 2, "second bookmark is greater"
Check rs.CompareBookmarks(b1, b1) = 1, "equal bookmarks"

Set c = rs.Clone
Check c.RecordCount = 2, "clone has the rows"
Check c.Fields.Count = 2, "clone has the fields"
c.MoveFirst
c.MoveNext
Check c.Fields("name").Value = "two", "clone has its own position"
Check rs.Fields("name").Value = "two", "original keeps its position"
c.MoveFirst
Check c.Fields("name").Value = "uno", "clone has the values"
Check rs.Fields("name").Value = "two", "moving the clone leaves the original"

Set nxt = rs.NextRecordset
Check nxt Is Nothing, "no next recordset"

Set cn = CreateObject("ADODB.Connection")
Check cn.Errors.Count = 0, "a connection has no errors"
cn.Errors.Clear
Check cn.Errors.Count = 0, "Errors.Clear"
Set e1 = cn.Errors
Set e2 = cn.Errors
Check e1 Is e2, "Errors is the same collection each time"
On Error Resume Next
cn.BeginTrans
Check Err.Number <> 0, "BeginTrans on a closed connection fails"
Err.Clear

If fails = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL " & fails
End If
