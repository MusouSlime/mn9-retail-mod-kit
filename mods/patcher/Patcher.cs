using System.Diagnostics;
using System.Globalization;
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
    public bool Laa = false;
    public bool LockConfig = true;
    public bool RetroTextures;

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

                // High-quality defaults, always written so switching "Retro
                // textures" back off actually restores the original filtering
                // and mip limits instead of leaving the point/256px values.
                ["TEXTUREGROUP_World"] = "(MinLODSize=64,MaxLODSize=2048,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=2)",
                ["TEXTUREGROUP_WorldNormalMap"] = "(MinLODSize=32,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
                ["TEXTUREGROUP_WorldSpecular"] = "(MinLODSize=32,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
                ["TEXTUREGROUP_Character"] = "(MinLODSize=256,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
                ["TEXTUREGROUP_CharacterNormalMap"] = "(MinLODSize=128,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
                ["TEXTUREGROUP_CharacterSpecular"] = "(MinLODSize=128,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
                ["TEXTUREGROUP_Weapon"] = "(MinLODSize=512,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
                ["TEXTUREGROUP_Vehicle"] = "(MinLODSize=256,MaxLODSize=1024,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
                ["TEXTUREGROUP_Effects"] = "(MinLODSize=128,MaxLODSize=512,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
                ["TEXTUREGROUP_EffectsNotFiltered"] = "(MinLODSize=128,MaxLODSize=256,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
                ["TEXTUREGROUP_Skybox"] = "(MinLODSize=1024,MaxLODSize=2048,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
                ["TEXTUREGROUP_UI"] = "(MinLODSize=2048,MaxLODSize=2048,LODBias=0,MinMagFilter=aniso,MipFilter=point,NumStreamedMips=0)",
            };

            if (s.RetroTextures)
            {
                // Texture-level retro look: point sampling and small max mips.
                system["MaxAnisotropy"] = "1";
                system["bAllowHighQualityMaterials"] = "False";
                system["DetailMode"] = "1";
                system["TEXTUREGROUP_World"] = "(MinLODSize=32,MaxLODSize=256,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_WorldNormalMap"] = "(MinLODSize=32,MaxLODSize=256,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_WorldSpecular"] = "(MinLODSize=32,MaxLODSize=256,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_Character"] = "(MinLODSize=64,MaxLODSize=256,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_CharacterNormalMap"] = "(MinLODSize=64,MaxLODSize=256,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_CharacterSpecular"] = "(MinLODSize=64,MaxLODSize=256,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_Weapon"] = "(MinLODSize=64,MaxLODSize=256,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_Vehicle"] = "(MinLODSize=64,MaxLODSize=256,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_Effects"] = "(MinLODSize=32,MaxLODSize=128,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_EffectsNotFiltered"] = "(MinLODSize=32,MaxLODSize=128,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_Skybox"] = "(MinLODSize=128,MaxLODSize=512,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
                system["TEXTUREGROUP_UI"] = "(MinLODSize=128,MaxLODSize=1024,LODBias=0,MinMagFilter=point,MipFilter=point,NumStreamedMips=0)";
            }

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
            log.AppendLine(s.Laa ? LargeAddressAware.Set(gameDir, true)
                                 : LargeAddressAware.Revert(gameDir));
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
            log.AppendLine(LargeAddressAware.Revert(gameDir));
            log.AppendLine(RestoreBinaryShim(gameDir));
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

    public static readonly string[] CharacterIds = { "beck", "gold", "retro", "call", "ray" };
    public static readonly string[] CharacterNames =
        { "Beck", "Golden Beck", "Retro Hero", "Call", "Ray (Beck moveset)" };

    public static string CharacterConfigPath(string gameDir) =>
        Path.Combine(gameDir, ".mn9charsel", "character");

    public static string OptionsPath(string gameDir) =>
        Path.Combine(gameDir, ".mn9charsel", "options");

    public static (bool SkipCutscenes, bool DisableDialog, bool CelShading, string CelStrength, bool Fxaa)
        ReadOptions(string gameDir)
    {
        bool skip = false, dialog = false, cel = true, fxaa = true;
        string strength = "metallic";
        try
        {
            string path = OptionsPath(gameDir);
            if (File.Exists(path))
            {
                foreach (string line in File.ReadAllLines(path))
                {
                    string[] parts = line.Split('=', 2);
                    if (parts.Length != 2) continue;
                    string key = parts[0].Trim();
                    string raw = parts[1].Trim();
                    bool value = raw is "1" or "true" or "True";
                    if (key == "skip_cutscenes") skip = value;
                    else if (key == "disable_dialog") dialog = value;
                    else if (key == "cel_shading") cel = value;
                    else if (key == "cel_strength") strength = raw is "subtle" or "bold" or "comic" or "noir" or "dotmatrix" or "cinematic" or "posterize" or "metallic" ? raw : "metallic";
                    else if (key == "fxaa") fxaa = value;
                }
            }
        }
        catch { }
        return (skip, dialog, cel, strength, fxaa);
    }

    public static string WriteOptions(string gameDir, bool skipCutscenes, bool disableDialog,
        bool celShading = true, string celStrength = "metallic", bool fxaa = true)
    {
        string path = OptionsPath(gameDir);
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        celStrength = celStrength is "subtle" or "bold" or "comic" or "noir" or "dotmatrix"
            or "cinematic" or "posterize" or "metallic" ? celStrength : "metallic";
        File.WriteAllText(path,
            $"skip_cutscenes={(skipCutscenes ? 1 : 0)}{Environment.NewLine}" +
            $"disable_dialog={(disableDialog ? 1 : 0)}{Environment.NewLine}" +
            $"cel_shading={(celShading ? 1 : 0)}{Environment.NewLine}" +
            $"cel_strength={celStrength}{Environment.NewLine}" +
            $"fxaa={(fxaa ? 1 : 0)}{Environment.NewLine}");
        return "Options saved.";
    }

    public static (bool Enabled, double Rate, int Button, string Key, int Pad) ReadTurbo(string gameDir)
    {
        bool enabled = false;
        double rate = 10.0;
        int button = 2;          // SDL X
        string key = "";         // blank = always on
        int pad = -1;            // -1 = no controller toggle
        try
        {
            string path = Path.Combine(gameDir, ".mn9charsel", "turbo.conf");
            if (!File.Exists(path)) return (enabled, rate, button, key, pad);
            foreach (string line in File.ReadLines(path))
            {
                string[] parts = line.Split('=', 2);
                if (parts.Length != 2) continue;
                string k = parts[0].Trim();
                string v = parts[1].Trim();
                if (k == "enabled") enabled = v is "1" or "true" or "True";
                else if (k == "rate" &&
                         double.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out double rv))
                    rate = Math.Clamp(rv, 2.0, 30.0);
                else if (k == "button" && int.TryParse(v, out int bv))
                    button = Math.Clamp(bv, 0, 3);
                else if (k == "key")
                    key = v is "none" or "off" or "-" ? "" : v;
                else if (k == "pad" && int.TryParse(v, out int pv))
                    pad = pv is >= -1 and <= 19 ? pv : -1;
            }
        }
        catch { }
        return (enabled, rate, button, key, pad);
    }

    public static string WriteTurbo(string gameDir, bool enabled, double rate, int button, string key, int pad)
    {
        string dir = Path.Combine(gameDir, ".mn9charsel");
        Directory.CreateDirectory(dir);
        rate = Math.Clamp(rate, 2.0, 30.0);
        button = Math.Clamp(button, 0, 3);
        pad = pad is >= -1 and <= 19 ? pad : -1;
        key = (key ?? "").Trim();
        if (key is "none" or "off" or "-") key = "";
        File.WriteAllText(Path.Combine(dir, "turbo.conf"),
            $"enabled={(enabled ? 1 : 0)}{Environment.NewLine}" +
            $"rate={rate.ToString("0", CultureInfo.InvariantCulture)}{Environment.NewLine}" +
            $"button={button}{Environment.NewLine}" +
            $"pad={pad}{Environment.NewLine}" +
            $"key={key}{Environment.NewLine}");
        return "Turbo saved.";
    }

    public static (int Fps, int Pool, int Physics) ReadPcTuning(string gameDir)
    {
        int fps = 62, pool = 256, physics = 5;
        try
        {
            string configDir = Path.Combine(gameDir, "MN9Game", "Config");
            if (!Directory.Exists(configDir)) return (fps, pool, physics);
            string prefix = File.Exists(Path.Combine(configDir, "SDL2Linux-MN9Engine.ini"))
                ? "SDL2Linux-MN9" : "MN9";

            foreach (string name in new[] { "DefaultEngine.ini", prefix + "Engine.ini" })
            {
                string path = Path.Combine(configDir, name);
                if (Ini.ReadValue(path, "Engine.Engine", "MaxSmoothedFrameRate") is string f &&
                    int.TryParse(f, out int fv)) fps = fv;
                if (Ini.ReadValue(path, "TextureStreaming", "PoolSize") is string p &&
                    int.TryParse(p, out int pv)) pool = pv;
            }
            foreach (string name in new[] { "DefaultGame.ini", prefix + "Game.ini" })
            {
                string path = Path.Combine(configDir, name);
                if (Ini.ReadValue(path, "Engine.WorldInfo", "MaxPhysicsSubsteps") is string ph &&
                    int.TryParse(ph, out int phv)) physics = phv;
            }
        }
        catch { }
        return (fps, pool, physics);
    }

    public static string ReadCharacter(string gameDir)
    {
        try
        {
            string path = CharacterConfigPath(gameDir);
            if (File.Exists(path))
            {
                string value = File.ReadAllText(path).Trim().ToLowerInvariant();
                if (CharacterIds.Contains(value)) return value;
            }
        }
        catch { }
        return "beck";
    }

    public static string SetCharacter(string gameDir, string character)
    {
        character = (character ?? "beck").Trim().ToLowerInvariant();
        if (!CharacterIds.Contains(character)) character = "beck";
        string path = CharacterConfigPath(gameDir);
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        File.WriteAllText(path, character + Environment.NewLine);
        return $"Character set to {CharacterDisplay(character)}.";
    }

    public static string CharacterDisplay(string id) => id switch
    {
        "gold" => "Golden Beck",
        "retro" => "Retro Hero",
        "call" => "Call",
        "ray" => "Ray",
        _ => "Beck",
    };

    public static string FindCharacterWrapper(string gameDir)
    {
        string env = Environment.GetEnvironmentVariable("MN9_CHARSEL_WRAPPER");
        if (!string.IsNullOrEmpty(env) && File.Exists(env)) return Path.GetFullPath(env);

        string baseDir = AppContext.BaseDirectory;
        var candidates = new List<string>
        {
            Path.Combine(baseDir, "mn9-charselect.sh"),
            Path.Combine(baseDir, "..", "spawn_override", "mn9-charselect.sh"),
            Path.Combine(baseDir, "..", "..", "spawn_override", "mn9-charselect.sh"),
            Path.Combine(baseDir, "..", "..", "..", "spawn_override", "mn9-charselect.sh"),
            Path.Combine(baseDir, "..", "..", "..", "..", "spawn_override", "mn9-charselect.sh"),
            Path.Combine(gameDir, ".mn9charsel", "mn9-charselect.sh"),
        };
        foreach (string candidate in candidates)
        {
            try
            {
                string full = Path.GetFullPath(candidate);
                if (File.Exists(full)) return full;
            }
            catch { }
        }
        return null;
    }

    private const string BinaryShimMarker = "# MN9 spawn shim";

    private const string BinaryShim = """
#!/usr/bin/env bash
# MN9 spawn shim
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
GAME="$(cd "$HERE/../.." && pwd)"
CONF="$GAME/.mn9charsel/character"
LIB="$GAME/.mn9charsel/libmn9spawn.so"
FX="$GAME/.mn9charsel/libpostfx.so"
AUDIO="$(dirname "$FX")/libaudiofx.so"
char="beck"
if [ -f "$CONF" ]; then char="$(head -n1 "$CONF" | tr -d '[:space:]')"; fi
case "$char" in
  gold|retro|beck|call|ray) ;;
  *) char="beck" ;;
esac
export MN9_CHARACTER="$char"
export MN9_GAME_DIR="$GAME"
export MN9_CHARSEL_CONF="$CONF"
OPTS="$GAME/.mn9charsel/options"
skip=0
dialog=0
cel=1
strength="metallic"
fxaa=1
if [ -f "$OPTS" ]; then
  skip="$(sed -n 's/^skip_cutscenes=//p' "$OPTS" | tail -n1)"
  dialog="$(sed -n 's/^disable_dialog=//p' "$OPTS" | tail -n1)"
  cel="$(sed -n 's/^cel_shading=//p' "$OPTS" | tail -n1)"
  strength="$(sed -n 's/^cel_strength=//p' "$OPTS" | tail -n1)"
  fxaa="$(sed -n 's/^fxaa=//p' "$OPTS" | tail -n1)"
fi
export MN9_SKIP_CUTSCENES="${skip:-0}"
export MN9_DISABLE_DIALOG="${dialog:-0}"
export MN9_CEL_SHADING="${cel:-0}"
export MN9_FXAA="${fxaa:-1}"
AC="$GAME/.mn9charsel/audio.conf"
audio=0; amusic=game; adir=""; amap=""; apitch=0; avoice=game; amg=1; asg=1; avg=1; aduck=1; anight=0
if [ -f "$AC" ]; then
  audio="$(sed -n 's/^enabled=//p' "$AC" | tail -n1)"
  amusic="$(sed -n 's/^music=//p' "$AC" | tail -n1)"
  adir="$(sed -n 's/^music_dir=//p' "$AC" | tail -n1)"
  amap="$(sed -n 's/^music_map=//p' "$AC" | tail -n1)"
  apitch="$(sed -n 's/^sfx_pitch=//p' "$AC" | tail -n1)"
  avoice="$(sed -n 's/^voice=//p' "$AC" | tail -n1)"
  amg="$(sed -n 's/^music_gain=//p' "$AC" | tail -n1)"
  asg="$(sed -n 's/^sfx_gain=//p' "$AC" | tail -n1)"
  avg="$(sed -n 's/^voice_gain=//p' "$AC" | tail -n1)"
  aduck="$(sed -n 's/^duck=//p' "$AC" | tail -n1)"
  anight="$(sed -n 's/^night=//p' "$AC" | tail -n1)"
fi
case "$strength" in
  noir)       export MN9_POSTFX_MODE=noir ;;
  dotmatrix)  export MN9_POSTFX_MODE=dotmatrix ;;
  cinematic)  export MN9_POSTFX_MODE=cinematic ;;
  posterize)  export MN9_POSTFX_MODE=posterize ;;
  metallic)   export MN9_POSTFX_MODE=metallic ;;
  comic)      export MN9_CEL_LEVELS=4; export MN9_CEL_OUTLINE=1.25; export MN9_CEL_SATURATION=1.55 ;;
  subtle)     export MN9_CEL_LEVELS=6; export MN9_CEL_OUTLINE=0.75; export MN9_CEL_SATURATION=1.22 ;;
  *)          export MN9_CEL_LEVELS=5; export MN9_CEL_OUTLINE=1.00; export MN9_CEL_SATURATION=1.35 ;;
esac
RUNTIME="${XDG_RUNTIME_DIR:-/tmp}/mn9mod-$(id -u)"
mkdir -p "$RUNTIME"
ln -sf "$LIB" "$RUNTIME/libmn9spawn.so"
PRELOAD="$RUNTIME/libmn9spawn.so"
if [ "$cel" = "1" ] && [ -f "$FX" ]; then
  ln -sf "$FX" "$RUNTIME/libpostfx.so"
  PRELOAD="$PRELOAD:$RUNTIME/libpostfx.so"
fi
if [ "${audio:-0}" = "1" ] && [ -f "$AUDIO" ]; then
  ln -sf "$AUDIO" "$RUNTIME/libaudiofx.so"
  PRELOAD="$PRELOAD:$RUNTIME/libaudiofx.so"
  export MN9_AUDIO_MUSIC="${amusic:-game}"
  export MN9_AUDIO_MUSIC_DIR="${adir:-}"
  export MN9_AUDIO_SFX_PITCH="${apitch:-0}"
  export MN9_AUDIO_VOICE="${avoice:-game}"
  export MN9_AUDIO_MUSIC_GAIN="${amg:-1}"
  export MN9_AUDIO_SFX_GAIN="${asg:-1}"
  export MN9_AUDIO_VOICE_GAIN="${avg:-1}"
  export MN9_AUDIO_MUSIC_MAP="${amap:-}"
  export MN9_AUDIO_DUCK="${aduck:-1}"
  export MN9_AUDIO_NIGHT="${anight:-0}"
fi
TURBO="$GAME/.mn9charsel/turbo.conf"
TLIB="$GAME/.mn9charsel/libmn9turbo.so"
turbo=0; trate=10; tbutton=2; tpad=-1; tkey=
if [ -f "$TURBO" ]; then
  turbo="$(sed -n 's/^enabled=//p' "$TURBO" | tail -n1)"
  trate="$(sed -n 's/^rate=//p' "$TURBO" | tail -n1)"
  tbutton="$(sed -n 's/^button=//p' "$TURBO" | tail -n1)"
  tpad="$(sed -n 's/^pad=//p' "$TURBO" | tail -n1)"
  tkey="$(sed -n 's/^key=//p' "$TURBO" | tail -n1)"
fi
if [ "${turbo:-0}" = "1" ] && [ -f "$TLIB" ]; then
  ln -sf "$TLIB" "$RUNTIME/libmn9turbo.so"
  PRELOAD="$PRELOAD:$RUNTIME/libmn9turbo.so"
  export MN9_TURBO_ENABLED=1
  export MN9_TURBO_RATE="${trate:-10}"
  export MN9_TURBO_BUTTON="${tbutton:-2}"
  export MN9_TURBO_PAD="${tpad:--1}"
  export MN9_TURBO_KEY="$tkey"
fi
# Keep Steam's overlay first so Steam Input and the overlay keep working.
if [ -n "${LD_PRELOAD:-}" ]; then
  export LD_PRELOAD="$LD_PRELOAD:$PRELOAD"
else
  export LD_PRELOAD="$PRELOAD"
fi
exec "$HERE/MN9Game.bin.x86_64.real" "$@"
""";

    private static bool IsBinaryShim(string path)
    {
        try
        {
            return File.Exists(path) &&
                   File.ReadLines(path).Take(4).Any(l => l.Contains(BinaryShimMarker));
        }
        catch { return false; }
    }

    private static string InstallBinaryShim(string gameDir)
    {
        string binDir = Path.Combine(gameDir, "Binaries", "Linux");
        string bin = Path.Combine(binDir, "MN9Game.bin.x86_64");
        string real = bin + ".real";
        if (!File.Exists(bin))
            return "Game binary not found: " + bin;

        if (!File.Exists(real))
        {
            File.Move(bin, real);
        }
        else if (!IsBinaryShim(bin))
        {
            File.Copy(bin, real, overwrite: true);
        }

        File.WriteAllText(bin, BinaryShim);
        try
        {
            if (OperatingSystem.IsLinux())
            {
                File.SetUnixFileMode(bin,
                    UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute |
                    UnixFileMode.GroupRead | UnixFileMode.GroupExecute |
                    UnixFileMode.OtherRead | UnixFileMode.OtherExecute);
            }
        }
        catch { }
        return null;
    }

    public static string RestoreBinaryShim(string gameDir)
    {
        string bin = Path.Combine(gameDir, "Binaries", "Linux", "MN9Game.bin.x86_64");
        string real = bin + ".real";
        if (!File.Exists(real)) return "Binary shim is not installed.";
        try
        {
            if (IsBinaryShim(bin)) File.Delete(bin);
            File.Move(real, bin, overwrite: true);
            return "Game binary restored.";
        }
        catch (Exception ex)
        {
            return "Could not restore game binary: " + ex.Message;
        }
    }

    private static string InstallCharacterWrapper(string gameDir, string source)
    {
        string destDir = Path.Combine(gameDir, ".mn9charsel");
        Directory.CreateDirectory(destDir);
        string dest = Path.Combine(destDir, "mn9-charselect.sh");
        File.Copy(source, dest, true);
        string lib = Path.Combine(Path.GetDirectoryName(source) ?? "", "libmn9spawn.so");
        if (File.Exists(lib))
            File.Copy(lib, Path.Combine(destDir, "libmn9spawn.so"), true);
        string fx = Path.Combine(Path.GetDirectoryName(source) ?? "", "libpostfx.so");
        if (File.Exists(fx))
        {
            // Never let an older launcher/AppImage replace a newer shader that is
            // already installed. This bit us when a stale bundle re-copied the
            // gated build over the always-on one.
            string destFx = Path.Combine(destDir, "libpostfx.so");
            bool newer = !File.Exists(destFx) ||
                         File.GetLastWriteTimeUtc(fx) > File.GetLastWriteTimeUtc(destFx);
            if (newer) File.Copy(fx, destFx, true);
        }
        string afx = Path.Combine(Path.GetDirectoryName(source) ?? "", "libaudiofx.so");
        if (File.Exists(afx))
            File.Copy(afx, Path.Combine(destDir, "libaudiofx.so"), true);
        string tlib = Path.Combine(Path.GetDirectoryName(source) ?? "", "libmn9turbo.so");
        if (File.Exists(tlib))
            File.Copy(tlib, Path.Combine(destDir, "libmn9turbo.so"), true);
        try
        {
            if (OperatingSystem.IsLinux())
            {
                File.SetUnixFileMode(dest,
                    UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute |
                    UnixFileMode.GroupRead | UnixFileMode.GroupExecute |
                    UnixFileMode.OtherRead | UnixFileMode.OtherExecute);
            }
        }
        catch { }
        return dest;
    }

    public static string InstallModFiles(string gameDir)
    {
        if (!OperatingSystem.IsLinux())
            return "Linux-only mod files; selection saved for a future Windows hook.";

        string source = FindCharacterWrapper(gameDir);
        if (source == null)
            return "Spawn wrapper not found.";

        InstallCharacterWrapper(gameDir, source);
        string shimError = InstallBinaryShim(gameDir);
        return shimError ?? "Mod files installed; normal Steam launches will use them.";
    }

    public static string LaunchCharacter(string gameDir, string character)
    {
        string message = SetCharacter(gameDir, character);
        string install = InstallModFiles(gameDir);
        if (install.StartsWith("Spawn wrapper") || install.StartsWith("Game binary"))
            return message + " " + install;

        string steam = FindSteam() ?? "steam";
        try
        {
            var psi = new ProcessStartInfo
            {
                FileName = steam,
                Arguments = "-applaunch 314710",
                UseShellExecute = false,
            };
            Process.Start(psi);
            return message + " " + install + " Launched through Steam.";
        }
        catch (Exception ex)
        {
            return message + " Steam launch failed: " + ex.Message;
        }
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
    public static string? ReadValue(string path, string section, string key)
    {
        try
        {
            if (!File.Exists(path)) return null;
            bool inSection = false;
            foreach (string raw in File.ReadLines(path))
            {
                string line = raw.Trim();
                if (line.StartsWith('['))
                {
                    inSection = line.Equals($"[{section}]", StringComparison.OrdinalIgnoreCase);
                    continue;
                }
                if (!inSection) continue;
                int eq = line.IndexOf('=');
                if (eq <= 0) continue;
                if (line[..eq].Trim().Equals(key, StringComparison.OrdinalIgnoreCase))
                    return line[(eq + 1)..].Trim();
            }
        }
        catch { }
        return null;
    }

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

    private static string FindSdlDll(string gameDir)
    {
        string[] preferred =
        {
            Path.Combine(gameDir, "Binaries/Win64/SDL2.dll"),
            Path.Combine(gameDir, "Binaries/Win32/SDL2.dll"),
            Path.Combine(gameDir, "SDL2.dll"),
            Path.Combine(gameDir, "MN9Game/Binaries/Win64/SDL2.dll"),
            Path.Combine(gameDir, "MN9Game/Binaries/Win32/SDL2.dll"),
            Path.Combine(gameDir, "Engine/Binaries/Win64/SDL2.dll"),
        };
        foreach (string p in preferred)
            if (File.Exists(p)) return p;

        try
        {
            string exact = Directory.EnumerateFiles(gameDir, "SDL2.dll", SearchOption.AllDirectories)
                .OrderBy(p => p.Count(c => c == Path.DirectorySeparatorChar))
                .FirstOrDefault();
            if (exact != null) return exact;

            // Some builds rename or relocate the SDL DLL; find the one that
            // actually exports the functions the mouse patch needs.
            foreach (string candidate in Directory.EnumerateFiles(gameDir, "*.dll", SearchOption.AllDirectories))
            {
                try
                {
                    var exports = PeExports.Read(File.ReadAllBytes(candidate));
                    if (exports.ContainsKey("SDL_SetWindowGrab") &&
                        exports.ContainsKey("SDL_ShowCursor"))
                        return candidate;
                }
                catch { }
            }
        }
        catch { }
        return null;
    }

    private static string SetWindows(string gameDir, bool enable)
    {
        string dll = FindSdlDll(gameDir);

        if (dll == null)
            return "mouse fix: SDL2.dll not found (searched the game folder)";
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
        string rel = Path.GetRelativePath(gameDir, dll);
        return hit > 0 ? $"mouse patches applied ({hit} functions in {rel})"
                       : $"mouse fix: no SDL exports found in {rel}";
    }
}

