/* Printer change notifications and user forms (patches/sg/1628).
 *
 *  - FindFirstPrinterChangeNotification failed (INVALID_HANDLE_VALUE) and
 *    FindNext/FindClose were stubs: the handle is now signalled when a
 *    form is added, changed or deleted and when a job's spool file
 *    appears or goes, and FindNext says which;
 *  - with notify options, FindNext gives a PRINTER_NOTIFY_INFO that says
 *    "read everything again" (PRINTER_NOTIFY_INFO_DISCARDED);
 *  - AddForm, SetForm and DeleteForm did nothing: user forms are kept,
 *    read by GetForm and EnumForms (FORM_USER), duplicates refused, the
 *    built-in forms left alone.
 */
#include <windows.h>
#include <winspool.h>
#include <stdio.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static DWORD wait_change(HANDLE change, DWORD want)
{
    DWORD got = 0, c, start = GetTickCount();

    while (GetTickCount() - start < 8000 && !(got & want))
    {
        if (WaitForSingleObject(change, 500) != WAIT_OBJECT_0) continue;
        c = 0;
        if (FindNextPrinterChangeNotification(change, &c, NULL, NULL)) got |= c;
    }
    return got;
}

int main(void)
{
    WCHAR name[] = L"SG Probe Form", spool[MAX_PATH];
    PRINTER_NOTIFY_OPTIONS_TYPE type = { JOB_NOTIFY_TYPE, 0, 0, 0, 0, NULL };
    PRINTER_NOTIFY_OPTIONS opts = { 2, 0, 1, &type };
    PRINTER_NOTIFY_INFO *info = NULL;
    FORM_INFO_1W form, *got;
    HANDLE printer = NULL, change, change2, file;
    DWORD needed, count, c, i;
    BYTE buf[65536];
    BOOL ok, found = FALSE;

    ok = OpenPrinterW(NULL, &printer, NULL);
    printf("server handle: %d %p\n", ok, printer);
    check(ok, "OpenPrinter(NULL) gives the print server");
    if (!ok) { printf("RESULT: FAIL\n"); return 1; }
    DeleteFormW(printer, name);

    change = FindFirstPrinterChangeNotification(printer, PRINTER_CHANGE_FORM | PRINTER_CHANGE_JOB, 0, NULL);
    check(change != INVALID_HANDLE_VALUE, "FindFirstPrinterChangeNotification");
    change2 = FindFirstPrinterChangeNotification(printer, 0, 0, &opts);
    check(change2 != INVALID_HANDLE_VALUE, "... with notify options");

    /* forms */
    memset(&form, 0, sizeof(form));
    form.pName = name;
    form.Size.cx = 100000; form.Size.cy = 200000;
    form.ImageableArea.right = 100000; form.ImageableArea.bottom = 200000;
    ok = AddFormW(printer, 1, (BYTE *)&form);
    printf("AddForm: %d err %lu\n", ok, ok ? 0 : GetLastError());
    check(ok, "AddForm");
    c = wait_change(change, PRINTER_CHANGE_ADD_FORM);
    printf("changes after AddForm: %#lx\n", c);
    check(c & PRINTER_CHANGE_ADD_FORM, "... PRINTER_CHANGE_ADD_FORM");
    ok = GetFormW(printer, name, 1, buf, sizeof(buf), &needed);
    got = (FORM_INFO_1W *)buf;
    check(ok && got->Flags == FORM_USER && got->Size.cx == 100000 && got->Size.cy == 200000,
          "GetForm reads the user form");
    ok = EnumFormsW(printer, 1, buf, sizeof(buf), &needed, &count);
    for (i = 0; ok && i < count; i++)
        if (!wcscmp(((FORM_INFO_1W *)buf)[i].pName, name) && ((FORM_INFO_1W *)buf)[i].Flags == FORM_USER) found = TRUE;
    check(found, "EnumForms lists it");
    SetLastError(0xdeadbeef);
    check(!AddFormW(printer, 1, (BYTE *)&form) && GetLastError() == ERROR_FILE_EXISTS, "adding it again: ERROR_FILE_EXISTS");
    form.Size.cx = 150000; form.ImageableArea.right = 150000;
    check(SetFormW(printer, name, 1, (BYTE *)&form), "SetForm");
    c = wait_change(change, PRINTER_CHANGE_SET_FORM);
    check(c & PRINTER_CHANGE_SET_FORM, "... PRINTER_CHANGE_SET_FORM");
    ok = GetFormW(printer, name, 1, buf, sizeof(buf), &needed);
    check(ok && ((FORM_INFO_1W *)buf)->Size.cx == 150000, "... and the new size reads back");
    check(DeleteFormW(printer, name), "DeleteForm");
    c = wait_change(change, PRINTER_CHANGE_DELETE_FORM);
    check(c & PRINTER_CHANGE_DELETE_FORM, "... PRINTER_CHANGE_DELETE_FORM");
    SetLastError(0xdeadbeef);
    check(!GetFormW(printer, name, 1, buf, sizeof(buf), &needed) && GetLastError() == ERROR_INVALID_FORM_NAME,
          "... and it is gone");
    SetLastError(0xdeadbeef);
    check(!DeleteFormW(printer, (WCHAR *)L"Letter"), "a built-in form is not deleted");

    /* a job's spool file */
    GetSystemDirectoryW(spool, MAX_PATH);
    wcscat(spool, L"\\spool\\PRINTERS\\99999.SPL");
    file = CreateFileW(spool, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, "x", 1, &c, NULL);
    CloseHandle(file);
    c = wait_change(change, PRINTER_CHANGE_ADD_JOB);
    printf("changes after a spool file: %#lx\n", c);
    check(c & PRINTER_CHANGE_ADD_JOB, "a job's spool file: PRINTER_CHANGE_ADD_JOB");
    DeleteFileW(spool);
    c = wait_change(change, PRINTER_CHANGE_DELETE_JOB);
    check(c & PRINTER_CHANGE_DELETE_JOB, "... gone: PRINTER_CHANGE_DELETE_JOB");

    /* the notification with options */
    check(WaitForSingleObject(change2, 5000) == WAIT_OBJECT_0, "the notification with options was signalled");
    ok = FindNextPrinterChangeNotification(change2, &c, NULL, (void **)&info);
    check(ok && info && (info->Flags & PRINTER_NOTIFY_INFO_DISCARDED), "... and its info says to read everything again");
    if (info) FreePrinterNotifyInfo(info);

    check(FindClosePrinterChangeNotification(change) && FindClosePrinterChangeNotification(change2),
          "FindClosePrinterChangeNotification");
    SetLastError(0xdeadbeef);
    check(!FindClosePrinterChangeNotification(change), "... a closed one is refused");
    ClosePrinter(printer);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
