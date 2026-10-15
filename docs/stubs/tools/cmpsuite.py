import sys,re,glob,os
a,b=sys.argv[1],sys.argv[2]
def load(d):
    r={}
    for f in sorted(glob.glob(d+'/*.out')):
        n=os.path.basename(f)
        fails=[];ex={}
        for l in open(f,errors='replace'):
            l=l.rstrip('\n')
            m=re.match(r'^[0-9a-f]{4}:(\w+): (\d+) tests executed \((\d+) marked as todo, (\d+) as flaky, (\d+) failures\)',l)
            if m: ex[m.group(1)]=tuple(int(x) for x in m.groups()[1:])
            if 'Test failed' in l: fails.append(re.sub(r'^[0-9a-f]{4}:','',re.sub(r'(\w+\.c):\d+:',r'\1:',l)))
            if re.search(r'Unhandled exception|Test marked todo|Test succeeded inside todo',l): fails.append(re.sub(r'^[0-9a-f]{4}:','',re.sub(r'(\w+\.c):\d+:',r'\1:',l)))
        r[n]=(fails,ex)
    return r
A,B=load(a),load(b)
bad=0
for n in sorted(set(A)|set(B)):
    fa,ea=A.get(n,([],{}));fb,eb=B.get(n,([],{}))
    new=[x for x in fb if x not in fa]; gone=[x for x in fa if x not in fb]
    if new or gone or ea.keys()!=eb.keys() or any(ea[k][3]!=eb[k][3] or ea[k][1]!=eb[k][1] for k in ea if k in eb):
        print('==',n)
        for x in new: print('  NEW  ',x)
        for x in gone: print('  GONE ',x)
        for k in sorted(set(ea)|set(eb)):
            if ea.get(k)!=eb.get(k): print('  ',k,ea.get(k),'->',eb.get(k))
        if new: bad+=1
print('new-failure files:',bad)
