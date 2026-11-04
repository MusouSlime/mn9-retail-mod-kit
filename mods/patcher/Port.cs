using System.Diagnostics;
using System.Text;

namespace Mn9Patcher;

/// <summary>Settings for the ReXGlue Xbox 360 port (managed install or mn9-hybrid).</summary>
public sealed class PortSettings
{
    public string Backend = "rex";          // rex | pc
    public string Quality = "high";         // max | high | balanced | performance
    public int GameHz = 120;                // 120 = 60 FPS, 60 = 30 FPS low-power
    public int Width = 1280;
    public int Height = 720;
    public bool Fullscreen;
    public bool Vsync;
    public bool SkipIntro = true;
    public string StartMode = "menu";       // title | menu | game
    public string BootMode = "fast";        // fast | normal | title | stage
    public int SkipPresses = 2;
    public int SkipGap = 4;
    public int SkipDelay = 14;

    // Fine quality overrides; empty means "follow the preset".
    public string Scale = "";
    public string Aniso = "";
    public string Msaa = "";
    public string Dither = "";
    public string Fxaa = "preset";          // preset | on | extreme | off
    public string AsyncShaders = "0";       // 0 = synchronous (valid frames)
    public int TextureCacheMb;              // 0 = runtime default
    public int TextureMipSkip;              // 0 = native, 1-3 = lower-resolution textures
    public bool LowMemory = true;           // 960x540 + reduced caches
    public bool MnkMode = true;             // keyboard as Xbox pad

    // Port audio post-processing. Per-bus gains / music replacement are PC-only.
    public bool AudioEnabled;
    public int AudioMaster = 100;           // 0..100
    public bool AudioMute;
    public bool AudioNight;

    // Addon packages staged into game/Content/.../584114A4/00000002.
    public bool DlcGolden = true;
    public bool DlcMegaXel = true;
    public bool DlcRay = true;
    public bool DlcRetro = true;

    // Managed install. Empty = platform default.
    public string InstallDir = "";
    public bool Managed;
    public bool ReplaceGame = true;

    public string LicenseMask = "4294967295";
    public string UserDataRoot = "";
    public Dictionary<string, string> Keybinds = new();  // keybind_a -> "Space"

    public string Resolution => $"{Width}x{Height}";
}

public sealed record DlcPackage(string Key, string Name, string Folder, string File);

public sealed class PortSaveInfo
{
    public string Id { get; set; } = "";
    public string Path { get; set; } = "";
    public DateTimeOffset Time { get; set; }
    public int Files { get; set; }
    public long Bytes { get; set; }
}

