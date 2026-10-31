"""Ephemeral test-only credentials. No private material is committed or printed."""
import datetime as dt
import ipaddress
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID


def credentials(folder):
    now = dt.datetime.utcnow()
    def write(name, data):
        path = folder / name
        path.write_bytes(data)
        path.chmod(0o600)
    def authority(name):
        key = ec.generate_private_key(ec.SECP256R1())
        subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, name)])
        cert = (x509.CertificateBuilder().subject_name(subject).issuer_name(subject)
                .public_key(key.public_key()).serial_number(x509.random_serial_number())
                .not_valid_before(now - dt.timedelta(days=1)).not_valid_after(now + dt.timedelta(days=3))
                .add_extension(x509.BasicConstraints(ca=True, path_length=1), critical=True)
                .add_extension(x509.KeyUsage(False, False, False, False, False, True, True, False, False), critical=True)
                .sign(key, hashes.SHA256()))
        return key, cert
    root_key, root = authority("OpenV2G ephemeral test root")
    other_key, other = authority("OpenV2G unrelated test root")
    write("ca.pem", root.public_bytes(serialization.Encoding.PEM))
    write("other-ca.pem", other.public_bytes(serialization.Encoding.PEM))
    serials = {}
    def leaf(name, issuer, issuer_key, hostname="localhost", start=-1, end=1, client=False, subject=None):
        key = ec.generate_private_key(ec.SECP256R1())
        alternatives = [x509.DNSName(hostname)]
        if not client:
            alternatives += [x509.IPAddress(ipaddress.ip_address("::1")), x509.IPAddress(ipaddress.ip_address("127.0.0.1"))]
        cert = (x509.CertificateBuilder().subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, subject or hostname)]))
                .issuer_name(issuer.subject).public_key(key.public_key()).serial_number(x509.random_serial_number())
                .not_valid_before(now + dt.timedelta(days=start)).not_valid_after(now + dt.timedelta(days=end))
                .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
                .add_extension(x509.SubjectAlternativeName(alternatives), critical=False)
                .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.CLIENT_AUTH if client else ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
                .add_extension(x509.KeyUsage(True, False, False, False, False, False, False, False, False), critical=True)
                .sign(issuer_key, hashes.SHA256()))
        write(name + ".pem", cert.public_bytes(serialization.Encoding.PEM))
        write(name + ".der", cert.public_bytes(serialization.Encoding.DER))
        write(name + ".key", key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
        serials[name] = cert.serial_number
        return key, cert
    server_key, _ = leaf("server", root, root_key)
    r, s = utils.decode_dss_signature(server_key.sign(b"abc", ec.ECDSA(hashes.SHA256())))
    write("external.sig", r.to_bytes(32, "big") + s.to_bytes(32, "big"))
    leaf("client", root, root_key, "evcc.test", client=True)
    leaf("contract", root, root_key, "evcc.test", client=True, subject="DEABC1234567890")
    leaf("contract-wrong-id", root, root_key, "evcc.test", client=True, subject="DEABC1234567891")
    leaf("client-expired", root, root_key, "evcc.test", start=-3, end=-2, client=True)
    leaf("client-future", root, root_key, "evcc.test", start=1, end=2, client=True)
    leaf("client-untrusted", other, other_key, "evcc.test", client=True)
    leaf("client-wrong-name", root, root_key, "other.test", client=True)
    leaf("untrusted", other, other_key)
    leaf("expired", root, root_key, start=-3, end=-2)
    leaf("future", root, root_key, start=1, end=2)
    revoked = x509.RevokedCertificateBuilder().serial_number(serials["server"]).revocation_date(now - dt.timedelta(minutes=1)).build()
    for name, entries in [("revoked.crl", [revoked]), ("clean.crl", []), ("contract-revoked.crl", [x509.RevokedCertificateBuilder().serial_number(serials["contract"]).revocation_date(now - dt.timedelta(minutes=1)).build()]), ("client-revoked.crl", [x509.RevokedCertificateBuilder().serial_number(serials["client"]).revocation_date(now - dt.timedelta(minutes=1)).build()])]:
        crl = x509.CertificateRevocationListBuilder().issuer_name(root.subject).last_update(now - dt.timedelta(minutes=2)).next_update(now + dt.timedelta(days=1))
        for entry in entries:
            crl = crl.add_revoked_certificate(entry)
        write(name, crl.sign(root_key, hashes.SHA256()).public_bytes(serialization.Encoding.PEM))


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="openv2g-tls-test-") as directory:
        folder = Path(directory)
        folder.chmod(0o700)
        credentials(folder)
        subprocess.run([sys.argv[1], str(folder), *sys.argv[2:]], check=True, timeout=45)
        produced = folder / "produced.sig"
        if produced.exists():
            signature = produced.read_bytes()
            if len(signature) != 64:
                raise AssertionError("Invalid raw P-256 signature size")
            public = x509.load_pem_x509_certificate((folder / "server.pem").read_bytes()).public_key()
            public.verify(utils.encode_dss_signature(int.from_bytes(signature[:32], "big"), int.from_bytes(signature[32:], "big")), b"abc", ec.ECDSA(hashes.SHA256()))

        receipt_signature = folder / "receipt.sig"
        if receipt_signature.exists():
            signature = receipt_signature.read_bytes()
            if len(signature) != 64:
                raise AssertionError("Invalid receipt signature size")
            public = x509.load_pem_x509_certificate((folder / "client.pem").read_bytes()).public_key()
            public.verify(utils.encode_dss_signature(int.from_bytes(signature[:32], "big"), int.from_bytes(signature[32:], "big")),
                          (folder / "receipt-info.exi").read_bytes(), ec.ECDSA(hashes.SHA256()))
            assert hashlib.sha256((folder / "receipt-body.exi").read_bytes()).digest() == (folder / "receipt-digest.bin").read_bytes()
            oracle = os.environ.get("OPENV2G_EXI_ORACLE")
            if oracle:
                for name, schema in [("receipt.exi", "iso2"), ("receipt-body.exi", "iso2-fragment"), ("receipt-info.exi", "iso2-fragment")]:
                    data = (folder / name).read_bytes()
                    result = subprocess.run([oracle, schema], input=data, capture_output=True, check=True, timeout=5)
                    assert result.stdout == data, name

        auth_signature = folder / "auth.sig"
        if auth_signature.exists():
            signature = auth_signature.read_bytes()
            assert len(signature) == 64
            public = x509.load_pem_x509_certificate((folder / "contract.pem").read_bytes()).public_key()
            public.verify(utils.encode_dss_signature(int.from_bytes(signature[:32], "big"), int.from_bytes(signature[32:], "big")),
                          (folder / "auth-info.exi").read_bytes(), ec.ECDSA(hashes.SHA256()))
            assert hashlib.sha256((folder / "auth-body.exi").read_bytes()).digest() == (folder / "auth-digest.bin").read_bytes()
            oracle = os.environ.get("OPENV2G_EXI_ORACLE")
            if oracle:
                for name, schema in [("payment-details.exi", "iso2"), ("payment-details-response.exi", "iso2"), ("auth.exi", "iso2"), ("auth-body.exi", "iso2-fragment"), ("auth-info.exi", "iso2-fragment")]:
                    data = (folder / name).read_bytes()
                    result = subprocess.run([oracle, schema], input=data, capture_output=True, check=True, timeout=5)
                    assert result.stdout == data, name