public static class LargeAddressAware
{
    // IMAGE_FILE_LARGE_ADDRESS_AWARE: lets a 32-bit process use up to 4 GB
    // (instead of 2 GB) on 64-bit Windows.  The retail MN9 game is a 32-bit
    // UE3 build; this is the closest it can get to a 64-bit address space.
    private const ushort LaaFlag = 0x0100;

    private static string FindExe(string gameDir) => new[]
    {
        Path.Combine(gameDir, "Binaries/Win32/MN9Game.exe"),
        Path.Combine(gameDir, "Binaries/Win64/MN9Game.exe"),
        Path.Combine(gameDir, "MN9Game.exe"),
        Path.Combine(gameDir, "Binaries/MN9Game.exe"),
    }.FirstOrDefault(File.Exists);

    public static string Set(string gameDir, bool enable)
    {
        string exe = FindExe(gameDir);
        if (exe == null) return "LAA: MN9Game.exe not found";

        try
        {
            byte[] data = File.ReadAllBytes(exe);
            int pe = BitConverter.ToInt32(data, 0x3C);
            if (pe < 0 || pe + 0x40 > data.Length ||
                BitConverter.ToUInt32(data, pe) != 0x00004550)
                return "LAA: not a PE executable";

            int coff = pe + 4;
            ushort machine = BitConverter.ToUInt16(data, coff);
            if (machine == 0x8664) return "LAA: game is already 64-bit";
            if (machine != 0x014C) return $"LAA: unexpected machine 0x{machine:x4}";

            int flagsOff = coff + 18;
            ushort flags = BitConverter.ToUInt16(data, flagsOff);
            bool on = (flags & LaaFlag) != 0;
            if (on == enable)
                return enable ? "LAA: already on" : "LAA: already off";

            string backup = exe + ".laa.orig";
            if (!File.Exists(backup)) File.Copy(exe, backup);

            ushort updated = (ushort)(enable ? flags | LaaFlag : flags & ~LaaFlag);
            BitConverter.GetBytes(updated).CopyTo(data, flagsOff);

            var attrs = File.GetAttributes(exe);
            if ((attrs & FileAttributes.ReadOnly) != 0)
                File.SetAttributes(exe, attrs & ~FileAttributes.ReadOnly);
            File.WriteAllBytes(exe, data);
            return enable
                ? "LAA: on (32-bit game can address up to 4 GB)"
                : "LAA: off";
        }
        catch (Exception ex)
        {
            return "LAA: " + ex.Message;
        }
    }

