' adofilter.vbs: Recordset.Filter as business scripts use it, for test/adofilter-gate.sh (Stained Glass OS wine-sg 9450)
On Error Resume Next
Function Names(rs)
  Dim s : s = ""
  If rs.RecordCount > 0 Then rs.MoveFirst
  Do While Not rs.EOF
    If s <> "" Then s = s & ","
    s = s & rs("Name")
    rs.MoveNext
  Loop
  Names = s
End Function
Set c = CreateObject("ADODB.Connection")
c.Open "Driver={SG ADO Mock}"
WScript.Echo "open=" & Hex(Err.Number) : Err.Clear
Set rs = CreateObject("ADODB.Recordset")
rs.CursorLocation = 3
rs.Open "SELECT * FROM People ORDER BY Id", c, 3, 3
WScript.Echo "rs_open=" & Hex(Err.Number) : Err.Clear
WScript.Echo "all=" & rs.RecordCount & ":" & Names(rs)
rs.Filter = "Age > 35"
WScript.Echo "gt_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "gt=" & rs.RecordCount & ":" & Names(rs)
rs.Filter = "Age > 35"
WScript.Echo "first=" & rs("Name") & "," & rs.AbsolutePosition
rs.Filter = "Name = 'Ann' OR Name = 'cy'"
WScript.Echo "or=" & rs.RecordCount & ":" & Names(rs)
rs.Filter = "Name LIKE 'D*'"
WScript.Echo "like=" & rs.RecordCount & ":" & Names(rs)
rs.Filter = "(Age >= 30 AND Age <= 40) OR Name = 'Eve'"
WScript.Echo "group=" & rs.RecordCount & ":" & Names(rs)
rs.Filter = "Age >= 30 AND Age < 50 AND Name <> 'Bo'"
WScript.Echo "and3=" & rs.RecordCount & ":" & Names(rs)
rs.Filter = "Name IS NULL"
WScript.Echo "isnull=" & rs.RecordCount & ":" & rs("Id")
rs.Filter = "Name = NULL"
WScript.Echo "eqnull=" & rs.RecordCount
rs.Filter = "Age < 35"
WScript.Echo "nullnotless=" & rs.RecordCount
rs.Filter = "Age > 1000"
WScript.Echo "none=" & rs.RecordCount & "," & rs.EOF & "," & rs.BOF
rs.Filter = 0
WScript.Echo "cleared=" & rs.RecordCount & ":" & Names(rs) & ":" & rs.Filter
rs.Filter = "Nosuch = 1"
WScript.Echo "badfield=" & (Err.Number <> 0) : Err.Clear
rs.Filter = ""
WScript.Echo "empty=" & rs.RecordCount & ":" & rs.Filter
' bookmarks stay valid while a filter comes and goes
rs.MoveFirst : rs.Move 2
bm = rs.Bookmark
WScript.Echo "bm_row=" & rs("Name")
rs.Filter = "Age > 35"
rs.Bookmark = bm
WScript.Echo "bm_filtered=" & Hex(Err.Number) & "," & rs("Name") : Err.Clear
rs.Filter = 0
rs.Bookmark = bm
WScript.Echo "bm_back=" & rs("Name")
' an array of bookmarks
rs.MoveFirst : b1 = rs.Bookmark
rs.MoveLast : b2 = rs.Bookmark
rs.Filter = Array(b1, b2)
WScript.Echo "bmarray=" & rs.RecordCount & ":" & Names(rs)
' a change under a filter is kept where the row is
rs.Filter = "Name = 'Cy'"
rs("Age") = 51
rs.Update
WScript.Echo "edit_err=" & Hex(Err.Number) : Err.Clear
rs.Filter = 0
rs.MoveFirst : rs.Move 2
WScript.Echo "edited=" & rs("Name") & "," & rs("Age") & ":" & Names(rs)
' a row deleted under a filter goes, the others come back
rs.Filter = "Name = 'Bo'"
rs.Delete
WScript.Echo "delete_err=" & Hex(Err.Number) : Err.Clear
rs.Filter = 0
WScript.Echo "deleted=" & rs.RecordCount & ":" & Names(rs)
' a row added under a filter is at the end without it
rs.Filter = "Age > 45"
rs.AddNew
rs("Id") = 7 : rs("Name") = "Flo" : rs("Age") = 70
rs.Update
WScript.Echo "add_err=" & Hex(Err.Number) & "," & rs.RecordCount : Err.Clear
rs.Filter = 0
WScript.Echo "added=" & rs.RecordCount & ":" & Names(rs)
' a sort under a filter sorts the shown rows; without the filter all are sorted
rs.Filter = "Age > 35"
rs.Sort = "Age DESC"
WScript.Echo "sorted_filtered=" & Names(rs)
rs.Filter = 0
WScript.Echo "sorted_all=" & Names(rs)
rs.Sort = ""
rs.Close
' a recordset of its own (no data source) with a date column
Set r2 = CreateObject("ADODB.Recordset")
r2.Fields.Append "Name", 202, 20
r2.Fields.Append "Due", 7
r2.Open
r2.AddNew : r2("Name") = "a" : r2("Due") = DateSerial(2024, 1, 15) : r2.Update
r2.AddNew : r2("Name") = "b" : r2("Due") = DateSerial(2024, 3, 2) : r2.Update
r2.AddNew : r2("Name") = "c" : r2("Due") = DateSerial(2024, 6, 30) : r2.Update
r2.Filter = "Due >= #2/1/2024# AND Due < #6/1/2024#"
WScript.Echo "date_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "date=" & r2.RecordCount & ":" & Names(r2)
' the rows a batch has changed
Set r3 = CreateObject("ADODB.Recordset")
r3.CursorLocation = 3
r3.Open "SELECT * FROM People ORDER BY Id", c, 3, 4
r3.MoveFirst : r3.Move 1
r3("Age") = 41
r3.Update
r3.Filter = 1
WScript.Echo "pending=" & r3.RecordCount & ":" & Names(r3)
r3.Filter = 0
r3.CancelBatch
r3.Close
c.Close
WScript.Echo "done=1"
