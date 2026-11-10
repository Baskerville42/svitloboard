"""Convert the shared PEM roots into compact BearSSL trust anchors at build time."""

import base64
import re
from pathlib import Path


def der_fields(data):
    """Yield (tag, content, original encoding) for definite-length DER fields."""
    pos = 0
    while pos < len(data):
        start = pos
        tag, length = data[pos:pos + 2]
        pos += 2
        if length & 0x80:
            count = length & 0x7f
            if not count or pos + count > len(data):
                raise ValueError("Invalid DER length")
            length = int.from_bytes(data[pos:pos + count], "big")
            pos += count
        end = pos + length
        if end > len(data):
            raise ValueError("Truncated DER field")
        yield tag, data[pos:end], data[start:end]
        pos = end


def anchor(certificate):
    [(tag, cert, _)] = der_fields(certificate)
    if tag != 0x30:
        raise ValueError("Expected X.509 certificate sequence")
    tbs = list(der_fields(next(der_fields(cert))[1]))
    if tbs[0][0] == 0xa0:  # Explicit version is optional in X.509.
        tbs = tbs[1:]
    issuer, subject, spki = tbs[2][2], tbs[4][2], tbs[5][1]
    if issuer != subject:
        raise ValueError("Expected a self-issued root")
    algorithm, key = list(der_fields(spki))
    if key[0] != 0x03 or key[1][0] != 0:
        raise ValueError("Expected a byte-aligned public key")
    oid, parameters = list(der_fields(algorithm[1]))
    key_bytes = key[1][1:]
    if oid[1] == bytes.fromhex("2a864886f70d010101"):  # rsaEncryption
        [(tag, rsa, _)] = der_fields(key_bytes)
        fields = list(der_fields(rsa))
        if tag != 0x30 or [field[0] for field in fields] != [0x02, 0x02]:
            raise ValueError("Expected an RSA modulus and exponent")
        modulus, exponent = (field[1].lstrip(b"\0") for field in fields)
        return subject, (modulus, exponent), None
    if oid[1] == bytes.fromhex("2a8648ce3d0201"):  # id-ecPublicKey
        curves = {
            bytes.fromhex("2a8648ce3d030107"): ("BR_EC_secp256r1", 65),
            bytes.fromhex("2b81040022"): ("BR_EC_secp384r1", 97),
        }
        curve, length = curves[parameters[1]]
        if len(key_bytes) != length or key_bytes[0] != 0x04:
            raise ValueError("Expected an uncompressed EC public key")
        return subject, (key_bytes,), curve
    raise ValueError("Unsupported public key algorithm")


def emit_bytes(name, payload):
    rows = [
        "  " + ", ".join(f"0x{byte:02x}" for byte in payload[offset:offset + 16]) + ","
        for offset in range(0, len(payload), 16)
    ]
    # BearSSL's public key structs take non-const pointers. The validator reads these buffers.
    return f"static unsigned char {name}[] = {{\n" + "\n".join(rows) + "\n};\n"


def generate(source, output):
    shared = source.read_text(encoding="utf-8").split('R"SVB_COMMON(', 1)[1].split(')SVB_COMMON"', 1)[0]
    encoded = re.findall(r"-----BEGIN CERTIFICATE-----\s*(.*?)\s*-----END CERTIFICATE-----", shared, re.S)
    if not encoded:
        raise ValueError("No shared TLS roots found")

    chunks = ["#pragma once\n\n// Generated from RootCa.h. Keep BearSSL key bytes addressable in RAM.\n"]
    descriptors = []
    for index, pem in enumerate(encoded):
        subject, keys, curve = anchor(base64.b64decode(pem))
        dn, key, exponent = (f"kRoot{index}{suffix}" for suffix in ("Dn", "Key", "Exponent"))
        chunks.extend((emit_bytes(dn, subject), emit_bytes(key, keys[0])))
        if curve:
            public_key = f"BR_KEYTYPE_EC, {{.ec = {{{curve}, {key}, sizeof({key})}}}}"
        else:
            chunks.append(emit_bytes(exponent, keys[1]))
            public_key = f"BR_KEYTYPE_RSA, {{.rsa = {{{key}, sizeof({key}), {exponent}, sizeof({exponent})}}}}"
        descriptors.append(f"  {{{{{dn}, sizeof({dn})}}, BR_X509_TA_CA, {{{public_key}}}}},")

    chunks.append("static const br_x509_trust_anchor kTlsTrustAnchors[] = {\n" + "\n".join(descriptors) + "\n};\n")
    generated = "\n".join(chunks)
    if not output.exists() or output.read_text(encoding="utf-8") != generated:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(generated, encoding="utf-8")


Import("env")  # type: ignore[name-defined]  # PlatformIO/SCons injects Import.
if env.subst("$PIOPLATFORM") == "espressif8266":
    output = Path(env.subst("$BUILD_DIR")) / "RootCaBearSSL.h"
    generate(Path(env.subst("$PROJECT_DIR")) / "include/RootCa.h", output)
    env.Append(CPPPATH=[str(output.parent)])
