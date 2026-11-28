#!/usr/bin/python3
# sharing-gate's stand-in for sg-admind (sg-shell): answers share-add / share-remove requests in a
# spool (argv[1]) by keeping a Samba-style shares file (argv[2]), and records each request in
# argv[3]. "share-add Off ..." is refused as with smbd not running.
import os, sys, time
spool, conf, log = sys.argv[1:4]
shares = []

def write():
    with open(conf, "w") as f:
        for name, opts in shares:
            f.write("[%s]\n" % name)
            for k, v in opts:
                f.write("   %s = %s\n" % (k, v))

while True:
    for e in sorted(os.listdir(os.path.join(spool, "requests"))):
        if not e.endswith(".req"):
            continue
        p = os.path.join(spool, "requests", e)
        fields = open(p, encoding="utf-8").read().split("\n")[:-1]
        os.unlink(p)
        with open(log, "a", encoding="utf-8") as f:
            f.write("|".join(fields) + "\n")
        rep = "OK\n"
        if fields[0] == "share-add":
            name, path, comment, maxu, access = fields[1:6]
            if name == "Off":
                rep = "FAILED File and printer sharing is turned off.\n"
            elif any(n.lower() == name.lower() for n, _ in shares):
                rep = "FAILED The share name is already in use.\n"
            else:
                opts = [("path", path), ("read only", "yes" if access == "read" else "no")]
                if comment:
                    opts.append(("comment", comment))
                if maxu != "0":
                    opts.append(("max connections", maxu))
                shares.append((name, opts))
                write()
        elif fields[0] == "share-remove":
            kept = [s for s in shares if s[0].lower() != fields[1].lower()]
            if len(kept) == len(shares):
                rep = "FAILED The share does not exist.\n"
            shares[:] = kept
            write()
        tmp = os.path.join(spool, "replies", "." + e[:-4])
        with open(tmp, "w") as f:
            f.write(rep)
        os.rename(tmp, os.path.join(spool, "replies", e[:-4] + ".rep"))
    time.sleep(0.1)
