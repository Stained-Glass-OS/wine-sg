/* A DLL laid out the way Chromium's and Electron's are: 512-byte file
 * alignment (mingw's default), with a large read-only blob (wine-sg 0431).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>

/* the blob is the gate's file, placed in the read-only data section */
__asm__( ".section .rdata,\"dr\"\n"
         ".globl blob\nblob:\n"
         ".incbin \"" BLOB_FILE "\"\n"
         ".globl blob_end\nblob_end:\n"
         ".text\n" );
extern const unsigned char blob[], blob_end[];

/* writable, initialised: copy-on-write per process */
unsigned char wdata[16384] = { 1, 2, 3, 4 };

__declspec(dllexport) const unsigned char *get_blob( SIZE_T *size )
{
    *size = blob_end - blob;
    return blob;
}

__declspec(dllexport) unsigned char *get_wdata(void) { return wdata; }
