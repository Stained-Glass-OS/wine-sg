// clrmgr gate: the AppDomainManager a native host names (ICLRDomainManager::SetAppDomainManagerType). Its
// InitializeNewDomain gives the default domain another configuration file, as Visual Studio's does.
using System;
namespace SgMgr
{
    public class Manager : AppDomainManager
    {
        public static bool Initialized;
        public override void InitializeNewDomain(AppDomainSetup appDomainInfo)
        {
            Initialized = true;
            if (appDomainInfo != null)
                appDomainInfo.ConfigurationFile = AppDomain.CurrentDomain.BaseDirectory + "other.config";
        }
    }
}
