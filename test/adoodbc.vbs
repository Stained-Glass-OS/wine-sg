' adoodbc.vbs: ADO over ODBC as a script uses it, for test/adoodbc-gate.sh (Stained Glass OS wine-sg 6032)
On Error Resume Next
Set c = CreateObject("ADODB.Connection")
c.Open "Driver={SG ADO Mock}"
WScript.Echo "open=" & Hex(Err.Number) : Err.Clear
Set rs = c.Execute("SELECT * FROM People ORDER BY Id")
WScript.Echo "execute=" & Hex(Err.Number) : Err.Clear
n = 0
If Not IsObject(rs) Then Set rs = Nothing
If rs Is Nothing Then WScript.Quit 1
Do While Not rs.EOF And n < 10
  n = n + 1
  WScript.Echo "row=" & rs.Fields("Id").Value & "," & rs.Fields("Name").Value & "," & rs.Fields("Age").Value
  rs.MoveNext
Loop
WScript.Echo "rows=" & n
Set rs2 = CreateObject("ADODB.Recordset")
rs2.Open "SELECT Name, Age FROM People WHERE Age > 35", c, 3, 1
WScript.Echo "open_byref=" & Hex(Err.Number) : Err.Clear
WScript.Echo "collect=" & rs2("Name")
WScript.Echo "collect_err=" & Hex(Err.Number) : Err.Clear
Set rs3 = CreateObject("ADODB.Recordset")
rs3.CursorLocation = 3
rs3.Open "SELECT * FROM People ORDER BY Id", c, 3, 4
rs3("Age") = 5
WScript.Echo "collect_put_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "collect_put=" & rs3("Age")
c.Execute "DELETE FROM People WHERE Id = 99"
WScript.Echo "second_execute=" & Hex(Err.Number) : Err.Clear
c.Close
WScript.Echo "close=" & Hex(Err.Number)
