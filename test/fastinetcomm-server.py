#!/usr/bin/env python3
# scripted IMAP and SMTP servers for fastinetcomm-gate.sh: writes "imapport smtpport" to argv[1],
# the SMTP session to argv[2]
import socket, sys, threading, time

def line(conn):
    buf = b""
    while not buf.endswith(b"\n"):
        c = conn.recv(1)
        if not c:
            return None
        buf += c
    return buf[:-2] if buf.endswith(b"\r\n") else buf[:-1]

def imap(conn):
    conn.sendall(b"* OK [CAPABILITY IMAP4rev1 IDLE] mock ready\r\n")
    while True:
        l = line(conn)
        if l is None:
            return
        tag, _, rest = l.decode().partition(" ")
        verb = rest.split(" ")[0].upper()
        up = rest.upper()
        out = []
        status = "OK done"
        if verb == "LOGIN":
            status = "OK LOGIN done" if rest == 'LOGIN "fast" "secret"' else "NO bad login"
        elif verb == "SELECT" or verb == "EXAMINE":
            out = ["* 3 EXISTS", "* 1 RECENT", "* OK [UNSEEN 2] first unseen", "* OK [UIDVALIDITY 777] ids",
                   "* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)", "* OK [PERMANENTFLAGS (\\Seen \\Deleted)] ok",
                   "* OK [READ-WRITE] ok"]
            status = "OK [READ-WRITE] SELECT done"
        elif verb == "LIST" or verb == "LSUB":
            out = ['* %s (\\Noselect) "/" "Top"' % verb, '* %s () "/" "INBOX"' % verb]
        elif verb == "SEARCH":
            out = ["* SEARCH 1 2 4"]
        elif up.startswith("FETCH 1:2 (FLAGS"):
            out = ['* 1 FETCH (FLAGS (\\Seen) UID 100 RFC822.SIZE 55 INTERNALDATE "01-Jan-2000 00:00:00 +0000")',
                   '* 2 FETCH (FLAGS (\\Seen \\Answered) UID 101 RFC822.SIZE 66 INTERNALDATE "17-Jul-1996 02:44:25 -0700")']
        elif up.startswith("UID FETCH 1:2 (BODY[])"):
            conn.sendall(b"* 1 FETCH (UID 100 BODY[] {11}\r\nhello world)\r\n")
        elif verb == "STORE":
            out = ["* 1 FETCH (FLAGS (\\Seen \\Deleted))"]
        elif verb == "STATUS":
            out = ['* STATUS "INBOX" (MESSAGES 3 UIDNEXT 103)']
        elif verb == "APPEND":
            n = int(rest[rest.rindex("{") + 1:rest.rindex("}")])
            conn.sendall(b"+ go ahead\r\n")
            data = b""
            while len(data) < n + 2:
                data += conn.recv(n + 2 - len(data))
            status = "OK APPEND done" if data[:n].endswith(b"appended mail") else "NO bad data"
        elif verb == "EXPUNGE":
            out = ["* 2 EXPUNGE"]
        elif verb == "DELETE":
            status = "NO no such mailbox"
        elif verb == "LOGOUT":
            conn.sendall(("* BYE bye\r\n%s OK LOGOUT done\r\n" % tag).encode())
            return
        for o in out:
            conn.sendall((o + "\r\n").encode())
        conn.sendall(("%s %s\r\n" % (tag, status)).encode())

def smtp(conn, log):
    conn.sendall(b"220 mock ESMTP ready\r\n")
    data = b""
    while True:
        c = conn.recv(4096)
        if not c:
            break
        data += c
        for l in data.split(b"\n")[:-1]:
            u = l.strip().upper()
            if u.startswith(b"EHLO") or u.startswith(b"HELO"):
                conn.sendall(b"250 mock\r\n")
            elif u.startswith(b"RCPT") or u.startswith(b"MAIL") or u.startswith(b"RSET"):
                conn.sendall(b"250 ok\r\n")
            elif u.startswith(b"QUIT"):
                conn.sendall(b"221 bye\r\n")
        data_all = data
        open(log, "wb").write(data_all)
    open(log, "wb").write(data)

def serve(sock, fn, *args):
    conn, _ = sock.accept()
    conn.settimeout(20)
    try:
        fn(conn, *args)
    except Exception as e:
        sys.stderr.write("server error: %r\n" % (e,))
    finally:
        conn.close()

imap_s = socket.socket(); imap_s.bind(("127.0.0.1", 0)); imap_s.listen(1)
smtp_s = socket.socket(); smtp_s.bind(("127.0.0.1", 0)); smtp_s.listen(1)
open(sys.argv[1], "w").write("%d %d\n" % (imap_s.getsockname()[1], smtp_s.getsockname()[1]))
t1 = threading.Thread(target=serve, args=(imap_s, imap)); t1.start()
t2 = threading.Thread(target=serve, args=(smtp_s, smtp, sys.argv[2])); t2.start()
t1.join(150); t2.join(150)
