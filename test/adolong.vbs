' adolong.vbs: long text columns through ADO and MSDASQL, for test/adolong-gate.sh (Stained Glass OS wine-sg 6038)
On Error Resume Next
Set c = CreateObject("ADODB.Connection")
c.Open "Driver={SG ADO Mock}"
Set rs = c.Execute("SELECT Id, Name FROM People ORDER BY Id")
WScript.Echo "exec_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "value=" & rs("Name").Value
WScript.Echo "type=" & rs("Name").Type
WScript.Echo "long=" & ((rs("Name").Attributes And 128) <> 0)
rs.MoveNext
WScript.Echo "null=" & IsNull(rs("Name").Value)
rs.MoveNext
WScript.Echo "third=" & rs(1).Value
Set rs = CreateObject("ADODB.Recordset")
rs.Open "SELECT * FROM People", c, 1, 3
WScript.Echo "open_err=" & Hex(Err.Number) : Err.Clear
rs("Name") = "Anne"
rs.Update
WScript.Echo "update_err=" & Hex(Err.Number) : Err.Clear
Set q = c.Execute("SELECT Name FROM People WHERE Id = 1")
WScript.Echo "updated=" & q(0).Value
