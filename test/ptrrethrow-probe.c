/* ptrrethrow-probe: std::rethrow_exception throws a copy of the object the
 * exception_ptr holds (patches/sg/1530), so the catch that ends destroys
 * only the copy. 64-bit; the throw info is built by hand. Prints
 * "copies=<n> thrown=<same|copy> stored=<ok|destroyed>" */
#include <windows.h>
#include <stdio.h>

typedef struct { int this_offset, vbase_descr, vbase_offset; } offsets;
typedef struct { UINT flags; unsigned int type_info; offsets off; unsigned int size; unsigned int copy_ctor; } type_info_rva;
typedef struct { UINT count; unsigned int info[1]; } type_table;
typedef struct { UINT flags; unsigned int destructor; unsigned int custom_handler; unsigned int table; } throw_info;
typedef struct { void *vtable; char *name; char mangled[16]; } td;

struct obj { int magic; };
static int copies;
static void *thrown, *stored;

static struct obj * __cdecl obj_copy(struct obj *this, const struct obj *src) { copies++; this->magic = src->magic; return this; }
static void __cdecl obj_dtor(struct obj *this) { this->magic = 0xdead; }

static td descr = { NULL, NULL, ".?AUobj@@" };
static type_info_rva ti;
static type_table table;
static throw_info info;

#define RVA(p) ((unsigned int)((char *)(p) - (char *)GetModuleHandleW(NULL)))

static LONG CALLBACK veh(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode != 0xe06d7363) return EXCEPTION_CONTINUE_SEARCH;
    thrown = (void *)ep->ExceptionRecord->ExceptionInformation[1];
    ExitThread(0);
}

static void (__cdecl *pCreate)(void *);
static void (__cdecl *pCopyException)(void *, const void *, const void *);
static void (__cdecl *pRethrow)(const void *);
static void *ptr[2];

static DWORD WINAPI rethrow_thread(void *arg)
{
    pRethrow(ptr);
    return 0;
}

int main(void)
{
    HMODULE m = LoadLibraryW(L"msvcp140.dll");
    struct obj o = { 0x1234 };
    HANDLE t;

    pCreate = (void *)GetProcAddress(m, "?__ExceptionPtrCreate@@YAXPEAX@Z");
    pCopyException = (void *)GetProcAddress(m, "?__ExceptionPtrCopyException@@YAXPEAXPEBX1@Z");
    pRethrow = (void *)GetProcAddress(m, "?__ExceptionPtrRethrow@@YAXPEBX@Z");
    if (!pCreate || !pCopyException || !pRethrow) { printf("missing\n"); return 0; }

    ti.flags = 0; ti.type_info = RVA(&descr); ti.off.vbase_descr = -1; ti.size = sizeof(struct obj); ti.copy_ctor = RVA(obj_copy);
    table.count = 1; table.info[0] = RVA(&ti);
    info.destructor = RVA(obj_dtor); info.table = RVA(&table);

    pCreate(ptr);
    pCopyException(ptr, &o, &info);
    /* Wine's exception_ptr: { EXCEPTION_RECORD *rec; LONG *ref; } */
    stored = (void *)((EXCEPTION_RECORD *)ptr[0])->ExceptionInformation[1];

    AddVectoredExceptionHandler(1, veh);
    t = CreateThread(NULL, 0, rethrow_thread, NULL, 0, NULL);
    WaitForSingleObject(t, 10000);
    printf("copies=%d thrown=%s stored=%s\n", copies, thrown == stored ? "same" : "copy",
           ((struct obj *)stored)->magic == 0x1234 ? "ok" : "destroyed");
    return 0;
}
