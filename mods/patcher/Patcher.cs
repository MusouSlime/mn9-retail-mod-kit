using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace Mn9Patcher;

public sealed class Settings
{
    public bool Shadows = true;
    public bool Bloom = true;
    public bool MotionBlur = true;
    public bool LightShafts = true;
    public bool Distortion = true;
    public bool LensFlares = true;
    public bool AmbientOcclusion = true;
    public bool Mlaa;
    public bool Msaa;
    public bool Fullscreen;
    public bool Vsync = true;
    public bool Voice;
    public bool Movies;
    public bool MouseFix = false;
    public bool LockConfig = true;

    public int Width = 1280;
    public int Height = 720;
    public int Fps = 62;
    public int Pool = 256;
    public int Physics = 5;

    public string Resolution => $"{Width}x{Height}";
}

public sealed class SaveBackupInfo
{
    public string Id { get; set; } = "";
    public string Path { get; set; } = "";
    public DateTimeOffset Time { get; set; }
    public int Files { get; set; }
    public long Bytes { get; set; }
    public string Label { get; set; } = "";
}

public static class Patcher
{
    private const string GameFolder = "Mighty No. 9";
    private static readonly string[] SystemSections =
    {
        "SystemSettings", "SystemSettingsLow", "SystemSettingsMid",
        "SystemSettingsHigh", "SystemSettingsUltra",
    };

    public static string FindGameDir()
    {
        var candidates = new List<string>();
        if (OperatingSystem.IsWindows())
        {
            foreach (var (hive, key) in new[]
            {
                (RegistryHive.CurrentUser, @"Software\Valve\Steam"),
                (RegistryHive.LocalMachine, @"SOFTWARE\WOW6432Node\Valve\Steam"),
                (RegistryHive.LocalMachine, @"SOFTWARE\Valve\Steam"),
            })
            {
                try
                {
                    using var baseKey = RegistryKey.OpenBaseKey(hive, RegistryView.Default);
                    using var k = baseKey.OpenSubKey(key);
                    foreach (string name in new[] { "InstallPath", "SteamPath" })
                    {
                        string v = (string)k?.GetValue(name);
                        if (!string.IsNullOrEmpty(v))
                            candidates.Add(Path.Combine(v, "steamapps", "common", GameFolder));
                    }
                }
                catch { }
            }
            candidates.Add(Path.Combine(@"C:\Program Files (x86)\Steam\steamapps\common", GameFolder));
            candidates.Add(Path.Combine(@"C:\Program Files\Steam\steamapps\common", GameFolder));
        }

        string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        candidates.Add(Path.Combine(home, ".steam/steam/steamapps/common", GameFolder));
        candidates.Add(Path.Combine(home, ".local/share/Steam/steamapps/common", GameFolder));

        foreach (string dir in candidates)
        {
            try
            {
                if (Directory.Exists(Path.Combine(dir, "MN9Game", "Config")))
                    return dir;
            }
            catch { }
        }
        return null;
    }

    public static bool IsGameRunning()
    {
        foreach (var p in Process.GetProcesses())
        {
            try
            {
                if (p.ProcessName.StartsWith("MN9Game", StringComparison.OrdinalIgnoreCase))
                    return true;
            }
            catch { }
        }
        return false;
    }

    private readonly record struct DisplaySize(int Width, int Height);

    private static DisplaySize? NativeDisplaySize()
    {
        try
        {
            if (OperatingSystem.IsWindows())
            {
                int w = GetSystemMetrics(0);
                int h = GetSystemMetrics(1);
                return w > 0 && h > 0 ? new DisplaySize(w, h) : null;
            }
            if (!OperatingSystem.IsLinux()) return null;

            var psi = new ProcessStartInfo("xrandr", "--current")
            {
                RedirectStandardOutput = true,
                UseShellExecute = false,
            };
            using var proc = Process.Start(psi);
            if (proc == null) return null;
            string output = proc.StandardOutput.ReadToEnd();
            proc.WaitForExit(3000);
            var m = Regex.Match(output, @"(\d+)x(\d+)[^\n]*\*");
            if (!m.Success) return null;
            return new DisplaySize(int.Parse(m.Groups[1].Value), int.Parse(m.Groups[2].Value));
        }
        catch
        {
            return null;
        }
    }

    [DllImport("user32.dll")]
    private static extern int GetSystemMetrics(int nIndex);

