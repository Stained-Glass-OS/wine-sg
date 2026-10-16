' msi automation members (patch 3218), run by test/fastmsiaut-gate.sh
Dim fails, v, fso, dir, inst
fails = 0
Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        fails = fails + 1
    End If
End Sub

Function Failed()
    Failed = (Err.Number <> 0 Or Err.Description <> "")
End Function

Sub RunSql(db, sql)
    Dim vw
    Set vw = db.OpenView(sql)
    vw.Execute
    vw.Close
End Sub

Function CountRows(db, sql)
    Dim vw, rec, n
    Set vw = db.OpenView(sql)
    vw.Execute
    n = 0
    Do
        Set rec = vw.Fetch
        If rec Is Nothing Then Exit Do
        n = n + 1
    Loop
    vw.Close
    CountRows = n
End Function

Function ReadFile(path)
    Dim t
    Set t = fso.OpenTextFile(path, 1)
    ReadFile = t.ReadAll
    t.Close
End Function

Function CompName(i)
    If i = 0 Then CompName = "comp" Else CompName = "comp" & i
End Function

Sub BuildPackage(path, bigfiles)
    Dim db, i
    Set db = inst.OpenDatabase(path, 3)
    RunSql db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Property`)"
    If bigfiles Then
        RunSql db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{3E218000-1111-4000-8000-0000000000A2}')"
    Else
        RunSql db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{3E218000-1111-4000-8000-0000000000A1}')"
    End If
    RunSql db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'SG Automation Test')"
    RunSql db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.2.3')"
    RunSql db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')"
    RunSql db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'Wine')"
    RunSql db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ALLUSERS', '1')"
    RunSql db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('PropOne', 'plain')"
    RunSql db, "CREATE TABLE `Directory` (`Directory` CHAR(255) NOT NULL, `Directory_Parent` CHAR(255), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)"
    RunSql db, "INSERT INTO `Directory` (`Directory`, `DefaultDir`) VALUES ('TARGETDIR', 'SourceDir')"
    RunSql db, "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)"
    If bigfiles Then
        RunSql db, "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `KeyPath`) VALUES ('comp', '{3E218000-2222-4000-8000-0000000000C1}', 'TARGETDIR', 0, 'f0')"
    Else
        RunSql db, "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `KeyPath`) VALUES ('comp', '{3E218000-2222-4000-8000-0000000000C1}', 'TARGETDIR', 4, 'reg1')"
    End If
    RunSql db, "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64), `Description` CHAR(255), `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)"
    RunSql db, "INSERT INTO `Feature` (`Feature`, `Title`, `Description`, `Level`, `Directory_`, `Attributes`) VALUES ('feat', 'Feature title', 'Feature description', 1, 'TARGETDIR', 0)"
    RunSql db, "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)"
    RunSql db, "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('feat', 'comp')"
    RunSql db, "CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)"
    RunSql db, "INSERT INTO `Registry` (`Registry`, `Root`, `Key`, `Name`, `Value`, `Component_`) VALUES ('reg1', 2, 'Software\SGMsiAut', 'One', '[PropOne]', 'comp')"
    RunSql db, "CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` SHORT NOT NULL, `DiskPrompt` CHAR(64), `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(32) PRIMARY KEY `DiskId`)"
    RunSql db, "CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` SHORT NOT NULL PRIMARY KEY `File`)"
    If bigfiles Then
        ' four components of 250 files of 2 GB each: 2 TB, more than a disk has free
        For i = 0 To 999
            RunSql db, "INSERT INTO `File` (`File`, `Component_`, `FileName`, `FileSize`, `Sequence`) VALUES ('f" & i & "', '" & CompName(i \ 250) & "', 'f" & i & ".dat', 2147483647, " & (i + 1) & ")"
        Next
        For i = 1 To 3
            RunSql db, "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `KeyPath`) VALUES ('" & CompName(i) & "', '{3E218000-2222-4000-8000-0000000000C" & (i + 1) & "}', 'TARGETDIR', 0, 'f" & (i * 250) & "')"
            RunSql db, "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('feat', '" & CompName(i) & "')"
        Next
    End If
    RunSql db, "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)"
    RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('CostInitialize', 800)"
    RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('FileCost', 900)"
    RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('CostFinalize', 1000)"
    RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallValidate', 1400)"
    If Not bigfiles Then
        RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallInitialize', 1500)"
        RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('ProcessComponents', 1600)"
        RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('WriteRegistryValues', 5000)"
        RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('RemoveRegistryValues', 3600)"
        RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('RegisterProduct', 6100)"
        RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('PublishFeatures', 6300)"
        RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('PublishProduct', 6400)"
        RunSql db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallFinalize', 6600)"
    End If
    If Not bigfiles Then
        RunSql db, "CREATE TABLE `AdvtExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)"
        RunSql db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('CostInitialize', 800)"
        RunSql db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('CostFinalize', 1000)"
        RunSql db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallValidate', 1400)"
        RunSql db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallInitialize', 1500)"
        RunSql db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('PublishFeatures', 1600)"
        RunSql db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('PublishProduct', 1700)"
        RunSql db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallFinalize', 6600)"
    End If
    RunSql db, "CREATE TABLE `AutSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)"
    RunSql db, "INSERT INTO `AutSequence` (`Action`, `Sequence`) VALUES ('CostInitialize', 800)"
    RunSql db, "INSERT INTO `AutSequence` (`Action`, `Sequence`) VALUES ('FileCost', 900)"
    RunSql db, "INSERT INTO `AutSequence` (`Action`, `Sequence`) VALUES ('CostFinalize', 1000)"
    RunSql db, "INSERT INTO `AutSequence` (`Action`, `Sequence`) VALUES ('InstallValidate', 1400)"
    Set si = db.SummaryInformation(20)
    si.Property(2) = "SG Automation Test"
    si.Property(7) = ";1033"
    si.Property(9) = "{3E218000-AAAA-4000-8000-0000000000B1}"
    si.Property(14) = CLng(200)
    si.Property(15) = CLng(2)
    si.Persist
    Set si = Nothing
    db.Commit
