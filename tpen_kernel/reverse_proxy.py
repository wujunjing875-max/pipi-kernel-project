# -*- coding: utf-8 -*-
import socket
import sys
import threading

HOST, PORT = '0.0.0.0', 8080

def pipe(a, b):
    try:
        while True:
            d = a.recv(65536)
            if not d:
                break
            b.sendall(d)
    except Exception:
        pass
    finally:
        try:
            a.shutdown(socket.SHUT_RD); b.shutdown(socket.SHUT_WR)
        except Exception:
            pass

def handle(client):
    try:
        client.settimeout(15)
        req = client.recv(65536)
        if not req:
            sys.stderr.write("EMPTY\n"); sys.stderr.flush(); return
        first_line = req.split(b'\r\n', 1)[0]
        parts = first_line.split(b' ')
        if len(parts) < 3:
            sys.stderr.write("BADLINE %r\n" % first_line); sys.stderr.flush(); return
        method, target, ver = parts[0], parts[1], parts[2]
        if method == b'CONNECT':
            if b':' in target:
                host, port = target.rsplit(b':', 1); port = int(port)
            else:
                host, port = target, 443
            sys.stderr.write("CONNECT %s:%d\n" % (host.decode(), port)); sys.stderr.flush()
            try:
                remote = socket.create_connection((host.decode(), port), timeout=15)
            except Exception as e:
                sys.stderr.write("CONNERR %s\n" % e); sys.stderr.flush()
                try: client.sendall(b'HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n')
                except Exception: pass
                return
            client.sendall(b'HTTP/1.1 200 Connection Established\r\n\r\n')
            t1 = threading.Thread(target=pipe, args=(client, remote), daemon=True)
            t2 = threading.Thread(target=pipe, args=(remote, client), daemon=True)
            t1.start(); t2.start(); t1.join(); t2.join()
        else:
            if target.startswith(b'http://'):
                rest = target[7:]
                if b'/' in rest:
                    idx = rest.find(b'/'); hostport = rest[:idx]; path = rest[idx:]
                else:
                    hostport = rest; path = b'/'
            else:
                hostport = b''
                for line in req.split(b'\r\n'):
                    if line.lower().startswith(b'host:'):
                        hostport = line.split(b':', 1)[1].strip()
                path = target
            if b':' in hostport:
                h, p = hostport.rsplit(b':', 1); h, p = h.decode(), int(p)
            else:
                h, p = hostport.decode(), 80
            sys.stderr.write("HTTP %s %s:%d\n" % (method.decode(), h, p)); sys.stderr.flush()
            lines = req.split(b'\r\n'); out = []
            for l in lines:
                out.append(b'Host: ' + hostport if l.lower().startswith(b'host:') else l)
            req = b'\r\n'.join(out)
            try:
                remote = socket.create_connection((h, p), timeout=15)
            except Exception as e:
                sys.stderr.write("CONNERR %s\n" % e); sys.stderr.flush()
                try: client.sendall(b'HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n')
                except Exception: pass
                return
            new_req = req.replace(first_line, method + b' ' + path + b' ' + ver)
            remote.sendall(new_req)
            t1 = threading.Thread(target=pipe, args=(remote, client), daemon=True)
            t2 = threading.Thread(target=pipe, args=(client, remote), daemon=True)
            t1.start(); t2.start(); t1.join(); t2.join()
    except Exception as e:
        sys.stderr.write("HANDLEERR %r\n" % e); sys.stderr.flush()
    finally:
        try: client.close()
        except Exception: pass

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((HOST, PORT)); s.listen(128)
    sys.stderr.write("proxy up on %s:%d\n" % (HOST, PORT)); sys.stderr.flush()
    while True:
        c, _ = s.accept()
        sys.stderr.write("ACCEPT %s\n" % str(_)); sys.stderr.flush()
        threading.Thread(target=handle, args=(c,), daemon=True).start()

if __name__ == '__main__':
    main()
