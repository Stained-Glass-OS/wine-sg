// tlbgen: writes an IDL type library of mscorlib's COM-visible types (the list, GUIDs, interface types and
// class interfaces from the reference assembly's attributes in guids.txt; members from the running corlib)
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;

class Info { public string Kind, Name, Guid; public Dictionary<string,string> A = new Dictionary<string,string>(); public Type T; }

class TlbGen
{
    static Dictionary<string, Info> infos = new Dictionary<string, Info>();
    static HashSet<string> emitted = new HashSet<string>();
    static HashSet<string> reserved = new HashSet<string>(new[]{ "interface","library","module","coclass","dispinterface","enum","struct","union","typedef","import","importlib","in","out","retval","default","source","string","handle","properties","methods","property","object","char","short","long","int","float","double","void","boolean","byte","hyper","signed","unsigned","const","case","switch","context_handle","version","uuid","lcid","propget","propput","propputref","id","vararg","optional","readonly","bindable","hidden","restricted","local","call_as","size_is","length_is","iid_is","range","pointer_default","ref","ptr","unique","noncreatable","appobject","control","dual","oleautomation","nonextensible","helpstring","helpcontext","defaultvalue","immediatebind","requestedit","displaybind","defaultbind","static","inline","register","extern","volatile","auto","sizeof","error_status_t","wchar_t","small","Object","Delegate","Enum","Array","Type","Guid","Decimal","DateTime","Attribute","Exception","Activator","Assembly","Module","Thread" });

    static string Id(string n) { return reserved.Contains(n) ? n + "_" : n; }
    static string Short(Type t) { return t.Name; }

    static string Iface(Type t)
    {
        Info i;
        if (infos.TryGetValue(t.FullName ?? "", out i) && i.Kind == "interface" && Visible(i) && !t.IsGenericType) return Id(t.Name) + "*";
        if (t == typeof(Type)) return "_Type*";
        if (t == typeof(Assembly)) return "_Assembly*";
        return null;
    }

    static bool Visible(Info i) { string v; return i.A.TryGetValue("comvisible", out v) && v == "true"; }

    // the IDL type of a parameter or return value, or null when it cannot be expressed
    static string Map(Type t)
    {
        if (t == typeof(void)) return "void";
        if (t == typeof(bool)) return "VARIANT_BOOL";
        if (t == typeof(byte)) return "unsigned char";
        if (t == typeof(sbyte)) return "signed char";
        if (t == typeof(short)) return "short";
        if (t == typeof(ushort) || t == typeof(char)) return "unsigned short";
        if (t == typeof(int)) return "long";
        if (t == typeof(uint)) return "unsigned long";
        if (t == typeof(long)) return "hyper";
        if (t == typeof(ulong)) return "unsigned hyper";
        if (t == typeof(float)) return "float";
        if (t == typeof(double)) return "double";
        if (t == typeof(string)) return "BSTR";
        if (t == typeof(object)) return "VARIANT";
        if (t == typeof(decimal)) return "DECIMAL";
        if (t == typeof(DateTime)) return "DATE";
        if (t == typeof(Guid)) return "GUID";
        if (t == typeof(IntPtr) || t == typeof(UIntPtr)) return "long";
        if (t.IsArray && t.GetArrayRank() == 1)
        {
            var e = Map(t.GetElementType());
            if (e == null || e.StartsWith("SAFEARRAY")) return null;
            /* widl takes no interface pointers nor enums in a SAFEARRAY: VARIANTs and longs */
            if (e.EndsWith("*")) e = "VARIANT";
            if (t.GetElementType().IsEnum) e = "long";
            return "SAFEARRAY(" + e + ")";
        }
        if (t.IsEnum)
        {
            Info i;
            if (infos.TryGetValue(t.FullName ?? "", out i) && Visible(i) && emitted.Contains(t.FullName)) return Id(t.Name);
            return "long";
        }
        if (t.IsGenericType || t.IsGenericParameter || t.IsPointer || t.IsByRef) return null;
        var f = Iface(t);
        if (f != null) return f;
        if (t.IsInterface || t.IsClass) return "IUnknown*";
        return null;
    }

