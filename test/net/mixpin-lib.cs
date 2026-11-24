// mixpin-lib.cs: the library test/mixpin-gate.sh turns into a mixed-mode image (Stained Glass OS wine-sg 9452)
public class SgMixed
{
    public static int Answer() { return 42; }
    public string Where() { return System.AppDomain.CurrentDomain.FriendlyName; }
}
