"""Read health; create two temporary private sessions; exercise host/ready/start; leave.
Never prints session tokens or TURN passwords. No game assets are uploaded.
"""
import json
import secrets
import socket
import struct
import urllib.request

BASE = "https://epochremasters.com/online/v1/"


def post(action, data=None, token=None):
    request = urllib.request.Request(BASE + action, json.dumps(data or {}).encode(),
                                     {"Content-Type": "application/json"}, method="POST")
    if token:
        request.add_header("Authorization", "Bearer " + token)
    with urllib.request.urlopen(request, timeout=15) as response:
        return json.load(response)


if __name__ == "__main__":
    transaction = secrets.token_bytes(12)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        udp.settimeout(5)
        udp.sendto(struct.pack("!HHI", 1, 0, 0x2112A442) + transaction, ("146.190.227.180", 3478))
        reply, _ = udp.recvfrom(2048)
        assert reply[:2] == b"\x01\x01" and reply[8:20] == transaction
    print("Public STUN reachable")
    host = post("create", dict(game="epoch-lego-racers", version="prototype-2", name="Smoke host"))
    try:
        guest = post("join", dict(game="epoch-lego-racers", version="prototype-2", name="Smoke guest", code=host["room"]["code"]))
        assert host["ice"]["turn"] and guest["ice"]["turn"]
        for peer in (host, guest):
            post("ready", {"ready": True}, peer["token"])
        assert post("start", token=host["token"])["room"]["phase"] == "loading"
        post("loaded", token=host["token"])
        assert post("loaded", token=guest["token"])["room"]["phase"] == "in_progress"
        print("HTTPS lobby, ready/start barrier and temporary relay credentials passed")
    finally:
        post("leave", token=host["token"])
