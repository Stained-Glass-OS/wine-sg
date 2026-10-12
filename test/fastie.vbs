' ieframe batch (patches/sg/3013), run by test/fastie-gate.sh
Dim fails, v
fails = 0
Sub Check(ok, what)
    If ok Then
        WScript.Echo "PASS  " & what
    Else
        WScript.Echo "FAIL  " & what
        fails = fails + 1
    End If
End Sub

Set ie = CreateObject("InternetExplorer.Application")
ie.Visible = False

ie.Left = 10
ie.Top = 20
ie.Width = 640
ie.Height = 480
v = (ie.Left = 10 And ie.Top = 20)
Check v, "Left and Top round trip"
v = (ie.Width = 640 And ie.Height = 480)
Check v, "Width and Height round trip"

v = (ie.Name = "Microsoft Internet Explorer")
Check v, "Name"
v = (LCase(Right(ie.FullName, 12)) = "iexplore.exe")
Check v, "FullName names iexplore.exe"
v = (Right(ie.Path, 1) = "\")
Check v, "Path ends with a backslash"
v = (ie.ReadyState = 0)
Check v, "ReadyState is uninitialized before a navigation"
v = (ie.TopLevelContainer = True)
Check v, "TopLevelContainer"
v = (ie.Application Is ie)
Check v, "Application is the object itself"
v = (ie.Parent Is ie)
Check v, "Parent is the object itself"

ie.Navigate "about:blank"
n = 0
Do While ie.Busy Or ie.ReadyState <> 4
    WScript.Sleep 100
    n = n + 1
    If n > 150 Then Exit Do
Loop
v = (ie.ReadyState = 4)
Check v, "ReadyState is complete after the navigation"
v = (TypeName(ie.Document) = "DispHTMLDocument" Or TypeName(ie.Document) = "HTMLDocument")
Check v, "Document is an HTML document"
v = (ie.LocationName = "about:blank")
Check v, "LocationName"

ie.Document.Write "<html><head><title>fastie page</title></head><body>hi</body></html>"
ie.Document.Close
v = (ie.Document.Title = "fastie page")
Check v, "Document is usable"
v = (ie.LocationName = "fastie page")
Check v, "LocationName follows the title"

ie.PutProperty "foo", 42
v = (ie.GetProperty("FOO") = 42)
Check v, "PutProperty / GetProperty (case insensitive)"
ie.PutProperty "foo", "bar"
v = (ie.GetProperty("foo") = "bar")
Check v, "PutProperty replaces"
v = IsEmpty(ie.GetProperty("missing"))
Check v, "GetProperty of a missing name is empty"

ie.StatusBar = False
v = (ie.StatusBar = False)
Check v, "StatusBar off"
ie.StatusBar = True
v = (ie.StatusBar = True)
Check v, "StatusBar on"
ie.StatusText = "working"
v = (ie.StatusText = "working")
Check v, "StatusText round trips"
ie.MenuBar = False
v = (ie.MenuBar = False)
Check v, "MenuBar off"
ie.AddressBar = False
v = (ie.AddressBar = False)
Check v, "AddressBar off"
ie.Silent = True
v = (ie.Silent = True)
Check v, "Silent"
ie.Offline = True
v = (ie.Offline = True)
Check v, "Offline"
ie.RegisterAsBrowser = True
v = (ie.RegisterAsBrowser = True)
Check v, "RegisterAsBrowser"
ie.TheaterMode = True
v = (ie.TheaterMode = True)
Check v, "TheaterMode"

ie.Quit
If fails = 0 Then WScript.Echo "RESULT: PASS" Else WScript.Echo "RESULT: FAIL (" & fails & ")"
