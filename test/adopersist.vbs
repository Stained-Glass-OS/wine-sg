' adopersist.vbs: a saved recordset opened again, for test/adopersist-gate.sh (Stained Glass OS wine-sg 9454)
On Error Resume Next
Set fso = CreateObject("Scripting.FileSystemObject")
dir = fso.GetSpecialFolder(2)
f1 = dir & "\sgpersist1.xml" : f2 = dir & "\sgpersist2.xml" : f3 = dir & "\sgpersist3.xml"
For Each f In Array(f1, f2, f3)
  If fso.FileExists(f) Then fso.DeleteFile f
Next
Set rs = CreateObject("ADODB.Recordset")
rs.Fields.Append "Id", 3
rs.Fields.Append "Name", 202, 20, 32
rs.Fields.Append "Price", 6, , 32
rs.Fields.Append "Born", 7, , 32
rs.Fields.Append "Ok", 11
rs.Fields.Append "Code", 200, 10, 32
rs.Open
rs.AddNew : rs("Id") = 1 : rs("Name") = "Ann" : rs("Price") = CCur("12.5") : rs("Born") = DateSerial(1990, 5, 17) + TimeSerial(8, 30, 0) : rs("Ok") = True : rs("Code") = "a1" : rs.Update
rs.AddNew : rs("Id") = 2 : rs("Name") = "Bo" : rs("Price") = Null : rs("Born") = DateSerial(2001, 12, 31) : rs("Ok") = False : rs("Code") = "b2" : rs.Update
rs.AddNew : rs("Id") = 3 : rs("Name") = "Cy & <x> 'q'" : rs("Price") = CCur("0.25") : rs("Born") = DateSerial(1970, 1, 1) : rs("Ok") = True : rs("Code") = Null : rs.Update
rs.Save f1, 1
WScript.Echo "save=" & Hex(Err.Number) : Err.Clear

Set r2 = CreateObject("ADODB.Recordset")
r2.Open f1
WScript.Echo "open=" & Hex(Err.Number) & "," & r2.State & "," & r2.RecordCount : Err.Clear
WScript.Echo "fields=" & r2.Fields.Count & "," & r2(0).Name & "," & r2(1).Name & "," & r2(5).Name
WScript.Echo "types=" & r2("Id").Type & "," & r2("Name").Type & "," & r2("Price").Type & "," & r2("Born").Type & "," & r2("Ok").Type & "," & r2("Code").Type
WScript.Echo "row1=" & r2("Id") & "," & r2("Name") & "," & r2("Price") & "," & Year(r2("Born")) & "-" & Month(r2("Born")) & "-" & Day(r2("Born")) & " " & Hour(r2("Born")) & ":" & Minute(r2("Born")) & "," & r2("Ok") & "," & r2("Code")
r2.MoveNext
WScript.Echo "row2=" & r2("Id") & "," & IsNull(r2("Price").Value) & "," & r2("Ok") & "," & TypeName(r2("Price").Value)
r2.MoveNext
WScript.Echo "row3=" & r2("Name") & "," & r2("Price") & "," & IsNull(r2("Code").Value)
WScript.Echo "cursor=" & r2.CursorLocation & "," & r2.CursorType & "," & r2.LockType
r2.MoveFirst
r2("Name") = "Anne"
r2.Update
WScript.Echo "edit=" & Hex(Err.Number) : Err.Clear
r2.Save f2, 1
r2.Close
Set r3 = CreateObject("ADODB.Recordset")
r3.Open f2, "Provider=MSPersist;", 3, 4, 256
WScript.Echo "mspersist=" & Hex(Err.Number) & "," & r3.RecordCount & "," & r3("Name") : Err.Clear
r3.Close

' a Stream
Set stm = CreateObject("ADODB.Stream")
stm.Type = 1
stm.Open
rs.Save stm, 1
WScript.Echo "save_stream=" & Hex(Err.Number) : Err.Clear
Set r4 = CreateObject("ADODB.Recordset")
r4.Open stm
WScript.Echo "stream=" & Hex(Err.Number) & "," & r4.RecordCount & "," & r4("Name") : Err.Clear
r4.Close

