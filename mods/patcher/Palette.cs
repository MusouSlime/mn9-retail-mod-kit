using System.Globalization;

namespace Mn9Patcher;

/// <summary>Beck boss-palette selection for the palette shim.</summary>
public static class PaletteSettings
{
    public static readonly string[] Names =
    {
        "Regular Beck",
        "Boss 1 — Pyrogen",
        "Boss 2 — Cryosphere",
        "Boss 3 — Dynatron",
        "Boss 4 — Seismic",
        "Boss 5 — Battalion",
        "Boss 6 — Brandish",
        "Boss 7 — Countershade",
        "Boss 8 — Trinity",
        "Random boss per stage",
    };

    public static string PathFor(string gameDir) =>
        System.IO.Path.Combine(gameDir, ".mn9charsel", "palette.conf");

    public static int Read(string gameDir)
    {
        try
        {
            string path = PathFor(gameDir);
            if (!File.Exists(path)) return 0;
            string mode = "regular";
            int boss = 0;
            foreach (string raw in File.ReadAllLines(path))
            {
                string line = raw.Trim();
                int eq = line.IndexOf('=');
                if (eq <= 0) continue;
                string key = line[..eq].Trim(), value = line[(eq + 1)..].Trim();
                if (key == "mode") mode = value;
                else if (key == "boss") int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out boss);
            }
            if (mode == "random") return 9;
            if (mode == "boss" && boss >= 1 && boss <= 8) return boss;
            return 0;
        }
        catch { return 0; }
    }

    public static string Write(string gameDir, int index)
    {
        try
        {
            string path = PathFor(gameDir);
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(path)!);
            int boss = Math.Clamp(index, 1, 8);
            string mode = index == 9 ? "random" : index >= 1 && index <= 8 ? "boss" : "regular";
            File.WriteAllText(path,
                $"enabled=1{Environment.NewLine}" +
                $"mode={mode}{Environment.NewLine}" +
                $"boss={(mode == "regular" ? 1 : boss)}{Environment.NewLine}");
            return mode switch
            {
                "random" => "Palette: random boss per stage.",
                "boss" => $"Palette: {Names[boss]}.",
                _ => "Palette: regular Beck.",
            };
        }
        catch (Exception ex)
        {
            return "Palette settings failed: " + ex.Message;
        }
    }
}
