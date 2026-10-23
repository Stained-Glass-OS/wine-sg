/* junction-probe: how a junction and a symbolic link read back, for test/junction-gate.sh
 * (Stained Glass OS wine-sg 6039).
 *   junction-probe PATH   the reparse point at PATH: "tag=" (FileAttributeTagInfo), "fsctl=" the
 *                         tag FSCTL_GET_REPARSE_POINT gives, "subst=" / "print=" its names, "find="
 *                         FindFirstFile's dwReserved0, "attr_reparse=" 0/1
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>

typedef struct
{
    ULONG tag;
    USHORT len, reserved;
    USHORT subst_off, subst_len, print_off, print_len;
    BYTE rest[1];
} reparse_header;

int main( int argc, char **argv )
{
    BYTE buffer[16384];
    reparse_header *rp = (reparse_header *)buffer;
    FILE_ATTRIBUTE_TAG_INFO tag;
    WIN32_FIND_DATAA fd;
    HANDLE file, find;
    DWORD size;
    WCHAR *names;

    if (argc < 2) return 1;
    file = CreateFileA( argv[1], 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL );
    if (file == INVALID_HANDLE_VALUE) { printf( "open=%lu\n", GetLastError() ); return 1; }
    if (GetFileInformationByHandleEx( file, FileAttributeTagInfo, &tag, sizeof(tag) ))
        printf( "tag=%08lx\nattr_reparse=%d\n", tag.ReparseTag, !!(tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) );
    if (DeviceIoControl( file, FSCTL_GET_REPARSE_POINT, NULL, 0, buffer, sizeof(buffer), &size, NULL ))
    {
        /* a junction's names follow its 16-byte header, a symbolic link's its flags */
        names = (WCHAR *)(buffer + (rp->tag == IO_REPARSE_TAG_SYMLINK ? 20 : 16));
        printf( "fsctl=%08lx\n", rp->tag );
        printf( "subst=%.*ls\n", rp->subst_len / 2, names + rp->subst_off / 2 );
        printf( "print=%.*ls\n", rp->print_len / 2, names + rp->print_off / 2 );
    }
    else printf( "fsctl_error=%lu\n", GetLastError() );
    CloseHandle( file );
    find = FindFirstFileA( argv[1], &fd );
    if (find != INVALID_HANDLE_VALUE)
    {
        printf( "find=%08lx\nfind_attr=%08lx\n", fd.dwReserved0, fd.dwFileAttributes );
        FindClose( find );
    }
    return 0;
}
