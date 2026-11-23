# ftpex-server.py PORTFILE DIR: a minimal passive-mode FTP server on 127.0.0.1 for the ftpex gate
# (USER/PASS/TYPE/PASV/EPSV/RETR/STOR/SIZE/PWD/CWD/SYST/FEAT/OPTS/NOOP/QUIT); writes its port to PORTFILE
import os, socket, sys, threading
portfile, root = sys.argv[1], sys.argv[2]
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(('127.0.0.1', 0)); srv.listen(4)
open(portfile + '.tmp', 'w').write(str(srv.getsockname()[1])); os.rename(portfile + '.tmp', portfile)
def session(c):
    f = c.makefile('rb'); pasv = None
    def say(s): c.sendall((s + '\r\n').encode())
    say('220 sg gate ftp')
    for line in f:
        cmd, _, arg = line.decode('utf-8', 'replace').rstrip('\r\n').partition(' ')
        cmd = cmd.upper()
        if cmd == 'USER': say('331 password please')
        elif cmd == 'PASS': say('230 logged in')
        elif cmd in ('TYPE', 'OPTS', 'NOOP', 'CWD', 'MODE', 'STRU'): say('200 ok')
        elif cmd == 'SYST': say('215 UNIX Type: L8')
        elif cmd == 'FEAT': say('211 no features')
        elif cmd == 'PWD': say('257 "/"')
        elif cmd == 'SIZE':
            p = os.path.join(root, os.path.basename(arg))
            say('213 %d' % os.path.getsize(p) if os.path.exists(p) else '550 no such file')
        elif cmd in ('PASV', 'EPSV'):
            pasv = socket.socket(); pasv.bind(('127.0.0.1', 0)); pasv.listen(1)
            port = pasv.getsockname()[1]
            say('227 Entering Passive Mode (127,0,0,1,%d,%d)' % (port >> 8, port & 255) if cmd == 'PASV'
                else '229 Entering Extended Passive Mode (|||%d|)' % port)
        elif cmd in ('RETR', 'STOR'):
            p = os.path.join(root, os.path.basename(arg))
            if cmd == 'RETR' and not os.path.exists(p): say('550 no such file'); continue
            if not pasv: say('425 no data connection'); continue
            say('150 opening data connection')
            d, _ = pasv.accept()
            if cmd == 'RETR': d.sendall(open(p, 'rb').read())
            else:
                with open(p, 'wb') as out:
                    while True:
                        b = d.recv(65536)
                        if not b: break
                        out.write(b)
            d.close(); pasv.close(); pasv = None
            say('226 transfer complete')
        elif cmd == 'QUIT': say('221 bye'); break
        else: say('502 not implemented')
    c.close()
srv.settimeout(90)
try:
    while True:
        c, _ = srv.accept()
        threading.Thread(target=session, args=(c,), daemon=True).start()
except socket.timeout:
    pass
