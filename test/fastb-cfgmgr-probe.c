/* cfgmgr32, fast mode: range lists (CM_Create_Range_List ... CM_Find_Range) */
#include <windows.h>
#include <cfgmgr32.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef CONFIGRET (WINAPI *create_fn)(DWORD_PTR *, ULONG);
typedef CONFIGRET (WINAPI *free_fn)(DWORD_PTR, ULONG);
typedef CONFIGRET (WINAPI *add_fn)(DWORD_PTR, ULONGLONG, ULONGLONG, ULONG);
typedef CONFIGRET (WINAPI *del_fn)(ULONGLONG, ULONGLONG, DWORD_PTR, ULONG);
typedef CONFIGRET (WINAPI *first_fn)(DWORD_PTR, ULONGLONG *, ULONGLONG *, DWORD_PTR *, ULONG);
typedef CONFIGRET (WINAPI *next_fn)(DWORD_PTR *, ULONGLONG *, ULONGLONG *, ULONG);
typedef CONFIGRET (WINAPI *dup_fn)(DWORD_PTR, DWORD_PTR *, ULONG);
typedef CONFIGRET (WINAPI *two_fn)(DWORD_PTR, DWORD_PTR, DWORD_PTR, ULONG);
typedef CONFIGRET (WINAPI *inv_fn)(DWORD_PTR, DWORD_PTR, ULONGLONG, ULONG);
typedef CONFIGRET (WINAPI *find_fn)(ULONGLONG *, ULONGLONG, ULONG, ULONGLONG, ULONGLONG, DWORD_PTR, ULONG);

int main(void)
{
    HMODULE m = LoadLibraryA("cfgmgr32.dll");
    create_fn create = (void *)GetProcAddress(m, "CM_Create_Range_List");
    free_fn free_list = (void *)GetProcAddress(m, "CM_Free_Range_List");
    add_fn add = (void *)GetProcAddress(m, "CM_Add_Range");
    del_fn del = (void *)GetProcAddress(m, "CM_Delete_Range");
    del_fn test = (void *)GetProcAddress(m, "CM_Test_Range_Available");
    first_fn first = (void *)GetProcAddress(m, "CM_First_Range");
    next_fn next = (void *)GetProcAddress(m, "CM_Next_Range");
    dup_fn dup = (void *)GetProcAddress(m, "CM_Dup_Range_List");
    two_fn intersect = (void *)GetProcAddress(m, "CM_Intersect_Range_List");
    two_fn merge = (void *)GetProcAddress(m, "CM_Merge_Range_List");
    inv_fn invert = (void *)GetProcAddress(m, "CM_Invert_Range_List");
    find_fn find = (void *)GetProcAddress(m, "CM_Find_Range");
    DWORD_PTR a, b, c, el;
    ULONGLONG s, e;

    if (!create || !find) { printf("FAIL  exports\nRESULT: FAIL\n"); return 1; }
    check(create(&a, 0) == CR_SUCCESS && a, "create");
    check(create(NULL, 0) == CR_INVALID_POINTER, "create without pointer");
    check(create(&b, 1) == CR_INVALID_FLAG, "create with flag");
    check(first(a, &s, &e, &el, 0) == CR_FAILURE, "empty list has no first range");
    check(add(a, 30, 40, 0) == CR_SUCCESS && add(a, 10, 20, 0) == CR_SUCCESS, "add out of order");
    check(first(a, &s, &e, &el, 0) == CR_SUCCESS && s == 10 && e == 20, "first is 10-20");
    check(next(&el, &s, &e, 0) == CR_SUCCESS && s == 30 && e == 40, "next is 30-40");
    check(next(&el, &s, &e, 0) == CR_FAILURE, "no third range");
    check(add(a, 25, 28, CM_ADD_RANGE_DONOTADDIFCONFLICT) == CR_SUCCESS, "no conflict add");
    check(add(a, 15, 22, CM_ADD_RANGE_DONOTADDIFCONFLICT) == CR_FAILURE, "conflicting add refused");
    check(add(a, 21, 24, 0) == CR_SUCCESS, "touching add joins");
    check(first(a, &s, &e, &el, 0) == CR_SUCCESS && s == 10 && e == 28, "10-28 after joining");
    check(add(a, 29, 29, 0) == CR_SUCCESS && first(a, &s, &e, &el, 0) == CR_SUCCESS && e == 40, "adjacent value joins all");
    check(test(15, 35, a, 0) == CR_SUCCESS, "15-35 available");
    check(test(5, 12, a, 0) == CR_FAILURE, "5-12 not available");
    check(add(a, 9, 1, 0) == CR_INVALID_RANGE, "reversed range");
    check(del(20, 30, a, 0) == CR_SUCCESS, "delete the middle");
    check(first(a, &s, &e, &el, 0) == CR_SUCCESS && s == 10 && e == 19 && next(&el, &s, &e, 0) == CR_SUCCESS && s == 31 && e == 40,
          "10-19 and 31-40 remain");
    check(dup(a, &b, 0) == CR_SUCCESS && test(31, 40, b, 0) == CR_SUCCESS, "dup");
    check(create(&c, 0) == CR_SUCCESS, "second list");
    check(invert(a, c, 100, 0) == CR_SUCCESS && first(c, &s, &e, &el, 0) == CR_SUCCESS && s == 0 && e == 9 &&
          next(&el, &s, &e, 0) == CR_SUCCESS && s == 20 && e == 30 && next(&el, &s, &e, 0) == CR_SUCCESS && s == 41 && e == 100 &&
          next(&el, &s, &e, 0) == CR_FAILURE, "invert");
    free_list(c, 0);
    create(&c, 0);
    add(c, 15, 35, 0);
    {
        DWORD_PTR d;
        create(&d, 0);
        check(intersect(a, c, d, 0) == CR_SUCCESS && first(d, &s, &e, &el, 0) == CR_SUCCESS && s == 15 && e == 19 &&
              next(&el, &s, &e, 0) == CR_SUCCESS && s == 31 && e == 35, "intersect");
        free_list(d, 0);
        create(&d, 0);
        check(merge(a, c, d, 0) == CR_SUCCESS && first(d, &s, &e, &el, 0) == CR_SUCCESS && s == 10 && e == 40 &&
              next(&el, &s, &e, 0) == CR_FAILURE, "merge");
        free_list(d, 0);
    }
    free_list(c, 0);
    create(&c, 0);
    add(c, 0x1100, 0x2fff, 0);
    check(find(&s, 0, 0x100, 0xfffffffffffff000ULL, ~0ULL, c, 0) == CR_SUCCESS && s == 0x2000, "find, 4K aligned");
    check(find(&s, 0, 0x100, ~0ULL, ~0ULL, c, 0) == CR_SUCCESS && s == 0x1100, "find, unaligned");
    check(find(&s, 0, 0x3000, ~0ULL, ~0ULL, c, 0) == CR_FAILURE, "find, too long");
    check(free_list(c, 0) == CR_SUCCESS && free_list(0, 0) == CR_INVALID_RANGE_LIST, "free of a null list is invalid");
    free_list(a, 0); free_list(b, 0);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