    static StringBuilder sb = new StringBuilder();
    static HashSet<string> usedGuids = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

    static void EmitEnum(Info i)
    {
        var t = i.T;
        var names = Enum.GetNames(t);
        var vals = Enum.GetValues(t);
        var under = Enum.GetUnderlyingType(t);
        if (under == typeof(long) || under == typeof(ulong) || under == typeof(uint)) return; // enums are 32-bit in a type library
        if (usedGuids.Add(i.Guid)) sb.AppendFormat("    typedef [uuid({0}), version(1.0)]\n    enum {{\n", i.Guid);
        else sb.Append("    typedef [version(1.0)]\n    enum {\n");
        var seen = new HashSet<string>();
        for (int k = 0; k < names.Length; k++)
        {
            string n = Id(t.Name) + "_" + names[k];
            if (!seen.Add(n)) continue;
            long v = Convert.ToInt64(vals.GetValue(k));
            sb.AppendFormat("        {0} = {1}{2}\n", n, v, k + 1 < names.Length ? "," : "");
        }
        sb.AppendFormat("    }} {0};\n\n", Id(t.Name));
        emitted.Add(t.FullName);
    }

    static string MethodLine(MethodInfo m, string name, Dictionary<string,int> used, out string prop)
    {
        prop = null;
        var ps = m.GetParameters();
        var parts = new List<string>();
        foreach (var p in ps)
        {
            Type pt = p.ParameterType;
            bool byref = pt.IsByRef;
            if (byref) pt = pt.GetElementType();
            var mt = Map(pt);
            if (mt == null || mt == "void") return null;
            string dir = byref ? (p.IsOut ? "[out]" : "[in, out]") : "[in]";
            parts.Add(dir + " " + mt + (byref ? "*" : "") + " " + Id(p.Name ?? "arg") + "_p");
        }
        if (m.ReturnType != typeof(void))
        {
            var rt = Map(m.ReturnType);
            if (rt == null) return null;
            parts.Add("[out, retval] " + rt + "* pRetVal");
        }
        string n = name;
        if (m.IsSpecialName && (name.StartsWith("get_") || name.StartsWith("set_")))
        {
            prop = name.StartsWith("get_") ? "propget" : "propput";
            n = name.Substring(4);
        }
        string key = (prop ?? "") + n;
        int count;
        used.TryGetValue(key, out count);
        used[key] = count + 1;
        if (count > 0) n = n + "_" + (count + 1);
        return string.Format("HRESULT {0}({1});", Id(n), string.Join(", ", parts));
    }

    // A member's [DispId] (IEnumerable.GetEnumerator is DISPID_NEWENUM), else the next default id
    static int DispId(Type t, MethodInfo m, int next)
    {
        object[] a = m.GetCustomAttributes(typeof(System.Runtime.InteropServices.DispIdAttribute), false);
        if (a.Length == 0 && m.IsSpecialName && m.Name.Length > 4)
        {
            var p = t.GetProperty(m.Name.Substring(4), BindingFlags.Instance | BindingFlags.Public | BindingFlags.DeclaredOnly);
            if (p != null) a = p.GetCustomAttributes(typeof(System.Runtime.InteropServices.DispIdAttribute), false);
        }
        // the reference assembly's one [DispId], which Wine Mono's corlib does not carry
        if (a.Length == 0 && t.FullName == "System.Collections.IEnumerable" && m.Name == "GetEnumerator") return -4;
        return a.Length > 0 ? ((System.Runtime.InteropServices.DispIdAttribute)a[0]).Value : next;
    }

