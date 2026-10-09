/* Configuration manager notifications and service device events
 * (patches/sg/1698), run by test/cmnotify-gate.sh with the plug and play
 * client test/cmnotify-plugplay.idl makes. CM_Register_Notification
 * returned CR_CALL_NOT_IMPLEMENTED and CM_Unregister_Notification was not
 * exported; RegisterDeviceNotification with DEVICE_NOTIFY_SERVICE_HANDLE
 * accepted a service and never told it anything (a FIXME). The probe sends
 * what Wine's PnP manager sends when an interface arrives or goes or a
 * driver reports a custom event, through Wine's plug and play service.
 *
 *   cmnotify-probe.exe [service] */
#include <stddef.h>
#include <windows.h>
#include <winternl.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <dbt.h>
#include <stdio.h>
#include "pp.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

void __RPC_FAR * __RPC_USER MIDL_user_allocate(size_t len) { return HeapAlloc(GetProcessHeap(), 0, len); }
void __RPC_USER MIDL_user_free(void __RPC_FAR *ptr) { HeapFree(GetProcessHeap(), 0, ptr); }

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const GUID class_x = { 0x5a9e1001, 0x5347, 0x4f53, { 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x98 } };
static const GUID class_y = { 0x5a9e1002, 0x5347, 0x4f53, { 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x98 } };
static const GUID custom = { 0x5a9e10ee, 0x5347, 0x4f53, { 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x98 } };
static const GUID setup_class = { 0x4d36e97d, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };

typedef CONFIGRET (WINAPI *register_fn)(CM_NOTIFY_FILTER *, void *, PCM_NOTIFY_CALLBACK, HCMNOTIFICATION *);
typedef CONFIGRET (WINAPI *unregister_fn)(HCMNOTIFICATION);
static register_fn pRegister;
static unregister_fn pUnregister;

#define MAX_LOG 16
struct log
{
    CRITICAL_SECTION cs;
    HANDLE event;
    int count;
    CM_NOTIFY_ACTION action[MAX_LOG];
    GUID guid[MAX_LOG];
    WCHAR text[MAX_LOG][256];
    DWORD data_size[MAX_LOG];
    BYTE data[MAX_LOG][16];
    HCMNOTIFICATION self;
    int unregister_self;
};

static DWORD WINAPI callback(HCMNOTIFICATION notify, void *context, CM_NOTIFY_ACTION action,
                             CM_NOTIFY_EVENT_DATA *data, DWORD size)
{
    struct log *log = context;
    int i;

    EnterCriticalSection(&log->cs);
    i = log->count++;
    if (i < MAX_LOG)
    {
        log->action[i] = action;
        if (data->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE)
        {
            log->guid[i] = data->u.DeviceInterface.ClassGuid;
            lstrcpynW(log->text[i], data->u.DeviceInterface.SymbolicLink, 256);
        }
        else if (data->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE)
            lstrcpynW(log->text[i], data->u.DeviceInstance.InstanceId, 256);
        else if (data->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE && action == CM_NOTIFY_ACTION_DEVICECUSTOMEVENT)
        {
            log->guid[i] = data->u.DeviceHandle.EventGuid;
            log->data_size[i] = data->u.DeviceHandle.DataSize;
            memcpy(log->data[i], data->u.DeviceHandle.Data, min(data->u.DeviceHandle.DataSize, 16));
        }
    }
    LeaveCriticalSection(&log->cs);
    if (log->unregister_self) pUnregister(notify);
    SetEvent(log->event);
    return ERROR_SUCCESS;
}

static void init_log(struct log *log)
{
    memset(log, 0, sizeof(*log));
    InitializeCriticalSection(&log->cs);
    log->event = CreateEventW(NULL, FALSE, FALSE, NULL);
}

static BOOL wait_count(struct log *log, int count)
{
    DWORD end = GetTickCount() + 5000;
    while (log->count < count && (int)(end - GetTickCount()) > 0) WaitForSingleObject(log->event, 200);
    return log->count >= count;
}

static void send_event(const WCHAR *path, DWORD code, const void *data, unsigned int size)
{
    /* (mingw has no __try: an RPC failure ends the probe) */
    plugplay_send_event(path, code, data, size);
}

static void send_iface(DWORD code, const GUID *class, const WCHAR *path)
{
    BYTE buf[1024];
    DEV_BROADCAST_DEVICEINTERFACE_W *iface = (void *)buf;

    memset(buf, 0, sizeof(buf));
    iface->dbcc_size = offsetof(DEV_BROADCAST_DEVICEINTERFACE_W, dbcc_name[wcslen(path) + 1]);
    iface->dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    iface->dbcc_classguid = *class;
    lstrcpyW(iface->dbcc_name, path);
    send_event(L"", code, buf, iface->dbcc_size);
}

