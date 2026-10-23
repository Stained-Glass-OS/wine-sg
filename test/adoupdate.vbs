' adoupdate.vbs: Recordset changes written to the data source, for test/adoupdate-gate.sh (Stained Glass OS wine-sg 6037)
On Error Resume Next
Set c = CreateObject("ADODB.Connection")
c.Open "Driver={SG ADO Mock}"
Function table()
  Set q = c.Execute("SELECT Id, Name, Age FROM People ORDER BY Id")
  s = ""
  Do While Not q.EOF
    If s <> "" Then s = s & ";"
    s = s & q("Id").Value & "," & q("Name").Value & "," & q("Age").Value
    q.MoveNext
  Loop
  table = s
End Function
Set rs = CreateObject("ADODB.Recordset")
rs.Open "SELECT * FROM People", c, 1, 3
rs("Name") = "Anna"
rs.Update
WScript.Echo "update_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "after_update=" & table()
rs.MoveNext
rs("Age") = 41
rs.MoveNext
WScript.Echo "move_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "after_move=" & table()
rs.AddNew
rs("Id") = 4 : rs("Name") = "Dee" : rs("Age") = 20
rs.Update
WScript.Echo "addnew_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "after_addnew=" & table()
rs.MoveFirst
rs.Delete
WScript.Echo "delete_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "after_delete=" & table()
rs.Close
' batch mode
Set rs = CreateObject("ADODB.Recordset")
rs.Open "SELECT * FROM People", c, 1, 4
rs("Name") = "Bobby"
rs.MoveNext
rs.Delete
rs.AddNew
rs("Id") = 5 : rs("Name") = "Eve" : rs("Age") = 25
rs.Update
WScript.Echo "batch_pending=" & table()
rs.UpdateBatch
WScript.Echo "batch_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "batch_done=" & table()
rs.MoveFirst
rs("Name") = "Nope"
rs.CancelBatch
WScript.Echo "cancel=" & Hex(Err.Number) & "," & rs("Name").Value : Err.Clear
rs.UpdateBatch
WScript.Echo "after_cancel=" & table()
' someone else changed the row since it was read
rs.MoveFirst
rs("Age") = 99
c.Execute "UPDATE People SET Age = 77 WHERE Id = 2"
rs.UpdateBatch
WScript.Echo "conflict=" & Hex(Err.Number) : Err.Clear
WScript.Echo "after_conflict=" & table()
rs.CancelBatch
rs.Close
' a read-only recordset writes nothing
Set rs = CreateObject("ADODB.Recordset")
rs.Open "SELECT * FROM People", c, 0, 1
rs("Name") = "RO"
rs.Update
WScript.Echo "readonly_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "after_readonly=" & table()
