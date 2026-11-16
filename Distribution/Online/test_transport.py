"""Two real companion processes against the deployed service, no game launched."""
import pathlib
import subprocess
import sys
import time

root = pathlib.Path(__file__).resolve().parents[2]
exe = root / "Build/online/Release/EpochOnline.exe"
relay = "--relay-only" in sys.argv
players = 6 if "--six" in sys.argv else 2
label = "relay" if relay else "direct"
logs = root / "Build"
flags = (["--relay-only"] if relay else []) + ["--peers", str(players)]
processes = []
files = []
try:
    host_log = logs / ("transport-" + label + "-host.log")
    stream = host_log.open("w")
    files.append(stream)
    host = subprocess.Popen([str(exe), "--host", "--probe", "--name", "Transport host"] + flags,
                            stdin=subprocess.PIPE, stdout=stream, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
    processes.append(host)
    code = None
    for _ in range(200):
        output = host_log.read_text(errors="replace")
        for line in output.splitlines():
            if line.startswith("ROOM "):
                code = line.split()[1]
        if code or host.poll() is not None:
            break
        time.sleep(.1)
    if not code:
        raise RuntimeError("Host failed: " + output)
    paths = [host_log]
    for index in range(1, players):
        path = logs / ("transport-" + label + "-guest-" + str(index) + ".log")
        paths.append(path)
        stream = path.open("w")
        files.append(stream)
        guest = subprocess.Popen([str(exe), "--join", code, "--probe", "--name", "Guest " + str(index)] + flags,
                                 stdin=subprocess.PIPE, stdout=stream, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        processes.append(guest)
    deadline = time.monotonic() + 100
    while time.monotonic() < deadline:
        if all("PROBE PASS peers=" + str(players-1) in path.read_text(errors="replace") for path in paths):
            break
        if any(process.poll() is not None for process in processes):
            raise RuntimeError("A peer exited early; inspect transport logs")
        time.sleep(.2)
    for path in paths:
        output = path.read_text(errors="replace")
        assert "PROBE PASS peers=" + str(players-1) in output, "Incomplete full mesh: " + str(path)
        assert output.count("CONNECTED " + label) == players-1, "Unexpected route: " + str(path)
        print(path.name + ": " + next(line for line in output.splitlines() if line.startswith("PROBE PASS")))
    for process in processes:
        process.stdin.write(b"q\n")
        process.stdin.flush()
    for process in processes:
        process.wait(timeout=15)
    print("PASS: " + label + " route carried packets in both directions")
finally:
    for process in processes:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
    for stream in files:
        stream.close()
