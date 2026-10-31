"""Exercise the actual TLS CLI; generate no persistent test credentials."""
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
from fixture import credentials

binary = sys.argv[1]
with tempfile.TemporaryDirectory(prefix="openv2g-cli-tls-") as directory:
    folder = Path(directory)
    folder.chmod(0o700)
    credentials(folder)
    for version, ipv6, profile in [("tls12", False, "iso2"), ("tls13", True, "iso2-ac"), ("tls12", False, "iso2-ac3"), ("tls13", True, "iso2-ac3")]:
        with socket.socket(socket.AF_INET6 if ipv6 else socket.AF_INET, socket.SOCK_STREAM) as reservation:
            reservation.bind(("::1" if ipv6 else "127.0.0.1", 0))
            port = str(reservation.getsockname()[1])
        station = subprocess.Popen([binary, "station6" if ipv6 else "station", port, profile, version,
                                    str(folder / "server.pem"), str(folder / "server.key")], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            if not station.stdout.readline():
                raise AssertionError("TLS station failed to start")
            client = subprocess.run([binary, "evcc6" if ipv6 else "evcc", port, profile, version,
                                     str(folder / "ca.pem"), "localhost"], capture_output=True, text=True, timeout=8)
            output, error = station.communicate(timeout=8)
            assert client.returncode == 0 and station.returncode == 0, (profile,version,client.returncode,station.returncode,client.stdout,output,client.stderr,error)
            assert client.stdout.count("wire schema=") == (12 if profile in ("iso2-ac", "iso2-ac3") else 15)
        finally:
            if station.poll() is None:
                station.kill()
                station.wait()
    rejected = subprocess.run([binary, "station", "15118", "din", "tls12", str(folder / "server.pem"), str(folder / "server.key")], capture_output=True, text=True, timeout=2)
    assert rejected.returncode == 2, "DIN TLS must be rejected before opening a session"
print("PASS: TLS CLI DC/AC and DIN rejection")