    public static string Revert(string gameDir)
    {
        string exe = FindExe(gameDir);
        if (exe == null) return "LAA: MN9Game.exe not found";
        string backup = exe + ".laa.orig";
        if (!File.Exists(backup)) return "LAA: off";
        try
        {
            File.Copy(backup, exe, overwrite: true);
            File.Delete(backup);
            return "LAA: reverted";
        }
        catch (Exception ex)
        {
            return "LAA: " + ex.Message;
        }
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
        if (Patcher.IsGameRunning())
            return "aspect: close the game first";

        try
        {
            double aspect = (double)width / height;
            var log = new StringBuilder();

            log.AppendLine(PatchUpk(gameDir, (float)aspect));

            if (OperatingSystem.IsLinux())
            {
                // The Scaleform fill cave only helps in the non-wide bucket;
                // 16:10 and wider are still forced to 16:9 by the engine
                // before GFx runs.
                log.AppendLine(aspect < 1.5
                    ? PatchMovie(gameDir, enable: true)
                    : PatchMovie(gameDir, enable: false));
            }
            else
            {
                log.AppendLine("aspect: 3D aspect set; GFx UI fill is Linux-only");
            }

            return log.ToString().Trim();
        }
        catch (Exception ex)
        {
            return "aspect: " + ex.Message;
        }
    }

