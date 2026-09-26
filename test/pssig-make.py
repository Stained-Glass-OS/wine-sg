#!/usr/bin/python3
# Sign a PowerShell script the way PowerShell does, for pssig-gate.sh: a
# throwaway code-signing CA and signer (openssl), the digest over the script
# as UTF-16LE (a byte-order mark counting as its first character) up to the
# line break before the signature block, in an Authenticode
# SpcIndirectDataContent naming the PowerShell SIP, a PKCS #7 signature, base64 in
# "# SIG #" comment lines.
#   pssig-make.py DIR   -> DIR/root.der, DIR/signed.ps1, DIR/bom.ps1
import base64, hashlib, os, subprocess, sys

d = sys.argv[1]

def der(tag, body):
    n = len(body)
    if n < 0x80: l = bytes([n])
    elif n < 0x100: l = bytes([0x81, n])
    elif n < 0x10000: l = bytes([0x82, n >> 8, n & 0xff])
    else: l = bytes([0x83, n >> 16, (n >> 8) & 0xff, n & 0xff])
    return bytes([tag]) + l + body

def oid(s):
    p = [int(x) for x in s.split('.')]
    out = bytes([p[0] * 40 + p[1]])
    for v in p[2:]:
        b = [v & 0x7f]
        v >>= 7
        while v:
            b.insert(0, 0x80 | (v & 0x7f)); v >>= 7
        out += bytes(b)
    return der(0x06, out)

def integer(v):
    b = v.to_bytes((v.bit_length() + 8) // 8 or 1, 'big')
    return der(0x02, b)

seq = lambda *x: der(0x30, b''.join(x))
PS_GUID = bytes.fromhex('1fcc3b60594b084eb724d2c6297ef351')

def run(*a):
    subprocess.run(a, check=True, capture_output=True)

def pkcs7(indirect):
    """Authenticode's PKCS #7 SignedData (not CMS: the content is the
    SpcIndirectDataContent itself, and its digest is over the content without
    its outer tag and length)."""
    from cryptography import x509
    signer = x509.load_pem_x509_certificate(open(os.path.join(d, 'signer.pem'), 'rb').read())
    root = x509.load_pem_x509_certificate(open(os.path.join(d, 'root.pem'), 'rb').read())
    from cryptography.hazmat.primitives.serialization import Encoding
    body = indirect[2:] if indirect[1] < 0x80 else indirect[2 + (indirect[1] & 0x7f):]
    sha256 = seq(oid('2.16.840.1.101.3.4.2.1'), der(0x05, b''))
    attrs = [seq(oid('1.2.840.113549.1.9.3'), der(0x31, oid('1.3.6.1.4.1.311.2.1.4'))),
             seq(oid('1.3.6.1.4.1.311.2.1.12'), der(0x31, seq())),
             seq(oid('1.2.840.113549.1.9.4'), der(0x31, der(0x04, hashlib.sha256(body).digest())))]
    attrs.sort()
    open(os.path.join(d, 'attrs.der'), 'wb').write(der(0x31, b''.join(attrs)))
    sig = subprocess.run(['openssl', 'dgst', '-sha256', '-sign', os.path.join(d, 'signer.key'),
                          os.path.join(d, 'attrs.der')], check=True, capture_output=True).stdout
    signer_info = seq(integer(1), seq(signer.issuer.public_bytes(), integer(signer.serial_number)), sha256,
                      der(0xa0, b''.join(attrs)), seq(oid('1.2.840.113549.1.1.1'), der(0x05, b'')), der(0x04, sig))
    signed = seq(integer(1), der(0x31, sha256), seq(oid('1.3.6.1.4.1.311.2.1.4'), der(0xa0, indirect)),
                 der(0xa0, signer.public_bytes(Encoding.DER) + root.public_bytes(Encoding.DER)),
                 der(0x31, signer_info))
    return seq(oid('1.2.840.113549.1.7.2'), der(0xa0, signed))

def sign(name, text, bom):
    content = (('﻿' if bom else '') + text).encode('utf-16-le')
    digest = hashlib.sha256(content).digest()
    indirect = seq(seq(oid('1.3.6.1.4.1.311.2.1.30'),
                       seq(integer(65536), der(0x04, PS_GUID), *[integer(0)] * 5)),
                   seq(seq(oid('2.16.840.1.101.3.4.2.1'), der(0x05, b'')), der(0x04, digest)))
    open(os.path.join(d, 'sig.der'), 'wb').write(pkcs7(indirect))
    b64 = base64.b64encode(open(os.path.join(d, 'sig.der'), 'rb').read()).decode()
    block = '\r\n# SIG # Begin signature block\r\n' + ''.join('# %s\r\n' % b64[i:i + 64] for i in range(0, len(b64), 64)) \
            + '# SIG # End signature block\r\n'
    raw = (text + block).encode('utf-8')
    open(os.path.join(d, name), 'wb').write((b'\xef\xbb\xbf' if bom else b'') + raw)

cnf = os.path.join(d, 'ext.cnf')
open(cnf, 'w').write('[ca]\nbasicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\n'
                     'subjectKeyIdentifier=hash\n'
                     '[leaf]\nbasicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\n'
                     'extendedKeyUsage=codeSigning\nsubjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid\n')
run('openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '30', '-subj', '/CN=SGTEST Script Signing Root',
    '-keyout', os.path.join(d, 'root.key'), '-out', os.path.join(d, 'root.pem'), '-extensions', 'ca', '-config', cnf)
open(cnf, 'a').write('[req]\ndistinguished_name=dn\n[dn]\n')
run('openssl', 'req', '-new', '-newkey', 'rsa:2048', '-nodes', '-subj', '/CN=SGTEST Scripts', '-keyout',
    os.path.join(d, 'signer.key'), '-out', os.path.join(d, 'signer.csr'), '-config', cnf)
run('openssl', 'x509', '-req', '-in', os.path.join(d, 'signer.csr'), '-CA', os.path.join(d, 'root.pem'), '-CAkey',
    os.path.join(d, 'root.key'), '-CAcreateserial', '-days', '30', '-extfile', cnf, '-extensions', 'leaf',
    '-out', os.path.join(d, 'signer.pem'))
run('openssl', 'x509', '-in', os.path.join(d, 'root.pem'), '-outform', 'DER', '-out', os.path.join(d, 'root.der'))
script = 'Write-Output "signed by SGTEST"\r\nexit 0'
sign('signed.ps1', script, False)
sign('bom.psm1', 'function Get-SgTest { "module" }', True)
