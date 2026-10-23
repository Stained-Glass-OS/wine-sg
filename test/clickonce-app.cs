// The application test/clickonce-gate.sh runs through ICLRRuntimeHost::ExecuteApplication.
// SPDX-License-Identifier: LGPL-2.1-or-later
using System;
using System.IO;
using System.Runtime.CompilerServices;

class Finalized
{
    ~Finalized() { File.WriteAllText(@"C:\finalized.flag", "1"); }
}

class App
{
    [MethodImpl(MethodImplOptions.NoInlining)]
    static void Leave() { new Finalized(); }

    static int Main(string[] args)
    {
        File.WriteAllText(@"C:\app-args.txt", string.Join(",", args));
        Leave();
        return 7 + args.Length;
    }
}