    public static string Apply(string gameDir, Settings s)
    {
        if (IsGameRunning())
            return "Close Mighty No. 9 first.";
        if (!Directory.Exists(Path.Combine(gameDir, "MN9Game", "Config")))
            return "Not a Mighty No. 9 install.";

        var log = new StringBuilder();
        try
        {
            // Exclusive fullscreen only works at the panel's native mode on
            // this SDL2/XWayland build; a non-native size leaves a black
            // screen.  Force the native size whenever Fullscreen is selected.
            if (s.Fullscreen)
            {
                var native = NativeDisplaySize();
                if (native is { } n)
                {
                    if (s.Width != n.Width || s.Height != n.Height)
                        log.AppendLine($"fullscreen: native display {n.Width}x{n.Height}, ignoring preset {s.Width}x{s.Height}");
                    s.Width = n.Width;
                    s.Height = n.Height;
                }
                else
                {
                    log.AppendLine("fullscreen: native display size unknown; keeping preset");
                }
            }

            string configDir = Path.Combine(gameDir, "MN9Game", "Config");
            string root = Path.Combine(gameDir, ".mn9patcher");
            string baseline = Path.Combine(root, "baseline");
            Directory.CreateDirectory(baseline);

            var state = LoadState(root) ?? new PatchState();
            var created = new HashSet<string>(state.Created, StringComparer.OrdinalIgnoreCase);

            string prefix;
            if (File.Exists(Path.Combine(configDir, "SDL2Linux-MN9SystemSettings.ini")))
                prefix = "SDL2Linux-MN9";
            else if (File.Exists(Path.Combine(configDir, "MN9SystemSettings.ini")))
                prefix = "MN9";
            else
                prefix = OperatingSystem.IsWindows() ? "MN9" : "SDL2Linux-MN9";

            string sysPlatform = prefix + "SystemSettings.ini";
            string engPlatform = prefix + "Engine.ini";
            string gamePlatform = prefix + "Game.ini";

            EnsurePlatform(configDir, sysPlatform, "DefaultSystemSettings.ini", created, log);
            EnsurePlatform(configDir, engPlatform, "DefaultEngine.ini", created, log);
            EnsurePlatform(configDir, gamePlatform, "DefaultGame.ini", created, log);

            var system = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["ResX"] = s.Width.ToString(),
                ["ResY"] = s.Height.ToString(),
                ["ScreenPercentage"] = "100",
                ["Fullscreen"] = s.Fullscreen ? "True" : "False",
                ["Borderless"] = "False",
                ["UseVsync"] = s.Vsync ? "True" : "False",
                ["DynamicShadows"] = s.Shadows ? "True" : "False",
                ["LightEnvironmentShadows"] = s.Shadows ? "True" : "False",
                ["Bloom"] = s.Bloom ? "True" : "False",
                ["MotionBlur"] = s.MotionBlur ? "True" : "False",
                ["MotionBlurPause"] = s.MotionBlur ? "True" : "False",
                ["DepthOfField"] = "False",
                ["AmbientOcclusion"] = s.AmbientOcclusion ? "True" : "False",
                ["bAllowLightShafts"] = s.LightShafts ? "True" : "False",
                ["Distortion"] = s.Distortion ? "True" : "False",
                ["FilteredDistortion"] = s.Distortion ? "True" : "False",
                ["LensFlares"] = s.LensFlares ? "True" : "False",
                ["AllowRadialBlur"] = s.Distortion ? "True" : "False",
                ["AllowSubsurfaceScattering"] = s.Bloom ? "True" : "False",
                ["AllowImageReflections"] = s.Bloom ? "True" : "False",
                ["AllowImageReflectionShadowing"] = s.Bloom ? "True" : "False",
                ["bAllowPostprocessMLAA"] = s.Mlaa ? "True" : "False",
                ["bAllowTemporalAA"] = "False",
                ["bAllowD3D9MSAA"] = s.Msaa ? "True" : "False",
                ["MaxMultiSamples"] = s.Msaa ? "4" : "1",
                ["MobileEnableMSAA"] = "False",
                ["DetailMode"] = "2",
                ["SkeletalMeshLODBias"] = "0",
                ["ParticleLODBias"] = "0",
                ["MaxDrawDistanceScale"] = "1.0",
                ["MaxAnisotropy"] = "16",
                ["MinShadowResolution"] = s.Shadows ? "512" : "32",
                ["MaxShadowResolution"] = s.Shadows ? "2048" : "64",
                ["MaxWholeSceneDominantShadowResolution"] = s.Shadows ? "2048" : "64",
                ["UnbuiltNumWholeSceneDynamicShadowCascades"] = s.Shadows ? "3" : "1",
                ["bAllowHighQualityMaterials"] = "True",
            };

            foreach (string name in new[] { sysPlatform, "DefaultSystemSettings.ini" })
            {
                string path = Path.Combine(configDir, name);
                if (!File.Exists(path)) continue;
                Backup(path, baseline);
                Unlock(path);
                foreach (string section in SystemSections)
                    Ini.Patch(path, section, system, create: true);
                if (s.LockConfig) Lock(path); else Unlock(path);
            }

            var engineCore = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["bSmoothFrameRate"] = "True",
                ["MinSmoothedFrameRate"] = "20",
                ["MaxSmoothedFrameRate"] = s.Fps.ToString(),
                ["bStaticDecalsEnabled"] = "True",
                ["bDynamicDecalsEnabled"] = "True",
            };
            var stream = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["PoolSize"] = s.Pool.ToString(),
                ["MinTextureResidentMipCount"] = "7",
            };
            var audio = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["MaxChannels"] = "32",
            };
            var movie = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["bForceNoMovies"] = s.Movies ? "False" : "True",
                ["bShouldStopMovieAtEndOfLoadMap"] = "TRUE",
            };
            var voip = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["bHasVoiceEnabled"] = s.Voice ? "True" : "False",
            };

            foreach (string name in new[] { engPlatform, "DefaultEngine.ini" })
            {
                string path = Path.Combine(configDir, name);
                if (!File.Exists(path)) continue;
                Backup(path, baseline);
                Unlock(path);
                Ini.Patch(path, "Engine.Engine", engineCore, create: false);
                Ini.Patch(path, "TextureStreaming", stream, create: true);
                Ini.Patch(path, "XAudio2.XAudio2Device", audio, create: true);
                Ini.Patch(path, "ALAudio.ALAudioDevice", audio, create: true);
                Ini.Patch(path, "FullScreenMovie", movie, create: true);
                Ini.Patch(path, "VoIP", voip, create: true);
                if (s.LockConfig) Lock(path); else Unlock(path);
            }

            var world = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["MaxPhysicsSubsteps"] = s.Physics.ToString(),
            };
            foreach (string name in new[] { gamePlatform, "DefaultGame.ini" })
            {
                string path = Path.Combine(configDir, name);
                if (!File.Exists(path)) continue;
                Backup(path, baseline);
                Unlock(path);
                Ini.Patch(path, "Engine.WorldInfo", world, create: false);
                if (s.LockConfig) Lock(path); else Unlock(path);
            }

            string systemData = Path.Combine(gameDir, "MN9Game", "SaveData", "SystemData.dat");
            if (File.Exists(systemData))
            {
                if (s.LockConfig) Lock(systemData); else Unlock(systemData);
                log.AppendLine(s.LockConfig ? "SystemData.dat locked (no game-side settings save)" : "SystemData.dat writable");
            }

            state.Profile = "custom";
            state.Created = created.ToList();
            SaveState(root, state);

            log.AppendLine("Config patched.");
            log.AppendLine(MousePatch.Set(gameDir, s.MouseFix));
            log.AppendLine(AspectPatch.Set(gameDir, s.Width, s.Height));
        }
        catch (Exception ex)
        {
            log.AppendLine("error: " + ex.Message);
        }
        return log.ToString().Trim();
    }

    public static string Restore(string gameDir)
    {
        if (IsGameRunning())
            return "Close Mighty No. 9 first.";

        var log = new StringBuilder();
        try
        {
            string root = Path.Combine(gameDir, ".mn9patcher");
            string baseline = Path.Combine(root, "baseline");
            var state = LoadState(root);
            if (!Directory.Exists(baseline))
                return "No backup found.";

            foreach (string file in Directory.GetFiles(baseline))
            {
                string dst = Path.Combine(gameDir, "MN9Game", "Config", Path.GetFileName(file));
                Unlock(dst);
                File.Copy(file, dst, overwrite: true);
                Unlock(dst);
            }

            if (state != null)
            {
                foreach (string name in state.Created)
                {
                    string p = Path.Combine(gameDir, "MN9Game", "Config", name);
                    Unlock(p);
                    if (File.Exists(p)) File.Delete(p);
                }
            }

            string systemData = Path.Combine(gameDir, "MN9Game", "SaveData", "SystemData.dat");
            if (File.Exists(systemData)) Unlock(systemData);
            log.AppendLine("Config restored.");
            log.AppendLine(MousePatch.Set(gameDir, false));
            log.AppendLine(AspectPatch.Revert(gameDir));
        }
        catch (Exception ex)
        {
            log.AppendLine("error: " + ex.Message);
        }
        return log.ToString().Trim();
    }

    private static string SaveDataDir(string gameDir) =>
        Path.Combine(gameDir, "MN9Game", "SaveData");

    private static string SaveBackupRoot(string gameDir) =>
        Path.Combine(gameDir, ".mn9patcher", "saves");

    public static IReadOnlyList<SaveBackupInfo> ListSaveBackups(string gameDir)
    {
        var result = new List<SaveBackupInfo>();
        string root = SaveBackupRoot(gameDir);
        if (!Directory.Exists(root)) return result;

        foreach (string dir in Directory.GetDirectories(root))
        {
            try
            {
                var manifest = ReadSaveManifest(dir);
                string[] files = Directory.GetFiles(dir)
                    .Where(f => !Path.GetFileName(f).Equals("manifest.json", StringComparison.OrdinalIgnoreCase))
                    .ToArray();
                if (files.Length == 0 && manifest == null) continue;

                result.Add(new SaveBackupInfo
                {
                    Id = Path.GetFileName(dir),
                    Path = dir,
                    Time = manifest?.Created ?? new DateTimeOffset(Directory.GetLastWriteTime(dir)),
                    Files = files.Length,
                    Bytes = files.Sum(f => new FileInfo(f).Length),
                    Label = manifest?.Label ?? "",
                });
            }
            catch { }
        }

        return result.OrderByDescending(x => x.Time).ToList();
    }

    public static string BackupSaves(string gameDir, string label = "manual")
    {
        string saveDir = SaveDataDir(gameDir);
        if (!Directory.Exists(saveDir))
            return "No SaveData folder.";

        string[] files = Directory.GetFiles(saveDir, "*", SearchOption.TopDirectoryOnly)
            .OrderBy(f => f, StringComparer.OrdinalIgnoreCase)
            .ToArray();
        if (files.Length == 0)
            return "SaveData is empty; nothing to back up.";

        string root = SaveBackupRoot(gameDir);
        Directory.CreateDirectory(root);

        string stamp = DateTime.Now.ToString("yyyy-MM-dd_HH-mm-ss");
        string safeLabel = SanitizeLabel(label);
        string baseId = stamp + (safeLabel.Length == 0 ? "" : "_" + safeLabel);
        string dir = Path.Combine(root, baseId);
        int suffix = 2;
        while (Directory.Exists(dir))
            dir = Path.Combine(root, baseId + "-" + suffix++);
        Directory.CreateDirectory(dir);

        var manifest = new SaveManifest { Created = DateTimeOffset.Now, Label = label ?? "" };
        long total = 0;
        foreach (string src in files)
        {
            string name = Path.GetFileName(src);
            string dst = Path.Combine(dir, name);
            File.Copy(src, dst, overwrite: true);
            try { File.SetLastWriteTimeUtc(dst, File.GetLastWriteTimeUtc(src)); } catch { }
            long size = new FileInfo(src).Length;
            total += size;
            manifest.Files.Add(new SaveFileInfo { Name = name, Size = size, Sha256 = Sha256File(src) });
        }

        File.WriteAllText(Path.Combine(dir, "manifest.json"),
            JsonSerializer.Serialize(manifest, new JsonSerializerOptions { WriteIndented = true }),
            Encoding.UTF8);

        PruneSaveBackups(root, 20);

        string warning = IsGameRunning() ? " (game is running; snapshot may be mid-write)" : "";
        return $"Save backup: {Path.GetFileName(dir)} ({files.Length} files, {FormatBytes(total)}){warning}";
    }

    public static string RestoreSaves(string gameDir, string id = "latest")
    {
        if (IsGameRunning())
            return "Close Mighty No. 9 first.";

        var backups = ListSaveBackups(gameDir);
        if (backups.Count == 0)
            return "No save backups found.";

        bool latest = string.IsNullOrWhiteSpace(id) ||
                      id.Equals("latest", StringComparison.OrdinalIgnoreCase);
        SaveBackupInfo pick = latest ? backups[0] : backups.FirstOrDefault(b => b.Id == id);
        if (pick == null)
            return $"Save backup not found: {id}";

        string saveDir = SaveDataDir(gameDir);
        Directory.CreateDirectory(saveDir);

        string safety = BackupSaves(gameDir, "before-restore");

        int restored = 0;
        foreach (string src in Directory.GetFiles(pick.Path))
        {
            string name = Path.GetFileName(src);
            if (name.Equals("manifest.json", StringComparison.OrdinalIgnoreCase))
                continue;

            string dst = Path.Combine(saveDir, name);
            bool locked = File.Exists(dst) && IsLocked(dst);
            Unlock(dst);
            File.Copy(src, dst, overwrite: true);
            try { File.SetLastWriteTimeUtc(dst, File.GetLastWriteTimeUtc(src)); } catch { }
            if (locked) Lock(dst); else Unlock(dst);
            restored++;
        }

        return $"Restored {restored} save file(s) from {pick.Id}. {safety}";
    }

    private sealed class SaveManifest
    {
        public DateTimeOffset Created { get; set; }
        public string Label { get; set; } = "";
        public List<SaveFileInfo> Files { get; set; } = new();
    }

    private sealed class SaveFileInfo
    {
        public string Name { get; set; } = "";
        public long Size { get; set; }
        public string Sha256 { get; set; } = "";
    }

    private static SaveManifest ReadSaveManifest(string dir)
    {
        string path = Path.Combine(dir, "manifest.json");
        try
        {
            return File.Exists(path)
                ? JsonSerializer.Deserialize<SaveManifest>(File.ReadAllText(path))
                : null;
        }
        catch { return null; }
    }

    private static void PruneSaveBackups(string root, int keep)
    {
        try
        {
            foreach (string dir in Directory.GetDirectories(root)
                         .OrderByDescending(d => Directory.GetLastWriteTime(d))
                         .Skip(keep))
            {
                try { Directory.Delete(dir, recursive: true); } catch { }
            }
        }
        catch { }
    }

    private static string SanitizeLabel(string label)
    {
        if (string.IsNullOrWhiteSpace(label)) return "";
        string s = Regex.Replace(label.Trim(), @"[^A-Za-z0-9_-]+", "-").Trim('-');
        return s.Length > 24 ? s[..24] : s;
    }

    private static string Sha256File(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexString(SHA256.HashData(stream)).ToLowerInvariant();
    }

    private static string FormatBytes(long bytes) => bytes switch
    {
        < 1024 => $"{bytes} B",
        < 1024 * 1024 => $"{bytes / 1024.0:0.0} KB",
        _ => $"{bytes / (1024.0 * 1024.0):0.0} MB",
    };

    private static bool IsLocked(string path)
    {
        try
        {
            if (!File.Exists(path)) return false;
            if (OperatingSystem.IsWindows())
                return (File.GetAttributes(path) & FileAttributes.ReadOnly) != 0;
            var mode = File.GetUnixFileMode(path);
            return (mode & (UnixFileMode.UserWrite | UnixFileMode.GroupWrite |
                            UnixFileMode.OtherWrite)) == 0;
        }
        catch { return false; }
    }

    public static string LaunchSafe(string gameDir)
    {
        string marker = Path.Combine(StateDir(), "crash");
        Directory.CreateDirectory(StateDir());

        var log = new StringBuilder();
        if (File.Exists(marker))
        {
            log.AppendLine("Previous crash found; restoring safe config.");
            log.AppendLine(Restore(gameDir));
            var safe = new Settings { Fps = 62, MouseFix = true };
            log.AppendLine(Apply(gameDir, safe));
            try { File.Delete(marker); } catch { }
        }

        string steam = FindSteam();
        if (steam == null)
            return log.AppendLine("Steam not found.").ToString().Trim();

        Process.Start(new ProcessStartInfo
        {
            FileName = steam,
            Arguments = "-applaunch 314710",
            UseShellExecute = false,
        });

        Process game = null;
        for (int i = 0; i < 180 && game == null; i++)
        {
            Thread.Sleep(1000);
            game = FindGameProcess();
        }
        if (game == null)
            return log.AppendLine("Game process not detected.").ToString().Trim();

        game.WaitForExit();
        int rc = game.ExitCode;
        log.AppendLine($"Game exited with {rc}.");
        if (rc != 0 && rc != 130 && rc != 143)
        {
            File.WriteAllText(marker, $"{DateTimeOffset.Now:O} rc={rc}");
            log.AppendLine("Crash recorded; next launch restores safe config.");
        }
        return log.ToString().Trim();
    }

    private static Process FindGameProcess()
    {
        foreach (var p in Process.GetProcesses())
        {
            try
            {
                if (p.ProcessName.StartsWith("MN9Game", StringComparison.OrdinalIgnoreCase))
                    return p;
            }
            catch { }
        }
        return null;
    }

    private static string FindSteam()
    {
        foreach (string c in new[]
        {
            @"C:\Program Files (x86)\Steam\steam.exe",
            @"C:\Program Files\Steam\steam.exe",
            "/usr/bin/steam",
            "/usr/games/steam",
        })
            if (File.Exists(c)) return c;

        if (OperatingSystem.IsWindows())
        {
            foreach (var (hive, key) in new[]
            {
                (RegistryHive.CurrentUser, @"Software\Valve\Steam"),
                (RegistryHive.LocalMachine, @"SOFTWARE\WOW6432Node\Valve\Steam"),
            })
            {
                try
                {
                    using var baseKey = RegistryKey.OpenBaseKey(hive, RegistryView.Default);
                    using var k = baseKey.OpenSubKey(key);
                    string path = (string)k?.GetValue("SteamExe");
                    if (!string.IsNullOrEmpty(path) && File.Exists(path)) return path;
                }
                catch { }
            }
        }
        return null;
    }

    private static void EnsurePlatform(string configDir, string platform, string fallback,
                                       HashSet<string> created, StringBuilder log)
    {
        string path = Path.Combine(configDir, platform);
        if (File.Exists(path)) return;
        string src = Path.Combine(configDir, fallback);
        if (!File.Exists(src)) return;
        File.Copy(src, path, overwrite: false);
        created.Add(platform);
        log.AppendLine($"created {platform}");
    }

    private static void Backup(string path, string baseline)
    {
        string dst = Path.Combine(baseline, Path.GetFileName(path));
        if (File.Exists(dst)) return;
        File.Copy(path, dst, overwrite: false);
    }

    private sealed class PatchState
    {
        public string Profile { get; set; } = "";
        public List<string> Created { get; set; } = new();
    }

    private static PatchState LoadState(string root)
    {
        string path = Path.Combine(root, "state.json");
        try
        {
            return File.Exists(path) ? JsonSerializer.Deserialize<PatchState>(File.ReadAllText(path)) : null;
        }
        catch { return null; }
    }

    private static void SaveState(string root, PatchState state)
    {
        Directory.CreateDirectory(root);
        File.WriteAllText(Path.Combine(root, "state.json"),
            JsonSerializer.Serialize(state, new JsonSerializerOptions { WriteIndented = true }));
    }

    private static string StateDir()
    {
        string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        if (string.IsNullOrEmpty(local))
            local = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".local", "state");
        return Path.Combine(local, "Mn9Patcher");
    }

    private static void Lock(string path)
    {
        try
        {
            if (OperatingSystem.IsWindows())
            {
                var a = File.GetAttributes(path);
                File.SetAttributes(path, a | FileAttributes.ReadOnly);
            }
            else
            {
                File.SetUnixFileMode(path, UnixFileMode.UserRead | UnixFileMode.GroupRead | UnixFileMode.OtherRead);
            }
        }
        catch { }
    }

    private static void Unlock(string path)
    {
        try
        {
            if (!File.Exists(path)) return;
            if (OperatingSystem.IsWindows())
            {
                var a = File.GetAttributes(path);
                if ((a & FileAttributes.ReadOnly) != 0)
                    File.SetAttributes(path, a & ~FileAttributes.ReadOnly);
            }
            else
            {
                File.SetUnixFileMode(path,
                    UnixFileMode.UserRead | UnixFileMode.UserWrite |
                    UnixFileMode.GroupRead | UnixFileMode.OtherRead);
            }
        }
        catch { }
    }
}

