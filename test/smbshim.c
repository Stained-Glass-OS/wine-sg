/* A stand-in for the kernel's SMB client, for gates (LD_PRELOAD into Wine).
 *
 * Paths under $SG_SMBSHIM_ROOT (a tmpfs the gate mounts where sg-netmountd
 * mounts shares) look like an SMB mount to statfs, and answer as the CIFS
 * client does: "user.cifs.dosattrib" gives the server's DOS attributes at
 * once (hidden for names starting "hid"), while "user.DOSATTRIB" -- an
 * extended attribute the server is asked for -- costs a network round trip
 * ($SG_SMBSHIM_RTT_US, 3000 by default) and is never there. Opening a
 * folder below the share or an .exe in it costs a round trip too. What was
 * asked is counted in $SG_SMBSHIM_LOG (one letter per call: D a DOSATTRIB
 * read, F a folder opened below the share, E an .exe opened, S a statfs). */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/xattr.h>
#include <unistd.h>

#define SMB2_MAGIC 0xfe534d42

static int resolve(const char *path, char *out)
{
    char cwd[PATH_MAX];
    if (!path) return 0;
    if (path[0] == '/') snprintf(out, PATH_MAX, "%s", path);
    else if (getcwd(cwd, sizeof(cwd))) snprintf(out, PATH_MAX, "%s/%s", cwd, path);
    else return 0;
    return 1;
}

static int under_root(const char *abs)
{
    const char *root = getenv("SG_SMBSHIM_ROOT");
    size_t n = root ? strlen(root) : 0;
    return n && !strncmp(abs, root, n) && (abs[n] == '/' || !abs[n]);
}

static int path_smb(const char *path, char *abs)
{
    char buf[PATH_MAX];
    if (!abs) abs = buf;
    return resolve(path, abs) && under_root(abs);
}

static int fd_smb(int fd, char *abs)
{
    char link[64], buf[PATH_MAX];
    ssize_t n;
    if (!abs) abs = buf;
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    if ((n = readlink(link, abs, PATH_MAX - 1)) <= 0) return 0;
    abs[n] = 0;
    return under_root(abs);
}

static void note(char c)
{
    const char *log = getenv("SG_SMBSHIM_LOG");
    int fd;
    if (!log || (fd = open(log, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0666)) < 0) return;
    if (write(fd, &c, 1) < 0) {}
    close(fd);
}

static void round_trip(void)
{
    const char *rtt = getenv("SG_SMBSHIM_RTT_US");
    usleep(rtt ? atoi(rtt) : 3000);
}

static ssize_t attrib(const char *abs, const char *name, void *value, size_t size)
{
    const char *base = strrchr(abs, '/');
    unsigned int attr;

    base = base ? base + 1 : abs;
    if (!strcmp(name, "user.cifs.dosattrib"))
    {
        struct stat st;
        if (stat(abs, &st)) return -1;
        attr = S_ISDIR(st.st_mode) ? 0x10 : 0x20;
        if (!strncmp(base, "hid", 3)) attr |= 0x2;
        if (!size) return 4;
        if (size < 4) { errno = ERANGE; return -1; }
        memcpy(value, &attr, 4);
        return 4;
    }
    if (!strcmp(name, "user.DOSATTRIB"))
    {
        note('D');
        round_trip();
    }
    errno = ENODATA;
    return -1;
}

ssize_t getxattr(const char *path, const char *name, void *value, size_t size)
{
    static ssize_t (*real)(const char *, const char *, void *, size_t);
    char abs[PATH_MAX];
    if (path_smb(path, abs)) return attrib(abs, name, value, size);
    if (!real) real = dlsym(RTLD_NEXT, "getxattr");
    return real(path, name, value, size);
}

ssize_t lgetxattr(const char *path, const char *name, void *value, size_t size)
{
    static ssize_t (*real)(const char *, const char *, void *, size_t);
    char abs[PATH_MAX];
    if (path_smb(path, abs)) return attrib(abs, name, value, size);
    if (!real) real = dlsym(RTLD_NEXT, "lgetxattr");
    return real(path, name, value, size);
}

ssize_t fgetxattr(int fd, const char *name, void *value, size_t size)
{
    static ssize_t (*real)(int, const char *, void *, size_t);
    char abs[PATH_MAX];
    if (fd_smb(fd, abs)) return attrib(abs, name, value, size);
    if (!real) real = dlsym(RTLD_NEXT, "fgetxattr");
    return real(fd, name, value, size);
}

static void smb_statfs(struct statfs *buf)
{
    buf->f_type = SMB2_MAGIC;
    note('S');
}

int statfs(const char *path, struct statfs *buf)
{
    static int (*real)(const char *, struct statfs *);
    int ret;
    if (!real) real = dlsym(RTLD_NEXT, "statfs");
    if (!(ret = real(path, buf)) && path_smb(path, NULL)) smb_statfs(buf);
    return ret;
}

int fstatfs(int fd, struct statfs *buf)
{
    static int (*real)(int, struct statfs *);
    int ret;
    if (!real) real = dlsym(RTLD_NEXT, "fstatfs");
    if (!(ret = real(fd, buf)) && fd_smb(fd, NULL)) smb_statfs(buf);
    return ret;
}

/* a folder below the share, or a program in it, is opened over the network */
static void opening(const char *abs, int flags)
{
    const char *root = getenv("SG_SMBSHIM_ROOT"), *ext;
    struct stat st;

    if (!stat(abs, &st) && S_ISDIR(st.st_mode))
    {
        /* the share itself and the folders it is in are not counted */
        if (root && strlen(abs) > strlen(root) + 1 && strchr(abs + strlen(root) + 1, '/'))
        {
            note('F');
            round_trip();
        }
        return;
    }
    if ((ext = strrchr(abs, '.')) && !strcasecmp(ext, ".exe"))
    {
        note('E');
        round_trip();
        round_trip();
    }
    (void)flags;
}

int open(const char *path, int flags, ...)
{
    static int (*real)(const char *, int, ...);
    char abs[PATH_MAX];
    mode_t mode = 0;
    va_list ap;
    if (flags & (O_CREAT | O_TMPFILE)) { va_start(ap, flags); mode = va_arg(ap, mode_t); va_end(ap); }
    if (!real) real = dlsym(RTLD_NEXT, "open");
    if (path_smb(path, abs)) opening(abs, flags);
    return real(path, flags, mode);
}
int open64(const char *path, int flags, ...) __attribute__((alias("open")));

int openat(int dirfd, const char *path, int flags, ...)
{
    static int (*real)(int, const char *, int, ...);
    char abs[PATH_MAX], dir[PATH_MAX], link[64];
    mode_t mode = 0;
    ssize_t n;
    va_list ap;
    if (flags & (O_CREAT | O_TMPFILE)) { va_start(ap, flags); mode = va_arg(ap, mode_t); va_end(ap); }
    if (!real) real = dlsym(RTLD_NEXT, "openat");
    if (path[0] != '/' && dirfd != AT_FDCWD)
    {
        snprintf(link, sizeof(link), "/proc/self/fd/%d", dirfd);
        if ((n = readlink(link, dir, sizeof(dir) - 1)) > 0)
        {
            dir[n] = 0;
            snprintf(abs, sizeof(abs), "%s/%s", dir, path);
            if (under_root(abs)) opening(abs, flags);
        }
    }
    else if (path_smb(path, abs)) opening(abs, flags);
    return real(dirfd, path, flags, mode);
}
int openat64(int dirfd, const char *path, int flags, ...) __attribute__((alias("openat")));
