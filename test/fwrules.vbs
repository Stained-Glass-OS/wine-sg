' fwrules-gate.sh: a script installer's way -- For Each over the rules (IDispatch, _NewEnum)
Set pol = CreateObject("HNetCfg.FwPolicy2")
Set r = CreateObject("HNetCfg.FWRule")
r.Name = "SG Gate Script Rule"
r.Protocol = 17
r.LocalPorts = "1900"
r.Enabled = True
pol.Rules.Add r
n = 0
For Each x In pol.Rules
  If x.Name = "SG Gate Script Rule" Then n = n + 1 : p = x.LocalPorts
Next
WScript.Echo "VBS " & n & " " & p
pol.Rules.Remove "SG Gate Script Rule"