internal static class Ini
{
    public static void Patch(string path, string section, Dictionary<string, string> values, bool create)
    {
        string text = File.ReadAllText(path);
        string eol = text.Contains("\r\n") ? "\r\n" : "\n";
        var lines = new List<string>(text.Replace("\r\n", "\n").Split('\n'));
        if (lines.Count > 0 && lines[^1].Length == 0) lines.RemoveAt(lines.Count - 1);

        int start = lines.FindIndex(l => l.Trim() == $"[{section}]");
        if (start < 0)
        {
            if (!create) return;
            lines.Add($"[{section}]");
            foreach (var kv in values) lines.Add($"{kv.Key}={kv.Value}");
            File.WriteAllText(path, string.Join(eol, lines) + eol);
            return;
        }

        int end = start + 1;
        while (end < lines.Count && !lines[end].TrimStart().StartsWith("[")) end++;

        var replaced = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        for (int i = start + 1; i < end; i++)
        {
            var m = Regex.Match(lines[i], @"^(\s*)([A-Za-z0-9_]+)\s*=");
            if (!m.Success) continue;
            string key = m.Groups[2].Value;
            if (!values.TryGetValue(key, out string value)) continue;
            lines[i] = $"{m.Groups[1].Value}{key}={value}";
            replaced.Add(key);
        }

        var missing = values.Keys.Where(k => !replaced.Contains(k)).ToList();
        if (missing.Count > 0)
            lines.InsertRange(end, missing.Select(k => $"{k}={values[k]}"));
        File.WriteAllText(path, string.Join(eol, lines) + eol);
    }
}

