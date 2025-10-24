# 8. Build a server that facilitates a peer-to-peer (P2P) file-sharing network.
# The server maintains a directory of files and connected peers.
# Clients can request files, and the server connects them to peers
# that have the requested files. Data transfers happens over TCP.


import argparse
import json
import socket
import threading
import time
import atexit
import sys
import signal

ANNOUNCE_INTERVAL = 60


class Peer:
    def __init__(self, server_host, server_port, peer_port=0):
        self.server_host = server_host
        self.server_port = server_port
        self.peer_port = int(peer_port)
        self.running = threading.Event()
        self.running.set()
        self.server_thread = None
        self._server_socket = None
        self._server_lock = threading.Lock()

    def _send_server(self, payload, timeout=5):
        try:
            with socket.create_connection((self.server_host, self.server_port), timeout=timeout) as s:
                fp = s.makefile(mode="rwb")
                fp.write((json.dumps(payload) + "\n").encode())
                fp.flush()
                line = fp.readline()
                if not line:
                    return None
                return json.loads(line.decode().strip())
        except Exception:
            return None

    def register(self, filenames):
        payload = {"action": "register", "peer_port": self.peer_port, "files": filenames}
        resp = self._send_server(payload)
        if resp and resp.get("status") == "ok":
            print("registered with server")
            return True
        print("failed to register with server")
        return False

    def unregister(self):
        payload = {"action": "unregister", "peer_port": self.peer_port}
        resp = self._send_server(payload)
        if resp and resp.get("status") == "ok":
            print("unregistered from server")
            return True
        print("failed to unregister from server")
        return False

    def get_peer(self, filename):
        payload = {"action": "get", "filename": filename}
        resp = self._send_server(payload)
        if not resp or resp.get("status") != "ok":
            return {}
        return resp.get("peer", {})

    def _handle_conn(self, conn, addr):
        fp = conn.makefile(mode="rwb")
        try:
            line = fp.readline()
            if not line:
                return
            try:
                req = json.loads(line.decode().strip())
            except Exception:
                resp = {"status": "error", "message": "invalid json"}
                fp.write((json.dumps(resp) + "\n").encode()); fp.flush()
                return

            action = req.get("action")

            if action == "get":
                filename = req.get("filename")
                if not filename:
                    resp = {"status": "error", "message": "missing file"}
                    fp.write((json.dumps(resp) + "\n").encode()); fp.flush()
                    return

                resp = {
                    "status": "ok",
                    "simulated": True,
                    "filename": filename,
                    "message": "this is a simulated transfer; no bytes sent"
                }
                fp.write((json.dumps(resp) + "\n").encode()); fp.flush()
                return
            resp = {"status": "error", "message": "unknown action"}
            fp.write((json.dumps(resp) + "\n").encode()); fp.flush()
            return

        finally:
            try:
                fp.close()
            except Exception:
                pass
            try:
                conn.close()
            except Exception:
                pass

    def _file_server_loop(self):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind(("0.0.0.0", self.peer_port))
            with self._server_lock:
                self.peer_port = s.getsockname()[1]
                self._server_socket = s
            print(f"peer message server listening on 0.0.0.0:{self.peer_port}")
            s.listen()
            s.settimeout(1.0)
            while self.running.is_set():
                try:
                    conn, addr = s.accept()
                except socket.timeout:
                    continue
                t = threading.Thread(target=self._handle_conn, args=(conn, addr), daemon=True)
                t.start()

    def simulate_get_from_peer(self, peer_ip, peer_port, filename, timeout=5):
        try:
            with socket.create_connection((peer_ip, int(peer_port)), timeout=timeout) as s:
                fp = s.makefile(mode="rwb")
                req = {"action": "get", "filename": filename}
                fp.write((json.dumps(req) + "\n").encode())
                fp.flush()
                line = fp.readline()
                if not line:
                    return False, "no response"
                resp = json.loads(line.decode().strip())
                return True, resp
        except Exception as e:
            return False, str(e)

    def get(self, filename):
        peer = self.get_peer(filename)
        if not peer:
            print("server returned no peers for", filename)
            return False
        ip = peer.get("ip")
        port = peer.get("port")
        if ip == "0.0.0.0" or ip is None:
            return False
        print(f"connecting to {ip}:{port} ...")
        ok, resp = self.simulate_get_from_peer(ip, port, filename)
        if ok:
            print("simulated response:", json.dumps(resp))
            return True
        else:
            print("failed:", resp)
        return False

    def start(self, register_on_start=False):
        self.server_thread = threading.Thread(target=self._file_server_loop, daemon=True)
        self.server_thread.start()
        if register_on_start:
            self.register([])

    def stop(self):
        if self.running.is_set():
            self.running.clear()
            try:
                self.unregister()
            except Exception:
                pass
            with self._server_lock:
                try:
                    if self._server_socket:
                        self._server_socket.close()
                except Exception:
                    pass
            time.sleep(0.2)



def print_help():
    print("commands:")
    print("  help                show this help")
    print("  register            register with server")
    print("  unregister          unregister from server")
    print("  get <file>          simulate getting <file> from a peer (dummy message)")
    print("  exit | quit         stop and exit")


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("server", help="server host[:port]")
    args = parser.parse_args()

    host, port = args.server.split(":", 1) if ":" in args.server else (args.server, "9000")
    peer = Peer(host, int(port), peer_port=0)

    def handle_exit(signum=None, frame=None):
        peer.stop()
        sys.exit(0)

    atexit.register(handle_exit)
    signal.signal(signal.SIGINT, handle_exit)
    signal.signal(signal.SIGTERM, handle_exit)

    peer.start(register_on_start=False)
    time.sleep(0.1)
    print(f"Server: {host}:{port}")
    print(f"Peer listening on port: {peer.peer_port}")
    print_help()

    try:
        while True:
            try:
                line = input("> ").strip()
            except EOFError:
                break
            if not line:
                continue
            parts = line.split()
            cmd = parts[0].lower()
            if cmd == "exit":
                break
            elif cmd == "help":
                print_help()
            elif cmd == "register":
                files_line = input("files (space-separated, leave empty to register none): ").strip()
                filenames = files_line.split() if files_line else []
                print("registering...")
                peer.register(filenames)
            elif cmd == "unregister":
                print("unregistering...")
                peer.unregister()
            elif cmd == "get":
                if len(parts) < 2:
                    print("usage: get <file>")
                    continue
                fname = parts[1]
                peer.get(fname)
            else:
                print("unknown command, type help")
    finally:
        peer.stop()


if __name__ == "__main__":
    main()
