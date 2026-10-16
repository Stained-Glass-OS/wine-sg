/* A fake BlueZ on a private bus: one adapter, two devices, every state change logged.
 * fastb-btdev-fake ADDRESS LOGFILE */
#include <dbus/dbus.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#define ADAPTER "/org/bluez/hci0"
#define DEV1 ADAPTER "/dev_AA_BB_CC_DD_EE_01"
#define DEV2 ADAPTER "/dev_11_22_33_44_55_66"
#define A2DP "0000110b-0000-1000-8000-00805f9b34fb"
#define HFP  "0000111e-0000-1000-8000-00805f9b34fb"

static FILE *logf;
static int have_dev1 = 1, have_dev2 = 1, discoverable;
static char alias1[64] = "Fake Headset";

static void logline(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(logf, fmt, ap);
    fputc('\n', logf);
    fflush(logf);
    va_end(ap);
}

static void dict_str(DBusMessageIter *d, const char *key, const char *sig, int type, const void *value)
{
    DBusMessageIter e, v;
    dbus_message_iter_open_container(d, DBUS_TYPE_DICT_ENTRY, NULL, &e);
    dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&e, DBUS_TYPE_VARIANT, sig, &v);
    dbus_message_iter_append_basic(&v, type, value);
    dbus_message_iter_close_container(&e, &v);
    dbus_message_iter_close_container(d, &e);
}

static void dict_strings(DBusMessageIter *d, const char *key, const char **values, int n)
{
    DBusMessageIter e, v, a;
    int i;
    dbus_message_iter_open_container(d, DBUS_TYPE_DICT_ENTRY, NULL, &e);
    dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&e, DBUS_TYPE_VARIANT, "as", &v);
    dbus_message_iter_open_container(&v, DBUS_TYPE_ARRAY, "s", &a);
    for (i = 0; i < n; i++) dbus_message_iter_append_basic(&a, DBUS_TYPE_STRING, &values[i]);
    dbus_message_iter_close_container(&v, &a);
    dbus_message_iter_close_container(&e, &v);
    dbus_message_iter_close_container(d, &e);
}

static void adapter_props(DBusMessageIter *props)
{
    dbus_bool_t t = 1, f = 0, disc = discoverable;
    dbus_uint32_t cls = 0x1c010c;
    const char *addr = "00:11:22:33:44:55", *name = "fake-host";
    dict_str(props, "Address", "s", DBUS_TYPE_STRING, &addr);
    dict_str(props, "Name", "s", DBUS_TYPE_STRING, &name);
    dict_str(props, "Alias", "s", DBUS_TYPE_STRING, &name);
    dict_str(props, "Class", "u", DBUS_TYPE_UINT32, &cls);
    dict_str(props, "Powered", "b", DBUS_TYPE_BOOLEAN, &t);
    dict_str(props, "Discoverable", "b", DBUS_TYPE_BOOLEAN, &disc);
    dict_str(props, "Pairable", "b", DBUS_TYPE_BOOLEAN, &t);
    dict_str(props, "Discovering", "b", DBUS_TYPE_BOOLEAN, &f);
    {
        unsigned char version = 6;  /* Bluetooth 4.0 */
        dict_str(props, "Version", "y", DBUS_TYPE_BYTE, &version);
    }
}

static void device_props(DBusMessageIter *props, int which)
{
    dbus_bool_t t = 1, f = 0;
    const char *adapter = ADAPTER;
    if (which == 1)
    {
        dbus_uint32_t cls = 0x240404;
        const char *addr = "AA:BB:CC:DD:EE:01", *name = "Headset", *alias = alias1, *uuids[2] = { A2DP, HFP };
        dict_str(props, "Adapter", "o", DBUS_TYPE_OBJECT_PATH, &adapter);
        dict_str(props, "Address", "s", DBUS_TYPE_STRING, &addr);
        dict_str(props, "Name", "s", DBUS_TYPE_STRING, &name);
        dict_str(props, "Alias", "s", DBUS_TYPE_STRING, &alias);
        dict_str(props, "Class", "u", DBUS_TYPE_UINT32, &cls);
        dict_str(props, "Paired", "b", DBUS_TYPE_BOOLEAN, &t);
        dict_str(props, "Trusted", "b", DBUS_TYPE_BOOLEAN, &t);
        dict_str(props, "Connected", "b", DBUS_TYPE_BOOLEAN, &t);
        dict_strings(props, "UUIDs", uuids, 2);
    }
    else
    {
        dbus_uint32_t cls = 0x5a020c;
        dbus_int16_t rssi = -60;
        const char *addr = "11:22:33:44:55:66", *name = "Scanned Phone", *uuids[1] = { HFP };
        dict_str(props, "Adapter", "o", DBUS_TYPE_OBJECT_PATH, &adapter);
        dict_str(props, "Address", "s", DBUS_TYPE_STRING, &addr);
        dict_str(props, "Name", "s", DBUS_TYPE_STRING, &name);
        dict_str(props, "Alias", "s", DBUS_TYPE_STRING, &name);
        dict_str(props, "Class", "u", DBUS_TYPE_UINT32, &cls);
        dict_str(props, "Paired", "b", DBUS_TYPE_BOOLEAN, &f);
        dict_str(props, "Connected", "b", DBUS_TYPE_BOOLEAN, &f);
        dict_str(props, "RSSI", "n", DBUS_TYPE_INT16, &rssi);
        dict_strings(props, "UUIDs", uuids, 1);
    }
}

