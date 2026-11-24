// mixcb.cs: for test/mixcb-gate.sh (Stained Glass OS wine-sg 9453): DoCallBackInDefaultDomain called from
// the default application domain and from another one (through test/mixcb-probe.c).
using System;
using System.Runtime.InteropServices;
using System.Text;

public class Caller : MarshalByRefObject
{
    [DllImport("mixcb.dll", CharSet = CharSet.Unicode)]
    static extern int sg_callback_test(string dll, StringBuilder output, int len);

    public string Call(string dll)
    {
        StringBuilder sb = new StringBuilder(64);
        int r = sg_callback_test(dll, sb, sb.Capacity);
        return r == 0 ? sb.ToString() : "error " + r;
    }
}

public class Program
{
    public static int Main(string[] args)
    {
        foreach (string dll in new string[] { "msvcm90.dll", "msvcm80.dll" })
        {
            string tag = dll.Substring(0, 7);
            try
            {
                Console.WriteLine(tag + ".default=" + new Caller().Call(dll));
                AppDomain d = AppDomain.CreateDomain("second");
                Caller c = (Caller)d.CreateInstanceAndUnwrap(typeof(Caller).Assembly.FullName, "Caller");
                Console.WriteLine(tag + ".second=" + c.Call(dll));
                AppDomain.Unload(d);
            }
            catch (Exception e)
            {
                Console.WriteLine(tag + ".exception=" + e.GetType().Name + ": " + e.Message);
            }
        }
        Console.WriteLine("done=1");
        return 0;
    }
}