public static class MousePatch
{
    private static readonly (long Offset, string Orig, string Patch, string Note)[] LinuxGame =
    {
        (0x8CFC30, "ff25f21d0c02", "31c0c3909090", "game PLT SDL_SetWindowGrab"),
        (0x8CFEF0, "ff25921c0c02", "b801000000c3", "game PLT SDL_ShowCursor"),
    };

    private static readonly (long Offset, string Orig, string Patch, string Note)[] LinuxSdl =
    {
        (0x41A50, "", "c3", "SDL_SetWindowGrab"),
        (0x40860, "", "b801000000c3", "SDL_ShowCursor"),
        (0x407D0, "", "b800000000c3", "SDL_SetRelativeMouseMode"),
        (0x41C90, "", "b800000000c3", "SDL_CaptureMouse"),
        (0x407C0, "", "b801000000c3", "SDL_WarpMouseInWindow"),
        (0x41C50, "", "b801000000c3", "SDL_WarpMouseGlobal"),
    };

    public static string Set(string gameDir, bool enable)
    {
        try
        {
            if (OperatingSystem.IsWindows())
                return SetWindows(gameDir, enable);
            return SetLinux(gameDir, enable);
        }
        catch (Exception ex)
        {
            return "mouse patch: " + ex.Message;
        }
    }

