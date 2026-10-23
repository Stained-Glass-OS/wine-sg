// The managed program test/monoagent-gate.sh runs under Wine Mono's soft debugger agent.
// SPDX-License-Identifier: LGPL-2.1-or-later
class App
{
    static int Main(string[] args)
    {
        System.IO.File.WriteAllText(@"C:\app-ran.txt", "1");
        return 5;
    }
}
