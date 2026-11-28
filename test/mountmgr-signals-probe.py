#!/usr/bin/env python3
# test/mountmgr-signals-gate.sh's bus side, on the gate's private bus only:
#   udisks FLAG M    own org.freedesktop.UDisks2 with an empty GetManagedObjects
#                    (mountmgr then keeps its UDisks2 rules); when FLAG appears,
#                    send M PropertiesChanged of a block device and touch FLAG.done
#   flood N          N PropertiesChanged from another name (a systemd unit path)
#   stats PID...     the messages queued for these PIDs' connections, summed
#   has PID...       exit 0 when one of these PIDs is on the bus
import sys, os, dbus, dbus.service
from gi.repository import GLib
from dbus.mainloop.glib import DBusGMainLoop

def bus():
    return dbus.bus.BusConnection(os.environ["DBUS_SYSTEM_BUS_ADDRESS"])

PROPS = "org.freedesktop.DBus.Properties"

def changed(conn, path, n):
    for i in range(n):
        m = dbus.lowlevel.SignalMessage(path, PROPS, "PropertiesChanged")
        m.append("org.freedesktop.systemd1.Unit", dbus.Dictionary({"ActiveState": dbus.String("active" if i % 2 else "inactive")}, signature="sv"),
                 dbus.Array([], signature="s"), signature="sa{sv}as")
        conn.send_message(m)
    conn.flush()

class ObjMgr(dbus.service.Object):
    @dbus.service.method("org.freedesktop.DBus.ObjectManager", out_signature="a{oa{sa{sv}}}")
    def GetManagedObjects(self):
        return dbus.Dictionary({}, signature="oa{sa{sv}}")

cmd = sys.argv[1]
if cmd == "udisks":
    DBusGMainLoop(set_as_default=True)
    c = bus()
    name = dbus.service.BusName("org.freedesktop.UDisks2", c)
    ObjMgr(c, "/org/freedesktop/UDisks2")
    flag = sys.argv[2]          # touch this file to make the UDisks2 side flood
    m = int(sys.argv[3])
    def tick():
        if os.path.exists(flag):
            os.unlink(flag)
            changed(c, "/org/freedesktop/UDisks2/block_devices/sgfake", m)
            open(flag + ".done", "w").close()
        return True
    GLib.timeout_add(100, tick)
    GLib.MainLoop().run()
elif cmd == "flood":
    c = bus()
    changed(c, "/org/freedesktop/systemd1/unit/sg_2dfake_2eservice", int(sys.argv[2]))
elif cmd in ("stats", "has"):
    c = bus()
    d = c.get_object("org.freedesktop.DBus", "/org/freedesktop/DBus")
    pids = {int(p) for p in sys.argv[2:]}
    total = 0
    for n in dbus.Interface(d, "org.freedesktop.DBus").ListNames():
        if not n.startswith(":"): continue
        try:
            pid = int(dbus.Interface(d, "org.freedesktop.DBus").GetConnectionUnixProcessID(n))
        except dbus.DBusException:
            continue
        if pid in pids:
            if cmd == "has": sys.exit(0)
            s = dbus.Interface(d, "org.freedesktop.DBus.Debug.Stats").GetConnectionStats(n)
            total += int(s.get("OutgoingMessages", 0))
    if cmd == "has": sys.exit(1)
    print(total)
