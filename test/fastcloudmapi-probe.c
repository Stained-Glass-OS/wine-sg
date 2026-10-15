#include <windows.h>
#include <mapi.h>
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define LPSTR_ char *
typedef ULONG (WINAPI *save_t)(LHANDLE, ULONG_PTR, lpMapiMessage, FLAGS, ULONG, LPSTR);
typedef ULONG (WINAPI *read_t)(LHANDLE, ULONG_PTR, LPSTR, FLAGS, ULONG, lpMapiMessage *);
typedef ULONG (WINAPI *find_t)(LHANDLE, ULONG_PTR, LPSTR, LPSTR, FLAGS, ULONG, LPSTR);
typedef ULONG (WINAPI *del_t)(LHANDLE, ULONG_PTR, LPSTR, FLAGS, ULONG);
typedef ULONG (WINAPI *logon_t)(ULONG_PTR, LPSTR, LPSTR, FLAGS, ULONG, LPLHANDLE);
typedef ULONG (WINAPI *free_t)(void *);
static char *slurp(const char *p, DWORD *n)
{
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    static char buf[256];
    *n = 0;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    ReadFile(h, buf, sizeof(buf) - 1, n, NULL); buf[*n] = 0; CloseHandle(h); return buf;
}
int main(void)
{
    HMODULE m = LoadLibraryA("mapi32.dll");
    save_t save; read_t rd; find_t find; del_t del; logon_t logon; free_t fr;
    LHANDLE s = 0;
    char id1[64] = "", id2[64] = "", out[64], attach[MAX_PATH], tmpdir[MAX_PATH];
    MapiRecipDesc rec[2] = {{0, MAPI_TO, "Alice", "SMTP:alice@example.com"}, {0, MAPI_CC, "Bob", "SMTP:bob@example.com"}}, orig = {0, MAPI_ORIG, "Me", "SMTP:me@example.com"};
    MapiFileDesc file = {0};
    MapiMessage msg = {0};
    lpMapiMessage r;
    DWORD n; HANDLE h; ULONG ret;

    save = (save_t)GetProcAddress(m, "MAPISaveMail"); rd = (read_t)GetProcAddress(m, "MAPIReadMail");
    find = (find_t)GetProcAddress(m, "MAPIFindNext"); del = (del_t)GetProcAddress(m, "MAPIDeleteMail");
    logon = (logon_t)GetProcAddress(m, "MAPILogon"); fr = (free_t)GetProcAddress(m, "MAPIFreeBuffer");
    CHECK(logon(0, NULL, NULL, 0, 0, &s) == 0 && s, "logon");

    GetTempPathA(sizeof(tmpdir), tmpdir); GetTempFileNameA(tmpdir, "att", 0, attach);
    h = CreateFileA(attach, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); WriteFile(h, "ATTACHED", 8, &n, NULL); CloseHandle(h);
    file.lpszPathName = attach; file.lpszFileName = "doc.txt"; file.nPosition = 3;
    msg.lpszSubject = "Hello"; msg.lpszNoteText = "Body text"; msg.lpOriginator = &orig;
    msg.nRecipCount = 2; msg.lpRecips = rec; msg.nFileCount = 1; msg.lpFiles = &file; msg.flFlags = 1; /* unread */

    CHECK(find(s, 0, NULL, "", 0, 0, out) == MAPI_E_NO_MESSAGES, "empty store find");
    CHECK(rd(s, 0, "00000001", 0, 0, &r) == MAPI_E_INVALID_MESSAGE, "read in empty store");
    CHECK(save(0, 0, &msg, 0, 0, id1) == MAPI_E_INVALID_SESSION, "save bad session");
    ret = save(s, 0, &msg, 0, 0, id1); CHECK(ret == 0 && id1[0], "save %lu", ret);
    DeleteFileA(attach); /* the message keeps its own copy */
    msg.lpszSubject = "Second"; msg.nFileCount = 0; msg.flFlags = 0;
    ret = save(s, 0, &msg, 0, 0, id2); CHECK(ret == 0 && strcmp(id1, id2), "save 2 %lu %s %s", ret, id1, id2);

    CHECK(find(s, 0, NULL, NULL, 0, 0, out) == 0 && !strcmp(out, id1), "first %s", out);
    CHECK(find(s, 0, "", id1, 0, 0, out) == 0 && !strcmp(out, id2), "next %s", out);
    CHECK(find(s, 0, NULL, id2, 0, 0, out) == MAPI_E_NO_MESSAGES, "after last");
    CHECK(find(s, 0, NULL, "FFFF", 0, 0, out) == MAPI_E_INVALID_MESSAGE, "bad seed");
    CHECK(find(s, 0, "IPC.Other", NULL, 0, 0, out) == MAPI_E_NO_MESSAGES, "other class");
    CHECK(find(s, 0, NULL, NULL, MAPI_UNREAD_ONLY, 0, out) == 0 && !strcmp(out, id1), "unread only");
    CHECK(find(s, 0, NULL, id1, MAPI_UNREAD_ONLY, 0, out) == MAPI_E_NO_MESSAGES, "no other unread");

    ret = rd(s, 0, id1, MAPI_PEEK, 0, &r);
    CHECK(ret == 0 && r, "peek read %lu", ret);
    if (ret == 0)
    {
        CHECK(!strcmp(r->lpszSubject, "Hello") && !strcmp(r->lpszNoteText, "Body text"), "subject/body");
        CHECK(r->nRecipCount == 2 && !strcmp(r->lpRecips[1].lpszName, "Bob") && r->lpRecips[1].ulRecipClass == MAPI_CC, "recips");
        CHECK(r->lpOriginator && !strcmp(r->lpOriginator->lpszAddress, "SMTP:me@example.com"), "originator");
        CHECK(r->nFileCount == 1 && !strcmp(r->lpFiles[0].lpszFileName, "doc.txt") && r->lpFiles[0].nPosition == 3, "file desc");
        CHECK(r->nFileCount == 1 && slurp(r->lpFiles[0].lpszPathName, &n) && n == 8 && !memcmp(slurp(r->lpFiles[0].lpszPathName, &n), "ATTACHED", 8), "attachment content");
        if (r->nFileCount) DeleteFileA(r->lpFiles[0].lpszPathName);
        fr(r);
    }
    CHECK(find(s, 0, NULL, NULL, MAPI_UNREAD_ONLY, 0, out) == 0, "still unread after peek");
    ret = rd(s, 0, id1, MAPI_ENVELOPE_ONLY, 0, &r);
    CHECK(ret == 0 && !r->lpszNoteText && r->nFileCount == 0 && !strcmp(r->lpszSubject, "Hello"), "envelope only");
    if (ret == 0) fr(r);
    ret = rd(s, 0, id1, MAPI_BODY_AS_FILE, 0, &r);
    CHECK(ret == 0 && r->nFileCount == 2 && !r->lpszNoteText && !strcmp(r->lpFiles[1].lpszFileName, "doc.txt"), "body as file count");
    if (ret == 0)
    {
        char *c = slurp(r->lpFiles[0].lpszPathName, &n);
        CHECK(c && n == 9 && !memcmp(c, "Body text", 9), "body file content");
        DeleteFileA(r->lpFiles[0].lpszPathName);
        DeleteFileA(r->lpFiles[1].lpszPathName);
        fr(r);
    }
    ret = rd(s, 0, id1, MAPI_SUPPRESS_ATTACH, 0, &r);
    CHECK(ret == 0 && r->nFileCount == 0 && !strcmp(r->lpszNoteText, "Body text"), "suppress attach");
    if (ret == 0) fr(r);
    CHECK(find(s, 0, NULL, NULL, MAPI_UNREAD_ONLY, 0, out) == MAPI_E_NO_MESSAGES, "read marks as read");

    msg.lpszSubject = "Replaced";
    CHECK(save(s, 0, &msg, 0, 0, id1) == 0, "overwrite");
    ret = rd(s, 0, id1, MAPI_PEEK, 0, &r); CHECK(ret == 0 && !strcmp(r->lpszSubject, "Replaced") && r->nFileCount == 0, "overwritten content");
    if (ret == 0) fr(r);
    CHECK(del(s, 0, id1, 0, 0) == 0, "delete");
    CHECK(del(s, 0, id1, 0, 0) == MAPI_E_INVALID_MESSAGE, "delete twice");
    CHECK(rd(s, 0, id1, 0, 0, &r) == MAPI_E_INVALID_MESSAGE, "read deleted");
    CHECK(rd(0, 0, id2, 0, 0, &r) == MAPI_E_INVALID_SESSION, "read bad session");
    CHECK(find(s, 0, NULL, NULL, 0, 0, out) == 0 && !strcmp(out, id2), "remaining message");
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