    public static string Revert(string gameDir)
    {
        try
        {
            var log = new StringBuilder();
            log.AppendLine(PatchUpk(gameDir, 16f / 9f));
            if (OperatingSystem.IsLinux())
                log.AppendLine(PatchMovie(gameDir, enable: false));
            return log.ToString().Trim();
        }
        catch (Exception ex)
        {
            return "aspect: " + ex.Message;
        }
    }

    private static readonly byte[] AspectPrefix =
        { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    private static readonly byte[] AspectSuffix =
        { 0x7f, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    private static string PatchUpk(string gameDir, float aspect)
    {
        string upk = new[] { "CookedLinux", "CookedWindows", "CookedPCConsole", "CookedPC" }
            .Select(d => Path.Combine(gameDir, "MN9Game", d, "MN9Game.upk"))
            .FirstOrDefault(File.Exists);
        if (upk == null) return "aspect: MN9Game.upk not found";

        byte[] data = File.ReadAllBytes(upk);
        if (data.Length < 16 || BitConverter.ToUInt32(data, 0) != 0x9E2A83C1)
            return "aspect: unrecognized MN9Game.upk";

        int off = -1;
        bool linux = upk.Contains("CookedLinux", StringComparison.OrdinalIgnoreCase);

        // The fixed offset is only valid for our CookedLinux layout. Validate
        // the 8-byte prefix so a repacked Windows package cannot be written at
        // the wrong place.
        if (linux && UpkAspectOffset + 4 <= data.Length && UpkAspectOffset >= 8)
        {
            bool prefix = data.AsSpan((int)UpkAspectOffset - 8, 8).SequenceEqual(AspectPrefix);
            float cur = BitConverter.ToSingle(data, (int)UpkAspectOffset);
            if (prefix && cur > 0.5f && cur < 3.0f) off = (int)UpkAspectOffset;
        }

        if (off < 0)
        {
            // The 8-byte prefix alone is ambiguous (other floats share it),
            // so first try the full 20-byte window including the next name
            // index, then fall back to a unique 12-byte prefix hit.
            var exact = new List<int>();
            var loose = new List<int>();
            foreach (float known in KnownAspects)
            {
                byte[] pfx = new byte[12];
                AspectPrefix.CopyTo(pfx, 0);
                BitConverter.GetBytes(known).CopyTo(pfx, 8);

                byte[] full = new byte[20];
                pfx.CopyTo(full, 0);
                AspectSuffix.CopyTo(full, 12);
                exact.AddRange(Find(data, full).Select(h => h + 8));
                loose.AddRange(Find(data, pfx).Select(h => h + 8));
            }
            exact = exact.Distinct().OrderBy(h => h).ToList();
            loose = loose.Distinct().OrderBy(h => h).ToList();
            if (exact.Count == 1) off = exact[0];
            else if (loose.Count == 1) off = loose[0];
            else if (exact.Count > 1 || loose.Count > 1)
                return $"aspect: ambiguous ({Math.Max(exact.Count, loose.Count)} candidates)";
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
        if (args.Contains("--diag"))
        {
            string diag = $"patcher: {RuntimeInformation.ProcessArchitecture}, " +
                          $"64-bit={Environment.Is64BitProcess}, " +
                          $"{RuntimeInformation.FrameworkDescription}, " +
                          $"{RuntimeInformation.OSDescription}";
            Console.WriteLine(diag);
            try
            {
                File.WriteAllText(Path.Combine(AppContext.BaseDirectory, "mn9patcher-diag.txt"),
                                  diag + Environment.NewLine);
            }
            catch { }
            return 0;
        }
        // ReXGlue port front-end: does not need a PC game dir.
        if (args.Contains("--port-launch") || args.Contains("--port-apply"))
        {
            var ps = Port.Read();
            if (Arg(args, "--port-quality") is string pq) ps.Quality = pq;
            if (Arg(args, "--port-res") is string pres)
            {
                string[] p = pres.Split('x');
                if (p.Length == 2 && int.TryParse(p[0], out int pw) && int.TryParse(p[1], out int ph))
                {
                    ps.Width = pw;
                    ps.Height = ph;
                }
            }
            if (args.Contains("--port-windowed")) ps.Fullscreen = false;
            if (args.Contains("--port-fullscreen")) ps.Fullscreen = true;
            if (args.Contains("--port-vsync")) ps.Vsync = true;
            if (args.Contains("--port-no-vsync")) ps.Vsync = false;
            if (args.Contains("--port-play-intro")) ps.SkipIntro = false;
            if (args.Contains("--port-skip-intro")) ps.SkipIntro = true;
            Console.WriteLine(args.Contains("--port-launch") ? Port.Launch(ps) : Port.Apply(ps));
            return 0;
        }

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
        if (args.Contains("--show-character"))
        {
            Console.WriteLine(Patcher.ReadCharacter(game));
            return 0;
        }
        string character = Arg(args, "--character");
        if (character != null)
            Console.WriteLine(Patcher.SetCharacter(game, character));

        bool skipFlag = args.Contains("--skip-cutscenes");
        bool noSkip = args.Contains("--no-skip-cutscenes");
        bool dialogFlag = args.Contains("--disable-dialog");
        bool noDialog = args.Contains("--no-disable-dialog");
        bool celFlag = args.Contains("--cel-shading");
        bool noCel = args.Contains("--no-cel-shading");
        string celStrengthArg = Arg(args, "--cel-strength");
        if (skipFlag || noSkip || dialogFlag || noDialog || celFlag || noCel || celStrengthArg != null ||
            args.Contains("--fxaa") || args.Contains("--no-fxaa"))
        {
            var (skip, dialog, cel, strength, fxaa) = Patcher.ReadOptions(game);
            if (skipFlag) skip = true;
            if (noSkip) skip = false;
            if (dialogFlag) dialog = true;
            if (noDialog) dialog = false;
            if (celFlag) cel = true;
            if (noCel) cel = false;
            if (celStrengthArg != null) strength = celStrengthArg;
            if (args.Contains("--fxaa")) fxaa = true;
            if (args.Contains("--no-fxaa")) fxaa = false;
            Console.WriteLine(Patcher.WriteOptions(game, skip, dialog, cel, strength, fxaa));
            Console.WriteLine(Patcher.InstallModFiles(game));
        }
        if (args.Contains("--turbo") || args.Contains("--no-turbo") ||
            Arg(args, "--turbo-rate") != null || Arg(args, "--turbo-button") != null ||
            Arg(args, "--turbo-pad") != null || Arg(args, "--turbo-key") != null)
        {
            var (en, rate, button, key, pad) = Patcher.ReadTurbo(game);
            if (args.Contains("--turbo")) en = true;
            if (args.Contains("--no-turbo")) en = false;
            if (Arg(args, "--turbo-rate") is string trv &&
                double.TryParse(trv, NumberStyles.Float, CultureInfo.InvariantCulture, out double rv)) rate = rv;
            if (Arg(args, "--turbo-button") is string tbv)
            {
                button = tbv.ToLowerInvariant() switch
                {
                    "a" => 0,
                    "b" => 1,
                    "x" => 2,
                    "y" => 3,
                    _ => int.TryParse(tbv, out int bi) ? Math.Clamp(bi, 0, 3) : button,
                };
            }
            if (Arg(args, "--turbo-pad") is string tpv)
            {
                pad = tpv.ToLowerInvariant() switch
                {
                    "none" or "off" or "-" => -1,
                    "r3" => 8,
                    "l3" => 7,
                    "back" => 4,
                    _ => int.TryParse(tpv, out int pi) ? (pi is >= -1 and <= 19 ? pi : pad) : pad,
                };
            }
            if (Arg(args, "--turbo-key") is string tk) key = tk;
            Console.WriteLine(Patcher.WriteTurbo(game, en, rate, button, key, pad));
            Console.WriteLine(Patcher.InstallModFiles(game));
            return 0;
        }
        if (args.Contains("--install-mod-files"))
        {
            Console.WriteLine(Patcher.InstallModFiles(game));
            return 0;
        }
        if (args.Contains("--launch-character"))
        {
            Console.WriteLine(Patcher.LaunchCharacter(game, character ?? Patcher.ReadCharacter(game)));
            return 0;
        }
        if (args.Contains("--launch"))
        {
            if (character != null)
                Console.WriteLine(Patcher.LaunchCharacter(game, character));
            else
                Console.WriteLine(Patcher.LaunchSafe(game));
            return 0;
        }
        if (character != null)
            return 0;
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
        if (args.Contains("--laa")) s.Laa = true;
        if (args.Contains("--fullscreen")) s.Fullscreen = true;
        if (args.Contains("--windowed")) s.Fullscreen = false;
        if (args.Contains("--vsync")) s.Vsync = true;
        if (args.Contains("--no-vsync")) s.Vsync = false;
        if (args.Contains("--msaa")) s.Msaa = true;
        if (args.Contains("--no-msaa")) s.Msaa = false;
        if (args.Contains("--mlaa")) s.Mlaa = true;
        if (args.Contains("--no-mlaa")) s.Mlaa = false;
        if (args.Contains("--no-lock")) s.LockConfig = false;
        if (args.Contains("--retro-textures")) s.RetroTextures = true;
        if (args.Contains("--no-retro-textures")) s.RetroTextures = false;
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
