// The managed debuggee of test/cordbgobj-gate.sh (wine-sg 8200, 8201): objects, structs, enums and arrays for a
// debugger to look into, and a call it stops in (App.Hold, every round). What the debugger writes into them
// is seen here: when Hold's round number is 1234 (only a debugger makes it so), the values go to
// C:\seen-PID.txt.
// SPDX-License-Identifier: LGPL-2.1-or-later
using System;
using System.IO;
using System.Threading;

struct Point
{
    public int X;
    public int Y;
    public static int Made = 7;
}

enum Color { Red = 1, Green = 2, Blue = 4 }

class Node
{
    public int Id;
    public string Name;
    public Node Next;
    public double Weight = 0.25;   // (a constructor the debugger runs sets it)
    public Point At;
    public static string Kind = "sg-node";
}

class Derived : Node
{
    public long Extra;
}

class App
{
    static string seen;

    static void Main(string[] args)
    {
        seen = @"C:\seen-" + System.Diagnostics.Process.GetCurrentProcess().Id + ".txt";
        Derived d = new Derived();
        d.Id = 41; d.Name = "first"; d.Weight = 1.5; d.Extra = 9000000000;
        d.At.X = 3; d.At.Y = 4;
        d.Next = new Node();
        d.Next.Id = 42; d.Next.Name = "second";
        int[] nums = { 10, 20, 30, 40 };
        string[] words = { "alpha", null, "gamma" };
        int[,] grid = new int[2, 3];
        grid[1, 2] = 99;
        Point pt;
        pt.X = -5; pt.Y = 6;
        GC.KeepAlive(Point.Made + Node.Kind.Length);
        for (int i = 0; i < 2400; i++)   // (two minutes at most)
            Hold(d, nums, words, grid, pt, Color.Blue, i);
    }

    // the debugger stops here (a breakpoint at its start) and changes d.Id, nums[0] and round
    static void Hold(Derived d, int[] nums, string[] words, int[,] grid, Point pt, Color c, int round)
    {
        if (round == 30)
            LoadXml();   // an assembly loaded while the program runs: its LoadModule callback (8209)
        if (round == 1234)
            File.WriteAllText(seen, d.Id + " " + nums[0] + " " + round);
        Thread.Sleep(50);
    }

    static void LoadXml() { GC.KeepAlive(new System.Xml.XmlDocument()); }

    // called by the debugger (ICorDebugEval)
    static int Twice(int x) { return x * 2; }
    static int Fail() { throw new InvalidOperationException("sg-fail"); }
}