    private static string SetLinux(string gameDir, bool enable)
    {
        var log = new StringBuilder();
        string game = Path.Combine(gameDir, "Binaries/Linux/MN9Game.bin.x86_64");
        string sdl = Path.Combine(gameDir, "Binaries/Linux/lib64/libSDL2-2.0.so.0");

        PatchFile(game, LinuxGame, enable, log);
        PatchFile(sdl, LinuxSdl, enable, log);
        return log.Length == 0 ? (enable ? "mouse fix on" : "mouse fix off") : log.ToString().Trim();
    }

    private static void PatchFile(string path, (long Offset, string Orig, string Patch, string Note)[] patches,
                                  bool enable, StringBuilder log)
    {
        if (!File.Exists(path)) return;
        string backup = path + ".orig";
        if (enable)
        {
            if (!File.Exists(backup)) File.Copy(path, backup);
            byte[] data = File.ReadAllBytes(path);
            bool changed = false;
            foreach (var p in patches)
            {
                int off = checked((int)p.Offset);
                string patched = p.Patch;
                if (!string.IsNullOrEmpty(p.Orig))
                {
                    string cur = Convert.ToHexString(data, off, p.Orig.Length / 2).ToLowerInvariant();
                    if (cur == patched) continue;
                    if (cur != p.Orig)
                    {
                        log.AppendLine($"skip {p.Note}: unexpected bytes");
                        continue;
                    }
                }
                byte[] bytes = Convert.FromHexString(patched);
                Array.Copy(bytes, 0, data, off, bytes.Length);
                changed = true;
            }
            if (changed) File.WriteAllBytes(path, data);
            log.AppendLine("mouse patches applied");
        }
        else if (File.Exists(backup))
        {
            // Restore only the bytes this patch owns, so unrelated binary edits
            // (MSAA, Free Mouse, aspect fill, ...) survive a mouse-patch revert.
            byte[] data = File.ReadAllBytes(path);
            byte[] orig = File.ReadAllBytes(backup);
            bool changed = false;
            foreach (var p in patches)
            {
                int off = checked((int)p.Offset);
                int len = p.Patch.Length / 2;
                if (off + len > data.Length || off + len > orig.Length) continue;
                string cur = Convert.ToHexString(data, off, len).ToLowerInvariant();
                if (cur != p.Patch) continue;
                Array.Copy(orig, off, data, off, len);
                changed = true;
            }
            if (changed) File.WriteAllBytes(path, data);
            log.AppendLine("mouse patches reverted");
        }
    }

