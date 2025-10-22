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
from pathlib import Path
import signal

ANNOUNCE_INTERVAL = 60


class Peer:
    def __init__(self, server_host, server_port, peer_port=0, shared_dir="./shared"):
        self.server_host = server_host
        self.server_port = server_port
        self.peer_port = int(peer_port)
        self.shared_dir = Path(shared_dir).resolve()
        self.shared_dir.mkdir(parents=True, exist_ok=True)
        self.running = threading.Event()
        self.running.set()
        self.announce_thread = None
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

    def register(self):
        files = [p.name for p in self.shared_dir.iterdir() if p.is_file()]
        payload = {"action": "register", "peer_port": self.peer_port, "files": files}
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

    def announce(self):
        files = [p.name for p in self.shared_dir.iterdir() if p.is_file()]
        payload = {"action": "announce", "peer_port": self.peer_port, "files": files}
        self._send_server(payload)  # best-effort

    def lookup_file(self, filename):
        payload = {"action": "request", "file": filename}
        resp = self._send_server(payload)
        if not resp or resp.get("status") != "ok":
            return []
        return resp.get("peers", [])

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

            # Simulated GET: reply with a single JSON message, no raw bytes
            if action == "get":
                filename = req.get("file")
                if not filename:
                    resp = {"status": "error", "message": "missing file"}
                    fp.write((json.dumps(resp) + "\n").encode()); fp.flush()
                    return

                requested = (self.shared_dir / filename).resolve()
                try:
                    requested.relative_to(self.shared_dir)
                except Exception:
                    resp = {"status": "error", "message": "access denied"}
                    fp.write((json.dumps(resp) + "\n").encode()); fp.flush()
                    return

                if not requested.exists() or not requested.is_file():
                    resp = {"status": "error", "message": "not found"}
                    fp.write((json.dumps(resp) + "\n").encode()); fp.flush()
                    return

                # Return a dummy/simulated response instead of file bytes
                resp = {
                    "status": "ok",
                    "simulated": True,
                    "file": filename,
                    "message": "this is a simulated transfer; no bytes sent"
                }
                fp.write((json.dumps(resp) + "\n").encode()); fp.flush()
                return

            # Keep a simple message handler for 'msg' if needed
            if action == "msg":
                text = req.get("text", "")
                sender = f"{addr[0]}:{addr[1]}"
                print(f"[msg] from {sender}: {text}")
                resp = {"status": "ok", "received": True}
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

    def _announce_loop(self):
        while self.running.is_set():
            try:
                self.announce()
            except Exception:
                pass
            for _ in range(int(ANNOUNCE_INTERVAL)):
                if not self.running.is_set():
                    break
                time.sleep(1)

    # client-side: simulate getting a file by reading the dummy JSON response
    def simulate_get_from_peer(self, peer_ip, peer_port, filename, timeout=5):
        try:
            with socket.create_connection((peer_ip, int(peer_port)), timeout=timeout) as s:
                fp = s.makefile(mode="rwb")
                req = {"action": "get", "file": filename}
                fp.write((json.dumps(req) + "\n").encode()); fp.flush()
                line = fp.readline()
                if not line:
                    return False, "no response"
                resp = json.loads(line.decode().strip())
                return True, resp
        except Exception as e:
            return False, str(e)

    def get(self, filename):
        peers = self.lookup_file(filename)
        if not peers:
            print("server returned no peers for", filename)
            return False
        for p in peers:
            ip = p.get("ip")
            port = p.get("port")
            if ip == "0.0.0.0" or ip is None:
                continue
            print(f"connecting to {ip}:{port} ...")
            ok, resp = self.simulate_get_from_peer(ip, port, filename)
            if ok:
                print("simulated response:", json.dumps(resp))
                # announce to update tracker if desired
                self.announce()
                return True
            else:
                print("failed:", resp)
        print("all peers failed")
        return False

    def start(self, register_on_start=False):
        self.server_thread = threading.Thread(target=self._file_server_loop, daemon=True)
        self.server_thread.start()
        self.announce_thread = threading.Thread(target=self._announce_loop, daemon=True)
        self.announce_thread.start()
        if register_on_start:
            self.register()

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
    print("  list                list files known to server")
    print("  peers <file>        show peers for <file>")
    print("  files               list local shared files")
    print("  get <file>          simulate getting <file> from a peer (dummy message)")
    print("  setdir <path>       change shared directory")
    print("  exit | quit         stop and exit")


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("server", help="server host[:port]")
    args = parser.parse_args()

    host, port = args.server.split(":", 1) if ":" in args.server else (args.server, "9000")
    peer = Peer(host, int(port), peer_port=0, shared_dir="./shared")

    def handle_exit(signum=None, frame=None):
        peer.stop()
        sys.exit(0)

    atexit.register(handle_exit)
    signal.signal(signal.SIGINT, handle_exit)
    signal.signal(signal.SIGTERM, handle_exit)

    peer.start(register_on_start=False)

    time.sleep(0.1)
    print(f"Tracker: {host}:{port}")
    print(f"Sharing directory: {peer.shared_dir}")
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
            parts = line.split(maxsplit=2)
            cmd = parts[0].lower()
            if cmd in ("exit", "quit"):
                break
            elif cmd == "help":
                print_help()
            elif cmd == "register":
                print("registering...")
                peer.register()
            elif cmd == "unregister":
                print("unregistering...")
                peer.unregister()
            elif cmd == "list":
                resp = peer._send_server({"action": "list"})
                if resp and resp.get("status") == "ok":
                    files = resp.get("files", [])
                    for f in files:
                        print(f)
                else:
                    print("failed to list")
            elif cmd == "peers":
                if len(parts) < 2:
                    print("usage: peers <file>")
                    continue
                fname = parts[1]
                peers_list = peer.lookup_file(fname)
                if not peers_list:
                    print("no peers")
                else:
                    for p in peers_list:
                        print(f"{p.get('ip')}:{p.get('port')}")
            elif cmd == "files":
                for p in peer.shared_dir.iterdir():
                    if p.is_file():
                        print(p.name)
            elif cmd == "get":
                if len(parts) < 2:
                    print("usage: get <file>")
                    continue
                fname = parts[1]
                peer.get(fname)
            elif cmd == "setdir":
                if len(parts) < 2:
                    print("usage: setdir <path>")
                    continue
                newdir = parts[1]
                peer.stop()
                peer = Peer(host, int(port), peer_port=0, shared_dir=newdir)
                peer.start(register_on_start=False)
                time.sleep(0.1)
                print(f"now sharing: {peer.shared_dir}")
                print(f"peer listening on port: {peer.peer_port}")
            else:
                print("unknown command, type help")
    finally:
        peer.stop()


if __name__ == "__main__":
    main()