    static void EmitInterface(Info i)
    {
        var t = i.T;
        if (!usedGuids.Add(i.Guid)) return;
        string itype; i.A.TryGetValue("itype", out itype);
        bool unk = itype == "InterfaceIsIUnknown";
        bool disp = itype == "InterfaceIsIDispatch";
        var lines = new List<string>();
        var used = new Dictionary<string,int>();
        int id = 0x60020000;
        foreach (var m in t.GetMethods(BindingFlags.Instance | BindingFlags.Public | BindingFlags.DeclaredOnly))
        {
            if (m.IsGenericMethod) continue;
            string prop;
            var l = MethodLine(m, m.Name, used, out prop);
            if (l == null) continue;
            var attrs = new List<string>();
            if (!unk) attrs.Add("id(0x" + DispId(t, m, id++).ToString("x8") + ")");
            if (prop != null) attrs.Add(prop);
            lines.Add((attrs.Count > 0 ? "[" + string.Join(", ", attrs) + "] " : "") + l);
        }
        if (disp)
        {
            sb.AppendFormat("    [uuid({0}), version(1.0)]\n    dispinterface {1} {{\n    properties:\n    methods:\n", i.Guid, Id(t.Name));
            foreach (var l in lines) sb.Append("        ").Append(l).Append('\n');
            sb.Append("    };\n\n");
        }
        else
        {
            sb.AppendFormat("    [odl, uuid({0}), version(1.0){1}, oleautomation]\n    interface {2} : {3} {{\n",
                            i.Guid, unk ? "" : ", dual", Id(t.Name), unk ? "IUnknown" : "IDispatch");
            foreach (var l in lines) sb.Append("        ").Append(l).Append('\n');
            sb.Append("    };\n\n");
        }
        emitted.Add(t.FullName);
    }

    static void Main(string[] args)
    {
        foreach (var line in File.ReadAllLines(args[0]))
        {
            var f = line.Split('\t');
            if (f.Length < 4) continue;
            var i = new Info { Kind = f[0], Name = f[1], Guid = f[2] };
            foreach (var a in f[3].Split(',')) { var kv = a.Split('='); if (kv.Length == 2) i.A[kv[0]] = kv[1]; }
            infos[i.Name] = i;
        }
        var corlib = typeof(object).Assembly;
        foreach (var i in infos.Values)
        {
            i.T = corlib.GetType(i.Name, false);
            if (i.T != null && i.Guid == "-") i.Guid = i.T.GUID.ToString().ToUpperInvariant();
        }
        var list = infos.Values.Where(x => x.T != null && Visible(x) && !x.T.IsGenericType && x.T.IsPublic).ToList();
        sb.Append("/*\n * mscorlib.tlb: the COM-visible types of the .NET Framework's mscorlib.\n *\n"
            + " * Generated by wine-sg tools/mscorlib-tlb/gen.sh (do not edit): the type list and COM attributes come\n"
            + " * from Mono's MIT-licensed reference assembly source, members from Wine Mono's corlib.\n *\n"
            + " * This library is free software; you can redistribute it and/or\n"
            + " * modify it under the terms of the GNU Lesser General Public\n"
            + " * License as published by the Free Software Foundation; either\n"
            + " * version 2.1 of the License, or (at your option) any later version.\n */\n\n");
        sb.Append("#if 0\n#pragma makedep regtypelib\n#endif\n\n");
        sb.Append("import \"oaidl.idl\";\nimport \"ocidl.idl\";\n\n");
        sb.Append("[\n    uuid(BED7F4EA-1A96-11D2-8F08-00A0C9A6186D),\n    version(2.4),\n    helpstring(\"mscorlib.dll\")\n]\nlibrary mscorlib\n{\n    importlib(\"stdole2.tlb\");\n\n");
        foreach (var i in list.Where(x => x.Kind == "interface")) sb.AppendFormat("    interface {0};\n", Id(i.T.Name));
        foreach (var i in list.Where(x => x.Kind == "interface" && i_disp(x))) { }
        sb.Append('\n');
        foreach (var i in list.Where(x => x.Kind == "enum")) EmitEnum(i);
        // interfaces whose names collide (same short name in two namespaces) keep the first
        var names = new HashSet<string>();
        foreach (var i in list.Where(x => x.Kind == "interface"))
        {
            if (!names.Add(i.T.Name)) continue;
            EmitInterface(i);
        }
        sb.Append("};\n");
        Console.Write(sb.ToString());
    }
    static bool i_disp(Info x) { return false; }
}