    private static string SetWindows(string gameDir, bool enable)
    {
        string dll = new[]
        {
            Path.Combine(gameDir, "Binaries/Win64/SDL2.dll"),
            Path.Combine(gameDir, "Binaries/Win32/SDL2.dll"),
            Path.Combine(gameDir, "SDL2.dll"),
        }.FirstOrDefault(File.Exists);

        if (dll == null) return "mouse fix: SDL2.dll not found";
        string backup = dll + ".orig";
        var wanted = new Dictionary<string, byte[]>
        {
            ["SDL_SetWindowGrab"] = new byte[] { 0xC3 },
            ["SDL_ShowCursor"] = new byte[] { 0xB8, 1, 0, 0, 0, 0xC3 },
            ["SDL_SetRelativeMouseMode"] = new byte[] { 0xB8, 0, 0, 0, 0, 0xC3 },
            ["SDL_CaptureMouse"] = new byte[] { 0xB8, 0, 0, 0, 0, 0xC3 },
            ["SDL_WarpMouseInWindow"] = new byte[] { 0xB8, 1, 0, 0, 0, 0xC3 },
            ["SDL_WarpMouseGlobal"] = new byte[] { 0xB8, 1, 0, 0, 0, 0xC3 },
        };

        if (!enable)
        {
            if (!File.Exists(backup)) return "mouse fix: no backup";
            byte[] data = File.ReadAllBytes(dll);
            byte[] orig = File.ReadAllBytes(backup);
            var exports = PeExports.Read(orig);
            bool changed = false;
            foreach (var (name, bytes) in wanted)
            {
                if (!exports.TryGetValue(name, out int off)) continue;
                if (off + bytes.Length > data.Length) continue;
                if (!data.AsSpan(off, bytes.Length).SequenceEqual(bytes)) continue;
                Array.Copy(orig, off, data, off, bytes.Length);
                changed = true;
            }
            if (changed) File.WriteAllBytes(dll, data);
            return "mouse patches reverted";
        }

        if (!File.Exists(backup)) File.Copy(dll, backup);
        byte[] patched = File.ReadAllBytes(dll);
        var exportMap = PeExports.Read(patched);

        int hit = 0;
        foreach (var (name, bytes) in wanted)
        {
            if (!exportMap.TryGetValue(name, out int off)) continue;
            Array.Copy(bytes, 0, patched, off, bytes.Length);
            hit++;
        }
        if (hit > 0) File.WriteAllBytes(dll, patched);
        return hit > 0 ? $"mouse patches applied ({hit} functions)" : "mouse fix: no SDL exports found";
    }
}

public static class AspectPatch
{
    // Default__MN9GamePlayerCamera.DefaultAspectRatio in a retail MN9Game.upk.
    // The export precedes the Option GFX payload, so our repacks keep this offset.
    private const long UpkAspectOffset = 0x78E292;

    private static readonly float[] KnownAspects =
    {
        1.7777801f, 1.7777778f, 1.3333334f, 1.6f,
    };

    private const long RxVaddr = 0x400000;
    private const long RxFilesz = 0x278E628;
    private const long RxFileszNew = 0x278E8F0;
    private const long RxMemszNew = 0x2C000000 - RxVaddr;

