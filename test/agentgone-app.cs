// agentgone-app: a .NET program whose garbage collector stops its busy threads many times
// (wine-sg 7843). Prints gc=N when done.
using System; using System.Threading;
class P {
  static volatile bool stop;
  static void Main(string[] a) {
    int secs = a.Length > 0 ? int.Parse(a[0]) : 20;
    var ts = new Thread[3];
    for (int i = 0; i < ts.Length; i++) {
      ts[i] = new Thread(() => { object keep = null; long n = 0; while (!stop) { keep = new byte[256 + (int)(n++ % 4096)]; } GC.KeepAlive(keep); });
      ts[i].IsBackground = true; ts[i].Start();
    }
    var end = DateTime.UtcNow.AddSeconds(secs); int gcs = 0;
    while (DateTime.UtcNow < end) { GC.Collect(); gcs++; Thread.Sleep(50); }
    stop = true;
    foreach (var t in ts) t.Join(5000);
    Console.WriteLine("gc=" + gcs);
  }
}
