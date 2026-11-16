/* renamesafe-swap.c: the safety of 7814's retry (wine-sg 7817), on the helper itself
 * (include/wine/sg_mountpath.h), for test/renamesafe-gate.sh, inside a user and mount namespace:
 *   renamesafe-swap SRC DST ALT_SRC ALT_DST     renames SRC to DST by the alternative names;
 * prints "renamed" or "refused <errno name>" */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "wine/sg_mountpath.h"

int main(int argc, char **argv)
{
    struct stat st;
    if (argc < 5) return 2;
    if (lstat(argv[1], &st)) { printf("nosource\n"); return 0; }
    if (sg_rename_at_alt(argv[1], argv[2], argv[3], argv[4], 0, &st)) printf("renamed\n");
    else printf("refused %s\n", errno == EXDEV ? "EXDEV" : errno == ELOOP ? "ELOOP" : errno == ENOTDIR ? "ENOTDIR" : strerror(errno));
    return 0;
}