    private const long SiteVa = 0x201D251;
    private const long CaveVa = 0x2B8E7E0;
    private const long SiteOff = SiteVa - RxVaddr;
    private const long CaveOff = CaveVa - RxVaddr;

    // mov %rdi,%r15 ; lea 0xa0(%r15),%r14 ; movl $2,0xe8(%r15) ; jmp back
    private static readonly byte[] Cave =
        Convert.FromHexString("4989ff4d8db7a000000041c787e800000002000000e961ea48ff");

    private static readonly byte[] SiteOrig = Convert.FromHexString("4989ff4d8d");

    public static string Set(string gameDir, int width, int height)
    {
        if (!OperatingSystem.IsLinux())
            return "aspect: Linux only (GFx fill patch not implemented for Windows)";
        if (Patcher.IsGameRunning())
            return "aspect: close the game first";

        try
        {
            double aspect = (double)width / height;
            var log = new StringBuilder();

            log.AppendLine(PatchUpk(gameDir, (float)aspect));

            // The Scaleform fill cave only helps in the non-wide bucket; 16:10
            // and wider are still forced to 16:9 by the engine before GFx runs.
            if (aspect < 1.5)
                log.AppendLine(PatchMovie(gameDir, enable: true));
            else
                log.AppendLine(PatchMovie(gameDir, enable: false));

            return log.ToString().Trim();
        }
        catch (Exception ex)
        {
            return "aspect: " + ex.Message;
        }
    }

    public static string Revert(string gameDir)
    {
        if (!OperatingSystem.IsLinux()) return "aspect: Linux only";
        try
        {
            var log = new StringBuilder();
            log.AppendLine(PatchUpk(gameDir, 16f / 9f));
            log.AppendLine(PatchMovie(gameDir, enable: false));
            return log.ToString().Trim();
        }
        catch (Exception ex)
        {
            return "aspect: " + ex.Message;
        }
    }

    private static string PatchUpk(string gameDir, float aspect)
    {
        string upk = Path.Combine(gameDir, "MN9Game", "CookedLinux", "MN9Game.upk");
        if (!File.Exists(upk)) return "aspect: MN9Game.upk not found";

        byte[] data = File.ReadAllBytes(upk);
        int off = -1;

        if (UpkAspectOffset + 4 <= data.Length)
        {
            float cur = BitConverter.ToSingle(data, (int)UpkAspectOffset);
            if (cur > 0.5f && cur < 3.0f) off = (int)UpkAspectOffset;
        }

        if (off < 0)
        {
            foreach (float known in KnownAspects)
            {
                byte[] pat = BitConverter.GetBytes(known);
                var hits = Find(data, pat);
                if (hits.Count == 1) { off = hits[0]; break; }
            }
        }

        if (off < 0) return "aspect: DefaultAspectRatio not found";
        float old = BitConverter.ToSingle(data, off);
        if (Math.Abs(old - aspect) < 0.0001f)
            return $"aspect: already {aspect:0.000}";

        Array.Copy(BitConverter.GetBytes(aspect), 0, data, off, 4);
        File.WriteAllBytes(upk, data);
        return $"aspect: {old:0.000} -> {aspect:0.000}";
    }

    private static string PatchMovie(string gameDir, bool enable)
    {
        string bin = Path.Combine(gameDir, "Binaries/Linux/MN9Game.bin.x86_64");
        if (!File.Exists(bin)) return "aspect: game binary not found";

        byte[] data = File.ReadAllBytes(bin);
        bool patched = data[SiteOff] == 0xE9 &&
                       SiteOff + 5 <= data.Length &&
                       SiteVa + 5 + BitConverter.ToInt32(data, (int)SiteOff + 1) == CaveVa;

        if (enable == patched)
            return patched ? "aspect: UI fill already on" : "aspect: UI fill already off";

        if (!enable)
        {
            if (patched)
            {
                Array.Copy(SiteOrig, 0, data, SiteOff, SiteOrig.Length);
                Array.Fill(data, (byte)0, (int)CaveOff, Cave.Length);
                File.WriteAllBytes(bin, data);
            }
            return "aspect: UI fill off";
        }

        if (!data.AsSpan((int)SiteOff, 5).SequenceEqual(SiteOrig))
            return "aspect: unexpected binary bytes, UI fill skipped";

        int phdr = FindRxPhdr(data);
        long filesz = BitConverter.ToInt64(data, phdr + 0x20);
        if (filesz < RxFileszNew)
        {
            BitConverter.GetBytes(RxFileszNew).CopyTo(data, phdr + 0x20);
            BitConverter.GetBytes(RxMemszNew).CopyTo(data, phdr + 0x28);
        }

        Array.Copy(Cave, 0, data, CaveOff, Cave.Length);
        int rel = (int)(CaveVa - (SiteVa + 5));
        data[SiteOff] = 0xE9;
        BitConverter.GetBytes(rel).CopyTo(data, (int)SiteOff + 1);
        File.WriteAllBytes(bin, data);
        return "aspect: UI fill on (SM_ExactFit)";
    }

    private static int FindRxPhdr(byte[] data)
    {
        long phoff = BitConverter.ToInt64(data, 0x20);
        int phentsize = BitConverter.ToUInt16(data, 0x36);
        int phnum = BitConverter.ToUInt16(data, 0x38);
        for (int i = 0; i < phnum; i++)
        {
            int off = (int)(phoff + i * phentsize);
            uint type = BitConverter.ToUInt32(data, off);
            long vaddr = BitConverter.ToInt64(data, off + 0x10);
            if (type == 1 && vaddr == RxVaddr) return off;
        }
        throw new InvalidOperationException("RX segment not found");
    }