End Sub

Function Rev(t)
    Dim i, r
    r = ""
    For i = Len(t) To 1 Step -1
        r = r & Mid(t, i, 1)
    Next
    Rev = r
End Function

Function RevPairs(t)
    Dim i, r
    r = ""
    For i = 1 To Len(t) Step 2
        r = r & Rev(Mid(t, i, 2))
    Next
    RevPairs = r
End Function

Function Squash(g)
    Dim p
    p = Split(Mid(g, 2, 36), "-")
    Squash = Rev(p(0)) & Rev(p(1)) & Rev(p(2)) & RevPairs(p(3)) & RevPairs(p(4))
End Function

Set inst = CreateObject("WindowsInstaller.Installer")
Set fso = CreateObject("Scripting.FileSystemObject")
dir = fso.GetSpecialFolder(2).Path & "\fastmsiaut"
inst.UILevel = 2
If inst.ProductState("{3E218000-1111-4000-8000-0000000000A1}") <> -1 Then inst.ConfigureProduct "{3E218000-1111-4000-8000-0000000000A1}", 0, 2
If fso.FolderExists(dir) Then fso.DeleteFolder dir, True
fso.CreateFolder dir

' ---------------------------------------------------------------- Database
Set db = inst.OpenDatabase(dir & "\a.msi", 3)
Check db.DatabaseState = 1, "Database.DatabaseState of a created database is write"
RunSql db, "CREATE TABLE `T` (`K` CHAR(72) NOT NULL, `V` LONG PRIMARY KEY `K`)"
RunSql db, "INSERT INTO `T` (`K`, `V`) VALUES ('one', 1)"
RunSql db, "INSERT INTO `T` (`K`, `V`) VALUES ('two', 2)"
Set pk = db.PrimaryKeys("T")
Check pk.FieldCount = 1 And pk.StringData(1) = "K", "Database.PrimaryKeys names the key column"
Check db.TablePersistent("T") = 1, "Database.TablePersistent of an existing table is true"
Check db.TablePersistent("Missing") = 2, "Database.TablePersistent of a missing table is none"
On Error Resume Next
Set pk = db.PrimaryKeys("Missing")
Check Failed(), "Database.PrimaryKeys of a missing table fails"
Err.Clear
On Error GoTo 0

db.Commit
Check db.DatabaseState = 1, "Database.Commit keeps a write database"
db.Export "T", dir, "T.idt"
txt = ReadFile(dir & "\T.idt")
Check InStr(txt, "one" & vbTab & "1") > 0 And InStr(txt, "two" & vbTab & "2") > 0, "Database.Export writes the rows"
Check InStr(txt, "K" & vbTab & "V") = 1, "Database.Export writes the column header first"

