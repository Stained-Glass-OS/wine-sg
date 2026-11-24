// mixpin.cs: for test/mixpin-gate.sh (Stained Glass OS wine-sg 9452). A mixed-mode (C++/CLI) DLL loaded in
// an application domain that unloads stays loaded in the process, as with the CLR; loaded again in
// another domain, it works.
using System;
using System.Reflection;
using System.Runtime.InteropServices;

public class Loader : MarshalByRefObject
{
    public string Load(string path)
    {
        Assembly asm = Assembly.LoadFrom(path);
        object o = asm.CreateInstance("SgMixed");
        int answer = (int)asm.GetType("SgMixed").GetMethod("Answer").Invoke(null, null);
        return answer + "," + o.GetType().GetMethod("Where").Invoke(o, null);
    }
}

public class Program
{
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    static extern IntPtr GetModuleHandleW(string name);

    static string Run(string name, string path)
    {
        AppDomain d = AppDomain.CreateDomain(name);
        Loader l = (Loader)d.CreateInstanceAndUnwrap(typeof(Loader).Assembly.FullName, "Loader");
        string r = l.Load(path);
        AppDomain.Unload(d);
        return r;
    }

    public static int Main(string[] args)
    {
        string path = System.IO.Path.GetFullPath(args[0]);
        try
        {
            Console.WriteLine("first=" + Run("first", path));
            Console.WriteLine("after_unload=" + (GetModuleHandleW(path) != IntPtr.Zero ? 1 : 0));
            Console.WriteLine("second=" + Run("second", path));
            Console.WriteLine("after_second=" + (GetModuleHandleW(path) != IntPtr.Zero ? 1 : 0));
            Console.WriteLine("default=" + new Loader().Load(path));
        }
        catch (Exception e)
        {
            Console.WriteLine("exception=" + e.GetType().Name + ": " + e.Message);
        }
        Console.WriteLine("done=1");
        return 0;
    }
}
