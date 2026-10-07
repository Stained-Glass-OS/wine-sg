/* uiaview-probe (test/uiaview-gate.sh, wine-sg 1472): walk UI Automation's
 * control view from the desktop, DEPTH levels deep (argv[1]), printing each
 * element's name, class and process, indented by depth; "reverse" (argv[2]
 * or argv[3]) walks last child / previous sibling; argv[2] "skip:CLASS" walks a view of its own
 * that leaves out elements of class CLASS (Not of a class name condition),
 * their children seen through. A watchdog says where a walk stalls. */
#define COBJMACROS
#include <windows.h>
#include <uiautomation.h>
#include <stdio.h>
#include <string.h>

static volatile LONG step;
static int reverse;
static char where[512];

static DWORD WINAPI watchdog(void *arg)
{
    LONG last = -1;
    for (;;)
    {
        Sleep(10000);
        if (step == last) { printf("STALL at step %ld: %s\n", step, where); fflush(stdout); ExitProcess(3); }
        last = step;
    }
}

static void show(IUIAutomationElement *e, int depth)
{
    BSTR name = NULL, cls = NULL;
    UIA_HWND hwnd = 0;
    int pid = 0;
    IUIAutomationElement_get_CurrentName(e, &name);
    IUIAutomationElement_get_CurrentClassName(e, &cls);
    IUIAutomationElement_get_CurrentNativeWindowHandle(e, &hwnd);
    IUIAutomationElement_get_CurrentProcessId(e, &pid);
    printf("%*s[%ls] class=%ls hwnd=%p pid=%d\n", depth * 2, "", name ? name : L"", cls ? cls : L"", (void *)hwnd, pid);
    fflush(stdout);
    SysFreeString(name);
    SysFreeString(cls);
}

static void walk(IUIAutomationTreeWalker *w, IUIAutomationElement *parent, int depth, int maxdepth)
{
    IUIAutomationElement *c = NULL, *next;
    HRESULT hr;

    snprintf(where, sizeof(where), "first child at depth %d", depth);
    InterlockedIncrement(&step);
    hr = reverse ? IUIAutomationTreeWalker_GetLastChildElement(w, parent, &c)
                 : IUIAutomationTreeWalker_GetFirstChildElement(w, parent, &c);
    while (SUCCEEDED(hr) && c)
    {
        snprintf(where, sizeof(where), "show at depth %d", depth);
        InterlockedIncrement(&step);
        show(c, depth);
        if (depth < maxdepth) walk(w, c, depth + 1, maxdepth);
        snprintf(where, sizeof(where), "next sibling at depth %d", depth);
        InterlockedIncrement(&step);
        next = NULL;
        hr = reverse ? IUIAutomationTreeWalker_GetPreviousSiblingElement(w, c, &next)
                     : IUIAutomationTreeWalker_GetNextSiblingElement(w, c, &next);
        IUIAutomationElement_Release(c);
        c = next;
    }
    if (FAILED(hr)) printf("%*s(hr %08lx)\n", depth * 2, "", hr);
}

int main(int argc, char **argv)
{
    IUIAutomation *uia;
    IUIAutomationElement *root;
    IUIAutomationTreeWalker *w;
    HRESULT hr;
    int maxdepth = argc > 1 ? atoi(argv[1]) : 1;

    reverse = (argc > 2 && !strcmp(argv[2], "reverse")) || (argc > 3 && !strcmp(argv[3], "reverse"));

    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)&uia);
    printf("create %08lx\n", hr);
    if (FAILED(hr)) return 1;
    hr = IUIAutomation_GetRootElement(uia, &root);
    printf("root %08lx\n", hr);
    if (FAILED(hr)) return 1;
    show(root, 0);
    if (argc > 2 && !strncmp(argv[2], "skip:", 5))
    {
        IUIAutomationCondition *is_class, *not_class;
        VARIANT v;
        WCHAR cls[128];

        MultiByteToWideChar(CP_ACP, 0, argv[2] + 5, -1, cls, ARRAYSIZE(cls));
        V_VT(&v) = VT_BSTR;
        V_BSTR(&v) = SysAllocString(cls);
        IUIAutomation_CreatePropertyCondition(uia, UIA_ClassNamePropertyId, v, &is_class);
        hr = IUIAutomation_CreateNotCondition(uia, is_class, &not_class);
        printf("view %08lx\n", hr);
        if (FAILED(hr)) return 1;
        IUIAutomation_CreateTreeWalker(uia, not_class, &w);
    }
    else IUIAutomation_get_ControlViewWalker(uia, &w);
    walk(w, root, 1, maxdepth);
    printf("done\n");
    return 0;
}
