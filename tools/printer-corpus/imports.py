#!/usr/bin/env python3
"""Lists what a driver package's 64-bit DLLs import that Wine does not
provide (absent, or a stub), from the PE import tables (objdump -p) and
Wine's .spec files.

    imports.py WINE_SOURCE_TREE DIR...
"""
import os, re, subprocess, sys
from collections import defaultdict

src = sys.argv[1]
specs = {}

def spec_for(dll):
    dll = dll.lower()
    if dll in specs:
        return specs[dll]
    base = dll[:-4] if dll.endswith(('.dll', '.drv', '.sys')) else dll
    cands = [os.path.join(src, 'dlls', dll, base + '.spec'), os.path.join(src, 'dlls', base, base + '.spec'),
             os.path.join(src, 'dlls', dll, dll + '.spec')]
    names = None
    for c in cands:
        if os.path.exists(c):
            names = {}
            for line in open(c, errors='ignore'):
                m = re.match(r'\s*(?:@|\d+)\s+(\w+)\s+(?:-[\w=,.\-]+\s+)*([\w?@$]+)', line)
                if not m:
                    continue
                kind, name = m.group(1), m.group(2)
                names[name] = kind
            break
    specs[dll] = names
    return names

def imports(path):
    out = subprocess.run(['x86_64-w64-mingw32-objdump', '-p', path], capture_output=True, text=True).stdout
    if 'file format pei-x86-64' not in out:
        return None
    res = defaultdict(list)
    cur = None
    for line in out.splitlines():
        m = re.match(r'\s*DLL Name: (\S+)', line)
        if m:
            cur = m.group(1)
            continue
        # "vma  Ordinal  Hint  Member-Name": an import by name has no ordinal
        m = re.match(r'\s+[0-9a-f]+\s+<none>\s+[0-9a-f]+\s+(\S+)\s*$', line)
        if cur and m:
            res[cur].append(m.group(1))
        elif cur and not line.strip():
            pass
    return res

missing = defaultdict(set)
for d in sys.argv[2:]:
    for f in sorted(os.listdir(d)):
        p = os.path.join(d, f)
        if not f.lower().endswith(('.dll', '.exe', '.drv')):
            continue
        imp = imports(p)
        if not imp:
            continue
        for dll, funcs in imp.items():
            if dll.lower().startswith(('api-ms-', 'ext-ms-')):
                continue
            names = spec_for(dll)
            if names is None:
                missing[(dll, '*')].add(f)
                continue
            for fn in funcs:
                k = names.get(fn)
                if k is None or k == 'stub':
                    missing[(dll, fn + (' (stub)' if k == 'stub' else ''))].add(f)
for (dll, fn), users in sorted(missing.items()):
    print('%-14s %-40s %s' % (dll, fn, ' '.join(sorted(users))[:120]))
