#!/usr/bin/python3
# extract-guids.py MSCORLIB_CS: list mscorlib's types with their COM attributes (explicit GUID,
# ComVisible, InterfaceType, ClassInterface) from Mono's MIT-licensed reference assembly source
# (mono/external/binary-reference-assemblies/src/v4.7.2/mscorlib.cs), one per line:
# kind<TAB>full name<TAB>guid or -<TAB>attributes
import re,sys
src=open(sys.argv[1],encoding='utf-8').read()
# namespace tracking: find "namespace X {" blocks (file uses explicit namespaces)
out=[]
ns_stack=[]
lines=src.split('\n')
pending_guid=None; pending_attrs=[]
ns=None
for ln in lines:
    m=re.match(r'\s*namespace ([\w.]+)\s*$', ln)
    if m: ns=m.group(1); continue
    g=re.search(r'GuidAttribute\("([0-9A-Fa-f-]{36})"\)', ln)
    if g: pending_guid=g.group(1)
    it=re.search(r'InterfaceTypeAttribute\(\(?(?:System\.Runtime\.InteropServices\.ComInterfaceType\.)?(\w+)\)?', ln)
    if it: pending_attrs.append('itype='+it.group(1))
    cv=re.search(r'ComVisibleAttribute\((true|false)\)', ln)
    if cv: pending_attrs.append('comvisible='+cv.group(1))
    ci=re.search(r'ClassInterfaceAttribute\(\(?(?:System\.Runtime\.InteropServices\.ClassInterfaceType\.)?(\w+)\)?', ln)
    if ci: pending_attrs.append('classif='+ci.group(1))
    t=re.match(r'\s*public (?:abstract |sealed |static |partial |unsafe |readonly |ref )*(class|struct|interface|enum|delegate)\s+(?:[\w.<>\[\], ]+\s+)?(\w+)', ln)
    if t:
        kind,name=t.group(1),t.group(2)
        if kind=='delegate':
            name=re.match(r'.*delegate\s+.*?\s(\w+)\s*\(', ln).group(1) if re.match(r'.*delegate\s+.*?\s(\w+)\s*\(', ln) else name
        out.append('%s\t%s.%s\t%s\t%s'%(kind, ns, name, pending_guid or '-', ','.join(pending_attrs)))
        pending_guid=None; pending_attrs=[]
    elif ln.strip().startswith('[') or ln.strip()=='' :
        pass
    else:
        if not ln.strip().startswith('//'):
            pending_guid=None; pending_attrs=[]
print('\n'.join(out))
