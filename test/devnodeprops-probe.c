/* devnodeprops-probe.c (wine-sg 6086): the device properties Windows derives from a device node --
 * DEVPKEY_Device_InstanceId and DEVPKEY_Device_* (the SPDRP_* registry properties) -- through
 * CM_Get_DevNode_PropertyW and SetupDiGetDevicePropertyW. Prints KEY=VALUE lines. */
#include <initguid.h>
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <stdio.h>

static void say(const char *key, const char *fmt, ...)
{
    va_list args;
    printf("%s=", key);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

int main(void)
{
    HDEVINFO set = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    SP_DEVINFO_DATA dev = { sizeof(dev) };
    WCHAR id[512], value[512], reg[512];
    DEVPROPTYPE type;
    ULONG size;
    DWORD i, found = 0, regtype, need;
    CONFIGRET cr;
    GUID guid;
    BOOL ok;

    /* the first device that has a description and hardware IDs */
    for (i = 0; SetupDiEnumDeviceInfo(set, i, &dev); i++)
    {
        if (SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_DEVICEDESC, &regtype, (BYTE *)reg, sizeof(reg), NULL) &&
            SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_HARDWAREID, &regtype, (BYTE *)value, sizeof(value), NULL))
        {
            found = 1;
            break;
        }
    }
    say("device", "%lu", found);
    if (!found) { say("done", "0"); return 1; }
    CM_Get_Device_IDW(dev.DevInst, id, ARRAYSIZE(id), 0);

    size = sizeof(value);
    cr = CM_Get_DevNode_PropertyW(dev.DevInst, &DEVPKEY_Device_InstanceId, &type, (BYTE *)value, &size, 0);
    say("instance_id", "%lu type %#lx same %d", cr, type, cr == CR_SUCCESS && !wcscmp(value, id));
    say("instance_id_size", "%d", size == (wcslen(id) + 1) * sizeof(WCHAR));

    size = 2;
    cr = CM_Get_DevNode_PropertyW(dev.DevInst, &DEVPKEY_Device_InstanceId, &type, (BYTE *)value, &size, 0);
    say("small_buffer", "%lu needs %d", cr, size == (wcslen(id) + 1) * sizeof(WCHAR));

    size = sizeof(value);
    cr = CM_Get_DevNode_PropertyW(dev.DevInst, &DEVPKEY_Device_DeviceDesc, &type, (BYTE *)value, &size, 0);
    say("desc", "%lu type %#lx same %d", cr, type, cr == CR_SUCCESS && !wcscmp(value, reg));

    size = sizeof(value);
    cr = CM_Get_DevNode_PropertyW(dev.DevInst, &DEVPKEY_Device_HardwareIds, &type, (BYTE *)value, &size, 0);
    SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_HARDWAREID, &regtype, (BYTE *)reg, sizeof(reg), &need);
    say("hwids", "%lu type %#lx same %d", cr, type, cr == CR_SUCCESS && size == need && !memcmp(value, reg, need));

    ok = SetupDiGetDevicePropertyW(set, &dev, &DEVPKEY_Device_ClassGuid, &type, (BYTE *)&guid, sizeof(guid), &need, 0);
    say("classguid", "%d type %#lx size %lu same %d", ok, type, need, ok && IsEqualGUID(&guid, &dev.ClassGuid));

    size = sizeof(value);
    cr = CM_Get_DevNode_PropertyW(dev.DevInst, &DEVPKEY_Device_FriendlyName, &type, (BYTE *)value, &size, 0);
    say("friendly", "%d", cr == CR_SUCCESS || cr == CR_NO_SUCH_VALUE);

    say("done", "1");
    return 0;
}
