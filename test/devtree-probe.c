/* The device tree through the configuration manager (patches/sg/1637).
 * CM_Locate_DevNode, CM_Get_Child, CM_Get_Sibling, CM_Get_Device_ID_List
 * (and _Size), CM_Get_DevNode_Status, CM_Open_DevNode_Key were stubs (the
 * status call returned success and wrote nothing); CM_Get_Depth,
 * CM_Get_Class_Key_Name and CM_Enumerate_Enumerators were not exported;
 * there was no root device node and no idea of a device being present. */
#include <windows.h>
#include <cfgmgr32.h>
#include <stdio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures, walked, consistent = 1, depth_ok = 1, status_ok = 1;
static WCHAR list[65536];

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static BOOL in_list(const WCHAR *id)
{
    const WCHAR *p;
    for (p = list; *p; p += wcslen(p) + 1) if (!_wcsicmp(p, id)) return TRUE;
    return FALSE;
}

static int in_list_count = 0;

static void walk(DEVINST node, ULONG depth)
{
    DEVINST child, parent, next;
    WCHAR id[MAX_DEVICE_ID_LEN];
    ULONG d, status, problem;

    if (CM_Get_Child(&child, node, 0) != CR_SUCCESS) return;
    for (;;)
    {
        walked++;
        CM_Get_Device_IDW(child, id, MAX_DEVICE_ID_LEN, 0);
        if (walked <= 12) printf("  %*s%ls\n", (int)depth * 2, "", id);
        if (CM_Get_Parent(&parent, child, 0) != CR_SUCCESS || parent != node) consistent = 0;
        if (CM_Get_Depth(&d, child, 0) != CR_SUCCESS || d != depth + 1) depth_ok = 0;
        if (CM_Get_DevNode_Status(&status, &problem, child, 0) != CR_SUCCESS || !(status & DN_NT_ENUMERATOR) || !(status & (DN_STARTED | DN_HAS_PROBLEM))) status_ok = 0;
        if (in_list(id)) in_list_count++;
        walk(child, depth + 1);
        if (CM_Get_Sibling(&next, child, 0) != CR_SUCCESS) break;
        child = next;
    }
}

int main(void)
{
    static const GUID display = {0x4d36e968, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};
    DEVINST root, node;
    WCHAR id[MAX_DEVICE_ID_LEN], name[64];
    ULONG len = 0, status = 0xdead, problem = 0xdead;
    CONFIGRET cr;
    HKEY key;
    const WCHAR *p;
    int root_only = 1;
    DWORD disp;

    cr = CM_Locate_DevNodeW(&root, NULL, CM_LOCATE_DEVNODE_NORMAL);
    CM_Get_Device_IDW(root, id, MAX_DEVICE_ID_LEN, 0);
    printf("root %lu %ls\n", root, id);
    check(cr == CR_SUCCESS && !wcscmp(id, L"HTREE\\ROOT\\0"), "CM_Locate_DevNode(NULL): the root, HTREE\\ROOT\\0");
    check(CM_Get_DevNode_Status(&status, &problem, root, 0) == CR_SUCCESS && (status & DN_STARTED) && !problem,
          "the root is started");

    cr = CM_Get_Device_ID_List_SizeW(&len, NULL, CM_GETIDLIST_FILTER_NONE);
    printf("id list: %lu chars\n", len);
    check(cr == CR_SUCCESS && len > 1, "CM_Get_Device_ID_List_Size");
    cr = CM_Get_Device_ID_ListW(NULL, list, ARRAY_SIZE(list), CM_GETIDLIST_FILTER_NONE);
    check(cr == CR_SUCCESS && list[0], "CM_Get_Device_ID_List");
    cr = CM_Get_Device_ID_ListW(L"ROOT", list + 30000, ARRAY_SIZE(list) - 30000, CM_GETIDLIST_FILTER_ENUMERATOR);
    for (p = list + 30000; *p; p += wcslen(p) + 1) if (_wcsnicmp(p, L"ROOT\\", 5)) root_only = 0;
    check(cr == CR_SUCCESS && root_only, "... filtered by enumerator");
    list[30000] = 0;

    walk(root, 0);
    printf("walked %d devices, %d in the id list\n", walked, in_list_count);
    check(walked > 0, "CM_Get_Child / CM_Get_Sibling walk the tree");
    check(consistent, "... each child's CM_Get_Parent is its parent");
    check(depth_ok, "... and CM_Get_Depth its depth");
    check(status_ok, "... and CM_Get_DevNode_Status answers for each");
    check(in_list_count == walked, "every device in the tree is in the id list");

    check(CM_Locate_DevNodeW(&node, (WCHAR *)L"NOSUCH\\DEVICE\\0", 0) == CR_NO_SUCH_DEVNODE,
          "an unknown device: CR_NO_SUCH_DEVNODE");
    if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Enum\\SGBUS\\SGPHANTOM\\0000", 0, NULL, 0,
                         KEY_ALL_ACCESS, NULL, &key, &disp))
    {
        RegSetValueExW(key, L"ClassGUID", 0, REG_SZ, (const BYTE *)L"{4d36e968-e325-11ce-bfc1-08002be10318}", 39 * sizeof(WCHAR));
        RegCloseKey(key);
        check(CM_Locate_DevNodeW(&node, (WCHAR *)L"SGBUS\\SGPHANTOM\\0000", 0) == CR_NO_SUCH_DEVNODE,
              "a device not present is not found normally");
        cr = CM_Locate_DevNodeW(&node, (WCHAR *)L"SGBUS\\SGPHANTOM\\0000", CM_LOCATE_DEVNODE_PHANTOM);
        check(cr == CR_SUCCESS, "... but as a phantom");
        if (cr == CR_SUCCESS)
        {
            cr = CM_Open_DevNode_Key(node, KEY_READ, 0, RegDisposition_OpenAlways, &key, CM_REGISTRY_HARDWARE);
            check(cr == CR_SUCCESS, "CM_Open_DevNode_Key (the hardware key)");
            if (cr == CR_SUCCESS) RegCloseKey(key);
        }
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Enum\\SGBUS");
    }
    else printf("cannot make a phantom device (not an administrator)\n");

    len = ARRAY_SIZE(name);
    cr = CM_Get_Class_Key_NameW((GUID *)&display, name, &len, 0);
    printf("class key %ls\n", name);
    check(cr == CR_SUCCESS && !_wcsicmp(name, L"{4d36e968-e325-11ce-bfc1-08002be10318}") && len == 39,
          "CM_Get_Class_Key_Name");
    len = ARRAY_SIZE(name);
    cr = CM_Enumerate_EnumeratorsW(0, name, &len, 0);
    printf("first enumerator %ls\n", name);
    check(cr == CR_SUCCESS && name[0], "CM_Enumerate_Enumerators");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
