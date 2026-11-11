// clrhost gate: a strong-named library at two versions (SGCB_VERSION 1 or 2); Ver() tells which one loaded
[assembly: System.Reflection.AssemblyVersion(SgCb.Thing.VersionText)]
namespace SgCb
{
    public static class Thing
    {
#if V2
        public const string VersionText = "2.0.0.0";
        public static int Ver() { return 2; }
#else
        public const string VersionText = "1.0.0.0";
        public static int Ver() { return 1; }
#endif
    }
}
