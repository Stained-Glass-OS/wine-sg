// The managed debuggee of test/cordbgsdb-gate.sh (wine-sg 7792): a background worker and the main
// thread count, and C:\counter-PID.txt holds the count, so a stopped program is seen not to run.
// SPDX-License-Identifier: LGPL-2.1-or-later
using System;
using System.IO;
using System.Threading;

class App
{
    static long count;

    static void Main(string[] args)
    {
        string file = @"C:\counter-" + System.Diagnostics.Process.GetCurrentProcess().Id + ".txt";
        Thread worker = new Thread(() => { for (;;) { Interlocked.Increment(ref count); Thread.Sleep(20); } });
        worker.IsBackground = true;
        worker.Name = "sg-worker";
        worker.Start();
        for (int i = 0; i < 6000; i++)
        {
            File.WriteAllText(file, Interlocked.Read(ref count).ToString());
            Thread.Sleep(50);
        }
    }
}
