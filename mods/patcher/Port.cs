using System.Diagnostics;
using System.Text;

namespace Mn9Patcher;

/// <summary>Settings for the ReXGlue Xbox 360 port (mn9-hybrid rex backend).</summary>
public sealed class PortSettings
{
    public string Backend = "rex";          // rex | pc
    public string Quality = "high";         // max | high | balanced | performance
    public int Width = 960;
    public int Height = 540;
    public bool Fullscreen;
    public bool Vsync;
    public bool SkipIntro = true;
    public string StartMode = "menu";       // title | menu | game

    // Fine quality overrides; empty means "follow the preset".
    public string Scale = "";
    public string Aniso = "";
    public string Msaa = "";
    public string Dither = "";
    public string AsyncShaders = "0";       // 0 = synchronous (valid frames)
    public int TextureCacheMb;              // 0 = runtime default

    public string LicenseMask = "4294967295";
    public string UserDataRoot = "";
    public Dictionary<string, string> Keybinds = new();  // keybind_a -> "Space"

    public string Resolution => $"{Width}x{Height}";
}

public sealed class PortSaveInfo
{
    public string Id { get; set; } = "";
    public string Path { get; set; } = "";
    public DateTimeOffset Time { get; set; }
    public int Files { get; set; }
    public long Bytes { get; set; }
}

/// <summary>
/// Drives the ReXGlue recompilation through ~/Projects/mn9-hybrid: writes its
/// shared config, manages saves/content/diagnostics and launches the "rex"
/// backend instead of the Steam PC build.
/// </summary>
public static class Port
{
    public static string BaseDir =>
        Environment.GetEnvironmentVariable("MN9_HYBRID_DIR")
        ?? Path.Combine(Home(), "Projects", "mn9-hybrid");

    public static string HybridBin => Path.Combine(BaseDir, "mn9-hybrid");
    public static string ConfPath => Path.Combine(BaseDir, "mn9-hybrid.conf");
    public static string BackupDir => Path.Combine(BaseDir, "save-backups");

    public static string RecompDir =>
        Environment.GetEnvironmentVariable("MN9_REX_DIR")
        ?? Path.Combine(Home(), "Projects", "mn9-rexglue");

    public static string DefaultUserDataRoot => Path.Combine(RecompDir, "userdata");
    public static string GameRoot => Path.Combine(RecompDir, "game");

    public static string ContentDir =>
        Path.Combine(GameRoot, "Content", "0000000000000000", "584114A4", "00000002");

    public static string LogDir =>
        Path.Combine(RecompDir, "out", "build", "linux-amd64-release", "logs");

    public static readonly string[] Qualities = { "max", "high", "balanced", "performance" };
    public static readonly string[] StartModes = { "title", "menu", "game" };

    public static readonly string[] KeybindNames =
    {
        "keybind_a", "keybind_b", "keybind_x", "keybind_y",
        "keybind_start", "keybind_back",
        "keybind_dpad_up", "keybind_dpad_down", "keybind_dpad_left", "keybind_dpad_right",
        "keybind_left_shoulder", "keybind_right_shoulder",
        "keybind_left_trigger", "keybind_right_trigger",
        "keybind_lstick_press", "keybind_rstick_press",
    };

