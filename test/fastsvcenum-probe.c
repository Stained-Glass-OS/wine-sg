#include <windows.h>
#include <winsvc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int msz_equal(const char *a, const char *b, size_t blen)
{
    return !memcmp(a, b, blen);
}

int main(void)
{
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    DWORD need, ret, ret2, i, total, resume, got;
    ENUM_SERVICE_STATUSW *w, *w2;
    ENUM_SERVICE_STATUSA *a;
    ENUM_SERVICE_STATUS_PROCESSW *ex;
    BOOL ok;
    char compname[64];
    DWORD csz = sizeof(compname);

    CHECK(scm != NULL, "OpenSCManager %lu", GetLastError());

    /* EnumServicesStatusW: the full list, then in pieces with a resume handle */
    need = 0; ret = 0;
    ok = EnumServicesStatusW(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &ret, NULL);
    CHECK(!ok && GetLastError() == ERROR_MORE_DATA && need > 0 && ret == 0, "probe ok=%d err=%lu need=%lu ret=%lu", ok, GetLastError(), need, ret);
    w = malloc(need);
    ok = EnumServicesStatusW(scm, SERVICE_WIN32, SERVICE_STATE_ALL, w, need, &need, &ret, NULL);
    CHECK(ok && ret > 3 && need == 0, "full ok=%d err=%lu ret=%lu need=%lu", ok, GetLastError(), ret, need);
    total = ret;
    printf("TOTAL %lu\n", total);

    {
        DWORD bufsize = (total - 1) * sizeof(ENUM_SERVICE_STATUSW), n1, n2, need1, need2;
        DWORD allneed;
        BYTE *buf = malloc(100000);
        BYTE *buf2 = malloc(100000);
        /* the first piece */
        need1 = 0; n1 = 0; resume = 0;
        ok = EnumServicesStatusW(scm, SERVICE_WIN32, SERVICE_STATE_ALL, (ENUM_SERVICE_STATUSW *)buf, bufsize, &need1, &n1, &resume);
        CHECK(!ok && GetLastError() == ERROR_MORE_DATA, "piece1 ok=%d err=%lu", ok, GetLastError());
        CHECK(n1 > 0 && n1 < total, "piece1 returned %lu of %lu", n1, total);
        CHECK(resume == n1, "resume %lu != returned %lu", resume, n1);
        CHECK(need1 > 0, "piece1 needed %lu", need1);
        /* the rest with a big buffer and the resume handle */
        n2 = 0; need2 = 77;
        ok = EnumServicesStatusW(scm, SERVICE_WIN32, SERVICE_STATE_ALL, (ENUM_SERVICE_STATUSW *)buf2, 100000, &need2, &n2, &resume);
        CHECK(ok && need2 == 0 && resume == 0, "rest ok=%d err=%lu need=%lu resume=%lu", ok, GetLastError(), need2, resume);
        CHECK(n1 + n2 == total, "pieces %lu + %lu != %lu", n1, n2, total);
        /* names continue where the first piece stopped */
        w2 = (ENUM_SERVICE_STATUSW *)buf2;
        for (i = 0; i < n1; i++) if (!lstrcmpW(((ENUM_SERVICE_STATUSW *)buf)[i].lpServiceName, w2[0].lpServiceName)) break;
        CHECK(i == n1, "the second piece starts with a service of the first");
        CHECK(!lstrcmpW(w[n1].lpServiceName, w2[0].lpServiceName), "the second piece starts at index %lu", n1);
        allneed = need1;
        printf("NEED1 %lu\n", allneed);

        /* walk the whole list in small pieces */
        resume = 0; got = 0;
        for (i = 0; i < 400; i++)
        {
            DWORD cnt = 0, nd = 0;
            ok = EnumServicesStatusW(scm, SERVICE_WIN32, SERVICE_STATE_ALL, (ENUM_SERVICE_STATUSW *)buf, 1200, &nd, &cnt, &resume);
            got += cnt;
            if (ok) break;
            if (GetLastError() != ERROR_MORE_DATA) break;
            if (!cnt) break;
        }
        CHECK(ok && got == total, "walk ok=%d got %lu total %lu loops %lu", ok, got, total, i);

        /* the ANSI flavour agrees on the count and on the needed size of a probe */
        {
            DWORD nA = 0, rA = 0;
            ok = EnumServicesStatusA(scm, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &nA, &rA, NULL);
            CHECK(!ok && GetLastError() == ERROR_MORE_DATA && rA == 0, "A probe");
            a = malloc(nA);
            ok = EnumServicesStatusA(scm, SERVICE_WIN32, SERVICE_STATE_ALL, a, nA, &nA, &rA, NULL);
            CHECK(ok && rA == total, "A full ok=%d ret %lu total %lu", ok, rA, total);
            free(a);
            resume = 0; nA = 0; rA = 0;
            ok = EnumServicesStatusA(scm, SERVICE_WIN32, SERVICE_STATE_ALL, (ENUM_SERVICE_STATUSA *)buf, bufsize, &nA, &rA, &resume);
            CHECK(!ok && GetLastError() == ERROR_MORE_DATA && rA > 0 && rA < total && resume == rA, "A piece ok=%d err=%lu ret=%lu resume=%lu", ok, GetLastError(), rA, resume);
            if (!ok && rA > 0)
            {
                ENUM_SERVICE_STATUSA *pa = (ENUM_SERVICE_STATUSA *)buf;
                char nm[256];
                WideCharToMultiByte(CP_ACP, 0, w[0].lpServiceName, -1, nm, sizeof(nm), NULL, NULL);
                CHECK(!strcmp(pa[0].lpServiceName, nm), "A first name %s vs %s", pa[0].lpServiceName, nm);
            }
        }

        /* Ex flavour */
        need = 0; ret2 = 0;
        ok = EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &need, &ret2, NULL, NULL);
        CHECK(!ok && GetLastError() == ERROR_MORE_DATA && need > 0 && ret2 == 0, "Ex probe");
        ex = malloc(need);
        ok = EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, (BYTE *)ex, need, &need, &ret2, NULL, NULL);
        CHECK(ok && ret2 == total, "Ex full ok=%d ret %lu total %lu", ok, ret2, total);
        free(ex);
        resume = 0;
        ret2 = 0; need = 0;
        ok = EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, 1200, &need, &ret2, &resume, NULL);
        CHECK(!ok && GetLastError() == ERROR_MORE_DATA && ret2 > 0 && ret2 < total && resume == ret2 && need > 0, "Ex piece ok=%d err=%lu ret=%lu resume=%lu need=%lu", ok, GetLastError(), ret2, resume, need);
        {
            DWORD first = ret2;
            ret2 = 0; need = 0;
            ok = EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf2, 100000, &need, &ret2, &resume, NULL);
            CHECK(ok && ret2 + first == total && resume == 0 && need == 0, "Ex rest ok=%d ret=%lu first=%lu resume=%lu", ok, ret2, first, resume);
        }
        free(buf); free(buf2);
    }
    free(w);

    /* a service with dependencies: the multi-string comes back whole */
    {
        static const char deps[] = "Master1\0Master2\0+MasterGroup1\0";
        SC_HANDLE svc = CreateServiceA(scm, "FastSvcDeps", "Fast Svc Deps", SERVICE_ALL_ACCESS,
                SERVICE_WIN32_OWN_PROCESS, SERVICE_DISABLED, SERVICE_ERROR_IGNORE, "we_dont_care.exe",
                NULL, NULL, deps, NULL, NULL);
        CHECK(svc != NULL, "CreateService %lu", GetLastError());
        if (svc)
        {
            DWORD n = 0;
            QUERY_SERVICE_CONFIGA *ca;
            QUERY_SERVICE_CONFIGW *cw;
            ok = QueryServiceConfigA(svc, NULL, 0, &n);
            ca = malloc(n);
            ok = QueryServiceConfigA(svc, ca, n, &n);
            CHECK(ok, "QueryServiceConfigA %lu", GetLastError());
            CHECK(ok && msz_equal(ca->lpDependencies, deps, sizeof(deps)), "A dependencies differ");
            free(ca);
            n = 0;
            QueryServiceConfigW(svc, NULL, 0, &n);
            cw = malloc(n);
            ok = QueryServiceConfigW(svc, cw, n, &n);
            CHECK(ok, "QueryServiceConfigW %lu", GetLastError());
            CHECK(ok && !memcmp(cw->lpDependencies, L"Master1\0Master2\0+MasterGroup1\0", sizeof(L"Master1\0Master2\0+MasterGroup1\0")), "W dependencies differ");
            free(cw);
            DeleteService(svc);
            CloseServiceHandle(svc);
        }
        /* no dependencies: an empty multi-string */
        svc = CreateServiceA(scm, "FastSvcNoDeps", NULL, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                SERVICE_DISABLED, SERVICE_ERROR_IGNORE, "we_dont_care.exe", NULL, NULL, NULL, NULL, NULL);
        CHECK(svc != NULL, "CreateService 2");
        if (svc)
        {
            DWORD n = 0;
            QUERY_SERVICE_CONFIGA *ca;
            QueryServiceConfigA(svc, NULL, 0, &n);
            ca = malloc(n);
            ok = QueryServiceConfigA(svc, ca, n, &n);
            CHECK(ok && ca->lpDependencies && !ca->lpDependencies[0] && !ca->lpDependencies[1], "empty dependencies");
            free(ca);
            DeleteService(svc);
            CloseServiceHandle(svc);
        }
    }
    CloseServiceHandle(scm);

    /* OpenSCManager on other machines */
    {
        SC_HANDLE h;
        SetLastError(0xdeadbeef);
        h = OpenSCManagerA("DOESNOTEXIST", NULL, SC_MANAGER_CONNECT);
        CHECK(!h && GetLastError() == RPC_S_SERVER_UNAVAILABLE, "remote name: h=%p err=%lu", h, GetLastError());
        if (h) CloseServiceHandle(h);
        h = OpenSCManagerA("localhost", NULL, SC_MANAGER_CONNECT);
        CHECK(h != NULL, "localhost err=%lu", GetLastError());
        if (h) CloseServiceHandle(h);
        h = OpenSCManagerA(".", NULL, SC_MANAGER_CONNECT);
        CHECK(h != NULL, "dot err=%lu", GetLastError());
        if (h) CloseServiceHandle(h);
        h = OpenSCManagerA("", NULL, SC_MANAGER_CONNECT);
        CHECK(h != NULL, "empty err=%lu", GetLastError());
        if (h) CloseServiceHandle(h);
        GetComputerNameA(compname, &csz);
        h = OpenSCManagerA(compname, NULL, SC_MANAGER_CONNECT);
        CHECK(h != NULL, "own name %s err=%lu", compname, GetLastError());
        if (h) CloseServiceHandle(h);
        {
            char unc[80];
            sprintf(unc, "\\\\%s", compname);
            h = OpenSCManagerA(unc, NULL, SC_MANAGER_CONNECT);
            CHECK(h != NULL, "unc name %s err=%lu", unc, GetLastError());
            if (h) CloseServiceHandle(h);
        }
    }

    printf("checks %d fails %d\n", checks, fails);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
