// tcpipreg-gate.sh: what .NET's NetworkInterface reports of the adapters' DHCP state (patches/sg/9251)
// and name servers (9210, 9462)
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
            string dns = "";
            try { foreach (var a in p.DnsAddresses) dns += a + " "; } catch (Exception e) { dns = "E:" + e.GetType().Name; }
            Console.WriteLine("NETDNS " + ni.Id + " [" + dns.Trim() + "]");
        }
        // (9462, i0690) GetPerAdapterInfo per adapter: Wine Mono's IPv4InterfaceProperties must
        // allocate the size GetPerAdapterInfo asks for (several name servers)
        int ok = 0, errors = 0; string first = "";
        for (int round = 0; round < 200; round++) {
            foreach (NetworkInterface ni in NetworkInterface.GetAllNetworkInterfaces()) {
                if (ni.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;
                try { var v4 = ni.GetIPProperties().GetIPv4Properties(); if (v4 != null && v4.Index > 0) ok++; }
                catch (Exception e) { if (errors++ == 0) first = " first " + e.GetType().Name; }
            }
            if (round % 20 == 0) GC.Collect();
        }
        Console.WriteLine("NETV4 rounds 200 ok " + ok + " errors " + errors + first);
    }
}
