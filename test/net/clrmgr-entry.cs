// clrmgr gate: run in the hosted default domain. Bits: 1 the domain's manager is SgMgr.Manager,
// 2 its InitializeNewDomain ran, 4 the host's property is AppDomain data, 8 the domain's configuration
// file is the one the manager chose, 16 an assembly found only through that file's <codeBase> loads.
using System;
using System.Reflection;
namespace E2
{
    public static class E
    {
        public static int Run(string arg)
        {
            int r = 0;
            var f = typeof(AppDomain).GetField("_domain_manager", BindingFlags.Instance | BindingFlags.NonPublic);
            object m = f != null ? f.GetValue(AppDomain.CurrentDomain) : null;
            if (m != null && m.GetType().FullName == "SgMgr.Manager") r |= 1;
            if (SgMgr.Manager.Initialized) r |= 2;
            if ((AppDomain.CurrentDomain.GetData("SG_HOST_PROP") as string) == "sg-value") r |= 4;
            string cfg = AppDomain.CurrentDomain.SetupInformation.ConfigurationFile ?? "";
            if (cfg.EndsWith("other.config", StringComparison.OrdinalIgnoreCase)) r |= 8;
            try { if (Load() == 7) r |= 16; } catch (Exception) { }
            return r;
        }
        static int Load() { return SgOther.O.Seven(); }
    }
}