static void object_entry(DBusMessageIter *objects, const char *path, const char *iface, int which)
{
    DBusMessageIter e, ifaces, ie, props;
    dbus_message_iter_open_container(objects, DBUS_TYPE_DICT_ENTRY, NULL, &e);
    dbus_message_iter_append_basic(&e, DBUS_TYPE_OBJECT_PATH, &path);
    dbus_message_iter_open_container(&e, DBUS_TYPE_ARRAY, "{sa{sv}}", &ifaces);
    dbus_message_iter_open_container(&ifaces, DBUS_TYPE_DICT_ENTRY, NULL, &ie);
    dbus_message_iter_append_basic(&ie, DBUS_TYPE_STRING, &iface);
    dbus_message_iter_open_container(&ie, DBUS_TYPE_ARRAY, "{sv}", &props);
    if (which == 0) adapter_props(&props); else device_props(&props, which);
    dbus_message_iter_close_container(&ie, &props);
    dbus_message_iter_close_container(&ifaces, &ie);
    dbus_message_iter_close_container(&e, &ifaces);
    dbus_message_iter_close_container(objects, &e);
}

static void send_empty(DBusConnection *c, DBusMessage *msg)
{
    DBusMessage *r = dbus_message_new_method_return(msg);
    dbus_connection_send(c, r, NULL);
    dbus_message_unref(r);
}

static void send_error(DBusConnection *c, DBusMessage *msg, const char *name, const char *text)
{
    DBusMessage *r = dbus_message_new_error(msg, name, text);
    dbus_connection_send(c, r, NULL);
    dbus_message_unref(r);
}

static DBusHandlerResult handle(DBusConnection *c, DBusMessage *msg, void *data)
{
    const char *iface = dbus_message_get_interface(msg), *member = dbus_message_get_member(msg);
    const char *path = dbus_message_get_path(msg);
    DBusMessage *r;
    DBusMessageIter it;

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL || !member || !path) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (iface && !strcmp(iface, "org.freedesktop.DBus.ObjectManager") && !strcmp(member, "GetManagedObjects"))
    {
        DBusMessageIter objects;
        r = dbus_message_new_method_return(msg);
        dbus_message_iter_init_append(r, &it);
        dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "{oa{sa{sv}}}", &objects);
        object_entry(&objects, ADAPTER, "org.bluez.Adapter1", 0);
        if (have_dev1) object_entry(&objects, DEV1, "org.bluez.Device1", 1);
        if (have_dev2) object_entry(&objects, DEV2, "org.bluez.Device1", 2);
        dbus_message_iter_close_container(&it, &objects);
        dbus_connection_send(c, r, NULL);
        dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (iface && !strcmp(iface, "org.freedesktop.DBus.Properties") && !strcmp(member, "GetAll"))
    {
        DBusMessageIter props;
        r = dbus_message_new_method_return(msg);
        dbus_message_iter_init_append(r, &it);
        dbus_message_iter_open_container(&it, DBUS_TYPE_ARRAY, "{sv}", &props);
        if (!strcmp(path, ADAPTER)) adapter_props(&props);
        dbus_message_iter_close_container(&it, &props);
        dbus_connection_send(c, r, NULL);
        dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (iface && !strcmp(iface, "org.freedesktop.DBus.Properties") && !strcmp(member, "Set"))
    {
        DBusMessageIter var;
        const char *ifn, *name;
        dbus_message_iter_init(msg, &it);
        dbus_message_iter_get_basic(&it, &ifn);
        dbus_message_iter_next(&it);
        dbus_message_iter_get_basic(&it, &name);
        dbus_message_iter_next(&it);
        dbus_message_iter_recurse(&it, &var);
        if (dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_STRING)
        {
            const char *v;
            dbus_message_iter_get_basic(&var, &v);
            logline("Set %s %s=%s", path, name, v);
            if (!strcmp(path, DEV1) && !strcmp(name, "Alias")) snprintf(alias1, sizeof(alias1), "%s", v);
        }
        else
        {
            dbus_bool_t v;
            dbus_message_iter_get_basic(&var, &v);
            logline("Set %s %s=%d", path, name, v);
            if (!strcmp(name, "Discoverable")) discoverable = v;
        }
        send_empty(c, msg);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (!strcmp(member, "RemoveDevice"))
    {
        const char *dev;
        dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &dev, DBUS_TYPE_INVALID);
        logline("RemoveDevice %s", dev);
        if (!strcmp(dev, DEV1) && have_dev1) have_dev1 = 0;
        else if (!strcmp(dev, DEV2) && have_dev2) have_dev2 = 0;
        else { send_error(c, msg, "org.bluez.Error.DoesNotExist", "Does Not Exist"); return DBUS_HANDLER_RESULT_HANDLED; }
        send_empty(c, msg);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (!strcmp(member, "ConnectProfile") || !strcmp(member, "DisconnectProfile"))
    {
        const char *uuid;
        dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &uuid, DBUS_TYPE_INVALID);
        logline("%s %s %s", member, path, uuid);
        send_empty(c, msg);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int main(int argc, char **argv)
{
    DBusError err;
    DBusConnection *c;

    if (argc < 3) return 2;
    logf = fopen(argv[2], "a");
    dbus_error_init(&err);
    c = dbus_connection_open(argv[1], &err);
    if (!c || !dbus_bus_register(c, &err)) { fprintf(stderr, "no bus: %s\n", err.message); return 1; }
    if (dbus_bus_request_name(c, "org.bluez", 0, &err) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) { fprintf(stderr, "no name\n"); return 1; }
    dbus_connection_add_filter(c, handle, NULL, NULL);
    printf("ready\n");
    fflush(stdout);
    while (dbus_connection_read_write_dispatch(c, 200)) {}
    return 0;
}
