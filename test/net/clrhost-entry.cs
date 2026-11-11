// clrhost gate: run by a native host (ICLRRuntimeHost::ExecuteInDefaultAppDomain); its references are
// bound by the host program's configuration: SgCb 1.0 is redirected to 2.0 (a <codeBase>), SgPriv is
// in a <probing privatePath> directory
namespace Entry
{
    public static class E
    {
        public static int Run(string arg) { return SgCb.Thing.Ver() * 10 + SgPriv.P.Val(); }
    }
}
