' adoparam.vbs: ADO commands with parameters, for test/adoparam-gate.sh (Stained Glass OS wine-sg 6034)
On Error Resume Next
Set c = CreateObject("ADODB.Connection")
c.Open "Driver={SG ADO Mock}"
Set cmd = CreateObject("ADODB.Command")
Set cmd.ActiveConnection = c
cmd.CommandText = "SELECT Name FROM People WHERE Age > ?"
cmd.Parameters.Append cmd.CreateParameter("age", 3, 1, , 35)
Set rs = cmd.Execute
WScript.Echo "select_err=" & Hex(Err.Number) : Err.Clear
If IsObject(rs) Then WScript.Echo "select=" & rs("Name").Value & "|" & rs.RecordCount
cmd.Parameters("age").Value = 10
Set rs = cmd.Execute
n = 0
Do While Not rs.EOF And n < 10
  n = n + 1
  rs.MoveNext
Loop
WScript.Echo "reexecute=" & n
Set cmd2 = CreateObject("ADODB.Command")
Set cmd2.ActiveConnection = c
cmd2.CommandText = "INSERT INTO People (Id, Name, Age) VALUES (?, ?, ?)"
cmd2.Execute affected, Array(3, "O'Hara", Null)
WScript.Echo "insert_err=" & Hex(Err.Number) & " affected=" & affected : Err.Clear
Set cmd3 = CreateObject("ADODB.Command")
Set cmd3.ActiveConnection = c
cmd3.CommandText = "UPDATE People SET Age = ? WHERE Name = ?"
cmd3.Parameters.Append cmd3.CreateParameter("a", 3, 1, , 77)
cmd3.Parameters.Append cmd3.CreateParameter("n", 202, 1, 50, "O'Hara")
cmd3.Execute affected
WScript.Echo "update_err=" & Hex(Err.Number) & " affected=" & affected : Err.Clear
Set cmd4 = CreateObject("ADODB.Command")
Set cmd4.ActiveConnection = c
cmd4.CommandText = "AddAge"
cmd4.CommandType = 4
cmd4.Parameters.Append cmd4.CreateParameter("RETURN_VALUE", 3, 4)
cmd4.Parameters.Append cmd4.CreateParameter("x", 3, 3, , 5)
cmd4.Execute
WScript.Echo "proc_err=" & Hex(Err.Number) : Err.Clear
WScript.Echo "proc_return=" & cmd4.Parameters("RETURN_VALUE").Value
WScript.Echo "proc_inout=" & cmd4.Parameters("x").Value
Set cmd5 = CreateObject("ADODB.Command")
Set cmd5.ActiveConnection = c
cmd5.CommandText = "ListPeople"
cmd5.CommandType = 4
Set rp = cmd5.Execute
n = 0
If IsObject(rp) Then
  If Not rp Is Nothing Then
    Do While Not rp.EOF And n < 10
      n = n + 1
      rp.MoveNext
    Loop
  End If
End If
WScript.Echo "proc_rows=" & n
Set r = c.Execute("SELECT Id, Name, Age FROM People WHERE Id = 3")
WScript.Echo "row3=" & r("Id") & "," & r("Name") & "," & r("Age")