' a file as ADO on Windows writes it: a field with a name of its own, the changes pending in a batch
Set t = fso.CreateTextFile(f3, True)
t.WriteLine "<xml xmlns:s='uuid:BDC6E3F0-6DA3-11d1-A2A3-00AA00C14882' xmlns:dt='uuid:C2F41010-65B3-11d1-A29F-00AA00C14882' xmlns:rs='urn:schemas-microsoft-com:rowset' xmlns:z='#RowsetSchema'>"
t.WriteLine "<s:Schema id='RowsetSchema'>"
t.WriteLine "  <s:ElementType name='row' content='eltOnly' rs:updatable='true'>"
t.WriteLine "    <s:AttributeType name='Id' rs:number='1' rs:writeunknown='true'>"
t.WriteLine "      <s:datatype dt:type='int' dt:maxLength='4' rs:precision='10' rs:fixedlength='true' rs:maybenull='false'/>"
t.WriteLine "    </s:AttributeType>"
t.WriteLine "    <s:AttributeType name='c1' rs:name='Full Name' rs:number='2' rs:nullable='true' rs:writeunknown='true'>"
t.WriteLine "      <s:datatype dt:type='string' rs:dbtype='str' dt:maxLength='50'/>"
t.WriteLine "    </s:AttributeType>"
t.WriteLine "    <s:AttributeType name='Price' rs:number='3' rs:nullable='true' rs:writeunknown='true'>"
t.WriteLine "      <s:datatype dt:type='i8' rs:dbtype='currency' dt:maxLength='8' rs:precision='19' rs:fixedlength='true'/>"
t.WriteLine "    </s:AttributeType>"
t.WriteLine "    <s:AttributeType name='Seen' rs:number='4' rs:nullable='true' rs:writeunknown='true'>"
t.WriteLine "      <s:datatype dt:type='dateTime' rs:dbtype='timestamp' dt:maxLength='16' rs:scale='3' rs:precision='23' rs:fixedlength='true'/>"
t.WriteLine "    </s:AttributeType>"
t.WriteLine "    <s:extends type='rs:rowbase'/>"
t.WriteLine "  </s:ElementType>"
t.WriteLine "</s:Schema>"
t.WriteLine "<rs:data>"
t.WriteLine "  <z:row Id='1' c1='Ann' Price='12.5' Seen='2024-02-29T13:45:10'/>"
t.WriteLine "  <rs:update>"
t.WriteLine "    <rs:original><z:row Id='2' c1='Bo' Price='3'/></rs:original>"
t.WriteLine "    <z:row c1='Bob'/>"
t.WriteLine "  </rs:update>"
t.WriteLine "  <rs:insert><z:row Id='3' c1='Cy'/></rs:insert>"
t.WriteLine "  <rs:delete><z:row Id='4' c1='Dee'/></rs:delete>"
t.WriteLine "</rs:data>"
t.WriteLine "</xml>"
t.Close
Set r5 = CreateObject("ADODB.Recordset")
r5.Open f3
WScript.Echo "win_open=" & Hex(Err.Number) & "," & r5.RecordCount : Err.Clear
WScript.Echo "win_fields=" & r5(1).Name & "," & r5(1).Type & "," & r5(1).DefinedSize & "," & r5("Price").Type & "," & r5("Seen").Type
WScript.Echo "win_row1=" & r5("Full Name") & "," & r5("Price") & "," & Year(r5("Seen")) & "-" & Month(r5("Seen")) & "-" & Day(r5("Seen")) & " " & Hour(r5("Seen")) & ":" & Minute(r5("Seen")) & ":" & Second(r5("Seen"))
r5.MoveNext
WScript.Echo "win_updated=" & r5("Id") & "," & r5("Full Name") & "," & r5("Price") & "," & r5("Full Name").OriginalValue
r5.MoveNext
WScript.Echo "win_inserted=" & r5("Id") & "," & r5("Full Name")
r5.Filter = 1
WScript.Echo "win_pending=" & r5.RecordCount
r5.Filter = 0
r5.Close

Set r6 = CreateObject("ADODB.Recordset")
r6.Open dir & "\sg-no-such-file.xml", "Provider=MSPersist;", , , 256
WScript.Echo "missing=" & (Err.Number <> 0) & "," & r6.State : Err.Clear
Set t = fso.CreateTextFile(f3, True) : t.WriteLine "not xml" : t.Close
Set r7 = CreateObject("ADODB.Recordset")
r7.Open f3
WScript.Echo "garbage=" & (Err.Number <> 0) & "," & r7.State : Err.Clear
For Each f In Array(f1, f2, f3)
  If fso.FileExists(f) Then fso.DeleteFile f
Next
WScript.Echo "done=1"
