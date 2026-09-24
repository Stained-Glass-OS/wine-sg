#!/usr/bin/env python3
# Write a small, valid, unsigned AppX/MSIX package -- the fixture for
# test/appx-gate.sh. No Microsoft tool is needed (makeappx is one); the gate
# signs it with osslsigncode and a certificate it generates on the spot.
#
#   mkappx.py OUT.msix [variant]
#
# variants (each breaks exactly one rule a reader must enforce):
#   good           the package
#   extra          a file that the block map does not list
#   nomanifest     no AppxManifest.xml (what a bundle looks like to a package reader)
#   blocktamper    one byte of the payload changed, CRC-32 recomputed to match,
#                  so only the block map can tell
#
# SPDX-License-Identifier: LGPL-2.1-or-later
import base64
import hashlib
import struct
import sys
import zipfile
import zlib

PUBLISHER = 'CN=Stained Glass Test Publisher'
MANIFEST = f'''<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
         xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10">
  <Identity Name="StainedGlass.AppxTest" ProcessorArchitecture="x64"
            Publisher="{PUBLISHER}" Version="1.2.3.4" />
  <Properties>
    <DisplayName>Stained Glass AppX test</DisplayName>
    <PublisherDisplayName>Stained Glass OS</PublisherDisplayName>
    <Logo>Assets\\logo.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.17763.0" MaxVersionTested="10.0.22621.0" />
  </Dependencies>
  <Capabilities><Capability Name="internetClient" /></Capabilities>
  <Applications>
    <Application Id="App" Executable="app.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="AppX test app" Description="test" BackgroundColor="transparent"
                          Square150x150Logo="Assets\\logo.png" Square44x44Logo="Assets\\logo.png" />
    </Application>
  </Applications>
</Package>
'''.encode()
CONTENT_TYPES = (b'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
                 b'<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
                 b'<Default Extension="bin" ContentType="application/octet-stream"/>'
                 b'<Default Extension="png" ContentType="image/png"/>'
                 b'<Default Extension="xml" ContentType="application/vnd.ms-appx.manifest+xml"/>'
                 b'<Override PartName="/AppxBlockMap.xml" ContentType="application/vnd.ms-appx.blockmap+xml"/>'
                 b'<Override PartName="/AppxSignature.p7x" ContentType="application/vnd.ms-appx.signature"/>'
                 b'</Types>')


def payload():
    # 200 KiB, deterministic, compressible: several 64 KiB blocks
    out = bytearray()
    seed = 1
    while len(out) < 200 * 1024:
        seed = (seed * 1103515245 + 12345) & 0x7fffffff
        out += f'line {seed % 1000:04d} stained glass\n'.encode()
    return bytes(out[:200 * 1024])


def blockmap(files):
    parts = ['<?xml version="1.0" encoding="UTF-8" standalone="no"?>'
             '<BlockMap xmlns="http://schemas.microsoft.com/appx/2010/blockmap" '
             'HashMethod="http://www.w3.org/2001/04/xmlenc#sha256">']
    for name, data, _ in files:
        zname = name.replace('\\', '/')
        lfh = 30 + len(zname.encode())
        parts.append(f'<File Name="{name}" Size="{len(data)}" LfhSize="{lfh}">')
        for i in range(0, len(data), 65536):
            h = base64.b64encode(hashlib.sha256(data[i:i + 65536]).digest()).decode()
            parts.append(f'<Block Hash="{h}"/>')
        parts.append('</File>')
    parts.append('</BlockMap>')
    return ''.join(parts).encode()


def main():
    out, variant = sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else 'good'
    data = payload()
    files = [
        ('Public\\data.bin', data, zipfile.ZIP_DEFLATED),
        ('Assets\\logo.png', b'\x89PNG\r\n\x1a\n' + bytes(range(64)), zipfile.ZIP_STORED),
        ('AppxManifest.xml', MANIFEST, zipfile.ZIP_DEFLATED),
    ]
    bm = blockmap(files)
    with zipfile.ZipFile(out, 'w') as z:
        for name, content, method in files:
            if variant == 'nomanifest' and name == 'AppxManifest.xml':
                continue
            if variant == 'blocktamper' and name == 'Public\\data.bin':
                content = content[:70000] + bytes([content[70000] ^ 1]) + content[70001:]
            z.writestr(zipfile.ZipInfo(name.replace('\\', '/'), (2026, 1, 1, 0, 0, 0)), content, compress_type=method)
        if variant == 'extra':
            z.writestr(zipfile.ZipInfo('Public/unlisted.txt', (2026, 1, 1, 0, 0, 0)), b'not in the block map')
        z.writestr(zipfile.ZipInfo('AppxBlockMap.xml', (2026, 1, 1, 0, 0, 0)), bm, compress_type=zipfile.ZIP_DEFLATED)
        z.writestr(zipfile.ZipInfo('[Content_Types].xml', (2026, 1, 1, 0, 0, 0)), CONTENT_TYPES,
                   compress_type=zipfile.ZIP_DEFLATED)


if __name__ == '__main__':
    main()
