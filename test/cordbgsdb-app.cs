// The managed debuggee of test/cordbgsdb-gate.sh (wine-sg 7792): a background worker and the main
// thread count, and C:\counter-PID.txt holds the count, so a stopped program is seen not to run.
// SPDX-License-Identifier: LGPL-2.1-or-later
using System;
using System.IO;
using System.Threading;

class App
{
    static long count;
    static string file;

    static void Main(string[] args)
    {
        file = @"C:\counter-" + System.Diagnostics.Process.GetCurrentProcess().Id + ".txt";
        Thread worker = new Thread(() => { for (;;) { Interlocked.Increment(ref count); Thread.Sleep(20); } });
        worker.IsBackground = true;
        worker.Name = "sg-worker";
        worker.Start();
        new App().Inspect(12345, "sg-label");
    }

    // the frame the debugger looks into (wine-sg 7796): an instance method ("this" is argument 0),
    // known arguments and locals
    void Inspect(int marker, string label)
    {
        long big = 1234567890123;
        double ratio = 2.5;
        bool flag = true;
        string text = "Gr\u00fc\u00dfe \u2713";
        int i;
        for (i = 0; i < 6000; i++)
            Tick(i);
        GC.KeepAlive(big + ratio + (flag ? 1 : 0) + text.Length + marker + label.Length);
    }

    // called every round: where the debugger sets its breakpoint (its first line) and steps from
    static void Tick(int round)
    {
        File.WriteAllText(file, Interlocked.Read(ref count).ToString());
        Thread.Sleep(50);
    }
}
