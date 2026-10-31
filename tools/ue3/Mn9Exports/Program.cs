using System.Text;
using UELib;
using UELib.Core;

if (args.Length < 2) { Console.Error.WriteLine("usage: Mn9Exports <package.upk> <out.jsonl>"); return 2; }
string path = args[0], outp = args[1];
var pkg = UnrealLoader.LoadPackage(path, FileAccess.Read);
pkg.InitializePackage(UnrealPackage.InitFlags.All);
byte[] file = File.ReadAllBytes(path);
var sb = new StringBuilder();

static int FindPair(byte[] b, int start, int a, int c)
{
    for (int i = start; i + 8 <= b.Length; i++)
        if (BitConverter.ToInt32(b, i) == a && BitConverter.ToInt32(b, i + 4) == c) return i;
    return -1;
}

var sum = pkg.Summary;
sb.Append("{\"type\":\"summary\"")
  .Append(",\"header_size\":").Append(sum.HeaderSize)
  .Append(",\"depends_offset\":").Append(sum.DependsOffset)
  .Append(",\"import_export_guids_offset\":").Append(sum.ImportExportGuidsOffset)
  .Append(",\"thumbnail_table_offset\":").Append(sum.ThumbnailTableOffset)
  .Append(",\"name_offset\":").Append(sum.NameOffset)
  .Append(",\"import_offset\":").Append(sum.ImportOffset)
  .Append(",\"export_offset\":").Append(sum.ExportOffset)
  .Append("}\n");

for (int i = 0; i < pkg.Exports.Count; i++)
{
    var e = pkg.Exports[i];
    byte[] buf = new byte[e.Size];
    Array.Copy(file, e.Offset, buf, 0, e.Size);
    int a = FindPair(buf, 0, e.SerialSize, e.SerialOffset);
    int b = FindPair(buf, 0, e.SerialOffset, e.SerialSize);
    int so, ss;
    if (a >= 0) { ss = e.Offset + a; so = ss + 4; }
    else if (b >= 0) { so = e.Offset + b; ss = so + 4; }
    else { so = ss = -1; }

    string name = e.ObjectName.ToString().Replace("\\", "\\\\").Replace("\"", "\\\"");
    sb.Append("{\"i\":").Append(i)
      .Append(",\"name\":\"").Append(name).Append('"')
      .Append(",\"table_offset\":").Append(e.Offset)
      .Append(",\"table_size\":").Append(e.Size)
      .Append(",\"serial_offset\":").Append(e.SerialOffset)
      .Append(",\"serial_size\":").Append(e.SerialSize)
      .Append(",\"so_field\":").Append(so)
      .Append(",\"ss_field\":").Append(ss)
      .Append("}\n");
}
File.WriteAllText(outp, sb.ToString());
Console.WriteLine($"exports={pkg.Exports.Count}");
return 0;
