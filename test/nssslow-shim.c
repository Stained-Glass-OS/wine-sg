/* nssslow-shim.c: LD_PRELOAD for test/nssslow-gate.sh -- inside wineserver only, every account lookup
 * takes SG_SLOW_NSS seconds first, as through a winbindd waiting on a domain controller (7824) */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

static int slow = -1;
static void wait_a_while(void)
{
    if (slow < 0)
    {
        char comm[64] = "";
        FILE *f = NULL;
        const char *s = getenv( "SG_SLOW_NSS" );
        int fd = open( "/proc/self/comm", 0 );
        if (fd >= 0) { read( fd, comm, sizeof(comm) - 1 ); close( fd ); }
        slow = (s && !strncmp( comm, "wineserver", 10 )) ? atoi( s ) : 0;
        (void)f;
    }
    if (slow) sleep( slow );
}
#define WRAP(ret, name, params, args) \
    ret name params { static ret (*real) params; wait_a_while(); \
        if (!real) real = dlsym( RTLD_NEXT, #name ); return real args; }
WRAP(struct passwd *, getpwuid, (uid_t u), (u))
WRAP(int, getpwuid_r, (uid_t u, struct passwd *p, char *b, size_t l, struct passwd **r), (u, p, b, l, r))
WRAP(struct group *, getgrnam, (const char *n), (n))
WRAP(int, getgrnam_r, (const char *n, struct group *g, char *b, size_t l, struct group **r), (n, g, b, l, r))
