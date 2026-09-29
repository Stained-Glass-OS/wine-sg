/* tzdb-gate.sh's probe (0504): the C++ library's time zone database
 * through msvcp140_atomic_wait's __std_tzdb_* functions. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

struct zones { int err; const char *version; size_t n; const char **names, **links; };
struct cur { int err; const char *name; };
struct sys { int err; double begin, end; INT32 offset, save; const char *abbrev; };

static struct zones *(__stdcall *get_zones)(void);
static void (__stdcall *del_zones)(struct zones *);
static struct cur *(__stdcall *get_cur)(void);
static void (__stdcall *del_cur)(struct cur *);
static struct sys *(__stdcall *get_sys)(const char *, size_t, double);
static void (__stdcall *del_sys)(struct sys *);
static void *(__stdcall *get_leap)(size_t, size_t *);

static void show(const char *label, const char *tz, double ms)
{
    struct sys *s = get_sys(tz, strlen(tz), ms);
    if (!s || s->err) { printf("%s err %d\n", label, s ? s->err : -1); return; }
    printf("%s %d %d %s %.0f %.0f\n", label, s->offset / 1000, s->save / 60000, s->abbrev,
           s->begin / 1000, s->end / 1000);
    del_sys(s);
}

int main(void)
{
    HMODULE m = LoadLibraryA("msvcp140_atomic_wait.dll");
    struct zones *z;
    struct cur *c;
    size_t i, leapn = 99;
    int berlin = 0, link_ok = 0, sorted = 1;

    get_zones = (void *)GetProcAddress(m, "__std_tzdb_get_time_zones");
    del_zones = (void *)GetProcAddress(m, "__std_tzdb_delete_time_zones");
    get_cur = (void *)GetProcAddress(m, "__std_tzdb_get_current_zone");
    del_cur = (void *)GetProcAddress(m, "__std_tzdb_delete_current_zone");
    get_sys = (void *)GetProcAddress(m, "__std_tzdb_get_sys_info");
    del_sys = (void *)GetProcAddress(m, "__std_tzdb_delete_sys_info");
    get_leap = (void *)GetProcAddress(m, "__std_tzdb_get_leap_seconds");
    if (!get_zones || !get_sys) { printf("missing\n"); return 1; }

    z = get_zones();
    for (i = 0; z && !z->err && i < z->n; i++)
    {
        if (!strcmp(z->names[i], "Europe/Berlin") && !z->links[i]) berlin = 1;
        if (!strcmp(z->names[i], "US/Eastern") && z->links[i] && !strcmp(z->links[i], "America/New_York")) link_ok = 1;
        if (i && strcmp(z->names[i - 1], z->names[i]) > 0) sorted = 0;
    }
    printf("zones %d %d %d %d %d\n", z ? z->err : -1, z && z->n > 300, berlin, link_ok, sorted);
    printf("version %d\n", z && z->version && strlen(z->version) >= 5);
    del_zones(z);
    c = get_cur();
    printf("current %d %d\n", c ? c->err : -1, c && c->name && c->name[0]);
    del_cur(c);
    /* 2026-07-01 12:00 UTC, 2026-01-15 12:00 UTC, 2040-07-01 12:00 UTC */
    show("berlin-summer", "Europe/Berlin", 1782907200000.0);
    show("berlin-winter", "Europe/Berlin", 1768478400000.0);
    show("newyork-2040", "America/New_York", 2224843200000.0);
    show("utc", "Etc/UTC", 1782907200000.0);
    show("bad", "../../etc/passwd", 0.0);
    {
        void *leap = get_leap(0, &leapn);
        printf("leap %d %Iu\n", leap != NULL, leapn);
    }
    return 0;
}