/// <summary>
/// Drives the ReXGlue recompilation: imports XBLA/addon packages into a
/// self-contained managed port, or falls back to the ~/Projects/mn9-hybrid dev
/// launcher. Runtime binaries come from the bundled port-runtime/&lt;rid&gt;.
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

    public static bool IsWindows => OperatingSystem.IsWindows();
    public static string RuntimeRid => IsWindows ? "win-x64" : "linux-x64";
    public static string RuntimeExeName => IsWindows ? "mn9.exe" : "mn9";
    public static string ExtractorName => IsWindows ? "extract_stfs.exe" : "extract_stfs";

    /// <summary>Runtime payload shipped next to the launcher executable.</summary>
    public static string RuntimeSourceDir =>
        Environment.GetEnvironmentVariable("MN9_PORT_RUNTIME_DIR")
        ?? Path.Combine(AppContext.BaseDirectory, "port-runtime", RuntimeRid);

    public static bool RuntimeBundled =>
        File.Exists(Path.Combine(RuntimeSourceDir, RuntimeExeName));

    public static string DefaultManagedRoot
    {
        get
        {
            string? overrideDir = Environment.GetEnvironmentVariable("MN9_PORT_DIR");
            if (!string.IsNullOrWhiteSpace(overrideDir)) return overrideDir!;
            if (IsWindows)
            {
                string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
                if (string.IsNullOrWhiteSpace(local))
                    local = Path.Combine(Home(), "AppData", "Local");
                return Path.Combine(local, "mn9-rexglue");
            }
            string xdg = Environment.GetEnvironmentVariable("XDG_DATA_HOME") ?? "";
            if (!string.IsNullOrWhiteSpace(xdg))
                return Path.Combine(xdg, "mn9-rexglue");
            return Path.Combine(Home(), ".local", "share", "mn9-rexglue");
        }
    }

    public static string RegistryPath
    {
        get
        {
            string dir = IsWindows
                ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "xel-launcher")
                : Path.Combine(Environment.GetEnvironmentVariable("XDG_CONFIG_HOME") ?? Path.Combine(Home(), ".config"), "xel-launcher");
            return Path.Combine(dir, "last-port.txt");
        }
    }

    public static string DefaultUserDataRoot => Path.Combine(RecompDir, "userdata");
    public static string GameRoot => Path.Combine(RecompDir, "game");
    public static string ContentRoot =>
        Path.Combine(GameRoot, "Content", "0000000000000000", "584114A4");
    public static string ContentDir => Path.Combine(ContentRoot, "00000002");

    public static string LogDir =>
        Path.Combine(RecompDir, "out", "build", "linux-amd64-release", "logs");

    public static readonly string[] Qualities = { "max", "high", "balanced", "performance" };
    public static readonly string[] StartModes = { "title", "menu", "game" };
    public static readonly string[] FxaaModes = { "preset", "on", "extreme", "off" };

    public static readonly DlcPackage[] DlcPackages =
    {
        new("dlc_golden", "Golden Beck",
            "Mighty No. 9 - Golden Beck (World) (Addon)",
            "A366B9CE07ADFCF0EEE198FD6119B848D5C2A4FD58"),
        new("dlc_megaxel", "MegaXel Form",
            "Mighty No. 9 - MegaXel Form (World) (Addon)",
            "A66887582AD8108F587C5DE229F7170721EBD67F58"),
        new("dlc_ray", "Ray Expansion",
            "Mighty No. 9 - Ray Expansion (World) (Addon)",
            "ADC68AF3A34A3EBC889BA4CF79CDEFDD077444CF58"),
        new("dlc_retro", "Retro Hero",
            "Mighty No. 9 - Retro Hero (World) (Addon)",
            "7FD109F2FD0BAC6C09C21E384168DE065AC92BFB58"),
    };

    public static readonly string[] KeybindNames =
    {
        "keybind_a", "keybind_b", "keybind_x", "keybind_y",
        "keybind_start", "keybind_back",
        "keybind_dpad_up", "keybind_dpad_down", "keybind_dpad_left", "keybind_dpad_right",
        "keybind_left_shoulder", "keybind_right_shoulder",
        "keybind_left_trigger", "keybind_right_trigger",
        "keybind_lstick_press", "keybind_rstick_press",
    };

    public static string Home() =>
        Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);

    public static string ManagedRootFor(PortSettings s) =>
        string.IsNullOrWhiteSpace(s.InstallDir) ? DefaultManagedRoot : s.InstallDir.Trim();

    public static string ManagedConfPathFor(PortSettings s) =>
        Path.Combine(ManagedRootFor(s), "port.conf");

    public static string ManagedGameDirFor(PortSettings s) =>
        Path.Combine(ManagedRootFor(s), "game");

    public static string ManagedAddonCacheFor(PortSettings s) =>
        Path.Combine(ManagedRootFor(s), "addons");

    public static string ManagedRuntimeExeFor(PortSettings s)
    {
        string root = ManagedRootFor(s);
        string direct = Path.Combine(root, RuntimeExeName);
        if (File.Exists(direct)) return direct;
        string build = Path.Combine(root, "out", "build",
            IsWindows ? "win-amd64-release" : "linux-amd64-release", RuntimeExeName);
        return File.Exists(build) ? build : direct;
    }

    public static bool ManagedGamePresent(PortSettings s) =>
        File.Exists(Path.Combine(ManagedGameDirFor(s), "default.xex"));

    private static bool DevPortAt(string root)
    {
        if (string.IsNullOrWhiteSpace(root)) return false;
        if (!File.Exists(Path.Combine(root, "game", "default.xex"))) return false;
        string build = Path.Combine(root, "out", "build",
            IsWindows ? "win-amd64-release" : "linux-amd64-release", RuntimeExeName);
        return File.Exists(Path.Combine(root, "run-mn9.sh")) ||
               File.Exists(Path.Combine(root, "run-mn9.bat")) ||
               File.Exists(Path.Combine(root, "run-mn9.ps1")) ||
               File.Exists(build);
    }

    public static bool ManagedReady(PortSettings s) =>
        ManagedGamePresent(s) &&
        (File.Exists(ManagedRuntimeExeFor(s)) || DevPortAt(ManagedRootFor(s)));

    public static bool UseManaged(PortSettings s) =>
        ManagedReady(s) || (s.Managed && ManagedGamePresent(s));

    private static bool ManagedInstallAt(string root) =>
        !string.IsNullOrWhiteSpace(root) &&
        File.Exists(Path.Combine(root, "game", "default.xex")) &&
        (File.Exists(Path.Combine(root, "port.conf")) || DevPortAt(root));

    private static string? ReadRegistryPath()
    {
        try
        {
            if (!File.Exists(RegistryPath)) return null;
            string path = File.ReadAllText(RegistryPath).Trim();
            return path.Length > 0 ? path : null;
        }
        catch { return null; }
    }

    public static void RememberManagedRoot(string root)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(RegistryPath)!);
            File.WriteAllText(RegistryPath, root + Environment.NewLine);
        }
        catch { }
    }

    /// <summary>Find an installed port even if the configured/typed folder was
    /// changed or the folder was moved. The typed folder wins when valid; then
    /// the saved registry, the default, and common install roots are checked.</summary>
    public static string? DiscoverManagedRoot(PortSettings s)
    {
        var candidates = new List<string>();
        void Add(string? p)
        {
            if (!string.IsNullOrWhiteSpace(p)) candidates.Add(p!.Trim());
        }
        Add(s.InstallDir);
        Add(Environment.GetEnvironmentVariable("MN9_PORT_DIR"));
        Add(ReadRegistryPath());
        Add(DefaultManagedRoot);
        Add(Path.Combine(Home(), "mn9-rexglue"));
        Add(Path.Combine(Home(), "Games", "mn9-rexglue"));
        Add(Path.Combine(Home(), "Projects", "mn9-rexglue"));
        if (!string.IsNullOrWhiteSpace(s.InstallDir))
        {
            string? parent = Path.GetDirectoryName(s.InstallDir.Trim());
            if (!string.IsNullOrWhiteSpace(parent))
                Add(Path.Combine(parent!, "mn9-rexglue"));
        }

        foreach (string c in candidates.Distinct(StringComparer.OrdinalIgnoreCase))
            if (ManagedInstallAt(c)) return c;

        // Last resort: shallow scan of likely roots for a moved port.
        string[] bases =
        {
            Path.Combine(Home(), "Games"),
            Path.Combine(Home(), "Projects"),
            Path.Combine(Home(), "Desktop"),
            Path.Combine(Home(), "Documents"),
            Path.Combine(Home(), ".local", "share"),
        };
        foreach (string b in bases)
        {
            if (!Directory.Exists(b)) continue;
            string? hit = ScanForManaged(b, 3);
            if (hit != null) return hit;
        }
        return null;
    }

    private static string? ScanForManaged(string dir, int depth)
    {
        if (depth < 0) return null;
        try
        {
            if (ManagedInstallAt(dir)) return dir;
            foreach (string sub in Directory.GetDirectories(dir))
            {
                string name = Path.GetFileName(sub);
                if (name.StartsWith(".", StringComparison.Ordinal) ||
                    name.Equals("Steam", StringComparison.OrdinalIgnoreCase) ||
                    name.Equals("node_modules", StringComparison.OrdinalIgnoreCase))
                    continue;
                string? hit = ScanForManaged(sub, depth - 1);
                if (hit != null) return hit;
            }
        }
        catch { }
        return null;
    }

    /// <summary>Resolve the port path before launch/apply. If the user typed a
    /// different folder, keep it when it is a real install; otherwise use the
    /// discovered one.</summary>
    public static bool ResolveManagedRoot(PortSettings s)
    {
        string? found = DiscoverManagedRoot(s);
        if (found == null) return false;
        if (!string.Equals(found, s.InstallDir, StringComparison.OrdinalIgnoreCase))
            s.InstallDir = found;
        s.Managed = true;
        return true;
    }

    public static string ContentDirFor(PortSettings s)
    {
        string game = UseManaged(s) ? ManagedGameDirFor(s) : GameRoot;
        return Path.Combine(game, "Content", "0000000000000000", "584114A4", "00000002");
    }

    public static string LogDirFor(PortSettings s) =>
        UseManaged(s)
            ? Path.Combine(ManagedRootFor(s), "logs")
            : LogDir;

    public static bool IsAvailable() =>
        ManagedReady(Read()) ||
        (!IsWindows && File.Exists(HybridBin) && File.Exists(Path.Combine(RecompDir, "run-mn9.sh")));

    public static bool HasConfig(PortSettings s)
    {
        if (ResolveManagedRoot(s)) return true;
        return File.Exists(ConfPath);
    }

    public static bool IsRunning() =>
        Process.GetProcessesByName("mn9").Length > 0;

    public static string SaveRoot(PortSettings s)
    {
        if (UseManaged(s))
        {
            return string.IsNullOrWhiteSpace(s.UserDataRoot)
                ? Path.Combine(ManagedRootFor(s), "userdata")
                : s.UserDataRoot;
        }
        return string.IsNullOrWhiteSpace(s.UserDataRoot) ? DefaultUserDataRoot : s.UserDataRoot;
    }

    public static string BackupRootFor(PortSettings s) =>
        UseManaged(s) ? Path.Combine(ManagedRootFor(s), "save-backups") : BackupDir;

    // --- config -------------------------------------------------------------

    public static PortSettings Read()
    {
        var s = new PortSettings();
        if (File.Exists(ConfPath)) ReadInto(s, ConfPath);

        string root = string.IsNullOrWhiteSpace(s.InstallDir) ? DefaultManagedRoot : s.InstallDir.Trim();
        s.InstallDir = root;
        string? found = DiscoverManagedRoot(s);
        if (found != null)
        {
            s.InstallDir = found;
            string managedConf = Path.Combine(found, "port.conf");
            if (File.Exists(managedConf)) ReadInto(s, managedConf);
            s.Managed = true;
            s.InstallDir = found;
        }
        return s;
    }

    private static void ReadInto(PortSettings s, string path)
    {
        foreach (string raw in File.ReadAllLines(path))
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
                case "game_hz" when int.TryParse(value, out int hz): s.GameHz = hz == 60 ? 60 : 120; break;
                case "fullscreen": s.Fullscreen = value != "0"; break;
                case "vsync": s.Vsync = value != "0"; break;
                case "skip_intro": s.SkipIntro = value != "0"; break;
                case "start_mode" when value.Length > 0: s.StartMode = value; break;
                case "boot_mode" when value.Length > 0: s.BootMode = value; break;
                case "skip_presses" when int.TryParse(value, out int presses): s.SkipPresses = presses; break;
                case "skip_gap" when int.TryParse(value, out int gap): s.SkipGap = gap; break;
                case "skip_delay" when int.TryParse(value, out int delay): s.SkipDelay = delay; break;
                case "scale_override": s.Scale = value; break;
                case "aniso_override": s.Aniso = value; break;
                case "msaa": s.Msaa = value; break;
                case "dither": s.Dither = value; break;
                case "fxaa" when value.Length > 0: s.Fxaa = value; break;
                case "async_shaders": s.AsyncShaders = value.Length > 0 ? value : "0"; break;
                case "texture_cache_mb" when int.TryParse(value, out int mb): s.TextureCacheMb = mb; break;
                case "texture_mip_skip" when int.TryParse(value, out int skip): s.TextureMipSkip = Math.Clamp(skip, 0, 3); break;
                case "lowmem": s.LowMemory = value != "0"; break;
                case "mnk_mode": s.MnkMode = value != "0"; break;
                case "audio_enabled": s.AudioEnabled = value != "0"; break;
                case "audio_master" when int.TryParse(value, out int vol): s.AudioMaster = vol; break;
                case "audio_mute": s.AudioMute = value != "0"; break;
                case "audio_night": s.AudioNight = value != "0"; break;
                case "dlc_golden": s.DlcGolden = value != "0"; break;
                case "dlc_megaxel": s.DlcMegaXel = value != "0"; break;
                case "dlc_ray": s.DlcRay = value != "0"; break;
                case "dlc_retro": s.DlcRetro = value != "0"; break;
                case "install_dir" when value.Length > 0: s.InstallDir = value; break;
                case "managed": s.Managed = value != "0"; break;
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
    }

    /// <summary>Rewrite the keys we own in the active port config, keeping comments
    /// and any keys the launcher does not manage.</summary>
    public static string Apply(PortSettings s)
    {
        if (!ManagedInstallAt(ManagedRootFor(s))) ResolveManagedRoot(s);
        bool managed = UseManaged(s);
        string path = managed ? ManagedConfPathFor(s) : ConfPath;
        if (!managed && !File.Exists(path))
            return $"Port config not found: {path}";
        if (managed) Directory.CreateDirectory(ManagedRootFor(s));

        var values = new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["backend"] = s.Backend,
            ["resolution"] = s.Resolution,
            ["fullscreen"] = s.Fullscreen ? "1" : "0",
            ["vsync"] = s.Vsync ? "1" : "0",
            ["skip_intro"] = s.SkipIntro ? "1" : "0",
            ["skip_presses"] = Math.Clamp(s.SkipPresses, 0, 20).ToString(),
            ["skip_gap"] = Math.Clamp(s.SkipGap, 1, 30).ToString(),
            ["skip_delay"] = Math.Clamp(s.SkipDelay, 0, 120).ToString(),
            ["start_mode"] = s.StartMode,
            ["boot_mode"] = s.BootMode,
            ["quality"] = s.Quality,
            ["game_hz"] = s.GameHz == 60 ? "60" : "120",
            ["scale_override"] = s.Scale,
            ["aniso_override"] = s.Aniso,
            ["msaa"] = s.Msaa,
            ["dither"] = s.Dither,
            ["fxaa"] = string.IsNullOrWhiteSpace(s.Fxaa) ? "preset" : s.Fxaa,
            ["async_shaders"] = s.AsyncShaders,
            ["texture_cache_mb"] = s.TextureCacheMb.ToString(),
            ["texture_mip_skip"] = Math.Clamp(s.TextureMipSkip, 0, 3).ToString(),
            ["lowmem"] = s.LowMemory ? "1" : "0",
            ["mnk_mode"] = s.MnkMode ? "1" : "0",
            ["audio_enabled"] = s.AudioEnabled ? "1" : "0",
            ["audio_master"] = Math.Clamp(s.AudioMaster, 0, 100).ToString(),
            ["audio_mute"] = s.AudioMute ? "1" : "0",
            ["audio_night"] = s.AudioNight ? "1" : "0",
            ["dlc_golden"] = s.DlcGolden ? "1" : "0",
            ["dlc_megaxel"] = s.DlcMegaXel ? "1" : "0",
            ["dlc_ray"] = s.DlcRay ? "1" : "0",
            ["dlc_retro"] = s.DlcRetro ? "1" : "0",
            ["install_dir"] = ManagedRootFor(s),
            ["managed"] = managed ? "1" : "0",
            ["license_mask"] = s.LicenseMask,
            ["user_data_root"] = s.UserDataRoot,
        };
        foreach (var kv in s.Keybinds)
            values[kv.Key] = kv.Value;

        var lines = File.Exists(path) ? File.ReadAllLines(path).ToList() : new List<string>();
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

        File.WriteAllLines(path, lines);
        if (managed) RememberManagedRoot(ManagedRootFor(s));
        string content = s.Backend == "pc" ? "" : ApplyContent(s);
        return $"Port settings written: {s.Quality}, {s.Resolution}, " +
               $"{(s.Fullscreen ? "fullscreen" : "windowed")}, " +
               $"vsync {(s.Vsync ? "on" : "off")}, start {s.StartMode}. " + content;
    }

    public static string Launch(PortSettings s)
    {
        if (s.Backend == "pc")
            return LaunchPc();

        ResolveManagedRoot(s);
        if (UseManaged(s))
            return LaunchManaged(s);
        if (IsWindows)
            return "Install the port from XBLA first; the dev hybrid launcher is Linux-only.";

        if (!IsAvailable())
            return $"ReXGlue port not found ({HybridBin}).";
        if (IsRunning())
            return "The port is already running.";

        string applied;
        try
        {
            applied = Apply(s);
        }
        catch (Exception ex)
        {
            return "Port settings failed: " + ex.Message;
        }
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

    /// <summary>Launch a self-contained imported port directly, without bash or
    /// the dev project. Works on Linux and Windows.</summary>
    public static string LaunchManaged(PortSettings s)
    {
        string root = ManagedRootFor(s);
        string exe = ManagedRuntimeExeFor(s);
        if (!File.Exists(exe))
            return $"Managed port runtime missing: {exe}. Re-run Install port.";
        if (!ManagedGamePresent(s))
            return $"Managed port game data missing: {ManagedGameDirFor(s)}. Re-run Install port.";
        if (IsRunning())
            return "The port is already running.";

        string applied;
        try
        {
            applied = Apply(s);
        }
        catch (Exception ex)
        {
            return "Port settings failed: " + ex.Message;
        }

        try
        {
            string logs = LogDirFor(s);
            Directory.CreateDirectory(logs);
            string gameDir = ManagedGameDirFor(s);
            string userDir = SaveRoot(s);
            Directory.CreateDirectory(userDir);

            var psi = new ProcessStartInfo
            {
                FileName = exe,
                WorkingDirectory = root,
                UseShellExecute = false,
            };
            psi.ArgumentList.Add("--game_data_root");
            psi.ArgumentList.Add(gameDir);
            psi.ArgumentList.Add("--user_data_root");
            psi.ArgumentList.Add(userDir);
            psi.ArgumentList.Add("--license_mask");
            psi.ArgumentList.Add(string.IsNullOrWhiteSpace(s.LicenseMask) ? "4294967295" : s.LicenseMask);
            psi.ArgumentList.Add("--no-async_shader_compilation");
            psi.ArgumentList.Add("--resolution");
            psi.ArgumentList.Add(s.Resolution);
            psi.ArgumentList.Add("--window_width");
            psi.ArgumentList.Add(s.Width.ToString());
            psi.ArgumentList.Add("--window_height");
            psi.ArgumentList.Add(s.Height.ToString());
            switch (s.BootMode)
            {
                case "normal":
                    break;
                case "title":
                    psi.ArgumentList.Add("--cl");
                    psi.ArgumentList.Add("TitleMenu");
                    break;
                case "stage":
                    psi.ArgumentList.Add("--cl");
                    psi.ArgumentList.Add("Map_StageSelect");
                    break;
                default:
                    psi.ArgumentList.Add("--cl");
                    psi.ArgumentList.Add("BootGame -novideo");
                    break;
            }
            if (s.Fullscreen) psi.ArgumentList.Add("--fullscreen");
            else psi.ArgumentList.Add("--no-fullscreen");
            if (s.MnkMode)
            {
                psi.ArgumentList.Add("--mnk_mode");
                psi.ArgumentList.Add("true");
            }
            psi.ArgumentList.Add("--log_file");
            psi.ArgumentList.Add(Path.Combine(logs, "mn9_managed.log"));

            psi.Environment["MN9_QUALITY"] = s.Quality;
            psi.Environment["MN9_VSYNC"] = s.Vsync ? "true" : "false";
            psi.Environment["REX_FULLSCREEN"] = s.Fullscreen ? "true" : "false";
            psi.Environment["REX_VSYNC"] = s.Vsync ? "true" : "false";
            psi.Environment["MN9_START_MODE"] = s.StartMode;
            psi.Environment["MN9_ASYNC"] = s.AsyncShaders;
            psi.Environment["MN9_TEXCACHE"] = s.TextureCacheMb.ToString();
            psi.Environment["MN9_TEXSKIP"] = Math.Clamp(s.TextureMipSkip, 0, 3).ToString();
            psi.Environment["MN9_LOWMEM"] = s.LowMemory ? "1" : "0";
            psi.Environment["MN9_GAME_HZ"] = s.GameHz == 60 ? "60" : "120";
            psi.Environment["MN9_FXAA"] = s.Fxaa;
            psi.Environment["MN9_AUDIO_ENABLED"] = s.AudioEnabled ? "1" : "0";
            psi.Environment["MN9_AUDIO_MASTER"] = s.AudioMaster.ToString();
            psi.Environment["MN9_AUDIO_MUTE"] = s.AudioMute ? "1" : "0";
            psi.Environment["MN9_AUDIO_NIGHT"] = s.AudioNight ? "1" : "0";
            if (!string.IsNullOrWhiteSpace(s.Scale)) psi.Environment["MN9_SCALE"] = s.Scale;
            if (!string.IsNullOrWhiteSpace(s.Aniso)) psi.Environment["MN9_ANISO"] = s.Aniso;
            if (!string.IsNullOrWhiteSpace(s.Msaa)) psi.Environment["MN9_MSAA"] = s.Msaa;
            if (!string.IsNullOrWhiteSpace(s.Dither)) psi.Environment["MN9_DITHER"] = s.Dither;
            foreach (var kv in s.Keybinds)
                psi.Environment[kv.Key.ToUpperInvariant().Replace("KEYBIND_", "MN9_KEYBIND_")] = kv.Value;

            if (!IsWindows)
            {
                string old = Environment.GetEnvironmentVariable("LD_LIBRARY_PATH") ?? "";
                string libs = root;
                string sdk = Path.Combine(Home(), ".local", "rexglue-sdk-0.10.0", "lib");
                if (DevPortAt(root) && Directory.Exists(sdk)) libs = sdk + ":" + root;
                psi.Environment["LD_LIBRARY_PATH"] = libs + (old.Length > 0 ? ":" + old : "");
                psi.Environment["SDL_VIDEODRIVER"] = "x11";
            }

            Process? proc = Process.Start(psi);
            // Fast BootGame startup skips the boot movie but still stops at the
            // title screen. Send a single A press there to open the main menu;
            // the extra presses used by the other boot modes can advance story
            // slides, so they stay opt-in.
            if (proc != null && s.SkipIntro)
            {
                bool fast = s.BootMode == "fast";
                _ = Task.Run(() => IntroSkipper.RunAsync(
                    proc, s, fast ? 1 : s.SkipPresses, fast ? Math.Max(s.SkipDelay, 18) : s.SkipDelay));
            }
        }
        catch (Exception ex)
        {
            return applied + Environment.NewLine + "Launch failed: " + ex.Message;
        }
        return applied + Environment.NewLine + "Launched the installed ReXGlue port.";
    }

    public static string LaunchPc()
    {
        try
        {
            if (IsWindows)
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "steam://run/314710",
                    UseShellExecute = true,
                });
            }
            else
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "steam",
                    Arguments = "-applaunch 314710",
                    UseShellExecute = false,
                });
            }
            return "Launched the PC build through Steam.";
        }
        catch (Exception ex)
        {
            return "PC launch failed: " + ex.Message;
        }
    }

    // --- managed install ----------------------------------------------------

    /// <summary>Gives the runtime locations to copy from. The release bundles
    /// port-runtime/&lt;rid&gt;; a dev checkout can fall back to its build tree.</summary>
    public static IReadOnlyList<string> RuntimeSourceDirs()
    {
        var dirs = new List<string>();
        if (RuntimeBundled) dirs.Add(RuntimeSourceDir);
        if (!RuntimeBundled)
        {
            if (IsWindows)
            {
                dirs.Add(Path.Combine(RecompDir, "out", "build", "win-amd64-release"));
            }
            else
            {
                dirs.Add(Path.Combine(RecompDir, "out", "build", "linux-amd64-release"));
                dirs.Add(Path.Combine(RecompDir, "tools"));
                string sdkLib = Path.Combine(Home(), ".local", "rexglue-sdk-0.10.0", "lib");
                if (Directory.Exists(sdkLib)) dirs.Add(sdkLib);
            }
        }
        return dirs;
    }

    /// <summary>Copy the runtime next to the game, extract the XBLA package, add
    /// optional addon packages, and write an unlocked port.conf.</summary>
    public static string InstallPort(PortSettings s, string gamePackage,
                                     IReadOnlyList<string> addons, bool replace,
                                     Action<string>? progress = null)
    {
        progress ??= _ => { };
        if (!IsWindows && !OperatingSystem.IsLinux())
            return "Port install is supported on Windows and Linux only.";
        if (string.IsNullOrWhiteSpace(gamePackage) || (!File.Exists(gamePackage) && !Directory.Exists(gamePackage)))
            return "Choose the XBLA package or an extracted game folder first.";

        string root = ManagedRootFor(s);
        string gameDir = Path.Combine(root, "game");
        string staging = Path.Combine(root, ".game-new");
        string temp = Path.Combine(root, ".import-tmp");
        try
        {
            Directory.CreateDirectory(root);
            Directory.CreateDirectory(Path.Combine(root, "userdata"));
            Directory.CreateDirectory(Path.Combine(root, "addons"));

            progress("Copying runtime...");
            CopyRuntime(root);

            progress("Preparing game data...");
            DeleteDirectory(staging);
            DeleteDirectory(temp);
            Directory.CreateDirectory(staging);

            string? sourceRoot;
            if (Directory.Exists(gamePackage))
            {
                sourceRoot = FindGameRoot(gamePackage);
                if (sourceRoot == null)
                    return "That folder does not contain default.xex. Point at the XBLA package or its extracted root.";
            }
            else if (Path.GetExtension(gamePackage).Equals(".xex", StringComparison.OrdinalIgnoreCase))
            {
                Directory.CreateDirectory(staging);
                File.Copy(gamePackage, Path.Combine(staging, "default.xex"), overwrite: true);
                sourceRoot = staging;
                progress("Copied a bare XEX; cooked game data must be supplied separately.");
            }
            else
            {
                progress("Extracting XBLA package (this can take a minute)...");
                RunExtractor(root, gamePackage, temp, progress);
                sourceRoot = FindGameRoot(temp);
                if (sourceRoot == null)
                    return "Extraction finished but no default.xex was found. Is this the Mighty No. 9 XBLA package?";
            }

            if (sourceRoot != staging)
            {
                string fullGame = Path.GetFullPath(gameDir);
                string fullSource = Path.GetFullPath(sourceRoot);
                if (fullSource == fullGame ||
                    fullSource.StartsWith(fullGame + Path.DirectorySeparatorChar, StringComparison.Ordinal))
                    return "Choose an XBLA package or game folder outside the install folder.";

                progress("Copying game files...");
                CopyDirectory(sourceRoot, staging, progress);
            }

            if (replace) DeleteDirectory(gameDir);
            if (Directory.Exists(gameDir))
            {
                progress("Merging game files...");
                CopyDirectory(staging, gameDir, progress);
                DeleteDirectory(staging);
            }
            else
            {
                Directory.Move(staging, gameDir);
            }

            progress("Importing addon packages...");
            int addonCount = ImportAddons(s, addons, progress);
            SyncDlcToggles(s);

            // Unlock while building: every license bit is set, so the XBLA
            // title and its content mount as fully licensed.
            s.InstallDir = root;
            s.Managed = true;
            s.Backend = "rex";
            s.LicenseMask = "4294967295";
            string applied = Apply(s);
            RememberManagedRoot(root);

            long bytes = DirectorySize(gameDir);
            return $"Port installed at {root}. Game data {FormatSize(bytes)}, " +
                   $"{addonCount} addon package(s), unlocked (license mask 4294967295). " +
                   applied;
        }
        catch (Exception ex)
        {
            return "Port install failed: " + ex.Message;
        }
        finally
        {
            DeleteDirectory(staging);
            DeleteDirectory(temp);
        }
    }

    /// <summary>Only tick the addon checkboxes for packages that were actually
    /// imported, so a fresh install does not report missing packs.</summary>
    private static void SyncDlcToggles(PortSettings s)
    {
        string cache = ManagedAddonCacheFor(s);
        bool Has(string file) => File.Exists(Path.Combine(cache, file));
        s.DlcGolden = Has(DlcPackages[0].File);
        s.DlcMegaXel = Has(DlcPackages[1].File);
        s.DlcRay = Has(DlcPackages[2].File);
        s.DlcRetro = Has(DlcPackages[3].File);
    }

    private static void CopyRuntime(string root)
    {
        var dirs = RuntimeSourceDirs();
        if (dirs.Count == 0 || !File.Exists(Path.Combine(dirs[0], RuntimeExeName)))
            throw new InvalidOperationException(
                $"Bundled runtime not found in {RuntimeSourceDir}. Rebuild the launcher with port-runtime/{RuntimeRid}.");

        var allowed = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            RuntimeExeName, ExtractorName,
            "librexruntime.so", "librexgpu-xenos.so",
            "rexruntime.dll", "rexgpu-xenos.dll",
            "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll",
            "msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll",
            "vcruntime140.dll", "vcruntime140_1.dll", "vcruntime140_threads.dll",
            "concrt140.dll", "vccorlib140.dll",
        };

        foreach (string dir in dirs)
        {
            if (!Directory.Exists(dir)) continue;
            foreach (string src in Directory.GetFiles(dir))
            {
                string name = Path.GetFileName(src);
                if (!allowed.Contains(name)) continue;
                string dest = Path.Combine(root, name);
                File.Copy(src, dest, overwrite: true);
                if (!IsWindows) MakeExecutable(src, dest);
            }
        }

        foreach (string required in new[] { RuntimeExeName, ExtractorName })
        {
            if (!File.Exists(Path.Combine(root, required)))
                throw new InvalidOperationException($"Runtime payload is missing {required}.");
        }
    }

    private static void MakeExecutable(string src, string dest)
    {
        try
        {
            var mode = File.GetUnixFileMode(src);
            mode |= UnixFileMode.UserExecute | UnixFileMode.GroupExecute | UnixFileMode.OtherExecute;
            File.SetUnixFileMode(dest, mode);
        }
        catch { }
    }

    private static void RunExtractor(string root, string package, string outDir,
                                     Action<string> progress)
    {
        string exe = Path.Combine(root, ExtractorName);
        if (!File.Exists(exe)) throw new InvalidOperationException($"Extractor missing: {exe}");
        Directory.CreateDirectory(outDir);

        var psi = new ProcessStartInfo
        {
            FileName = exe,
            WorkingDirectory = root,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        psi.ArgumentList.Add(package);
        psi.ArgumentList.Add(outDir);
        if (!IsWindows)
        {
            string old = Environment.GetEnvironmentVariable("LD_LIBRARY_PATH") ?? "";
            psi.Environment["LD_LIBRARY_PATH"] = root + (old.Length > 0 ? ":" + old : "");
        }

        progress("Extracting STFS package...");
        using var p = Process.Start(psi) ?? throw new InvalidOperationException("Could not start extract_stfs.");
        // Drain both pipes concurrently: the extractor prints one line per file
        // and will block if stdout fills while we are reading stderr.
        var stdoutTask = p.StandardOutput.ReadToEndAsync();
        var stderrTask = p.StandardError.ReadToEndAsync();
        p.WaitForExit();
        Task.WaitAll(stdoutTask, stderrTask);
        if (p.ExitCode != 0)
            throw new InvalidOperationException(
                $"extract_stfs failed ({p.ExitCode}). {FirstLine(stderrTask.Result)}");
    }

    private static string FirstLine(string text)
    {
        foreach (string line in text.Split('\n'))
            if (!string.IsNullOrWhiteSpace(line)) return line.Trim();
        return "";
    }

    private static int ImportAddons(PortSettings s, IReadOnlyList<string> addons,
                                    Action<string> progress)
    {
        string cache = ManagedAddonCacheFor(s);
        Directory.CreateDirectory(cache);
        int count = 0;
        foreach (string path in addons)
        {
            if (string.IsNullOrWhiteSpace(path)) continue;
            if (File.Exists(path))
            {
                string dest = Path.Combine(cache, Path.GetFileName(path));
                if (!File.Exists(dest) || new FileInfo(dest).Length != new FileInfo(path).Length)
                {
                    progress("Importing " + Path.GetFileName(path) + "...");
                    File.Copy(path, dest, overwrite: true);
                }
                count++;
            }
            else if (Directory.Exists(path))
            {
                foreach (string file in AddonFilesIn(path))
                {
                    string dest = Path.Combine(cache, Path.GetFileName(file));
                    if (!File.Exists(dest) || new FileInfo(dest).Length != new FileInfo(file).Length)
                    {
                        progress("Importing " + Path.GetFileName(file) + "...");
                        File.Copy(file, dest, overwrite: true);
                    }
                    count++;
                }
            }
        }
        return count;
    }

    private static IEnumerable<string> AddonFilesIn(string dir)
    {
        var found = new List<string>();
        void Walk(DirectoryInfo d, int depth)
        {
            if (depth > 5) return;
            if (d.Name.Equals("00000002", StringComparison.OrdinalIgnoreCase))
            {
                found.AddRange(d.GetFiles().Select(f => f.FullName));
                return;
            }
            foreach (var sub in d.GetDirectories()) Walk(sub, depth + 1);
        }
        Walk(new DirectoryInfo(dir), 0);
        if (found.Count == 0)
        {
            // A flat folder of addon package files.
            found.AddRange(Directory.GetFiles(dir).Where(f =>
                !Path.GetFileName(f).Equals("default.xex", StringComparison.OrdinalIgnoreCase)));
        }
        return found.Distinct(StringComparer.OrdinalIgnoreCase);
    }

    private static string? FindGameRoot(string dir)
    {
        if (File.Exists(Path.Combine(dir, "default.xex"))) return dir;
        try
        {
            foreach (string sub in Directory.GetDirectories(dir))
            {
                if (File.Exists(Path.Combine(sub, "default.xex"))) return sub;
            }
            foreach (string sub in Directory.GetDirectories(dir))
            {
                string? nested = FindGameRoot(sub);
                if (nested != null) return nested;
            }
        }
        catch { }
        return null;
    }

    private static void CopyDirectory(string src, string dst, Action<string>? progress = null)
    {
        Directory.CreateDirectory(dst);
        foreach (string file in Directory.GetFiles(src))
        {
            string dest = Path.Combine(dst, Path.GetFileName(file));
            File.Copy(file, dest, overwrite: true);
        }
        foreach (string sub in Directory.GetDirectories(src))
        {
            string name = Path.GetFileName(sub);
            if (name.StartsWith(".import", StringComparison.OrdinalIgnoreCase)) continue;
            CopyDirectory(sub, Path.Combine(dst, name), progress);
        }
    }

    private static void DeleteDirectory(string dir)
    {
        try
        {
            if (Directory.Exists(dir)) Directory.Delete(dir, recursive: true);
        }
        catch { }
    }

    private static long DirectorySize(string dir)
    {
        try
        {
            return Directory.GetFiles(dir, "*", SearchOption.AllDirectories)
                            .Sum(f => new FileInfo(f).Length);
        }
        catch { return 0; }
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
        string dest = Path.Combine(BackupRootFor(s), id);
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

    public static IReadOnlyList<PortSaveInfo> ListBackups(PortSettings? s = null)
    {
        string backupDir = s == null ? BackupDir : BackupRootFor(s);
        if (!Directory.Exists(backupDir)) return Array.Empty<PortSaveInfo>();
        return Directory.GetDirectories(backupDir)
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
        var backups = ListBackups(s);
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
            if (IsWindows)
                Process.Start(new ProcessStartInfo { FileName = path, UseShellExecute = true });
            else
                Process.Start(new ProcessStartInfo { FileName = "xdg-open", Arguments = $"\"{path}\"", UseShellExecute = false });
            return path;
        }
        catch { return path; }
    }

    // --- content ------------------------------------------------------------

    public static string DlcSourceFor(PortSettings s, DlcPackage p)
    {
        string cache = Path.Combine(ManagedAddonCacheFor(s), p.File);
        if (UseManaged(s)) return cache;
        if (File.Exists(cache)) return cache;
        return Path.Combine(RecompDir, p.Folder, "584114A4", "00000002", p.File);
    }

    public static string DlcTargetFor(PortSettings s, DlcPackage p) =>
        Path.Combine(ContentDirFor(s), p.File);

    public static bool IsStaged(PortSettings s, DlcPackage p) => File.Exists(DlcTargetFor(s, p));

    private static bool DlcWanted(PortSettings s, string key) => key switch
    {
        "dlc_golden" => s.DlcGolden,
        "dlc_megaxel" => s.DlcMegaXel,
        "dlc_ray" => s.DlcRay,
        "dlc_retro" => s.DlcRetro,
        _ => true,
    };

    /// <summary>Copy the selected addon packages into the game's DLC folder and
    /// remove the ones that are switched off. Sources are never modified.</summary>
    public static string ApplyContent(PortSettings s)
    {
        string contentDir = ContentDirFor(s);
        var staged = new List<string>();
        var removed = new List<string>();
        var missing = new List<string>();
        try
        {
            Directory.CreateDirectory(contentDir);
            foreach (var p in DlcPackages)
            {
                string target = DlcTargetFor(s, p);
                if (DlcWanted(s, p.Key))
                {
                    string source = DlcSourceFor(s, p);
                    if (!File.Exists(source))
                    {
                        missing.Add(p.Name);
                        continue;
                    }
                    bool current = File.Exists(target) &&
                                   new FileInfo(target).Length == new FileInfo(source).Length;
                    if (!current)
                    {
                        File.Copy(source, target, overwrite: true);
                        staged.Add(p.Name);
                    }
                }
                else if (File.Exists(target))
                {
                    File.SetAttributes(target, FileAttributes.Normal);
                    File.Delete(target);
                    removed.Add(p.Name);
                }
            }

            // Unknown addon packages imported by the user are staged verbatim.
            string cache = ManagedAddonCacheFor(s);
            if (Directory.Exists(cache))
            {
                var known = DlcPackages.Select(p => p.File).ToHashSet(StringComparer.OrdinalIgnoreCase);
                foreach (string src in Directory.GetFiles(cache))
                {
                    string name = Path.GetFileName(src);
                    if (known.Contains(name)) continue;
                    string dst = Path.Combine(contentDir, name);
                    if (!File.Exists(dst) || new FileInfo(dst).Length != new FileInfo(src).Length)
                    {
                        File.Copy(src, dst, overwrite: true);
                        staged.Add(name);
                    }
                }
            }
        }
        catch (Exception ex)
        {
            return "DLC staging failed: " + ex.Message;
        }

        var parts = new List<string>();
        if (staged.Count > 0) parts.Add("staged " + string.Join(", ", staged));
        if (removed.Count > 0) parts.Add("removed " + string.Join(", ", removed));
        if (missing.Count > 0) parts.Add("missing " + string.Join(", ", missing));
        return parts.Count == 0 ? "DLC unchanged." : "DLC: " + string.Join("; ", parts) + ".";
    }

    public static string ContentStatusFor(PortSettings s)
    {
        string dir = ContentDirFor(s);
        if (!Directory.Exists(dir)) return "No addon packages installed.";
        var files = Directory.GetFiles(dir);
        if (files.Length == 0) return "No addon packages installed.";
        var known = DlcPackages.Where(p => IsStaged(s, p)).Select(p => p.Name).ToList();
        var knownFiles = DlcPackages.Select(p => Path.GetFileName(DlcTargetFor(s, p)))
                                     .ToHashSet(StringComparer.OrdinalIgnoreCase);
        int extra = files.Count(f => !knownFiles.Contains(Path.GetFileName(f)));
        long bytes = files.Sum(f => new FileInfo(f).Length);
        string list = known.Count > 0 ? string.Join(", ", known) : "unrecognized packages";
        if (extra > 0) list += $", +{extra} other";
        return $"{files.Length} addon package(s), {FormatSize(bytes)} — {list}  —  {dir}";
    }

    public static string DlcStatusFor(PortSettings s)
    {
        if (!UseManaged(s)) return "Addon management uses the dev project folders.";
        string cache = ManagedAddonCacheFor(s);
        if (!Directory.Exists(cache)) return "No imported addon packages.";
        var files = Directory.GetFiles(cache);
        return files.Length == 0
            ? "No imported addon packages."
            : $"{files.Length} imported addon package(s) in {cache}";
    }

    // --- diagnostics --------------------------------------------------------

    public static string? NewestLog(PortSettings? s = null)
    {
        string dir = s == null ? LogDir : LogDirFor(s);
        if (!Directory.Exists(dir)) return null;
        return Directory.GetFiles(dir, "*.log")
                        .OrderByDescending(File.GetLastWriteTime)
                        .FirstOrDefault();
    }

    public static string TailLog(PortSettings? s = null, int lines = 14)
    {
        string? log = NewestLog(s);
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
        string mode = UseManaged(s) ? "installed" : "dev";
        string save = SaveFiles(s).Count > 0 ? "save present" : "no save";
        string content = Directory.Exists(ContentDirFor(s)) &&
                         Directory.GetFiles(ContentDirFor(s)).Length > 0
            ? "DLC staged" : "no DLC";
        return $"Port: {run} · {mode} · {save} · {content} · quality {s.Quality} · {s.Resolution}";
    }

    private static string FormatSize(long bytes) => bytes switch
    {
        < 1024 => $"{bytes} B",
        < 1024 * 1024 => $"{bytes / 1024.0:0.0} KB",
        _ => $"{bytes / (1024.0 * 1024.0):0.0} MB",
    };
}