static HCMNOTIFICATION reg(CM_NOTIFY_FILTER_TYPE type, DWORD flags, const void *what, struct log *log)
{
    CM_NOTIFY_FILTER filter;
    HCMNOTIFICATION notify = NULL;
    CONFIGRET cr;

    memset(&filter, 0, sizeof(filter));
    filter.cbSize = sizeof(filter);
    filter.Flags = flags;
    filter.FilterType = type;
    if (type == CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE && what) filter.u.DeviceInterface.ClassGuid = *(const GUID *)what;
    if (type == CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE) filter.u.DeviceHandle.hTarget = (HANDLE)what;
    if (type == CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE && what) lstrcpyW(filter.u.DeviceInstance.InstanceId, what);
    cr = pRegister(&filter, log, callback, &notify);
    if (cr) printf("      register %d: cr %#lx\n", type, cr);
    return cr ? NULL : notify;
}

/* the service side: RegisterDeviceNotification with its status handle */
static SERVICE_STATUS_HANDLE status_handle;
static SERVICE_STATUS status;
static HANDLE stop_event;

static DWORD WINAPI service_handler(DWORD control, DWORD type, void *data, void *context)
{
    if (control == SERVICE_CONTROL_DEVICEEVENT && type == DBT_DEVICEARRIVAL && data
            && ((DEV_BROADCAST_HDR *)data)->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE
            && IsEqualGUID(&((DEV_BROADCAST_DEVICEINTERFACE_W *)data)->dbcc_classguid, &class_x))
    {
        HANDLE got = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\SgCmNotifyServiceGot");
        if (got) { SetEvent(got); CloseHandle(got); }
        return NO_ERROR;
    }
    if (control == SERVICE_CONTROL_STOP)
    {
        status.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(status_handle, &status);
        SetEvent(stop_event);
        return NO_ERROR;
    }
    return control == SERVICE_CONTROL_INTERROGATE ? NO_ERROR : ERROR_CALL_NOT_IMPLEMENTED;
}

