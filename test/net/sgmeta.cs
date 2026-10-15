// Fixture for the NET-META / NET-HOST gates, built with Wine Mono's mcs:
//   mcs -target:library -out:sgmeta.dll sgmeta.cs
//   mcs -target:library -keyfile:sgtest.snk -out:sgmeta-signed.dll sgmeta.cs
using System;
using System.Runtime.InteropServices;
[assembly: System.Reflection.AssemblyVersion("1.2.3.4")]
namespace SgMeta
{
    public interface IShape { double Area(); string Name { get; } }
    public enum Color { Red = 1, Green = 2, Blue = 4 }
    [AttributeUsage(AttributeTargets.All)] public class TagAttribute : Attribute { public string T; public TagAttribute(string t) { T = t; } }
    public delegate void Notify(int code);
    [Tag("circle")]
    public class Circle : IShape
    {
        public const int Sides = 0;
        public static readonly string Kind = "round";
        public double Radius;
        private int id;
        public event Notify Changed;
        public Circle(double r) { Radius = r; }
        public double Area() { return 3.14159 * Radius * Radius; }
        public string Name { get { return "circle"; } }
        public int Id { get { return id; } set { id = value; if (Changed != null) Changed(value); } }
        public class Inner { public int Z; }
        [DllImport("kernel32.dll", EntryPoint = "GetTickCount")] public static extern uint Tick();
        public T Pick<T>(T a, T b) { return a; }
    }
    public struct Point { public int X, Y; }
    public class Box<T> where T : class { public T Value; }
}