    private static string Home() =>
        Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);

    public static bool IsAvailable() =>
        File.Exists(HybridBin) && File.Exists(Path.Combine(RecompDir, "run-mn9.sh"));

    public static bool IsRunning() =>
        Process.GetProcessesByName("mn9").Length > 0;

    public static string SaveRoot(PortSettings s) =>
        string.IsNullOrWhiteSpace(s.UserDataRoot) ? DefaultUserDataRoot : s.UserDataRoot;

    // --- config -------------------------------------------------------------

    public static PortSettings Read()
    {
        var s = new PortSettings();
        if (!File.Exists(ConfPath)) return s;
        foreach (string raw in File.ReadAllLines(ConfPath))
        {
            string line = raw.Trim();
            if (line.Length == 0 || line.StartsWith('#')) continue;
            int eq = line.IndexOf('=');
            if (eq <= 0) continue;
            string key = line[..eq].Trim(), value = line[(eq + 1)..].Trim();
            switch (key)
            {
                case "backend" when value.Length > 0: s.Backend = value; break;
                case "quality" when value.Length > 0: s.Quality = value; break;
                case "fullscreen": s.Fullscreen = value != "0"; break;
                case "vsync": s.Vsync = value != "0"; break;
                case "skip_intro": s.SkipIntro = value != "0"; break;
                case "start_mode" when value.Length > 0: s.StartMode = value; break;
                case "scale_override": s.Scale = value; break;
                case "aniso_override": s.Aniso = value; break;
                case "msaa": s.Msaa = value; break;
                case "dither": s.Dither = value; break;
                case "async_shaders": s.AsyncShaders = value.Length > 0 ? value : "0"; break;
                case "texture_cache_mb" when int.TryParse(value, out int mb): s.TextureCacheMb = mb; break;
                case "license_mask" when value.Length > 0: s.LicenseMask = value; break;
                case "user_data_root": s.UserDataRoot = value; break;
                case "resolution":
                    string[] wh = value.Split('x');
                    if (wh.Length == 2 && int.TryParse(wh[0], out int w) && int.TryParse(wh[1], out int h))
                    {
                        s.Width = w;
                        s.Height = h;
                    }
                    break;
                default:
                    if (key.StartsWith("keybind_", StringComparison.Ordinal))
                        s.Keybinds[key] = value;
                    break;
            }
        }
        return s;
    }

    /// <summary>Rewrite the keys we own in mn9-hybrid.conf, keeping comments and
    /// any keys the launcher does not manage.</summary>
    public static string Apply(PortSettings s)
    {
        if (!File.Exists(ConfPath))
            return $"Port config not found: {ConfPath}";

        var values = new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["backend"] = s.Backend,
            ["resolution"] = s.Resolution,
            ["fullscreen"] = s.Fullscreen ? "1" : "0",
            ["vsync"] = s.Vsync ? "1" : "0",
            ["skip_intro"] = s.SkipIntro ? "1" : "0",
            ["skip_presses"] = s.StartMode switch { "title" => "0", "game" => "4", _ => "2" },
            ["start_mode"] = s.StartMode,
            ["quality"] = s.Quality,
            ["scale_override"] = s.Scale,
            ["aniso_override"] = s.Aniso,
            ["msaa"] = s.Msaa,
            ["dither"] = s.Dither,
            ["async_shaders"] = s.AsyncShaders,
            ["texture_cache_mb"] = s.TextureCacheMb.ToString(),
            ["license_mask"] = s.LicenseMask,
            ["user_data_root"] = s.UserDataRoot,
        };
        foreach (var kv in s.Keybinds)
            values[kv.Key] = kv.Value;

        var lines = File.ReadAllLines(ConfPath).ToList();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        for (int i = 0; i < lines.Count; i++)
        {
            string line = lines[i];
            int eq = line.IndexOf('=');
            if (eq <= 0 || line.TrimStart().StartsWith('#')) continue;
            string key = line[..eq].Trim();
            if (values.TryGetValue(key, out string? value))
            {
                lines[i] = $"{key}={value}";
                seen.Add(key);
            }
        }
        foreach (var kv in values)
            if (!seen.Contains(kv.Key))
                lines.Add($"{kv.Key}={kv.Value}");

        File.WriteAllLines(ConfPath, lines);
        return $"Port settings written: {s.Quality}, {s.Resolution}, " +
               $"{(s.Fullscreen ? "fullscreen" : "windowed")}, " +
               $"vsync {(s.Vsync ? "on" : "off")}, start {s.StartMode}.";
    }

    public static string Launch(PortSettings s)
    {
        if (s.Backend == "pc")
            return LaunchPc();

        if (!IsAvailable())
            return $"ReXGlue port not found ({HybridBin}).";
        if (IsRunning())
            return "The port is already running.";

        string applied = Apply(s);
        try
        {
            var psi = new ProcessStartInfo
            {
                FileName = "/usr/bin/setsid",
                WorkingDirectory = BaseDir,
                UseShellExecute = false,
            };
            psi.ArgumentList.Add(HybridBin);
            psi.ArgumentList.Add("rex");
            Process.Start(psi);
        }
        catch (Exception ex)
        {
            return applied + Environment.NewLine + "Launch failed: " + ex.Message;
        }
        return applied + Environment.NewLine + "Launched the ReXGlue port.";
    }

    public static string LaunchPc()
    {
        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = "steam",
                Arguments = "-applaunch 314710",
                UseShellExecute = false,
            });
            return "Launched the PC build through Steam.";
        }
        catch (Exception ex)
        {
            return "PC launch failed: " + ex.Message;
        }
    }

    // --- saves --------------------------------------------------------------

    public static IReadOnlyList<string> SaveFiles(PortSettings s)
    {
        string root = SaveRoot(s);
        if (!Directory.Exists(root)) return Array.Empty<string>();
        return Directory.GetFiles(root, "SystemData*", SearchOption.AllDirectories)
                        .OrderBy(p => p, StringComparer.Ordinal)
                        .ToList();
    }

    public static string SaveStatus(PortSettings s)
    {
        string root = SaveRoot(s);
        var files = SaveFiles(s);
        if (files.Count == 0) return $"No port save data in {root}";
        long bytes = files.Sum(f => new FileInfo(f).Length);
        string when = files.Max(f => File.GetLastWriteTime(f)).ToString("yyyy-MM-dd HH:mm");
        return $"{files.Count} file(s), {FormatSize(bytes)}, last write {when}  —  {root}";
    }

    public static string BackupSave(PortSettings s)
    {
        var files = SaveFiles(s);
        if (files.Count == 0) return "Nothing to back up (no port save data yet).";
        string root = SaveRoot(s);
        string id = DateTime.Now.ToString("yyyy-MM-dd_HH-mm-ss") + "_port";
        string dest = Path.Combine(BackupDir, id);
        Directory.CreateDirectory(dest);
        var manifest = new StringBuilder();
        foreach (string file in files)
        {
            string rel = Path.GetRelativePath(root, file);
            string target = Path.Combine(dest, rel);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.Copy(file, target, overwrite: true);
            manifest.AppendLine($"{rel}\t{new FileInfo(file).Length}\t{File.GetLastWriteTime(file):O}");
        }
        File.WriteAllText(Path.Combine(dest, "manifest.tsv"), manifest.ToString());
        return $"Backed up {files.Count} file(s) to {dest}";
    }

    public static IReadOnlyList<PortSaveInfo> ListBackups()
    {
        if (!Directory.Exists(BackupDir)) return Array.Empty<PortSaveInfo>();
        return Directory.GetDirectories(BackupDir)
            .Select(d => new PortSaveInfo
            {
                Id = Path.GetFileName(d),
                Path = d,
                Time = Directory.GetCreationTime(d),
                Files = Directory.GetFiles(d, "*", SearchOption.AllDirectories).Length,
                Bytes = Directory.GetFiles(d, "*", SearchOption.AllDirectories)
                                 .Sum(f => new FileInfo(f).Length),
            })
            .OrderByDescending(b => b.Id, StringComparer.Ordinal)
            .ToList();
    }

    public static string RestoreLatestSave(PortSettings s)
    {
        var backups = ListBackups();
        if (backups.Count == 0) return "No port save backups yet.";
        var latest = backups[0];
        string root = SaveRoot(s);
        int restored = 0;
        foreach (string file in Directory.GetFiles(latest.Path, "*", SearchOption.AllDirectories))
        {
            if (Path.GetFileName(file) == "manifest.tsv") continue;
            string rel = Path.GetRelativePath(latest.Path, file);
            string target = Path.Combine(root, rel);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.Copy(file, target, overwrite: true);
            restored++;
        }
        return $"Restored {restored} file(s) from {latest.Id}";
    }

    public static string ClearSave(PortSettings s)
    {
        var files = SaveFiles(s);
        if (files.Count == 0) return "No port save data to clear.";
        foreach (string file in files)
        {
            try { File.SetAttributes(file, FileAttributes.Normal); File.Delete(file); }
            catch { }
        }
        return $"Deleted {files.Count} port save file(s).";
    }

    public static string OpenFolder(string path)
    {
        try
        {
            Directory.CreateDirectory(path);
            Process.Start(new ProcessStartInfo { FileName = "xdg-open", Arguments = $"\"{path}\"", UseShellExecute = false });
            return path;
        }
        catch { return path; }
    }

    // --- content ------------------------------------------------------------

    public static string ContentStatus()
    {
        if (!Directory.Exists(ContentDir)) return "No addon packages installed.";
        var files = Directory.GetFiles(ContentDir);
        if (files.Length == 0) return "No addon packages installed.";
        long bytes = files.Sum(f => new FileInfo(f).Length);
        return $"{files.Length} addon package(s), {FormatSize(bytes)}  —  {ContentDir}";
    }

    // --- diagnostics --------------------------------------------------------

    public static string? NewestLog()
    {
        if (!Directory.Exists(LogDir)) return null;
        return Directory.GetFiles(LogDir, "mn9_*.log")
                        .OrderByDescending(File.GetLastWriteTime)
                        .FirstOrDefault();
    }

    public static string TailLog(int lines = 14)
    {
        string? log = NewestLog();
        if (log == null) return "No port log yet.";
        try
        {
            var all = File.ReadAllLines(log);
            var tail = all.Skip(Math.Max(0, all.Length - lines));
            return $"# {Path.GetFileName(log)}\n" + string.Join('\n', tail);
        }
        catch (Exception ex)
        {
            return "Log read failed: " + ex.Message;
        }
    }

    public static string ClearShaderCache(PortSettings s)
    {
        string dir = Path.Combine(SaveRoot(s), "cache", "shaders");
        if (!Directory.Exists(dir)) return "Shader cache is already empty.";
        try
        {
            Directory.Delete(dir, recursive: true);
            return $"Cleared {dir}";
        }
        catch (Exception ex)
        {
            return "Shader cache clear failed: " + ex.Message;
        }
    }

    public static string StatusLine(PortSettings s)
    {
        string run = IsRunning() ? "running" : "stopped";
        string save = SaveFiles(s).Count > 0 ? "save present" : "no save";
        string content = Directory.Exists(ContentDir) && Directory.GetFiles(ContentDir).Length > 0
            ? "DLC staged" : "no DLC";
        return $"Port: {run} · {save} · {content} · quality {s.Quality} · {s.Resolution}";
    }

    private static string FormatSize(long bytes) => bytes switch
    {
        < 1024 => $"{bytes} B",
        < 1024 * 1024 => $"{bytes / 1024.0:0.0} KB",
        _ => $"{bytes / (1024.0 * 1024.0):0.0} MB",
    };
}