Set db2 = inst.OpenDatabase(dir & "\b.msi", 3)
db2.Import dir, "T.idt"
RunSql db2, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Property`)"
RunSql db2, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{3E218000-5555-4000-8000-0000000000E1}')"
RunSql db2, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')"
Check CountRows(db2, "SELECT `K` FROM `T`") = 2, "Database.Import creates the table with its rows"
db2.Commit

' transforms
Set db3 = inst.OpenDatabase(dir & "\c.msi", 3)
db3.Import dir, "T.idt"
RunSql db3, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Property`)"
RunSql db3, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{3E218000-5555-4000-8000-0000000000E1}')"
RunSql db3, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')"
RunSql db3, "UPDATE `T` SET `V` = 20 WHERE `K` = 'two'"
RunSql db3, "INSERT INTO `T` (`K`, `V`) VALUES ('three', 3)"
db3.Commit
Check db3.GenerateTransform(db2, dir & "\x.mst") = True, "Database.GenerateTransform is true when the databases differ"
Check fso.FileExists(dir & "\x.mst"), "Database.GenerateTransform writes the transform file"
Check db2.GenerateTransform(db2, dir & "\none.mst") = False, "Database.GenerateTransform is false without differences"
Check Not fso.FileExists(dir & "\none.mst"), "no transform file without differences"
Set si = db3.SummaryInformation(20): si.Property(2) = "T": si.Persist
Set si = db2.SummaryInformation(20): si.Property(2) = "T": si.Persist
db3.CreateTransformSummaryInfo db2, dir & "\x.mst", 0, 0
db2.ApplyTransform dir & "\x.mst", 0
Check CountRows(db2, "SELECT `K` FROM `T`") = 3, "Database.ApplyTransform adds the row"
Set vw = db2.OpenView("SELECT `V` FROM `T` WHERE `K` = 'two'")
vw.Execute
Set rec = vw.Fetch
Check rec.IntegerData(1) = 20, "Database.ApplyTransform updates the row"
vw.Close
On Error Resume Next
db2.ApplyTransform dir & "\missing.mst", 0
Check Failed(), "Database.ApplyTransform of a missing file fails"
Err.Clear
On Error GoTo 0

' merge
Set db4 = inst.OpenDatabase(dir & "\d.msi", 3)
RunSql db4, "CREATE TABLE `T` (`K` CHAR(72) NOT NULL, `V` LONG PRIMARY KEY `K`)"
RunSql db4, "INSERT INTO `T` (`K`, `V`) VALUES ('four', 4)"
db4.Commit
db2.Merge db4, "MergeErrors"
Check CountRows(db2, "SELECT `K` FROM `T`") = 4, "Database.Merge adds the rows of the other database"
RunSql db4, "INSERT INTO `T` (`K`, `V`) VALUES ('one', 99)"
On Error Resume Next
db2.Merge db4, "MergeErrors"
Check Failed(), "Database.Merge with a conflicting row fails"
Err.Clear
On Error GoTo 0

' view
Set vw = db.OpenView("SELECT `K`, `V` FROM `T`")
vw.Execute
Set ci = vw.ColumnInfo(0)
Check ci.FieldCount = 2 And ci.StringData(1) = "K" And ci.StringData(2) = "V", "View.ColumnInfo names"
Set ci = vw.ColumnInfo(1)
Check ci.StringData(1) = "s72" And ci.StringData(2) = "I4", "View.ColumnInfo types: " & ci.StringData(1) & " " & ci.StringData(2)
Check vw.GetError = "", "View.GetError is empty without an error"
vw.Close

' record
Set rec = inst.CreateRecord(3)
rec.StringData(0) = "x=[1] y=[2]"
rec.StringData(1) = "A"
rec.IntegerData(2) = 5
Check rec.FormatText = "x=A y=5", "Record.FormatText"
Check rec.IsNull(3) = True And rec.IsNull(1) = False, "Record.IsNull"
Check rec.DataSize(1) = 1, "Record.DataSize of a string"
rec.ClearData
Check rec.IsNull(1) = True And rec.IsNull(2) = True, "Record.ClearData"

