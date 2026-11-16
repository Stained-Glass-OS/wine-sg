/* trashcancel-slowfs.c: a FUSE filesystem with one file, slow.bin (SIZE bytes of 'S'), whose reads take
 * a while (DELAY_MS per call), for test/trashcancel-gate.sh: a move of it into the Recycle Bin has to copy
 * (another filesystem) and runs long enough to watch its progress window and press Cancel.
 *   trashcancel-slowfs MOUNTPOINT [fuse options]      (runs in the foreground: -f is added)
 */
#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#define SIZE      (24 * 1024 * 1024)
#define DELAY_MS  25

static int removed;

static int fs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi)
{
    memset(st, 0, sizeof(*st));
    if (!strcmp(path, "/")) { st->st_mode = S_IFDIR | 0777; st->st_nlink = 2; return 0; }
    if (!strcmp(path, "/slow.bin") && !removed)
    {
        st->st_mode = S_IFREG | 0666;
        st->st_nlink = 1;
        st->st_size = SIZE;
        return 0;
    }
    return -ENOENT;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t fill, off_t off, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags)
{
    if (strcmp(path, "/")) return -ENOENT;
    fill(buf, ".", NULL, 0, 0);
    fill(buf, "..", NULL, 0, 0);
    if (!removed) fill(buf, "slow.bin", NULL, 0, 0);
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
    if (strcmp(path, "/slow.bin") || removed) return -ENOENT;
    fi->direct_io = 1;   /* every read reaches us: no page cache */
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
    struct timespec ts = { 0, DELAY_MS * 1000000L };
    if (off >= SIZE) return 0;
    if (off + (off_t)size > SIZE) size = SIZE - off;
    nanosleep(&ts, NULL);
    memset(buf, 'S', size);
    return size;
}

static int fs_write(const char *path, const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
    return -EROFS;
}

static int fs_unlink(const char *path)
{
    if (strcmp(path, "/slow.bin") || removed) return -ENOENT;
    removed = 1;
    return 0;
}

static int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi) { return -EROFS; }
static int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) { return 0; }
static int fs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) { return 0; }

static const struct fuse_operations ops =
{
    .getattr = fs_getattr, .readdir = fs_readdir, .open = fs_open, .read = fs_read, .write = fs_write,
    .unlink = fs_unlink, .truncate = fs_truncate, .utimens = fs_utimens, .chmod = fs_chmod,
};

int main(int argc, char **argv)
{
    char *args[16];
    int i, n = 0;

    args[n++] = argv[0];
    args[n++] = "-f";
    args[n++] = "-s";
    for (i = 1; i < argc && n < 15; i++) args[n++] = argv[i];
    args[n] = NULL;
    return fuse_main(n, args, &ops, NULL);
}
