"""Six real native bridges and headless game IPC fixtures against the deployed service."""
import pathlib
import subprocess
import time

root = pathlib.Path(__file__).resolve().parents[2]
binary = root / "Build/online/Release"
processes, streams, logs = [], [], []

def launch(index, code=None):
    path = root / "Build" / ("game-transport-" + str(index) + ".log")
    stream = path.open("w")
    streams.append(stream)
    logs.append(path)
    args = [str(binary / "EpochOnline.exe"), "--host"] if code is None else [str(binary / "EpochOnline.exe"), "--join", code]
    args += ["--name", "Fixture " + str(index), "--game", str(binary / "EpochTransportFixture.exe"), "--relay-only"]
    process = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=stream, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
    processes.append(process)

def wait_for(condition, timeout=90):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        if condition():
            return
        if any(process.poll() is not None for process in processes):
            raise AssertionError("A bridge exited early; inspect game-transport logs")
        time.sleep(.1)
    raise AssertionError("Test timed out; inspect game-transport and fixture logs")

try:
    launch(0)
    wait_for(lambda: "ROOM " in logs[0].read_text(errors="replace"))
    code = next(line.split()[1] for line in logs[0].read_text().splitlines() if line.startswith("ROOM "))
    for index in range(1, 6):
        launch(index, code)
    wait_for(lambda: all(path.read_text(errors="replace").count("CONNECTED relay") == 5 for path in logs))
    # Chat must be delivered as text, never interpreted as r/s/q commands.
    processes[1].stdin.write(b'chat "ready start quit - hello lobby"\n'); processes[1].stdin.flush()
    wait_for(lambda: all('"text":"ready start quit - hello lobby"' in path.read_text(errors="replace") for path in logs))
    processes[1].stdin.write(b"r\n"); processes[1].stdin.flush()
    wait_for(lambda: '"ready":true' in logs[1].read_text(errors="replace"))
    processes[1].stdin.write(b"u\n"); processes[1].stdin.flush()
    wait_for(lambda: [line for line in logs[1].read_text(errors="replace").splitlines() if line.startswith("STATE ")][-1].endswith('"ready":false}'))
    print("PASS: authenticated chat delivered to six peers, ready/unready acknowledged")
    for process in processes:
        process.stdin.write(b"r\n"); process.stdin.flush()
    wait_for(lambda: any(line.count("[ready]") == 6 for line in logs[0].read_text().splitlines()))
    processes[0].stdin.write(b"s\n"); processes[0].stdin.flush()
    wait_for(lambda: all("Back in lobby." in path.read_text(errors="replace") for path in logs))
    for index in range(6):
        text = (binary / ("fixture-" + str(index) + ".log")).read_text()
        assert "PASS builds=5 poses=5 barrier=ready" in text, text
        print("Fixture", index, text.strip())
    print("PASS: six-player reliable builds, compressed poses, loading barrier, and next lobby over TURN")
finally:
    for process in processes:
        if process.poll() is None:
            process.stdin.write(b"q\n"); process.stdin.flush()
    for process in processes:
        try:
            process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            process.terminate()
    for stream in streams:
        stream.close()