' streams
Set dbs = inst.OpenDatabase(dir & "\s.msi", 3)
RunSql dbs, "CREATE TABLE `B` (`K` CHAR(8) NOT NULL, `D` OBJECT PRIMARY KEY `K`)"
Set t = fso.CreateTextFile(dir & "\data.bin", True)
t.Write "ABCDEFGH"
t.Close
Set vw = dbs.OpenView("INSERT INTO `B` (`K`, `D`) VALUES (?, ?)")
Set rec = inst.CreateRecord(2)
rec.StringData(1) = "k1"
rec.SetStream 2, dir & "\data.bin"
vw.Execute rec
vw.Close
Set vw = dbs.OpenView("SELECT `K`, `D` FROM `B`")
vw.Execute
Set rec = vw.Fetch
Check rec.DataSize(2) = 8, "Record.DataSize of a stream"
Check rec.ReadStream(2, 3, 2) = "ABC", "Record.ReadStream as ANSI"
Check rec.ReadStream(2, 2, 1) = "DE", "Record.ReadStream as bytes continues"
vw.Close
Set vw = dbs.OpenView("SELECT `K`, `D` FROM `B`")
vw.Execute
Set rec = vw.Fetch
Check rec.ReadStream(2, 4, 0) = 1145258561, "Record.ReadStream as an integer"
vw.Close
Set rec = Nothing
Set vw = Nothing
Set dbs = Nothing

' summary information
Set si = db.SummaryInformation(20)
si.Property(2) = "SG title"
si.Property(4) = "SG author"
si.Persist
db.Commit
Set si = db.SummaryInformation(0)
Check si.Property(2) = "SG title" And si.Property(4) = "SG author", "SummaryInfo.Persist writes the properties"

' ---------------------------------------------------------------- Session
BuildPackage dir & "\pkg.msi", False
inst.UILevel = 2
Set sess = inst.OpenPackage(dir & "\pkg.msi", 0)
sess.DoAction "CostInitialize"
Set fi = sess.FeatureInfo("feat")
Check fi.Title = "Feature title", "FeatureInfo.Title"
Check fi.Description = "Feature description", "FeatureInfo.Description"
Check fi.Attributes = 1, "FeatureInfo.Attributes (favor local): " & fi.Attributes
fi.Attributes = 2
Check fi.Attributes = 2, "FeatureInfo.Attributes can be set: " & fi.Attributes
Check sess.ProductProperty("ProductName") = "SG Automation Test", "Session.ProductProperty"
rec = Empty
Set rec = inst.CreateRecord(1)
rec.StringData(0) = "[PropOne]/[1]"
rec.StringData(1) = "arg"
Check sess.FormatRecord(rec) = "plain/arg", "Session.FormatRecord resolves properties and fields"
sess.DoAction "FileCost"
sess.DoAction "CostFinalize"
sess.DoAction "InstallValidate"
Check sess.ComponentCurrentState("comp") = -1 Or sess.ComponentCurrentState("comp") = 2, "Session.ComponentCurrentState of a component not installed"
sess.ComponentRequestState("comp") = 4
Check sess.ComponentRequestState("comp") = 4, "Session.ComponentRequestState round trip"
sess.ComponentRequestState("comp") = 3
Check sess.ComponentRequestState("comp") = 3, "Session.ComponentRequestState round trip back"
v = ((sess.FeatureValidStates("feat") And 1) = 1)
Check v, "Session.FeatureValidStates allows local"
Set costs = sess.ComponentCosts("", 3)
Check costs.Count >= 1, "Session.ComponentCosts returns a RecordList"
Check Len(costs.Item(0).StringData(1)) = 2, "ComponentCosts record starts with a drive: " & costs.Item(0).StringData(1)
n = 0
For Each r In costs
    n = n + 1
