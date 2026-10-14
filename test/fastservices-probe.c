/* services.exe RPC probe (patch 3213): talks to the SCM over \pipe\svcctl with the interface's own client
 * stubs (generated from svcctl.idl), because the ANSI entry points of the interface are never reached
 * through advapi32. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "svcctl.h"

#ifndef SERVICE_TRIGGER_ACTION_SERVICE_START
#define SERVICE_TRIGGER_ACTION_SERVICE_START 1
#endif
#ifndef SERVICE_TRIGGER_TYPE_CUSTOM
#define SERVICE_TRIGGER_TYPE_CUSTOM 20
#endif
#ifndef SERVICE_TRIGGER_DATA_TYPE_STRING
#define SERVICE_TRIGGER_DATA_TYPE_STRING 2
#endif
#ifndef SERVICE_CONFIG_PREFERRED_NODE
#define SERVICE_CONFIG_PREFERRED_NODE 9
#endif

static int checks, fails;
#define OK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define ERR(e, want) OK((e) == (want), "%s: error %lu, want %lu", #e, (unsigned long)(e), (unsigned long)(want))

void *__RPC_USER MIDL_user_allocate(size_t len) { return malloc(len); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

static handle_t bind_name(const char *name)
{
    unsigned char *str = NULL;
    handle_t h = NULL;

    if (RpcStringBindingComposeA(NULL, (unsigned char *)"ncacn_np", (unsigned char *)name,
                                 (unsigned char *)"\\pipe\\svcctl", NULL, &str)) return NULL;
    RpcBindingFromStringBindingA(str, &h);
    RpcStringFreeA(&str);
    return h;
}
handle_t __RPC_USER MACHINE_HANDLEA_bind(MACHINE_HANDLEA name) { return bind_name((const char *)name); }
void __RPC_USER MACHINE_HANDLEA_unbind(MACHINE_HANDLEA name, handle_t h) { RpcBindingFree(&h); }
handle_t __RPC_USER MACHINE_HANDLEW_bind(MACHINE_HANDLEW name)
{
    char buf[256];
    if (!name) return bind_name(NULL);
    WideCharToMultiByte(CP_ACP, 0, name, -1, buf, sizeof(buf), NULL, NULL);
    return bind_name(buf);
}
void __RPC_USER MACHINE_HANDLEW_unbind(MACHINE_HANDLEW name, handle_t h) { RpcBindingFree(&h); }
handle_t __RPC_USER SVCCTL_HANDLEW_bind(SVCCTL_HANDLEW name) { return MACHINE_HANDLEW_bind(name); }
void __RPC_USER SVCCTL_HANDLEW_unbind(SVCCTL_HANDLEW name, handle_t h) { RpcBindingFree(&h); }

static const char *sname(const BYTE *base, DWORD off) { return (const char *)base + off; }
static const WCHAR *wname(const BYTE *base, DWORD off) { return (const WCHAR *)(base + off); }

/* the entry named "name" (ANSI) in a buffer of count entries of size esz; NULL when it is not there */
static const DWORD *find_a(const BYTE *buf, DWORD count, DWORD esz, const char *name)
{
    DWORD i;
    for (i = 0; i < count; i++)
    {
        const DWORD *e = (const DWORD *)(buf + i * esz);
        if (!lstrcmpiA(sname(buf, e[0]), name)) return e;
    }
    return NULL;
}
static const DWORD *find_w(const BYTE *buf, DWORD count, DWORD esz, const WCHAR *name)
{
    DWORD i;
    for (i = 0; i < count; i++)
    {
        const DWORD *e = (const DWORD *)(buf + i * esz);
        if (!lstrcmpiW(wname(buf, e[0]), name)) return e;
    }
    return NULL;
}