    private static List<int> Find(byte[] haystack, byte[] needle)
    {
        var hits = new List<int>();
        int start = 0;
        while (true)
        {
            int i = Array.IndexOf(haystack, needle[0], start);
            if (i < 0 || i + needle.Length > haystack.Length) break;
            if (haystack.AsSpan(i, needle.Length).SequenceEqual(needle)) hits.Add(i);
            start = i + 1;
        }
        return hits;
    }
}

internal static class PeExports
{
    public static Dictionary<string, int> Read(byte[] data)
    {
        var result = new Dictionary<string, int>(StringComparer.Ordinal);
        int pe = BitConverter.ToInt32(data, 0x3C);
        if (BitConverter.ToUInt32(data, pe) != 0x00004550) return result; // PE\0\0
        int coff = pe + 4;
        int sections = BitConverter.ToUInt16(data, coff + 2);
        int optSize = BitConverter.ToUInt16(data, coff + 16);
        int opt = coff + 20;
        ushort magic = BitConverter.ToUInt16(data, opt);
        int dir = opt + (magic == 0x20B ? 112 : 96);
        int exportRva = BitConverter.ToInt32(data, dir);
        if (exportRva == 0) return result;

        int sectionTable = opt + optSize;
        int exportOff = RvaToOffset(data, sectionTable, sections, exportRva);
        int names = BitConverter.ToInt32(data, exportOff + 24);
        int funcs = BitConverter.ToInt32(data, exportOff + 28);
        int namePtrs = BitConverter.ToInt32(data, exportOff + 32);
        int ords = BitConverter.ToInt32(data, exportOff + 36);

        int namesOff = RvaToOffset(data, sectionTable, sections, names);
        int funcsOff = RvaToOffset(data, sectionTable, sections, funcs);
        int namePtrsOff = RvaToOffset(data, sectionTable, sections, namePtrs);
        int ordsOff = RvaToOffset(data, sectionTable, sections, ords);

        for (int i = 0; i < 4096; i++)
        {
            int nameRva = BitConverter.ToInt32(data, namePtrsOff + i * 4);
            if (nameRva == 0) break;
            int nameOff = RvaToOffset(data, sectionTable, sections, nameRva);
            int end = nameOff;
            while (end < data.Length && data[end] != 0) end++;
            string name = Encoding.ASCII.GetString(data, nameOff, end - nameOff);
            int ordinal = BitConverter.ToUInt16(data, ordsOff + i * 2);
            int funcRva = BitConverter.ToInt32(data, funcsOff + ordinal * 4);
            int funcOff = RvaToOffset(data, sectionTable, sections, funcRva);
            if (funcOff > 0) result[name] = funcOff;
        }
        return result;
    }

    private static int RvaToOffset(byte[] data, int sectionTable, int count, int rva)
    {
        for (int i = 0; i < count; i++)
        {
            int s = sectionTable + i * 40;
            int va = BitConverter.ToInt32(data, s + 12);
            int rawSize = BitConverter.ToInt32(data, s + 16);
            int rawPtr = BitConverter.ToInt32(data, s + 20);
            if (rva >= va && rva < va + rawSize)
                return rawPtr + (rva - va);
        }
        return rva;
    }
}

internal static class Cli
{
    public static int Run(string[] args)
    {
        string game = Arg(args, "--game-dir") ?? Patcher.FindGameDir();
        if (game == null)
        {
            Console.Error.WriteLine("game not found");
            return 2;
        }
        if (args.Contains("--backup-saves"))
        {
            Console.WriteLine(Patcher.BackupSaves(game, "cli"));
            return 0;
        }
        if (args.Contains("--list-saves"))
        {
            var saves = Patcher.ListSaveBackups(game);
            if (saves.Count == 0)
            {
                Console.WriteLine("No save backups.");
                return 0;
            }
            foreach (var b in saves)
                Console.WriteLine($"{b.Id}\t{b.Time:yyyy-MM-dd HH:mm}\t{b.Files} files\t{b.Bytes} bytes\t{b.Label}");
            return 0;
        }
        if (args.Contains("--restore-saves"))
        {
            string id = Arg(args, "--save-id") ?? "latest";
            Console.WriteLine(Patcher.RestoreSaves(game, id));
            return 0;
        }
        if (args.Contains("--restore"))
        {
            Console.WriteLine(Patcher.Restore(game));
            return 0;
        }
        if (args.Contains("--launch"))
        {
            Console.WriteLine(Patcher.LaunchSafe(game));
            return 0;
        }
        var s = new Settings();
        if (Arg(args, "--fps") is string f && int.TryParse(f, out int fps)) s.Fps = fps;
        if (Arg(args, "--res") is string r)
        {
            string[] p = r.Split('x');
            if (p.Length == 2 && int.TryParse(p[0], out int w) && int.TryParse(p[1], out int h))
            {
                s.Width = w; s.Height = h;
            }
        }
        if (args.Contains("--mouse")) s.MouseFix = true;
        if (args.Contains("--fullscreen")) s.Fullscreen = true;
        if (args.Contains("--windowed")) s.Fullscreen = false;
        if (args.Contains("--vsync")) s.Vsync = true;
        if (args.Contains("--no-vsync")) s.Vsync = false;
        if (args.Contains("--msaa")) s.Msaa = true;
        if (args.Contains("--no-msaa")) s.Msaa = false;
        if (args.Contains("--mlaa")) s.Mlaa = true;
        if (args.Contains("--no-mlaa")) s.Mlaa = false;
        if (args.Contains("--no-lock")) s.LockConfig = false;
        Console.WriteLine(Patcher.Apply(game, s));
        return 0;
    }

    private static string Arg(string[] args, string name)
    {
        for (int i = 0; i < args.Length - 1; i++)
            if (args[i].Equals(name, StringComparison.OrdinalIgnoreCase))
                return args[i + 1];
        return null;
    }
}