Next
Check n = costs.Count, "RecordList enumerates"
c = sess.FeatureCost("feat", 0, 3)
Check IsNumeric(c), "Session.FeatureCost"
sess.TargetPath("TARGETDIR") = dir & "\target\"
Check LCase(sess.TargetPath("TARGETDIR")) = LCase(dir & "\target\"), "Session.TargetPath round trip"
Check Len(sess.SourcePath("TARGETDIR")) > 0, "Session.SourcePath"
Check sess.VerifyDiskSpace = True, "Session.VerifyDiskSpace is true for a small installation"
' ---------------------------------------------------------------- UIPreview
Set pre = sess.Database.EnableUIPreview
pre.Property("AUTFOO") = "bar"
Check pre.Property("AUTFOO") = "bar", "UIPreview.Property round trip"
On Error Resume Next
pre.ViewDialog "NoSuchDialog"
Check Failed(), "UIPreview.ViewDialog of a missing dialog fails"
Err.Clear
pre.ViewBillboard "NoSuchControl", "NoSuchBillboard"
Check Failed(), "UIPreview.ViewBillboard of a missing control fails"
Err.Clear
On Error GoTo 0

Set sess = Nothing

BuildPackage dir & "\big.msi", True
Set sess = inst.OpenPackage(dir & "\big.msi", 0)
sess.Sequence "AutSequence"
Check sess.VerifyDiskSpace = False, "Session.VerifyDiskSpace is false for a huge installation"
Set sess = Nothing

' ---------------------------------------------------------------- Installer
pcode = "{3E218000-1111-4000-8000-0000000000A1}"
ccode = "{3E218000-2222-4000-8000-0000000000C1}"
inst.InstallProduct dir & "\pkg.msi", "ALLUSERS=1"
Check inst.ProductState(pcode) = 5, "test product installed"
Set pl = inst.ProductsEx(pcode, "", 4)
Check pl.Count = 1, "Installer.ProductsEx finds the installed product: " & pl.Count
Check pl.Item(0).StringData(1) = pcode, "ProductsEx record: product code"
Check pl.Item(0).IntegerData(2) = 4, "ProductsEx record: context machine"
Set pl = inst.ProductsEx("{3E218000-9999-4000-8000-000000000099}", "", 4)
Check pl.Count = 0, "Installer.ProductsEx of another product is empty"
Set cl = inst.ComponentsEx("", 4)
found = False
For Each r In cl
    If r.StringData(1) = ccode Then found = True
Next
Check found, "Installer.ComponentsEx lists the component"
Set cl = inst.ClientsEx(ccode, "", 4)
Check cl.Count = 1 And cl.Item(0).StringData(1) = pcode, "Installer.ClientsEx lists the product"
Set cp = inst.ComponentPathEx(pcode, ccode, "", 4)
Check cp.Count = 1 And Len(cp.Item(0).StringData(1)) > 0 And cp.Item(0).IntegerData(2) >= 0, "Installer.ComponentPathEx gives the key path and the state: " & cp.Item(0).StringData(1) & " " & cp.Item(0).IntegerData(2)
Set pe = inst.PatchesEx(pcode, "", 4, 15)
Check pe.Count = 0, "Installer.PatchesEx without patches"

Set prod = inst.Product(pcode, "", 4)
Check prod.ProductCode = pcode, "Product.ProductCode"
Check prod.Context = 4, "Product.Context"
Check prod.UserSid = "", "Product.UserSid of a machine product is empty"
Check prod.State = 5, "Product.State is installed"
Check prod.InstallProperty("ProductName") = "SG Automation Test", "Product.InstallProperty"
v = (prod.FeatureState("feat") = 3 Or prod.FeatureState("feat") = 4)
Check v, "Product.FeatureState: " & prod.FeatureState("feat")
Check prod.ComponentState(ccode) = 3, "Product.ComponentState"
prod.SourceListAddSource 1, dir & "\srcA", 0
prod.SourceListAddSource 1, dir & "\srcB", 0
Set srcs = prod.Sources(1)
Check srcs.Count = 2 And srcs.Item(0) = dir & "\srcA\", "Product.SourceListAddSource and Sources (network sources end in a backslash)"
prod.SourceListClearSource 1, dir & "\srcA"
Set srcs = prod.Sources(1)
Check srcs.Count = 1 And srcs.Item(0) = dir & "\srcB\", "Product.SourceListClearSource"
prod.SourceListClearAll 1
Set srcs = prod.Sources(1)
Check srcs.Count = 0, "Product.SourceListClearAll"
prod.SourceListAddMediaDisk 7, "VolLabel", "Insert disk"
Set md = prod.MediaDisks
Check md.Count = 1, "Product.SourceListAddMediaDisk"
Check md.Item(0).IntegerData(1) = 7 And md.Item(0).StringData(2) = "VolLabel" And md.Item(0).StringData(3) = "Insert disk", "Product.MediaDisks record"
prod.SourceListClearMediaDisk 7
Set md = prod.MediaDisks
Check md.Count = 0, "Product.SourceListClearMediaDisk"
prod.SourceListInfo("PackageName") = "autpkg.msi"
Check prod.SourceListInfo("PackageName") = "autpkg.msi", "Product.SourceListInfo round trip"
prod.SourceListInfo("LastUsedSource") = "n;" & dir & "\srcZ"
Check prod.SourceListInfo("LastUsedType") = "n", "Product.SourceListInfo LastUsedSource"
prod.SourceListForceResolution
Check prod.SourceListInfo("LastUsedType") = "", "Product.SourceListForceResolution clears LastUsedSource"
v = inst.ProductElevated(pcode)
Check VarType(v) = 11, "Installer.ProductElevated is a boolean"

Set pat = inst.Patch("{3E218000-3333-4000-8000-0000000000D1}", pcode, "", 4)
Check pat.PatchCode = "{3E218000-3333-4000-8000-0000000000D1}" And pat.ProductCode = pcode And pat.Context = 4, "Patch identity properties"

' the source list of a registered patch
patchcode = "{3E218000-3333-4000-8000-0000000000D1}"
patchkey = "HKLM\Software\Microsoft\Windows\CurrentVersion\Installer\Patches\" & Squash(patchcode) & "\"
Set sh = CreateObject("WScript.Shell")
sh.Run "reg add ""HKLM\Software\Microsoft\Windows\CurrentVersion\Installer\Patches\" & Squash(patchcode) & "\SourceList\Net"" /v 1 /t REG_EXPAND_SZ /d C:\sgautpre /f /reg:64", 0, True
pat.SourceListAddSource 1, dir & "\psrc", 0
Set srcs = pat.Sources(1)
Check srcs.Count = 2, "Patch.SourceListAddSource adds to the list"
Check srcs.Item(1) = dir & "\psrc\", "Patch.Sources lists the source"
pat.SourceListInfo("PackageName") = "p.msp"
Check pat.SourceListInfo("PackageName") = "p.msp", "Patch.SourceListInfo round trip"
pat.SourceListAddMediaDisk 2, "PL", "PP"
Set md = pat.MediaDisks
Check md.Count = 1 And md.Item(0).IntegerData(1) = 2 And md.Item(0).StringData(2) = "PL", "Patch.SourceListAddMediaDisk and MediaDisks"
pat.SourceListClearMediaDisk 2
Set md = pat.MediaDisks
Check md.Count = 0, "Patch.SourceListClearMediaDisk"
pat.SourceListClearSource 1, dir & "\psrc"
Set srcs = pat.Sources(1)
Check srcs.Count = 1, "Patch.SourceListClearSource"
pat.SourceListAddSource 1, dir & "\psrc", 0
pat.SourceListClearAll 1
Set srcs = pat.Sources(1)
Check srcs.Count = 0, "Patch.SourceListClearAll"
pat.SourceListForceResolution
sh.Run "reg delete ""HKLM\Software\Microsoft\Windows\CurrentVersion\Installer\Patches\" & Squash(patchcode) & """ /f /reg:64", 0, True
On Error Resume Next
x = pat.State
Check Failed(), "Patch.State of an unknown patch fails"
Err.Clear
On Error GoTo 0

' scripts
inst.CreateAdvertiseScript dir & "\pkg.msi", dir & "\pkg.aas"
Check inst.ProductInfoFromScript(dir & "\pkg.aas", 0) = pcode, "ProductInfoFromScript product code"
Check inst.ProductInfoFromScript(dir & "\pkg.aas", 1) = 1033, "ProductInfoFromScript language"
Check inst.ProductInfoFromScript(dir & "\pkg.aas", 2) = &H01020003, "ProductInfoFromScript version"
Check inst.ProductInfoFromScript(dir & "\pkg.aas", 3) = "SG Automation Test", "ProductInfoFromScript name"
Check inst.ProductInfoFromScript(dir & "\pkg.aas", 4) = "pkg.msi", "ProductInfoFromScript package name"

' patch description
Set pdb = inst.OpenDatabase(dir & "\t.msp", 35)
Set si = pdb.SummaryInformation(20)
si.Property(2) = "SG patch"
si.Property(7) = pcode & ";{3E218000-9999-4000-8000-000000000099}"
si.Property(9) = "{3E218000-3333-4000-8000-0000000000D1}"
si.Persist
Set si = Nothing
pdb.Commit
Set pdb = Nothing
xml = inst.ExtractPatchXMLData(dir & "\t.msp")
Check InStr(xml, "<MsiPatch PatchGUID=""{3E218000-3333-4000-8000-0000000000D1}"">") = 1, "Installer.ExtractPatchXMLData names the patch: " & xml
Check InStr(xml, "<TargetProductCode>" & pcode & "</TargetProductCode>") > 0 And InStr(xml, "<TargetProductCode>{3E218000-9999-4000-8000-000000000099}</TargetProductCode>") > 0, "Installer.ExtractPatchXMLData names the target products"
On Error Resume Next
x = inst.ExtractPatchXMLData(dir & "\missing.msp")
Check Failed(), "Installer.ExtractPatchXMLData of a missing patch fails"
Err.Clear
On Error GoTo 0

' errors
On Error Resume Next
x = inst.FileSignatureInfo(dir & "\data.bin", 0, 0)
Check Failed(), "Installer.FileSignatureInfo of an unsigned file fails"
Err.Clear
inst.RemovePatches "{3E218000-3333-4000-8000-0000000000D1}", pcode, 0, ""
Check Failed(), "Installer.RemovePatches of a patch that is not applied fails"
Err.Clear
inst.ApplyMultiplePatches dir & "\missing.msp", pcode, ""
Check Failed(), "Installer.ApplyMultiplePatches of a missing patch fails"
Err.Clear
x = inst.PatchFiles(pcode, dir & "\missing.msp")
Check Failed(), "Installer.PatchFiles of a missing patch fails"
Err.Clear
On Error GoTo 0

' LastErrorRecord after a failing database call
On Error Resume Next
Set vw = db.OpenView("SELECT * FROM `NoSuchTable`")
Err.Clear
On Error GoTo 0
Set er = inst.LastErrorRecord
Check Not (er Is Nothing), "Installer.LastErrorRecord holds the record of the failure"

inst.ConfigureProduct pcode, 0, 2
Check inst.ProductState(pcode) = -1, "test product removed"

' a per-user installation
inst.InstallProduct dir & "\pkg.msi", "ALLUSERS="""""
Check inst.ProductState(pcode) = 5, "per-user test product installed"
Set pl = inst.ProductsEx(pcode, "", 2)
Check pl.Count = 1 And pl.Item(0).IntegerData(2) = 2, "ProductsEx finds the per-user-unmanaged product"
Set pl = inst.ProductsEx(pcode, "", 4)
Check pl.Count = 0, "ProductsEx does not find it in the machine context"
Set cl = inst.ClientsEx(ccode, "", 2)
Check cl.Count = 1 And cl.Item(0).StringData(1) = pcode And cl.Item(0).IntegerData(2) = 2, "ClientsEx reports the per-user-unmanaged context"
Check Len(cl.Item(0).StringData(3)) > 8, "ClientsEx reports the user SID"
Set cl = inst.ClientsEx(ccode, "", 4)
Check cl.Count = 0, "ClientsEx does not find the per-user product in the machine context"
inst.ConfigureProduct pcode, 0, 2
Check inst.ProductState(pcode) = -1, "per-user test product removed"

' advertising
inst.AdvertiseProduct dir & "\pkg.msi", 1
Check inst.ProductState(pcode) = 1, "Installer.AdvertiseProduct advertises the product"
Set pl = inst.ProductsEx(pcode, "", 2)
Check pl.Count = 1, "a user advertisement is in the per-user context"
inst.ConfigureProduct pcode, 0, 2
Check inst.ProductState(pcode) = -1, "advertised product removed"
inst.AdvertiseScript dir & "\pkg.aas", &H1A5, False
Check inst.ProductState(pcode) = 1, "Installer.AdvertiseScript advertises the product"
inst.AdvertiseScript dir & "\pkg.aas", &H1A5, True
Check inst.ProductState(pcode) = -1, "Installer.AdvertiseScript can remove the advertisement"

Set db = Nothing: Set db2 = Nothing: Set db3 = Nothing: Set db4 = Nothing
Set pre = Nothing
fso.DeleteFolder dir, True

If fails = 0 Then
    WScript.Echo "RESULT: PASS"
Else
    WScript.Echo "RESULT: FAIL (" & fails & ")"
End If
