/* Smoke probe for patches/sg/4556 (fast mode): msvcp60's
 * time_get<char>/time_get<unsigned short>::date_order and do_date_order
 * (the order of day, month and year of the facet's locale: mdy in "C"),
 * on facets built with the reference count constructor.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
#define N(a, b) b
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define TG(t) "?$time_get@" t "V?$istreambuf_iterator@" t "U?$char_traits@" t "@std@@@std@@@std@@"

int main(int argc, char **argv)
{
    static const char *const types[] = {"D", "G"};
    HMODULE mod = LoadLibraryA("msvcp60.dll");
    unsigned int i;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    for (i = 0; i < 2; i++)
    {
        char n_ctor[256], n_order[256], n_do[256];
        void *(THISCALL *ctor)(void *, size_t);
        int (THISCALL *order)(void *), (THISCALL *do_order)(void *);
        static char facet[2][512];

        if (i == 0)
        {
            sprintf(n_ctor, "??0" TG("D") N("QEAA@_K@Z", "QAE@I@Z"));
            sprintf(n_order, "?date_order@" TG("D") N("QEBAHXZ", "QBEHXZ"));
            sprintf(n_do, "?do_date_order@" TG("D") N("MEBAHXZ", "MBEHXZ"));
        }
        else
        {
            sprintf(n_ctor, "??0" TG("G") N("QEAA@_K@Z", "QAE@I@Z"));
            sprintf(n_order, "?date_order@" TG("G") N("QEBAHXZ", "QBEHXZ"));
            sprintf(n_do, "?do_date_order@" TG("G") N("MEBAHXZ", "MBEHXZ"));
        }
        ctor = (void *)GetProcAddress(mod, n_ctor);
        order = (void *)GetProcAddress(mod, n_order);
        do_order = (void *)GetProcAddress(mod, n_do);
        check(ctor && order && do_order, "%s: exports %p %p %p", types[i], ctor, order, do_order);
        if (!ctor || !order || !do_order) continue;
        ctor(facet[i], 1);
        check(order(facet[i]) == 2 && do_order(facet[i]) == 2, "%s: date_order %d, do_date_order %d (mdy)",
              types[i], order(facet[i]), do_order(facet[i]));
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
