' fwnetsh-gate.sh: the older firewall interfaces (INetFwMgr) an installer uses
Set mgr = CreateObject("HNetCfg.FwMgr")
Set app = CreateObject("HNetCfg.FwAuthorizedApplication")
app.ProcessImageFileName = "C:\Old\app.exe"
app.Name = "Old App"
app.Enabled = True
mgr.LocalPolicy.CurrentProfile.AuthorizedApplications.Add app
Set port = CreateObject("HNetCfg.FWOpenPort")
port.Name = "Old Port"
port.Port = 5555
port.Protocol = 17
port.Enabled = True
mgr.LocalPolicy.CurrentProfile.GloballyOpenPorts.Add port
Set pol = CreateObject("HNetCfg.FwPolicy2")
WScript.Echo "V1 OK " & pol.CurrentProfileTypes & " " & CInt(pol.FirewallEnabled(4)) & " " & CInt(pol.FirewallEnabled(2))
