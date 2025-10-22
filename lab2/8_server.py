# 8. Build a server that facilitates a peer-to-peer (P2P) file-sharing network.
# The server maintains a directory of files and connected peers.
# Clients can request files, and the server connects them to peers
# that have the requested files. Data transfers happens over TCP.

import socket
import threading
import json
import time

HOST = "0.0.0.0"
PORT = 9000
PEER_TIMEOUT = 300
CLEANUP_INTERVAL = 60

lock = threading.Lock()
peers = {}
file_index = {}


def register_peer(addr, peer_port, files):
    key = (addr, peer_port)
    now = time.time()
    with lock:
        peers[key] = {"files": set(files), "last_seen": now}
    for f in files:
        file_index.setdefault(f, set()).add(key)

def unregister_peer(addr, peer_port):
    key = (addr, peer_port)
    with lock:
        info = peers.pop(key, None)
        if info:
            for f in info["files"]:
                s = file_index.get(f)
                if s:
                    s.discard(key)
                    if not s:
                        file_index.pop(f, None)

def touch_peer(addr, peer_port, files=None):
    key = (addr, peer_port)
    now = time.time()
    with lock:
        if key in peers:
            peers[key]["last_seen"] = now
            if files is not None:
                old_files = peers[key]["files"]
                for f in old_files:
                    file_index.get(f, set()).discard(key)
                peers[key]["files"] = set(files)
                for f in files:
                    file_index.setdefault(f, set()).add(key)
        else:
            register_peer(addr, peer_port, files or [])

def lookup_file(filename):
    with lock:
        s = file_index.get(filename, set())
        return [{"ip": ip, "port": port} for (ip, port) in s]

def cleanup_loop():
    while True:
        time.sleep(CLEANUP_INTERVAL)
        now = time.time()
        removed = []
        with lock:
            for key, info in list(peers.items()):
                if now - info["last_seen"] > PEER_TIMEOUT:
                    removed.append(key)
        for (ip, port) in removed:
            unregister_peer(ip, port)

def handle_client(conn, addr):
    ip = addr[0]
    fp = conn.makefile(mode="rwb")
    try:
        while True:
            line = fp.readline()
            if not line:
                break
            try:
                msg = json.loads(line.decode("utf-8").strip())
            except Exception:
                resp = {"status": "error", "message": "Invalid request!"}
                fp.write((json.dumps(resp) + "\n").encode())
                fp.flush()
                continue

            action = msg.get("action")
            if action == "register":
                peer_port = int(msg.get("peer_port", 0))
                files = msg.get("files", [])
                register_peer(ip, peer_port, files)
                resp = {"status": "ok"}
            elif action == "unregister":
                peer_port = int(msg.get("peer_port", 0))
                unregister_peer(ip, peer_port)
                resp = {"status": "ok"}
            elif action == "announce":
                peer_port = int(msg.get("peer_port", 0))
                files = msg.get("files")
                touch_peer(ip, peer_port, files)
                resp = {"status": "ok"}
            elif action == "request":
                filename = msg.get("file")
                if not filename:
                    resp = {"status": "error", "message": "Missing file!"}
                else:
                    peers_list = lookup_file(filename)
                    resp = {"status": "ok", "peers": peers_list}
            elif action == "list":
                with lock:
                    resp = {"status": "ok", "peers": list(file_index.keys())}
            else:
                resp = {"status": "error", "message": "Unknown action!"}

            fp.write((json.dumps(resp) + "\n").encode())
            fp.flush()
    finally:
        fp.close()
        conn.close()

def start_server(host=HOST, port=PORT):
    threading.Thread(target=cleanup_loop, daemon=True).start()
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((host, port))
        s.listen()
        print(f"Listening on {host}:{port}")
        while True:
            conn, addr = s.accept()
            t = threading.Thread(target=handle_client, args=(conn, addr), daemon=True)
            t.start()

if __name__ == "__main__":
    start_server()
