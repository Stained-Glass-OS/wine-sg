// tcpipreg-gate.sh: what .NET's NetworkInterface reports of the adapters' DHCP state (patches/sg/9251)
using System;
using System.Net.NetworkInformation;
class P {
    static void Main() {
        foreach (NetworkInterface ni in NetworkInterface.GetAllNetworkInterfaces()) {
            if (ni.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;
            IPInterfaceProperties p = ni.GetIPProperties();
            string dhcp = "?", servers = "";
            try { dhcp = p.GetIPv4Properties().IsDhcpEnabled ? "1" : "0"; } catch (Exception e) { dhcp = "E:" + e.GetType().Name; }
            try { foreach (var a in p.DhcpServerAddresses) servers += a + " "; } catch (Exception e) { servers = "E:" + e.GetType().Name; }
            Console.WriteLine("NET " + ni.Id + " dhcp " + dhcp + " servers [" + servers.Trim() + "]");
        }
    }
}