static void test_all(void)
{
    static const BYTE deps[] = "RpcSs\0";
    static const BYTE privs[] = "SeChangeNotifyPrivilege\0SeShutdownPrivilege\0";
    SC_RPC_HANDLE scm = NULL, scm2 = NULL, svc = NULL, svc2 = NULL, wow = NULL, rpcss = NULL, tmp = NULL;
    QUERY_SERVICE_CONFIGA cfg;
    QUERY_SERVICE_CONFIGW cfgw;
    SERVICE_STATUS st;
    char buf[64];
    BYTE *big;
    DWORD err, needed, ret, n, i;
    const DWORD *e;

    err = svcctl_OpenSCManagerA(NULL, "ServicesActive", SC_MANAGER_ALL_ACCESS, &scm);
    ERR(err, 0);
    if (err) return;
    err = svcctl_OpenSCManagerA(NULL, "NoSuchDatabase", SC_MANAGER_CONNECT, &tmp);
    ERR(err, ERROR_INVALID_NAME);
    err = svcctl_OpenSCManagerA(NULL, "ServicesActive", SC_MANAGER_CONNECT, &scm2);
    ERR(err, 0);

    /* leftovers of an earlier run */
    {
        static const char *old[] = { "SgGateSvcA", "SgGateWow" };
        for (i = 0; i < 2; i++)
            if (!svcctl_OpenServiceA(scm, old[i], SERVICE_ALL_ACCESS, &tmp))
            {
                svcctl_DeleteService(tmp);
                svcctl_CloseServiceHandle(&tmp);
            }
    }

    /* ---- create, open, query */
    printf("-- create, open, query\n");
    err = svcctl_CreateServiceA(scm, "SgGateSvcA", "SG Gate Service A", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, "C:\\windows\\system32\\sggate.exe", NULL, NULL,
                                deps, sizeof(deps), NULL, NULL, 0, &svc);
    ERR(err, 0);
    if (err) return;
    err = svcctl_CreateServiceA(scm, "SgGateSvcA", NULL, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, "C:\\windows\\system32\\sggate.exe", NULL, NULL,
                                NULL, 0, NULL, NULL, 0, &tmp);
    ERR(err, ERROR_SERVICE_EXISTS);
    err = svcctl_CreateServiceWOW64A(scm, "SgGateWow", "SG Gate Wow", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                     SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, "C:\\windows\\syswow64\\sggate.exe", NULL, NULL,
                                     NULL, 0, NULL, NULL, 0, &wow);
    ERR(err, 0);
    err = svcctl_OpenServiceA(scm, "SgGateSvcA", SERVICE_ALL_ACCESS, &svc2);
    ERR(err, 0);
    err = svcctl_OpenServiceA(scm, "SgGateNoSuch", SERVICE_ALL_ACCESS, &tmp);
    ERR(err, ERROR_SERVICE_DOES_NOT_EXIST);
    err = svcctl_OpenServiceA(scm, "RpcSs", SERVICE_ALL_ACCESS, &rpcss);
    ERR(err, 0);

    memset(&cfg, 0, sizeof(cfg));
    err = svcctl_QueryServiceConfigA(svc, &cfg, sizeof(cfg), &needed);
    ERR(err, 0);
    if (!err)
    {
        OK(cfg.dwServiceType == SERVICE_WIN32_OWN_PROCESS, "type %lx", cfg.dwServiceType);
        OK(cfg.dwStartType == SERVICE_DEMAND_START, "start %lx", cfg.dwStartType);
        OK(cfg.lpBinaryPathName && !lstrcmpiA((char *)cfg.lpBinaryPathName, "C:\\windows\\system32\\sggate.exe"),
           "path %s", cfg.lpBinaryPathName ? (char *)cfg.lpBinaryPathName : "(null)");
        OK(cfg.lpDisplayName && !strcmp((char *)cfg.lpDisplayName, "SG Gate Service A"), "display %s", cfg.lpDisplayName);
        OK(cfg.lpDependencies && !strcmp((char *)cfg.lpDependencies, "RpcSs\001"), "deps %s", cfg.lpDependencies);
        free(cfg.lpBinaryPathName); free(cfg.lpLoadOrderGroup); free(cfg.lpDependencies);
        free(cfg.lpServiceStartName); free(cfg.lpDisplayName);
    }
    memset(&cfgw, 0, sizeof(cfgw));
    err = svcctl_QueryServiceConfigW(svc, &cfgw, sizeof(cfgw), &needed);
    ERR(err, 0);
    if (!err) OK(!lstrcmpW(cfgw.lpDisplayName, L"SG Gate Service A"), "wide display");

    /* ---- names */
    printf("-- names\n");
    n = sizeof(buf);
    err = svcctl_GetServiceDisplayNameA(scm, "SgGateSvcA", buf, &n);
    ERR(err, 0);
    OK(n == 17 && !strcmp(buf, "SG Gate Service A"), "display name %lu '%s'", n, buf);
    n = 5; buf[0] = 'x';
    err = svcctl_GetServiceDisplayNameA(scm, "SgGateSvcA", buf, &n);
    ERR(err, ERROR_INSUFFICIENT_BUFFER);
    OK(n == 17 && !buf[0], "short buffer: %lu", n);
    n = 16;
    err = svcctl_GetServiceDisplayNameA(scm, "SgGateSvcA", buf, &n);
    ERR(err, ERROR_INSUFFICIENT_BUFFER);
    n = 17;  /* the size does not count the null */
    err = svcctl_GetServiceDisplayNameA(scm, "SgGateSvcA", buf, &n);
    ERR(err, 0);
    n = sizeof(buf);
    err = svcctl_GetServiceDisplayNameA(scm, "SgGateNoSuch", buf, &n);
    ERR(err, ERROR_SERVICE_DOES_NOT_EXIST);
    n = sizeof(buf);
    err = svcctl_GetServiceKeyNameA(scm, "SG Gate Service A", buf, &n);
    ERR(err, 0);
    OK(n == 10 && !strcmp(buf, "SgGateSvcA"), "key name %lu '%s'", n, buf);
    n = 3;
    err = svcctl_GetServiceKeyNameA(scm, "SG Gate Service A", buf, &n);
    ERR(err, ERROR_INSUFFICIENT_BUFFER);
    OK(n == 10, "key name size %lu", n);
    n = sizeof(buf);
    err = svcctl_GetServiceKeyNameA(scm, "No Such Display", buf, &n);
    ERR(err, ERROR_SERVICE_DOES_NOT_EXIST);

    /* ---- change the configuration: group and display name */
    printf("-- change the configuration: group and display name\n");
    err = svcctl_ChangeServiceConfigA(svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL,
                                      (LPSTR)"SgGateGroup", NULL, NULL, 0, NULL, NULL, 0, (LPSTR)"SG Gate Renamed");
    ERR(err, 0);
    memset(&cfg, 0, sizeof(cfg));
    err = svcctl_QueryServiceConfigA(svc2, &cfg, sizeof(cfg), &needed);
    ERR(err, 0);
    if (!err)
    {
        OK(cfg.lpLoadOrderGroup && !strcmp((char *)cfg.lpLoadOrderGroup, "SgGateGroup"), "group");
        OK(cfg.lpDisplayName && !strcmp((char *)cfg.lpDisplayName, "SG Gate Renamed"), "renamed");
        OK(cfg.lpDependencies && !strcmp((char *)cfg.lpDependencies, "RpcSs\001"), "deps kept");
        free(cfg.lpBinaryPathName); free(cfg.lpLoadOrderGroup); free(cfg.lpDependencies);
        free(cfg.lpServiceStartName); free(cfg.lpDisplayName);
    }
    err = svcctl_ChangeServiceConfigA(svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL,
                                      (BYTE *)"NoSuchDepService\0", 18, NULL, NULL, 0, NULL);
    OK(err == 0 || err == ERROR_SERVICE_DEPENDENCY_DELETED || err == ERROR_INVALID_PARAMETER, "bad dependency: %lu", err);
    err = svcctl_ChangeServiceConfigA(svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL, NULL,
                                      (BYTE *)deps, sizeof(deps), NULL, NULL, 0, NULL);
    ERR(err, 0);

    /* ---- enumeration */
    printf("-- enumeration\n");
    big = malloc(64 * 1024);
    needed = ret = 0;
    err = svcctl_EnumServicesStatusA(scm, SERVICE_WIN32, SERVICE_STATE_ALL, big, 16, &needed, &ret, NULL);
    ERR(err, ERROR_MORE_DATA);
    OK(needed > 16 && !ret, "needed %lu returned %lu", needed, ret);
    n = needed;
    err = svcctl_EnumServicesStatusA(scm, SERVICE_WIN32, SERVICE_STATE_ALL, big, 64 * 1024, &needed, &ret, NULL);
    ERR(err, 0);
    OK(ret > 3, "enumerated %lu", ret);
    OK(n >= 36 * ret, "needed was too small: %lu for %lu", n, ret);
    e = find_a(big, ret, sizeof(struct enum_service_status), "SgGateSvcA");
    OK(e != NULL, "service not listed");
    if (e)
    {
        const struct enum_service_status *s = (const void *)e;
        OK(!strcmp(sname(big, s->display_name), "SG Gate Renamed"), "display '%s'", sname(big, s->display_name));
        OK(s->service_status.dwCurrentState == SERVICE_STOPPED, "state %lu", s->service_status.dwCurrentState);
        OK(s->service_status.dwServiceType == SERVICE_WIN32_OWN_PROCESS, "type %lx", s->service_status.dwServiceType);
    }
    err = svcctl_EnumServicesStatusA(scm, SERVICE_WIN32, SERVICE_ACTIVE, big, 64 * 1024, &needed, &ret, NULL);
    ERR(err, 0);
    OK(!find_a(big, ret, sizeof(struct enum_service_status), "SgGateSvcA"), "stopped service listed as active");

    err = svcctl_EnumServicesStatusExA(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_INACTIVE, big, 16, &needed, &ret,
                                       NULL, (LPCSTR)"SgGateGroup");
    ERR(err, ERROR_MORE_DATA);
    OK(needed > 16, "needed %lu", needed);
    err = svcctl_EnumServicesStatusExA(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_INACTIVE, big, 64 * 1024, &needed,
                                       &ret, NULL, (LPCSTR)"SgGateGroup");
    ERR(err, 0);
    OK(ret == 1, "group has %lu services", ret);
    e = find_a(big, ret, sizeof(struct enum_service_status_process), "SgGateSvcA");
    OK(e != NULL, "group member not listed");
    if (e)
    {
        const struct enum_service_status_process *s = (const void *)e;
        OK(!strcmp(sname(big, s->display_name), "SG Gate Renamed"), "display");
        OK(s->service_status_process.dwCurrentState == SERVICE_STOPPED && !s->service_status_process.dwProcessId, "state");
    }
    err = svcctl_EnumServicesStatusExA(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, big, 64 * 1024, &needed,
                                       &ret, NULL, (LPCSTR)"NoSuchGroup");
    ERR(err, ERROR_SERVICE_DOES_NOT_EXIST);

    err = svcctl_EnumServiceGroupW(scm, SERVICE_WIN32, SERVICE_STATE_ALL, big, 16, &needed, &ret, NULL, L"SgGateGroup");
    ERR(err, ERROR_MORE_DATA);
    OK(needed > 16, "group needed %lu", needed);
    err = svcctl_EnumServiceGroupW(scm, SERVICE_WIN32, SERVICE_STATE_ALL, big, 64 * 1024, &needed, &ret, NULL, L"SgGateGroup");
    ERR(err, 0);
    OK(ret == 1, "group enum returned %lu", ret);
    e = find_w(big, ret, sizeof(struct enum_service_status), L"SgGateSvcA");
    OK(e != NULL, "wide group member not listed");
    err = svcctl_EnumServiceGroupW(scm, SERVICE_WIN32, SERVICE_STATE_ALL, big, 64 * 1024, &needed, &ret, NULL, L"NoSuchGroup");
    ERR(err, ERROR_SERVICE_DOES_NOT_EXIST);
    err = svcctl_EnumServiceGroupW(scm, 0, SERVICE_STATE_ALL, big, 64 * 1024, &needed, &ret, NULL, L"SgGateGroup");
    ERR(err, ERROR_INVALID_PARAMETER);
    err = svcctl_EnumServiceGroupW(scm, SERVICE_WIN32, 8, big, 64 * 1024, &needed, &ret, NULL, L"SgGateGroup");
    ERR(err, ERROR_INVALID_PARAMETER);
    err = svcctl_EnumServiceGroupW(scm2, SERVICE_WIN32, SERVICE_STATE_ALL, big, 64 * 1024, &needed, &ret, NULL, L"SgGateGroup");
    ERR(err, ERROR_ACCESS_DENIED);

    err = svcctl_EnumDependentServicesA(rpcss, SERVICE_STATE_ALL, big, 16, &needed, &ret);
    ERR(err, ERROR_MORE_DATA);
    err = svcctl_EnumDependentServicesA(rpcss, SERVICE_STATE_ALL, big, 64 * 1024, &needed, &ret);
    ERR(err, 0);
    e = find_a(big, ret, sizeof(struct enum_service_status), "SgGateSvcA");
    OK(e != NULL, "dependent not listed (%lu entries)", ret);
    err = svcctl_EnumDependentServicesA(rpcss, 0, big, 64 * 1024, &needed, &ret);
    ERR(err, ERROR_INVALID_PARAMETER);
    free(big);

    /* ---- status, lock status */
    printf("-- status, lock status\n");
    memset(&st, 0xcc, sizeof(st));
    err = svcctl_QueryServiceStatus(svc, &st);
    ERR(err, 0);
    OK(st.dwCurrentState == SERVICE_STOPPED && st.dwServiceType == SERVICE_WIN32_OWN_PROCESS, "status %lu %lx",
       st.dwCurrentState, st.dwServiceType);
    {
        QUERY_SERVICE_LOCK_STATUSW lw;
        QUERY_SERVICE_LOCK_STATUSA la;

        memset(&lw, 0, sizeof(lw)); needed = 0;
        err = svcctl_QueryServiceLockStatusW(scm, &lw, 1, &needed);
        ERR(err, ERROR_INSUFFICIENT_BUFFER);
        OK(needed > sizeof(lw), "lock needed %lu", needed);
        err = svcctl_QueryServiceLockStatusW(scm, &lw, needed, &n);
        ERR(err, 0);
        OK(!lw.fIsLocked && lw.lpLockOwner && !lw.lpLockOwner[0] && !lw.dwLockDuration, "lock status");
        free(lw.lpLockOwner);
        memset(&la, 0, sizeof(la));
        err = svcctl_QueryServiceLockStatusA(scm, &la, needed, &n);
        ERR(err, 0);
        OK(!la.fIsLocked && la.lpLockOwner && !la.lpLockOwner[0], "lock status A");
        free(la.lpLockOwner);
        err = svcctl_QueryServiceLockStatusW(scm2, &lw, needed, &n);
        ERR(err, ERROR_ACCESS_DENIED);
    }

    /* ---- optional configuration */
    printf("-- optional configuration\n");
    {
        SC_RPC_CONFIG_INFOA info;
        SERVICE_DESCRIPTIONA descr = { (LPSTR)"SG gate description" };
        SC_ACTION acts[2] = { { SC_ACTION_RESTART, 1000 }, { SC_ACTION_RUN_COMMAND, 2000 } };
        SERVICE_FAILURE_ACTIONSA fa = { 60, (LPSTR)"reboot msg", (LPSTR)"cmd.exe /c x", 2, acts };
        SERVICE_RPC_REQUIRED_PRIVILEGES_INFO pi = { sizeof(privs), (BYTE *)privs };
        SERVICE_DELAYED_AUTO_START_INFO da = { 1 };
        SERVICE_PREFERRED_NODE_INFO pn = { 3, 0 };
        BYTE q[512];
        struct { USHORT size; char text[1]; } *d = (void *)q;
        struct service_failure_actions *f = (void *)q;

        memset(&info, 0, sizeof(info));
        info.dwInfoLevel = SERVICE_CONFIG_DESCRIPTION; info.descr = &descr;
        err = svcctl_ChangeServiceConfig2A(svc, info);
        ERR(err, 0);
        memset(q, 0xcc, sizeof(q));
        err = svcctl_QueryServiceConfig2A(svc, SERVICE_CONFIG_DESCRIPTION, q, sizeof(q), &needed);
        ERR(err, 0);
        OK(d->size == 20 && !strcmp(d->text, "SG gate description"), "description A: %u '%s'", d->size, d->text);
        err = svcctl_QueryServiceConfig2A(svc, SERVICE_CONFIG_DESCRIPTION, q, 4, &needed);
        ERR(err, ERROR_INSUFFICIENT_BUFFER);
        OK(needed >= 22, "description needed %lu", needed);
        memset(q, 0, sizeof(q));
        err = svcctl_QueryServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, q, sizeof(q), &needed);
        ERR(err, 0);
        OK(!lstrcmpW((WCHAR *)(q + 2), L"SG gate description"), "description W");

        info.dwInfoLevel = SERVICE_CONFIG_FAILURE_ACTIONS; info.actions = &fa;
        err = svcctl_ChangeServiceConfig2A(svc, info);
        ERR(err, 0);
        memset(q, 0xcc, sizeof(q));
        err = svcctl_QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, q, sizeof(q), &needed);
        ERR(err, 0);
        OK(f->reset_period == 60 && f->count == 2 && f->reboot_msg_size == 11 && f->command_size == 13, "failure actions %lu %lu %lu %lu",
           f->reset_period, f->count, f->reboot_msg_size, f->command_size);
        OK(!strcmp((char *)q + sizeof(*f) + 16, "reboot msg") && !strcmp((char *)q + sizeof(*f) + 16 + 11, "cmd.exe /c x"),
           "failure strings");
        OK(((SC_ACTION *)(q + sizeof(*f)))[1].Delay == 2000, "actions");

        info.dwInfoLevel = SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO; info.privinfo = &pi;
        err = svcctl_ChangeServiceConfig2A(svc, info);
        ERR(err, 0);
        memset(q, 0xcc, sizeof(q));
        err = svcctl_QueryServiceConfig2A(svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, q, sizeof(q), &needed);
        ERR(err, 0);
        OK(needed == sizeof(privs) && !memcmp(q, privs, sizeof(privs)), "privileges A (needed %lu)", needed);
        err = svcctl_QueryServiceConfig2A(svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, q, 4, &needed);
        ERR(err, ERROR_INSUFFICIENT_BUFFER);

        info.dwInfoLevel = SERVICE_CONFIG_DELAYED_AUTO_START_INFO; info.delayedstart = &da;
        err = svcctl_ChangeServiceConfig2A(svc, info);
        ERR(err, 0);
        err = svcctl_QueryServiceConfig2A(svc, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, q, sizeof(q), &needed);
        ERR(err, 0);
        OK(*(DWORD *)q == 1, "delayed autostart");
        err = svcctl_QueryServiceConfig2A(svc, 77, q, sizeof(q), &needed);
        ERR(err, ERROR_INVALID_LEVEL);

        /* preferred node */
        memset(q, 0xcc, sizeof(q));
        err = svcctl_QueryServiceConfig2W(svc, SERVICE_CONFIG_PREFERRED_NODE, q, sizeof(q), &needed);
        ERR(err, ERROR_FILE_NOT_FOUND);
        info.dwInfoLevel = SERVICE_CONFIG_PREFERRED_NODE; info.prefnode = &pn;
        err = svcctl_ChangeServiceConfig2A(svc, info);
        ERR(err, 0);
        err = svcctl_QueryServiceConfig2A(svc, SERVICE_CONFIG_PREFERRED_NODE, q, sizeof(q), &needed);
        ERR(err, 0);
        OK(needed == sizeof(SERVICE_PREFERRED_NODE_INFO) && ((SERVICE_PREFERRED_NODE_INFO *)q)->usPreferredNode == 3 &&
           !((SERVICE_PREFERRED_NODE_INFO *)q)->fDelete, "preferred node %u", ((SERVICE_PREFERRED_NODE_INFO *)q)->usPreferredNode);
        err = svcctl_QueryServiceConfig2W(svc, SERVICE_CONFIG_PREFERRED_NODE, q, 2, &needed);
        ERR(err, ERROR_INSUFFICIENT_BUFFER);
        err = svcctl_ChangeServiceConfigA(svc, SERVICE_WIN32_SHARE_PROCESS, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL,
                                          NULL, NULL, 0, NULL, NULL, 0, NULL);
        ERR(err, ERROR_INVALID_PARAMETER);
        pn.fDelete = 1;
        err = svcctl_ChangeServiceConfig2A(svc, info);
        ERR(err, 0);
        err = svcctl_QueryServiceConfig2A(svc, SERVICE_CONFIG_PREFERRED_NODE, q, sizeof(q), &needed);
        ERR(err, ERROR_FILE_NOT_FOUND);
        err = svcctl_ChangeServiceConfigA(svc, SERVICE_WIN32_SHARE_PROCESS, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL,
                                          NULL, NULL, 0, NULL, NULL, 0, NULL);
        ERR(err, 0);
        pn.fDelete = 0;
        err = svcctl_ChangeServiceConfig2A(svc, info);
        ERR(err, ERROR_INVALID_PARAMETER);
        err = svcctl_ChangeServiceConfigA(svc, SERVICE_WIN32_OWN_PROCESS, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, NULL, NULL,
                                          NULL, NULL, 0, NULL, NULL, 0, NULL);
        ERR(err, 0);
    }

    /* ---- triggers through QueryServiceConfigEx */
    printf("-- triggers through QueryServiceConfigEx\n");
    {
        SC_RPC_CONFIG_INFOW info, out;
        SERVICE_TRIGGER_SPECIFIC_DATA_ITEM item;
        SERVICE_TRIGGER trig;
        SERVICE_TRIGGER_INFO ti, ti_in;
        static const GUID guid = { 0x11223344, 0x5566, 0x7788, { 1, 2, 3, 4, 5, 6, 7, 8 } };
        static BYTE data[] = { 'a', 0, 'b', 0, 0, 0 };

        item.dwDataType = SERVICE_TRIGGER_DATA_TYPE_STRING; item.cbData = sizeof(data); item.pData = data;
        trig.dwTriggerType = SERVICE_TRIGGER_TYPE_CUSTOM; trig.dwAction = SERVICE_TRIGGER_ACTION_SERVICE_START;
        trig.pTriggerSubtype = (GUID *)&guid; trig.cDataItems = 1; trig.pDataItems = &item;
        ti.cTriggers = 1; ti.pTriggers = &trig; ti.pReserved = NULL;
        memset(&info, 0, sizeof(info));
        info.dwInfoLevel = SERVICE_CONFIG_TRIGGER_INFO; info.trigger = &ti;
        err = svcctl_ChangeServiceConfig2W(svc, info);
        ERR(err, 0);
        /* Wine's client stub sizes the [out] union as if it were an input: give it a valid arm */
        memset(&ti_in, 0, sizeof(ti_in));
        out.dwInfoLevel = SERVICE_CONFIG_TRIGGER_INFO; out.trigger = &ti_in;
        err = svcctl_QueryServiceConfigEx(svc, SERVICE_CONFIG_TRIGGER_INFO, &out);
        ERR(err, 0);
        OK(out.dwInfoLevel == SERVICE_CONFIG_TRIGGER_INFO && out.trigger && out.trigger->cTriggers == 1, "trigger info");
        if (!err && out.trigger && out.trigger->cTriggers == 1)
        {
            const SERVICE_TRIGGER *t = &out.trigger->pTriggers[0];
            OK(t->dwTriggerType == SERVICE_TRIGGER_TYPE_CUSTOM && t->dwAction == SERVICE_TRIGGER_ACTION_SERVICE_START, "trigger");
            OK(t->pTriggerSubtype && !memcmp(t->pTriggerSubtype, &guid, sizeof(guid)), "subtype");
            OK(t->cDataItems == 1 && t->pDataItems[0].dwDataType == SERVICE_TRIGGER_DATA_TYPE_STRING &&
               t->pDataItems[0].cbData == sizeof(data) && !memcmp(t->pDataItems[0].pData, data, sizeof(data)), "data item");
        }
        out.dwInfoLevel = SERVICE_CONFIG_TRIGGER_INFO; out.trigger = &ti_in;
        err = svcctl_QueryServiceConfigEx(svc, SERVICE_CONFIG_DESCRIPTION, &out);
        ERR(err, ERROR_INVALID_LEVEL);
        ti.cTriggers = 0; ti.pTriggers = NULL;
        err = svcctl_ChangeServiceConfig2W(svc, info);
        ERR(err, 0);
        out.dwInfoLevel = SERVICE_CONFIG_TRIGGER_INFO; out.trigger = &ti_in;
        err = svcctl_QueryServiceConfigEx(svc, SERVICE_CONFIG_TRIGGER_INFO, &out);
        ERR(err, 0);
        OK(out.trigger && !out.trigger->cTriggers, "triggers removed");
    }

    /* ---- ControlServiceEx on a stopped service */
    printf("-- ControlServiceEx on a stopped service\n");
    {
        SERVICE_CONTROL_STATUS_REASON_IN_PARAMSW inw = { 0, NULL };
        SERVICE_CONTROL_STATUS_REASON_IN_PARAMSA ina = { 0, NULL };
        SC_RPC_SERVICE_CONTROL_IN_PARAMSW uw;
        SC_RPC_SERVICE_CONTROL_IN_PARAMSA ua;
        SC_RPC_SERVICE_CONTROL_OUT_PARAMSW ow;
        SC_RPC_SERVICE_CONTROL_OUT_PARAMSA oa;

        uw.psrInParams = &inw; ua.psrInParams = &ina;
        memset(&ow, 0, sizeof(ow)); memset(&oa, 0, sizeof(oa));
        err = svcctl_ControlServiceExW(svc, SERVICE_CONTROL_INTERROGATE, 1, &uw, &ow);
        ERR(err, ERROR_SERVICE_NOT_ACTIVE);
        OK(ow.psrOutParams && ow.psrOutParams->ServiceStatus.dwCurrentState == SERVICE_STOPPED &&
           ow.psrOutParams->ServiceStatus.dwServiceType == SERVICE_WIN32_OWN_PROCESS, "control status");
        free(ow.psrOutParams);
        err = svcctl_ControlServiceExA(svc, SERVICE_CONTROL_INTERROGATE, 1, &ua, &oa);
        ERR(err, ERROR_SERVICE_NOT_ACTIVE);
        OK(oa.psrOutParams && oa.psrOutParams->ServiceStatus.dwCurrentState == SERVICE_STOPPED, "control status A");
        free(oa.psrOutParams);
        inw.pszComment = (LPWSTR)L"why"; ina.pszComment = (LPSTR)"why";
        err = svcctl_ControlServiceExW(svc, SERVICE_CONTROL_INTERROGATE, 1, &uw, &ow);
        ERR(err, ERROR_INVALID_PARAMETER);
        free(ow.psrOutParams);
        err = svcctl_ControlServiceExA(svc, SERVICE_CONTROL_INTERROGATE, 1, &ua, &oa);
        ERR(err, ERROR_INVALID_PARAMETER);
        free(oa.psrOutParams);
        err = svcctl_ControlServiceExW(svc, SERVICE_CONTROL_STOP, 1, &uw, &ow);
        ERR(err, ERROR_SERVICE_NOT_ACTIVE);  /* a stop may carry a comment */
        free(ow.psrOutParams);
    }

    /* ---- start (a disabled service answers before anything runs) */
    printf("-- start (a disabled service answers before anything runs)\n");
    err = svcctl_ChangeServiceConfigA(svc, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, 0,
                                      NULL, NULL, 0, NULL);
    ERR(err, 0);
    {
        LPCSTR args[2] = { "first", "second" };
        err = svcctl_StartServiceA(svc, 2, args);
        ERR(err, ERROR_SERVICE_DISABLED);
        err = svcctl_StartServiceA(svc, 0, NULL);
        ERR(err, ERROR_SERVICE_DISABLED);
    }

    /* ---- boot configuration (once per boot) */
    printf("-- boot configuration (once per boot)\n");
    err = svcctl_NotifyBootConfigStatus(NULL, 0);
    ERR(err, ERROR_NOT_SUPPORTED);
    err = svcctl_NotifyBootConfigStatus(NULL, 1);
    ERR(err, 0);
    err = svcctl_NotifyBootConfigStatus(NULL, 1);
    ERR(err, ERROR_BOOT_ALREADY_ACCEPTED);

    /* ---- clean up */
    printf("-- clean up\n");
    OK(!svcctl_DeleteService(svc), "delete");
    OK(!svcctl_DeleteService(wow), "delete wow");
    svcctl_CloseServiceHandle(&svc);
    svcctl_CloseServiceHandle(&svc2);
    svcctl_CloseServiceHandle(&wow);
    svcctl_CloseServiceHandle(&rpcss);
    svcctl_CloseServiceHandle(&scm2);
    svcctl_CloseServiceHandle(&scm);
    (void)i; (void)privs;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    test_all();
    printf("%d checks, %d failures\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
