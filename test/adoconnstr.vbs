' adoconnstr.vbs: a Recordset opened with a connection string, for test/adoconnstr-gate.sh (Stained Glass OS wine-sg 9451)
On Error Resume Next
Set rs = CreateObject("ADODB.Recordset")
rs.Open "SELECT * FROM People ORDER BY Id", "Driver={SG ADO Mock}", 3, 3
WScript.Echo "open=" & Hex(Err.Number) : Err.Clear
WScript.Echo "state=" & rs.State
WScript.Echo "count=" & rs.RecordCount & "," & rs("Name")
WScript.Echo "conn_type=" & IsObject(rs.ActiveConnection)
WScript.Echo "conn_state=" & rs.ActiveConnection.State
rs("Age") = 31
rs.Update
WScript.Echo "update=" & Hex(Err.Number) : Err.Clear
rs.Close
Set rs2 = CreateObject("ADODB.Recordset")
rs2.ActiveConnection = "Driver={SG ADO Mock}"
WScript.Echo "put=" & Hex(Err.Number) : Err.Clear
WScript.Echo "put_type=" & IsObject(rs2.ActiveConnection)
rs2.Open "SELECT Name, Age FROM People WHERE Age > 30"
WScript.Echo "open2=" & Hex(Err.Number) & "," & rs2("Name") & "," & rs2("Age") : Err.Clear
rs2.Close
Set rs3 = CreateObject("ADODB.Recordset")
rs3.Open "SELECT * FROM People", "Driver={No Such Driver}"
WScript.Echo "bad=" & (Err.Number <> 0) & "," & rs3.State : Err.Clear
WScript.Echo "done=1"
