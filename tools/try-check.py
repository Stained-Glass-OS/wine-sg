#!/usr/bin/env python3
"""try-check.py --patches DIR ROOT: no return, goto or break out of a __TRY body in the C files our
patches touch, in the patched tree ROOT (wine-sg 6270).

With Wine's PE builds, __TRY is a setjmp and a frame pushed on the thread's exception list (TEB);
__EXCEPT/__FINALLY/__ENDTRY pop it. Leaving the body any other way leaves the frame behind: a later
exception on that thread calls whatever that stack slot holds by then -- 6269 did it in user32
(sg_module_is_ours) and Teams died of a stack overflow of nested exceptions. Wine's own rule: never
return from inside __TRY. A break or continue inside a loop or switch that is itself in the body is
fine; at the body's own level it is not.

Exit 1 with file:line for each one found.
"""
import argparse, glob, os, re, sys

def touched_files(patch_dir):
    files = set()
    for p in glob.glob(os.path.join(patch_dir, '*.patch')):
        for line in open(p, errors='replace'):
            if line.startswith('+++ b/') and line.rstrip().endswith('.c'):
                files.add(line[6:].strip())
    return sorted(files)

def strip_code(text):
    """comments and string/char literals blanked, newlines kept (line numbers stay)"""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('/*', i):
            j = text.find('*/', i + 2); j = n if j < 0 else j + 2
            out.append(re.sub(r'[^\n]', ' ', text[i:j])); i = j
        elif text.startswith('//', i):
            j = text.find('\n', i); j = n if j < 0 else j
            out.append(' ' * (j - i)); i = j
        elif c in '"\'':
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == '\\' else 1
            out.append(c + re.sub(r'[^\n]', ' ', text[i + 1:j]) + c); i = j + 1
        else:
            out.append(c); i += 1
    return ''.join(out)

TOKEN = re.compile(r'\b(__TRY|__EXCEPT\w*|__FINALLY\w*|return|goto|break|continue|for|while|do|switch)\b|'
                   r'^[ \t]*(\w+)[ \t]*:(?!:)|[{}();]', re.M)
MUTANT = re.compile(r'^[ \t]*#[ \t]*ifdef[ \t]+SG_MUTANT_\w+.*?^[ \t]*#[ \t]*endif', re.M | re.S)

def check(path):
    try:
        text = strip_code(open(path, errors='replace').read())
    except OSError:
        return []
    # a mutant's lines are not built: blank them (line numbers kept)
    text = MUTANT.sub(lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text)
    found = []
    # a stack of __TRY bodies: the loop/switch statements open in each (their brace depth), the gotos
    # in it and the labels in it (a goto to a label in the same body stays in it)
    tries, depth, parens, pending_loop = [], 0, 0, 0
    line, pos = 1, 0
    for m in TOKEN.finditer(text):
        line += text.count('\n', pos, m.start()); pos = m.start()
        t = m.group(1) or m.group(0)
        if m.group(2):
            if tries and m.group(2) not in ('default', 'public', 'private'): tries[-1]['labels'].add(m.group(2))
            continue
        if t == '__TRY':
            tries.append({ 'loops': [], 'gotos': [], 'labels': set() })
        elif t.startswith('__EXCEPT') or t.startswith('__FINALLY'):
            if tries:
                body = tries.pop()
                for gline, label in body['gotos']:
                    if label not in body['labels']: found.append((path, gline, 'goto'))
        elif t in ('for', 'while', 'do', 'switch'):
            if tries: pending_loop = 1
        elif t == '(':
            parens += 1
        elif t == ')':
            parens = max( parens - 1, 0 )
        elif t == '{':
            depth += 1
            if tries and pending_loop:
                tries[-1]['loops'].append(depth); pending_loop = 0
        elif t == '}':
            if tries and tries[-1]['loops'] and tries[-1]['loops'][-1] == depth: tries[-1]['loops'].pop()
            depth -= 1
        elif t == ';':
            if not parens: pending_loop = 0   # a loop without braces ends here
        elif t == 'return' and tries:
            found.append((path, line, t))
        elif t == 'goto' and tries:
            label = re.match(r'goto\s+(\w+)', text[m.start():])
            tries[-1]['gotos'].append((line, label.group(1) if label else ''))
        elif t in ('break', 'continue') and tries and not tries[-1]['loops'] and not pending_loop:
            found.append((path, line, t))
    return found

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--patches', required=True)
    ap.add_argument('root')
    a = ap.parse_args()
    bad = []
    for rel in touched_files(a.patches):
        bad += check(os.path.join(a.root, rel))
    for path, line, what in bad:
        print('%s:%d: %s out of a __TRY body (leaves its frame on the exception list)' % (os.path.relpath(path, a.root), line, what))
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