static void WINAPI service_main(DWORD argc, WCHAR **argv)
{
    DEV_BROADCAST_DEVICEINTERFACE_W filter = { 0 };
    HDEVNOTIFY devnotify;
    HANDLE ready;

    stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    status_handle = RegisterServiceCtrlHandlerExW(L"SgCmNotifyProbe", service_handler, NULL);
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = SERVICE_RUNNING;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    SetServiceStatus(status_handle, &status);
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_classguid = class_x;
    devnotify = RegisterDeviceNotificationW(status_handle, &filter, DEVICE_NOTIFY_SERVICE_HANDLE);
    if ((ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Global\\SgCmNotifyServiceReady")))
    {
        if (devnotify) SetEvent(ready);
        CloseHandle(ready);
    }
    WaitForSingleObject(stop_event, 30000);
    UnregisterDeviceNotification(devnotify);
    status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(status_handle, &status);
}

static void test_service(void)
{
    WCHAR cmd[MAX_PATH + 16];
    SC_HANDLE scm, service;
    SERVICE_STATUS st;
    HANDLE ready = CreateEventW(NULL, TRUE, FALSE, L"Global\\SgCmNotifyServiceReady");
    HANDLE got = CreateEventW(NULL, TRUE, FALSE, L"Global\\SgCmNotifyServiceGot");
    int i;

    cmd[0] = '"';
    GetModuleFileNameW(NULL, cmd + 1, MAX_PATH);
    lstrcatW(cmd, L"\" service");
    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    service = CreateServiceW(scm, L"SgCmNotifyProbe", L"SgCmNotifyProbe", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                             SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, cmd, NULL, NULL, NULL, NULL, NULL);
    if (!service) service = OpenServiceW(scm, L"SgCmNotifyProbe", SERVICE_ALL_ACCESS);
    check(service && StartServiceW(service, 0, NULL), "a service started");
    check(WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0, "it registered for device events with its status handle");
    send_iface(DBT_DEVICEARRIVAL, &class_x, L"\\\\?\\ROOT#SGSERVICEPROBE#0000#{5a9e1001-5347-4f53-9100-000000001698}");
    check(WaitForSingleObject(got, 5000) == WAIT_OBJECT_0,
          "the service's HandlerEx gets SERVICE_CONTROL_DEVICEEVENT (was a FIXME)");
    if (service)
    {
        ControlService(service, SERVICE_CONTROL_STOP, &st);
        for (i = 0; i < 50 && QueryServiceStatus(service, &st) && st.dwCurrentState != SERVICE_STOPPED; i++) Sleep(100);
        DeleteService(service);
        CloseServiceHandle(service);
    }
    CloseServiceHandle(scm);
}

int main(int argc, char **argv)
{
    HMODULE cfg = LoadLibraryA("cfgmgr32.dll");
    SP_DEVINFO_DATA dev = { sizeof(dev) };
    SP_DEVICE_INTERFACE_DATA iface = { sizeof(iface) };
    struct log l_iface, l_all, l_inst, l_inst_all, l_handle, l_self;
    HCMNOTIFICATION n_iface, n_all, n_inst, n_inst_all, n_handle, n_self, dummy;
    WCHAR id[MAX_DEVICE_ID_LEN], path[512], other_path[256], nul_name[512];
    BYTE detail_buf[2048], handle_buf[256];
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail = (void *)detail_buf;
    CM_NOTIFY_FILTER filter;
    RPC_WSTR binding;
    HDEVINFO set;
    HANDLE nul;
    ULONG len;
    int i;

    if (argc > 1 && !strcmp(argv[1], "service"))
    {
        SERVICE_TABLE_ENTRYW table[] = { { (WCHAR *)L"SgCmNotifyProbe", service_main }, { NULL, NULL } };
        StartServiceCtrlDispatcherW(table);
        return 0;
    }

    pRegister = (void *)GetProcAddress(cfg, "CM_Register_Notification");
    pUnregister = (void *)GetProcAddress(cfg, "CM_Unregister_Notification");
    check(pRegister && pUnregister, "CM_Register_Notification and CM_Unregister_Notification are there");
    if (!pRegister || !pUnregister) goto done;

    RpcStringBindingComposeW(NULL, (RPC_WSTR)L"ncacn_np", NULL, (RPC_WSTR)L"\\pipe\\wine_plugplay", NULL, &binding);
    RpcBindingFromStringBindingW(binding, &plugplay_binding_handle);

    /* what is refused */
    init_log(&l_iface);
    memset(&filter, 0, sizeof(filter));
    filter.cbSize = sizeof(filter);
    filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    check(pRegister(NULL, NULL, callback, &dummy) != CR_SUCCESS, "no filter: refused");
    check(pRegister(&filter, NULL, NULL, &dummy) != CR_SUCCESS, "no callback: refused");
    filter.cbSize = 8;
    check(pRegister(&filter, NULL, callback, &dummy) == CR_INVALID_DATA, "a wrong size: CR_INVALID_DATA");
    filter.cbSize = sizeof(filter);
    filter.Flags = 0x100;
    check(pRegister(&filter, NULL, callback, &dummy) == CR_INVALID_FLAG, "unknown flags: CR_INVALID_FLAG");
    filter.Flags = 0;
    filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE;
    check(pRegister(&filter, NULL, callback, &dummy) != CR_SUCCESS, "no instance and not all of them: refused");
    filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE;
    check(pRegister(&filter, NULL, callback, &dummy) != CR_SUCCESS, "no handle: refused");
    filter.FilterType = CM_NOTIFY_FILTER_TYPE_MAX;
    check(pRegister(&filter, NULL, callback, &dummy) != CR_SUCCESS, "no such filter type: refused");

    /* a device with an interface for the instance's properties */
    set = SetupDiCreateDeviceInfoList(&setup_class, NULL);
    if (!SetupDiCreateDeviceInfoW(set, L"SGNOTIFYPROBE", &setup_class, NULL, NULL, DICD_GENERATE_ID, &dev)
            || !SetupDiRegisterDeviceInfo(set, &dev, 0, NULL, NULL, NULL)
            || !SetupDiCreateDeviceInterfaceW(set, &dev, &class_x, NULL, 0, &iface))
    {
        check(0, "a root device for the probe");
        goto done;
    }
    SetupDiGetDeviceInstanceIdW(set, &dev, id, ARRAY_SIZE(id), NULL);
    detail->cbSize = sizeof(*detail);
    SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, sizeof(detail_buf), NULL, NULL);
    lstrcpyW(path, detail->DevicePath);
    lstrcpyW(other_path, L"\\\\?\\ROOT#SGOTHER#0000#{5a9e1002-5347-4f53-9100-000000001698}");
    printf("  %ls\n", path);

    nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    {
        BYTE name_buf[1024];
        OBJECT_NAME_INFORMATION *info = (void *)name_buf;
        ULONG ret_len;
        nul_name[0] = 0;
        if (!NtQueryObject(nul, ObjectNameInformation, info, sizeof(name_buf) - 2, &ret_len))
        {
            memcpy(nul_name, info->Name.Buffer, info->Name.Length);
            nul_name[info->Name.Length / sizeof(WCHAR)] = 0;
        }
    }

    init_log(&l_all);
    init_log(&l_inst);
    init_log(&l_inst_all);
    init_log(&l_handle);
    init_log(&l_self);
    n_iface = reg(CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE, 0, &class_x, &l_iface);
    n_all = reg(CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE, CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES, NULL, &l_all);
    n_inst = reg(CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE, 0, id, &l_inst);
    n_inst_all = reg(CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE, CM_NOTIFY_FILTER_FLAG_ALL_DEVICE_INSTANCES, NULL, &l_inst_all);
    n_handle = reg(CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE, 0, nul, &l_handle);
    l_self.unregister_self = 1;
    n_self = reg(CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE, CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES, NULL, &l_self);
    check(n_iface && n_all && n_inst && n_inst_all && n_handle && n_self,
          "registered: an interface class, all of them, an instance, all of them, a handle (were not implemented)");

    /* an interface of the device arrives */
    send_iface(DBT_DEVICEARRIVAL, &class_x, path);
    check(wait_count(&l_iface, 1) && l_iface.action[0] == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL
          && IsEqualGUID(&l_iface.guid[0], &class_x) && !_wcsicmp(l_iface.text[0], path),
          "the class's registration: DEVICEINTERFACEARRIVAL with the class and path");
    check(wait_count(&l_all, 1) && l_all.action[0] == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL, "all classes': it too");
    check(wait_count(&l_inst, 2) && l_inst.action[0] == CM_NOTIFY_ACTION_DEVICEINSTANCEENUMERATED
          && l_inst.action[1] == CM_NOTIFY_ACTION_DEVICEINSTANCESTARTED && !_wcsicmp(l_inst.text[1], id),
          "the instance's: DEVICEINSTANCEENUMERATED, DEVICEINSTANCESTARTED with its id");
    check(wait_count(&l_inst_all, 2), "all instances': them too");
    check(wait_count(&l_self, 1), "one that unregisters from its own callback: called, no hang");

    /* another class's interface of another device */
    send_iface(DBT_DEVICEARRIVAL, &class_y, other_path);
    check(wait_count(&l_all, 2) && IsEqualGUID(&l_all.guid[1], &class_y), "all classes': another class");
    Sleep(500);
    check(l_iface.count == 1, "the class's: not another class");
    check(l_inst.count == 2, "the instance's: not another device");
    check(l_self.count == 1, "unregistered from its callback: nothing more");

    /* a custom event for the handle */
    {
        DEV_BROADCAST_HANDLE *h = (void *)handle_buf;
        memset(handle_buf, 0, sizeof(handle_buf));
        h->dbch_size = offsetof(DEV_BROADCAST_HANDLE, dbch_data[4]);
        h->dbch_devicetype = DBT_DEVTYP_HANDLE;
        h->dbch_eventguid = custom;
        h->dbch_nameoffset = -1;
        memcpy(h->dbch_data, "SG98", 4);
        send_event(nul_name, DBT_CUSTOMEVENT, handle_buf, h->dbch_size);
    }
    check(wait_count(&l_handle, 1) && l_handle.action[0] == CM_NOTIFY_ACTION_DEVICECUSTOMEVENT
          && IsEqualGUID(&l_handle.guid[0], &custom) && l_handle.data_size[0] == 4 && !memcmp(l_handle.data[0], "SG98", 4),
          "the handle's: DEVICECUSTOMEVENT with its GUID and data");

    /* unregistered: no more; the device goes */
    check(pUnregister(n_iface) == CR_SUCCESS, "CM_Unregister_Notification");
    send_iface(DBT_DEVICEREMOVECOMPLETE, &class_x, path);
    check(wait_count(&l_inst, 3) && l_inst.action[2] == CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED,
          "its last interface gone: DEVICEINSTANCEREMOVED");
    check(wait_count(&l_all, 3) && l_all.action[2] == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL, "all classes': DEVICEINTERFACEREMOVAL");
    Sleep(300);
    check(l_iface.count == 1, "unregistered: no callback");
    check(pUnregister(n_iface) != CR_SUCCESS, "unregistering twice: refused");

    pUnregister(n_all);
    pUnregister(n_inst);
    pUnregister(n_inst_all);
    pUnregister(n_handle);
    CloseHandle(nul);
    SetupDiRemoveDevice(set, &dev);
    SetupDiDestroyDeviceInfoList(set);

    test_service();
    (void)i; (void)len;
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
