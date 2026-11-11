using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using System.Runtime.InteropServices;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Styling;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.Platform.Storage;
using Avalonia.Themes.Fluent;

namespace Mn9Patcher;

internal static class Program
{
    [STAThread]
    public static void Main(string[] args)
    {
        if (args.Contains("--cli"))
        {
            Environment.Exit(Cli.Run(args));
            return;
        }
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static AppBuilder BuildAvaloniaApp() =>
        AppBuilder.Configure<App>().UsePlatformDetect().LogToTrace();
}

public class App : Application
{
    public override void Initialize()
    {
        RequestedThemeVariant = ThemeVariant.Dark;
        Styles.Add(new FluentTheme());
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            desktop.MainWindow = new MainWindow();
        base.OnFrameworkInitializationCompleted();
    }
}

public sealed class MainWindow : Window
{
    private readonly ComboBox _pRes = new() { Width = 140, ItemsSource = ResolutionPresets, SelectedIndex = 3 };
    private readonly ComboBox _pQuality = new()
    {
        Width = 150,
        ItemsSource = Port.Qualities,
        SelectedIndex = 1,
    };
    private readonly ComboBox _pFps = new()
    {
        Width = 170,
        ItemsSource = new[] { "60 FPS (smooth)", "30 FPS (low power)" },
        SelectedIndex = 0,
    };
    private readonly CheckBox _pFullscreen = new() { Content = "Fullscreen" };
    private readonly CheckBox _pVsync = new() { Content = "VSync" };
    private readonly CheckBox _pSkipIntro = new() { Content = "Skip intro screens", IsChecked = true };

    // PC Display tab dedicated controls
    private readonly ComboBox _pcResDisplay = new() { Width = 140, ItemsSource = ResolutionPresets, SelectedIndex = 3 };
    private readonly ComboBox _pcFpsDisplay = new() { Width = 170, ItemsSource = new[] { "30", "60", "62", "72", "90", "120", "144", "240" }, SelectedIndex = 2 };
    private readonly CheckBox _pcFullscreenDisplay = new() { Content = "Fullscreen" };
    private readonly CheckBox _pcVsyncDisplay = new() { Content = "VSync", IsChecked = true };

    // PC effects
    private readonly CheckBox _pcShadows = new() { Content = "Enabled", IsChecked = true };
    private readonly CheckBox _pcBloom = new() { Content = "Enabled", IsChecked = true };
    private readonly CheckBox _pcMotionBlur = new() { Content = "Enabled", IsChecked = true };
    private readonly CheckBox _pcLightShafts = new() { Content = "Enabled", IsChecked = true };
    private readonly CheckBox _pcDistortion = new() { Content = "Enabled", IsChecked = true };
    private readonly CheckBox _pcLensFlares = new() { Content = "Enabled", IsChecked = true };
    private readonly CheckBox _pcAmbientOcclusion = new() { Content = "Enabled", IsChecked = true };
    private readonly CheckBox _pcMlaa = new() { Content = "Enabled" };
    private readonly CheckBox _pcRetro = new() { Content = "Enabled" };

    // PC advanced
    private readonly CheckBox _pcVoice = new() { Content = "Steam Voice", IsChecked = true };
    private readonly CheckBox _pcMouse = new() { Content = "Mouse Release" };
    private readonly CheckBox _pcLaa = new() { Content = "Large Address Aware" };
    private readonly CheckBox _pcLock = new() { Content = "Config Lock", IsChecked = true };
    private readonly CheckBox _pcMovies = new() { Content = "Intro Movies" };

    // PC audio shim
    private readonly ComboBox _pcMusic = new() { Width = 140, ItemsSource = new[] { "game", "mute", "random", "playlist", "shuffle" }, SelectedIndex = 0 };
    private readonly TextBox _pcMusicDir = new() { Width = 300 };
    private static readonly (string Label, string Value)[] MusicVariantOptions =
    {
        ("Both (normal + retro)", "both"),
        ("Normal BGM only", "normal"),
        ("Retro (8-bit) only", "retro"),
    };
    private readonly ComboBox _pcMusicVariant = new()
    {
        Width = 200,
        ItemsSource = MusicVariantOptions.Select(o => o.Label).ToArray(),
        SelectedIndex = 0,
    };
    private readonly Button _pcMusicBrowse = new() { Content = "Browse" };
    private readonly ComboBox _pcVoiceMode = new() { Width = 140, ItemsSource = new[] { "game", "mute" }, SelectedIndex = 0 };
    private static readonly (string Label, string Value)[] TitleVoiceOptions =
    {
        ("Game (default)", "game"),
        ("Random", "random"),
        ("Beck", "MN9_TCV_beck.ogg"),
        ("Call", "MN9_TCV_call.ogg"),
        ("Brand", "MN9_TCV_brand.ogg"),
        ("Cryo", "MN9_TCV_cryo.ogg"),
        ("Dyna", "MN9_TCV_dyna.ogg"),
        ("Bat", "MN9_TCV_bat.ogg"),
        ("Avi", "MN9_TCV_avi.ogg"),
        ("Mic", "MN9_TCV_mic.ogg"),
        ("Pyro", "MN9_TCV_pyro.ogg"),
        ("Sanda", "MN9_TCV_sanda.ogg"),
        ("Dr. Sanda", "MN9_TCV_Drsanda.ogg"),
        ("Shade", "MN9_TCV_shade.ogg"),
        ("White", "MN9_TCV_white.ogg"),
        ("Blackwell", "MN9_TCV_blackwell.ogg"),
        ("Graham", "MN9_TCV_graham.ogg"),
        ("Narrator", "MN9_TCV_narr.ogg"),
    };
    private readonly ComboBox _pcTitleVoice = new()
    {
        Width = 180,
        ItemsSource = TitleVoiceOptions.Select(o => o.Label).ToArray(),
        SelectedIndex = 0,
    };
    private readonly ComboBox _pcSfxPitch = new() { Width = 140, ItemsSource = new[] { "0", "0.05", "0.10", "0.20" }, SelectedIndex = 0 };
    private readonly ComboBox _pcMusicGain = new() { Width = 110, ItemsSource = new[] { "0", "0.25", "0.5", "0.75", "1" }, SelectedIndex = 4 };
    private readonly ComboBox _pcSfxGain = new() { Width = 110, ItemsSource = new[] { "0", "0.25", "0.5", "0.75", "1" }, SelectedIndex = 4 };
    private readonly ComboBox _pcVoiceGain = new() { Width = 110, ItemsSource = new[] { "0", "0.25", "0.5", "0.75", "1" }, SelectedIndex = 4 };
    private readonly CheckBox _pcDuck = new() { Content = "Duck music during dialog", IsChecked = true };
    private readonly CheckBox _pcNight = new() { Content = "Night mode (compress peaks)" };
    private static readonly string[] ResolutionPresets =
    {
        "640x360", "960x540", "1024x768", "1280x720", "1280x800",
        "1280x960", "1440x1080", "1680x1050", "1920x1080", "1920x1200",
    };

    // ReXGlue port controls
    private readonly ComboBox _cvScale = new()
    {
        Width = 110,
        ItemsSource = new[] { "preset", "1", "2", "3" },
        SelectedIndex = 0,
    };
    private readonly ComboBox _cvAniso = new()
    {
        Width = 120,
        ItemsSource = new[] { "preset", "off", "1x", "2x", "4x", "8x", "16x" },
        SelectedIndex = 0,
    };
    private readonly ComboBox _cvMsaa = new() { Width = 100, ItemsSource = new[] { "preset", "on", "off" }, SelectedIndex = 0 };
    private readonly ComboBox _cvDither = new() { Width = 100, ItemsSource = new[] { "preset", "on", "off" }, SelectedIndex = 0 };
    private readonly ComboBox _cvFxaa = new()
    {
        Width = 140,
        ItemsSource = new[] { "preset", "on", "extreme", "off" },
        SelectedIndex = 0,
    };
    private readonly CheckBox _cvAsync = new() { Content = "Async shader compilation (may skip frames)" };
    private readonly CheckBox _cvLowMem = new() { Content = "Low-memory mode (960x540, reduced caches)", IsChecked = true };
    private readonly CheckBox _cvMnk = new() { Content = "Keyboard as controller (mnk mode)", IsChecked = true };
    private readonly TextBox _cvSkipPresses = new() { Width = 70, Text = "2" };
    private readonly TextBox _cvSkipGap = new() { Width = 70, Text = "4" };
    private readonly TextBox _cvSkipDelay = new() { Width = 70, Text = "14" };
    private readonly ComboBox _cvBoot = new()
    {
        Width = 190,
        ItemsSource = new[] { "Fast (BootGame)", "Normal boot", "Title menu", "Stage select" },
        SelectedIndex = 0,
    };
    private static readonly int[] AudioVolumes = { 0, 25, 50, 75, 100 };
    private readonly ComboBox _cvAudioMaster = new()
    {
        Width = 110,
        ItemsSource = new[] { "0%", "25%", "50%", "75%", "100%" },
        SelectedIndex = 4,
    };
    private readonly CheckBox _cvAudioEnable = new() { Content = "Enable port audio processing" };
    private readonly CheckBox _cvAudioMute = new() { Content = "Mute all audio" };
    private readonly CheckBox _cvAudioNight = new() { Content = "Night mode (compress peaks)" };
    private readonly CheckBox _cvDlcGolden = new() { Content = "Golden Beck", IsChecked = true };
    private readonly CheckBox _cvDlcMegaXel = new() { Content = "MegaXel Form", IsChecked = true };
    private readonly CheckBox _cvDlcRay = new() { Content = "Ray Expansion", IsChecked = true };
    private readonly CheckBox _cvDlcRetro = new() { Content = "Retro Hero", IsChecked = true };
    private readonly TextBox _cvXbla = new() { Width = 360 };
    private readonly TextBox _cvInstallDir = new() { Width = 360 };
    private readonly ListBox _cvAddonList = new() { Height = 56, Width = 520 };
    private readonly List<string> _addonSelections = new();
    private readonly CheckBox _cvReplace = new() { Content = "Replace existing game data", IsChecked = true };
    private readonly TextBlock _cvInstallStatus = new()
    {
        TextWrapping = TextWrapping.Wrap,
        Foreground = Brushes.Gray,
        FontSize = 12,
    };
    private static readonly int[] TexCacheValues = { 0, 64, 128, 256, 512, 1024, 2048, 4096 };
    private readonly ComboBox _cvTexCache = new()
    {
        Width = 260,
        ItemsSource = new[]
        {
            "Auto (runtime default)",
            "64 MB — minimum",
            "128 MB — low",
            "256 MB — balanced",
            "512 MB — high",
            "1024 MB — very high",
            "2048 MB — extreme",
            "4096 MB — maximum",
        },
        SelectedIndex = 0,
    };
    private readonly ComboBox _cvTexDetail = new()
    {
        Width = 230,
        ItemsSource = new[]
        {
            "Native (full texture set)",
            "Low (skip largest mip)",
            "Very low (skip two mips)",
        },
        SelectedIndex = 0,
    };
    private readonly ComboBox _cvStart = new()
    {
        Width = 160,
        ItemsSource = new[] { "Title screen", "Main menu", "Gameplay" },
        SelectedIndex = 1,
    };
    private readonly TextBox _cvLicense = new() { Width = 150, Text = "4294967295" };
    private readonly TextBox _cvUserData = new() { Width = 380 };
    private readonly TextBlock _cvContent = new() { TextWrapping = TextWrapping.Wrap, Foreground = Brushes.Gray };
    private readonly TextBlock _cvSave = new() { TextWrapping = TextWrapping.Wrap, Foreground = Brushes.Gray };
    private readonly TextBlock _cvStatus = new() { TextWrapping = TextWrapping.Wrap, Foreground = Brushes.Gray };
    private readonly TextBox _cvLog = new()
    {
        IsReadOnly = true,
        AcceptsReturn = true,
        Height = 170,
        FontFamily = new FontFamily("monospace"),
        FontSize = 11,
        TextWrapping = TextWrapping.NoWrap,
    };
    private readonly Dictionary<string, Button> _cvKeyButtons = new();
    private readonly Dictionary<string, Button> _cvGamepadButtons = new();
    private string _bindingKey = null;
    private bool _awaitingKey = false;
    private bool _awaitingGamepad = false;
    private string _selectedCharacter = "normal";
    private TextBlock _charStatus;

    // Character/skin controls
    private readonly ComboBox _skin = new() { Width = 180, ItemsSource = Patcher.CharacterNames, SelectedIndex = 0 };
    private readonly ComboBox _paletteBox = new() { Width = 230, ItemsSource = PaletteSettings.Names, SelectedIndex = 0 };
    private readonly CheckBox _pcSkipCutscenes = new() { Content = "Auto-skip cutscenes" };
    private readonly TextBox _pcWallpaper = new() { Width = 260, Margin = new Thickness(0, 0, 8, 4) };
    private readonly Button _pcWallpaperBrowse = new() { Content = "Browse", Margin = new Thickness(0, 0, 8, 4) };
    private readonly CheckBox _pcWallpaperClean = new() { Content = "Clean title (hide logo/art)", IsChecked = true };
    private readonly Button _pcWallpaperApply = new() { Content = "Apply wallpaper", Margin = new Thickness(0, 0, 8, 4) };
    private readonly Button _pcWallpaperRestore = new() { Content = "Restore title", Margin = new Thickness(0, 0, 8, 4) };
    private readonly TextBlock _pcWallpaperStatus = new()
    {
        TextWrapping = TextWrapping.Wrap,
        Foreground = Brushes.Gray,
        FontSize = 12,
    };
    private readonly CheckBox _pcDisableDialog = new() { Content = "Disable dialog" };
    private readonly TextBlock _characterNote = new() { Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock _saveInfo = new() { Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap };
    private readonly ComboBox _saveCombo = new() { Width = 300 };
    private readonly Button backupSaves = new() { Content = "Backup" };
    private readonly Button restoreSaves = new() { Content = "Restore" };
    private List<SaveBackupInfo> _saveBackups = new();

    // Buttons
    private readonly Button apply = new() { Content = "Apply settings" };
    private readonly Button restore = new() { Content = "Restore defaults" };
    private readonly Button launch = new() { Content = "Launch" };
    private readonly Button mods = new() { Content = "Mod loader" };
    private readonly Button addAddon = new() { Content = "Add addon" };
    private readonly Button removeAddon = new() { Content = "Remove addon" };
    private readonly Button installPort = new() { Content = "Install port" };
    private readonly Button browseXbla = new() { Content = "Browse" };
    private readonly Button browseInstall = new() { Content = "Browse" };
    private readonly TextBlock _status = new() { Foreground = Brushes.Gray, FontSize = 12 };


    public MainWindow()
    {
        Title = "Xel Launcher — Mighty No. 9";
        Width = 1000;
        Height = 720;
        MinWidth = 800;
        MinHeight = 600;
        Background = new SolidColorBrush(Color.Parse("#0B1A2A"));

        var root = new StackPanel();
        Content = root;

        var portTab = BuildPortTab();
        // PC controls (Steam build)
        var pcDisplay = new StackPanel
        {
            Margin = new Thickness(14),
            Spacing = 6,
            Children =
            {
                Section("Display",
                    SimpleRow("Resolution", _pcResDisplay),
                    SimpleRow("Fullscreen", _pcFullscreenDisplay),
                    SimpleRow("VSync", _pcVsyncDisplay),
                    SimpleRow("FPS cap", _pcFpsDisplay)),
                Section("Title screen",
                    SimpleRow("Wallpaper", new WrapPanel
                    {
                        Orientation = Orientation.Horizontal,
                        Children =
                        {
                            _pcWallpaper, _pcWallpaperBrowse, _pcWallpaperApply, _pcWallpaperRestore,
                        },
                    }),
                    CheckList(_pcWallpaperClean),
                    _pcWallpaperStatus,
                    new TextBlock { Text = "Replaces Title_I6 inside TitleMenu.upk (DXT5 960x540). Clean title also blanks the logo, Xel emblem, arcs and Beck line-art layers, leaving just the wallpaper and button prompts. A backup is kept; Restore title undoes everything. Close the game first.", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap })
            }
        };
        var pcQuality = new StackPanel
        {
            Margin = new Thickness(14),
            Spacing = 6,
            Children =
            {
                Section("Effects",
                    SimpleRow("Shadows", _pcShadows),
                    SimpleRow("Bloom", _pcBloom),
                    SimpleRow("Motion Blur", _pcMotionBlur),
                    SimpleRow("Light Shafts", _pcLightShafts),
                    SimpleRow("Distortion", _pcDistortion),
                    SimpleRow("Lens Flares", _pcLensFlares),
                    SimpleRow("Ambient Occlusion", _pcAmbientOcclusion),
                    SimpleRow("MLAA", _pcMlaa),
                    SimpleRow("Retro Textures", _pcRetro))
            }
        };
        var pcAudio = new StackPanel
        {
            Margin = new Thickness(14),
            Spacing = 6,
            Children =
            {
                Section("Audio (OpenAL shim)",
                    SimpleRow("Music", _pcMusic),
                    SimpleRow("Music folder", new StackPanel
                    {
                        Orientation = Orientation.Horizontal,
                        Spacing = 8,
                        Children = { _pcMusicDir, _pcMusicBrowse },
                    }),
                    SimpleRow("BGM variants", _pcMusicVariant),
                    SimpleRow("Music gain", _pcMusicGain),
                    SimpleRow("SFX gain", _pcSfxGain),
                    SimpleRow("SFX pitch", _pcSfxPitch),
                    SimpleRow("Voice", _pcVoiceMode),
                    SimpleRow("Voice gain", _pcVoiceGain),
                    SimpleRow("Title call", _pcTitleVoice),
                    CheckList(_pcDuck, _pcNight),
                    new TextBlock { Text = "The shim classifies sources by length: long = music, <=0.45s = SFX, 0.45-5s = voice. Music random/playlist/shuffle plays ogg/wav/flac/mp3 files from the music folder and moves to the next track when one ends (playlist follows folder order, random/shuffle pick their own); game/mute keep the original BGM. Title call replaces the \"Mighty No. 9\" voice line on the title screen (random, or one character).", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap })
            }
        };
        var pcGameplay = new StackPanel
        {
            Margin = new Thickness(14),
            Spacing = 6,
            Children =
            {
                Section("Gameplay",
                    CheckList(_pcSkipCutscenes, _pcDisableDialog),
                    new TextBlock { Text = "Writes .mn9charsel/options and installs the cutscene/dialog shim. The game must be closed when applying.", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap })
            }
        };
        var pcCharacter = new StackPanel
        {
            Margin = new Thickness(14),
            Spacing = 6,
            Children =
            {
                Section("Character",
                    SimpleRow("Skin", _skin),
                    SimpleRow("Palette", _paletteBox),
                    new TextBlock { Text = "Boss palette uses the Mighty Numbers' exact body/head/eye/cheek/mouth materials. It applies to regular Beck; Gold/Retro/Call/Ray keep their own colors.", Foreground = Brushes.Gray, TextWrapping = TextWrapping.Wrap, FontSize = 12 },
                    _characterNote)
            }
        };
        var pcSaves = new StackPanel
        {
            Margin = new Thickness(14),
            Spacing = 6,
            Children =
            {
                Section("Save backups",
                    new StackPanel
                    {
                        Orientation = Orientation.Horizontal,
                        Spacing = 10,
                        Children = { _saveCombo, backupSaves, restoreSaves },
                    },
                    _saveInfo)
            }
        };
        var pcAdvanced = new StackPanel
        {
            Margin = new Thickness(14),
            Spacing = 6,
            Children =
            {
                Section("Advanced",
                    CheckList(_pcVoice, _pcMouse, _pcLaa, _pcLock, _pcMovies))
            }
        };

        var steamTabs = BuildCategoryPager(
            new[] { "Display", "Quality", "Audio", "Gameplay", "Character", "Saves", "Advanced" },
            new Dictionary<string, Control>
            {
                ["Display"] = pcDisplay,
                ["Quality"] = pcQuality,
                ["Audio"] = pcAudio,
                ["Gameplay"] = pcGameplay,
                ["Character"] = pcCharacter,
                ["Saves"] = pcSaves,
                ["Advanced"] = pcAdvanced,
            },
            "Display");
        var pages = new TabControl
        {
            Margin = new Thickness(18, 10, 18, 0),
            Items =
            {
                new TabItem { Header = "Port", Content = portTab },
                new TabItem { Header = "Steam", Content = steamTabs },
            },
        };

        var launchSteamBtn = new Button { Content = "Launch Steam build" };
        var buttons = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10,
            Children = { apply, restore, launch, mods, launchSteamBtn },
        };

        var bottom = new StackPanel
        {
            Margin = new Thickness(18, 0, 18, 18),
            Spacing = 8,
            Children = { buttons, _status },
        };

        var logo = new Image
        {
            Source = LoadAsset("logo.png"),
            Height = 78,
            Stretch = Stretch.Uniform,
            VerticalAlignment = VerticalAlignment.Center,
        };

        var headerText = new StackPanel
        {
            Spacing = 4,
            Children =
            {
                new TextBlock { Text = "Xel Launcher", FontSize = 22, FontWeight = FontWeight.Bold, Foreground = new SolidColorBrush(Color.Parse("#5CE8FF")) },
                new TextBlock { Text = "Mighty No. 9 patcher & port launcher", FontSize = 12, Foreground = Brushes.Gray },
            },
        };

        var header = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 16,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(18, 10, 18, 0),
            Children = { logo, headerText },
        };

        root.Children.Add(header);
        root.Children.Add(new Border { BorderBrush = new SolidColorBrush(Color.Parse("#5CE8FF")), BorderThickness = new Thickness(0, 0, 0, 1), Margin = new Thickness(18, 0) });
        root.Children.Add(pages);
        root.Children.Add(bottom);

        apply.Click += (_, _) =>
        {
            // Write even when the install folder does not exist yet: the port
            // config path is independent, and Port.Apply reports if it is missing.
            string dir = PcDir();
            var messages = new List<string> { Port.Apply(CollectPort()) };
            if (dir.Length > 0 && Directory.Exists(dir) && LooksLikePcInstall(dir))
            {
                SavePcMods(dir);
                messages.Add(Patcher.Apply(dir, CollectPc()));
                var (_, _, cel, strength, fxaa) = Patcher.ReadOptions(dir);
                messages.Add(Patcher.WriteOptions(dir,
                    _pcSkipCutscenes.IsChecked == true, _pcDisableDialog.IsChecked == true,
                    cel, strength, fxaa));
                int ch = Math.Clamp(_skin.SelectedIndex, 0, Patcher.CharacterIds.Length - 1);
                messages.Add(Patcher.SetCharacter(dir, Patcher.CharacterIds[ch]));
                messages.Add(Patcher.InstallModFiles(dir));
            }
            else
            {
                messages.Add("Steam game folder not found; PC settings were not written.");
            }
            // Re-read what was actually written so the UI reflects the file.
            LoadPortSettings();
            LoadPcSettings(ReadPcSettings());
            LoadPcAudio(ReadPcAudio());
            _status.Text = string.Join("\n", messages);
        };
        restore.Click += (_, _) =>
        {
            PortSettings s = new();
            string dir = CurrentDir();
            if (dir != null)
            {
                string conf = Path.Combine(dir, "port.conf");
                if (File.Exists(conf)) Port.ReadInto(s, conf);
            }
            LoadPortSettingsFrom(s);
            LoadPcSettings(new Settings());
            LoadPcAudio(new AudioSettings());
            string dir2 = PcDir();
            if (dir2.Length > 0 && LooksLikePcInstall(dir2))
            {
                string msg = Patcher.Restore(dir2);
                Patcher.SetCharacter(dir2, "beck");
                _status.Text = "Restored defaults\n" + msg;
            }
            else
            {
                _status.Text = "Restored defaults";
            }
        };
        launch.Click += (_, _) =>
        {
            string dir = CurrentDir();
            if (dir == null) return;
            PortSettings ps = CollectPort();
            string msg = Port.Launch(ps);
            _status.Text = msg;
            if (msg.Contains("Launched", StringComparison.OrdinalIgnoreCase))
                WindowState = WindowState.Minimized;
        };
        launchSteamBtn.Click += (_, _) =>
        {
            string dir = PcDir();
            if (dir.Length == 0) return;
            int ch = Math.Clamp(_skin.SelectedIndex, 0, Patcher.CharacterIds.Length - 1);
            string msg = LooksLikePcInstall(dir)
                ? SavePcMods(dir) + "\n" + Patcher.Apply(dir, CollectPc()) + "\n" + Patcher.SetCharacter(dir, Patcher.CharacterIds[ch]) + "\n" + Patcher.InstallModFiles(dir)
                : "Not a Steam build install.";
            _status.Text = msg + "\n" + Port.LaunchPc();
            WindowState = WindowState.Minimized;
        };
        mods.Click += (_, _) =>
        {
            string dir = CurrentDir();
            if (dir == null) return;
            Environment.SetEnvironmentVariable("MN9_MODLOADER", "1");
            PortSettings ps = CollectPort();
            string msg = Port.Launch(ps, openModLoader: true);
            _status.Text = msg;
            if (msg.Contains("Launched", StringComparison.OrdinalIgnoreCase))
                WindowState = WindowState.Minimized;
        };

        browseXbla.Click += async (_, _) =>
        {
            var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
            {
                Title = "Select the XBLA package or extracted game folder",
                AllowMultiple = false,
                FileTypeFilter = new[] { new FilePickerFileType("XBLA package") { Patterns = new[] { "*.xex", "*.zip", "*.7z" } } },
            });
            if (files.Count > 0)
            {
                string path = files[0].TryGetLocalPath() ?? "";
                if (path.Length > 0) _cvXbla.Text = path;
            }
        };
        _pcWallpaperBrowse.Click += async (_, _) =>
        {
            var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
            {
                Title = "Select a title wallpaper image",
                AllowMultiple = false,
                FileTypeFilter = new[]
                {
                    new FilePickerFileType("Images")
                    {
                        Patterns = new[] { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.webp" },
                    },
                },
            });
            if (files.Count > 0)
                _pcWallpaper.Text = files[0].TryGetLocalPath() ?? _pcWallpaper.Text;
        };
        _pcWallpaperApply.Click += (_, _) =>
        {
            string dir = PcDir();
            string img = _pcWallpaper.Text?.Trim() ?? "";
            _pcWallpaperStatus.Text = img.Length == 0
                ? "Choose a wallpaper image first."
                : TitleWallpaper.Install(dir, img, _pcWallpaperClean.IsChecked == true);
            if (File.Exists(TitleWallpaper.ImagePath(dir)))
                _pcWallpaper.Text = TitleWallpaper.ImagePath(dir);
        };
        _pcWallpaperRestore.Click += (_, _) =>
        {
            _pcWallpaperStatus.Text = TitleWallpaper.Restore(PcDir());
            _pcWallpaper.Text = "";
        };
        _pcMusicBrowse.Click += async (_, _) =>
        {
            var folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
            {
                Title = "Select a folder of music files (ogg/wav/flac/mp3)",
                AllowMultiple = false,
            });
            if (folders.Count > 0)
                _pcMusicDir.Text = folders[0].TryGetLocalPath() ?? _pcMusicDir.Text;
        };
        browseInstall.Click += async (_, _) =>
        {
            var folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
            {
                Title = "Select the port install folder",
                AllowMultiple = false,
            });
            if (folders.Count > 0)
            {
                string path = folders[0].TryGetLocalPath() ?? "";
                if (path.Length > 0) _cvInstallDir.Text = path;
            }
        };
        addAddon.Click += async (_, _) =>
        {
            var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
            {
                Title = "Add addon package (DLC)",
                AllowMultiple = true,
            });
            foreach (var f in files)
            {
                string path = f.TryGetLocalPath() ?? "";
                if (path.Length > 0 && !_addonSelections.Contains(path))
                    _addonSelections.Add(path);
            }
            _cvAddonList.ItemsSource = _addonSelections.ToList();
        };
        removeAddon.Click += (_, _) =>
        {
            if (_cvAddonList.SelectedItem is string sel)
            {
                _addonSelections.Remove(sel);
                _cvAddonList.ItemsSource = _addonSelections.ToList();
            }
        };
        installPort.Click += async (_, _) =>
        {
            string xbla = _cvXbla.Text?.Trim() ?? "";
            if (xbla.Length == 0)
            {
                _cvInstallStatus.Text = "Choose the XBLA package (or an extracted game folder) first.";
                return;
            }
            PortSettings ps = CollectPort();
            ps.InstallDir = _cvInstallDir.Text?.Trim() ?? "";
            bool replace = _cvReplace.IsChecked == true;
            string[] addons = _addonSelections.ToArray();
            installPort.IsEnabled = false;
            _cvInstallStatus.Text = "Installing the port...";
            try
            {
                IProgress<string> progress = new Progress<string>(m => _cvInstallStatus.Text = m);
                string msg = await Task.Run(() => Port.InstallPort(ps, xbla, addons, replace, m => progress.Report(m)));
                LoadPortSettings();
                _cvInstallStatus.Text = msg;
            }
            catch (Exception ex)
            {
                _cvInstallStatus.Text = "Install failed: " + ex.Message;
            }
            finally
            {
                installPort.IsEnabled = true;
            }
        };

        backupSaves.Click += (_, _) =>
        {
            string dir = PcDir();
            if (dir.Length == 0 || !Directory.Exists(dir)) return;
            _saveInfo.Text = Patcher.BackupSaves(dir, "launcher");
            RefreshSaveList();
        };
        restoreSaves.Click += (_, _) =>
        {
            string dir = CurrentDir();
            if (dir == null) return;
            if (_saveCombo.SelectedIndex < 0 || _saveCombo.SelectedIndex >= _saveBackups.Count)
            {
                _saveInfo.Text = "Select a backup first.";
                return;
            }
            string id = _saveBackups[_saveCombo.SelectedIndex].Id;
            _saveInfo.Text = Patcher.RestoreSaves(PcDir(), id);
            RefreshSaveList();
        };

        LoadPortSettings();
        LoadPcSettings(ReadPcSettings());
        LoadPcAudio(ReadPcAudio());
        RefreshSaveList();
    }

    private static bool LooksLikePcInstall(string dir) =>
        Directory.Exists(Path.Combine(dir, "MN9Game", "Config"));

    // PC/Steam settings live in the Steam game folder, not in the ReXGlue port
    // folder. The Install folder box on the Port tab points at the port, so the
    // Steam tab must resolve its own target.
    private string PcDir()
    {
        string dir = _cvInstallDir.Text?.Trim() ?? "";
        if (dir.Length > 0 && LooksLikePcInstall(dir)) return dir;
        string found = Patcher.FindGameDir();
        return string.IsNullOrEmpty(found) ? dir : found;
    }

    private Settings ReadPcSettings()
    {
        try
        {
            string dir = PcDir();
            if (dir.Length == 0 || !Directory.Exists(dir)) return new Settings();
            return Patcher.ReadPcSettings(dir);
        }
        catch
        {
            return new Settings();
        }
    }

    private void LoadPcSettings(Settings s)
    {
        _pcShadows.IsChecked = s.Shadows;
        _pcBloom.IsChecked = s.Bloom;
        _pcMotionBlur.IsChecked = s.MotionBlur;
        _pcLightShafts.IsChecked = s.LightShafts;
        _pcDistortion.IsChecked = s.Distortion;
        _pcLensFlares.IsChecked = s.LensFlares;
        _pcAmbientOcclusion.IsChecked = s.AmbientOcclusion;
        _pcMlaa.IsChecked = s.Mlaa;
        _pcRetro.IsChecked = s.RetroTextures;
        _pcFullscreenDisplay.IsChecked = s.Fullscreen;
        _pcVsyncDisplay.IsChecked = s.Vsync;
        _pcVoice.IsChecked = s.Voice;
        _pcMovies.IsChecked = s.Movies;
        _pcMouse.IsChecked = s.MouseFix;
        _pcLaa.IsChecked = s.Laa;
        _pcLock.IsChecked = s.LockConfig;
        SelectComboValue(_pcResDisplay, s.Resolution);
        int fpsIdx = Array.IndexOf(new[] { 30, 60, 62, 72, 90, 120, 144, 240 }, s.Fps);
        _pcFpsDisplay.SelectedIndex = fpsIdx >= 0 ? fpsIdx : 2;

        string dir = PcDir();
        if (dir.Length > 0 && Directory.Exists(dir))
        {
            var (skip, dialog, _, _, _) = Patcher.ReadOptions(dir);
            _pcSkipCutscenes.IsChecked = skip;
            _pcDisableDialog.IsChecked = dialog;
            int ch = Array.IndexOf(Patcher.CharacterIds, Patcher.ReadCharacter(dir));
            _skin.SelectedIndex = ch < 0 ? 0 : ch;
            int pal = PaletteSettings.Read(dir);
            _paletteBox.SelectedIndex = pal >= 0 && pal < PaletteSettings.Names.Length ? pal : 0;
            string wp = TitleWallpaper.ImagePath(dir);
            _pcWallpaper.Text = File.Exists(wp) ? wp : "";
        }
    }

    private Settings CollectPc()
    {
        var s = new Settings
        {
            Shadows = _pcShadows.IsChecked == true,
            Bloom = _pcBloom.IsChecked == true,
            MotionBlur = _pcMotionBlur.IsChecked == true,
            LightShafts = _pcLightShafts.IsChecked == true,
            Distortion = _pcDistortion.IsChecked == true,
            LensFlares = _pcLensFlares.IsChecked == true,
            AmbientOcclusion = _pcAmbientOcclusion.IsChecked == true,
            Mlaa = _pcMlaa.IsChecked == true,
            RetroTextures = _pcRetro.IsChecked == true,
            Fullscreen = _pcFullscreenDisplay.IsChecked == true,
            Vsync = _pcVsyncDisplay.IsChecked == true,
            Voice = _pcVoice.IsChecked == true,
            Movies = _pcMovies.IsChecked == true,
            MouseFix = _pcMouse.IsChecked == true,
            Laa = _pcLaa.IsChecked == true,
            LockConfig = _pcLock.IsChecked == true,
            Fps = int.TryParse(_pcFpsDisplay.SelectedItem as string, out int fps) ? fps : 62,
        };
        string res = _pcResDisplay.SelectedItem as string ?? "1280x720";
        string[] wh = res.Split('x');
        if (wh.Length == 2 && int.TryParse(wh[0], out int w) && int.TryParse(wh[1], out int h))
        {
            s.Width = w;
            s.Height = h;
        }
        return s;
    }

    private AudioSettings ReadPcAudio()
    {
        string dir = PcDir();
        if (dir.Length == 0 || !Directory.Exists(dir)) return new AudioSettings();
        return AudioSettings.Read(dir);
    }

    private void LoadPcAudio(AudioSettings a)
    {
        _pcMusic.SelectedItem = a.Music;
        _pcMusicDir.Text = a.MusicDir;
        int mv = Array.FindIndex(MusicVariantOptions, o => o.Value == a.MusicVariant);
        _pcMusicVariant.SelectedIndex = mv < 0 ? 0 : mv;
        _pcVoiceMode.SelectedItem = a.Voice;
        _pcSfxPitch.SelectedItem = a.SfxPitch;
        _pcMusicGain.SelectedItem = a.MusicGain;
        _pcSfxGain.SelectedItem = a.SfxGain;
        _pcVoiceGain.SelectedItem = a.VoiceGain;
        int tv = Array.FindIndex(TitleVoiceOptions, o => o.Value == a.TitleVoice);
        _pcTitleVoice.SelectedIndex = tv < 0 ? 0 : tv;
        _pcDuck.IsChecked = a.Duck;
        _pcNight.IsChecked = a.Night;
    }

    private AudioSettings CollectAudio()
    {
        string dir = PcDir();
        var a = dir.Length > 0 && Directory.Exists(dir) ? AudioSettings.Read(dir) : new AudioSettings();
        a.Music = _pcMusic.SelectedItem as string ?? "game";
        a.MusicDir = string.IsNullOrWhiteSpace(_pcMusicDir.Text) ? AudioSettings.DefaultMusicDir : _pcMusicDir.Text.Trim();
        int mvIdx = _pcMusicVariant.SelectedIndex;
        a.MusicVariant = mvIdx >= 0 && mvIdx < MusicVariantOptions.Length ? MusicVariantOptions[mvIdx].Value : "both";
        a.Voice = _pcVoiceMode.SelectedItem as string ?? "game";
        a.SfxPitch = _pcSfxPitch.SelectedItem as string ?? "0";
        a.MusicGain = _pcMusicGain.SelectedItem as string ?? "1";
        a.SfxGain = _pcSfxGain.SelectedItem as string ?? "1";
        a.VoiceGain = _pcVoiceGain.SelectedItem as string ?? "1";
        int tv = _pcTitleVoice.SelectedIndex;
        a.TitleVoice = tv >= 0 && tv < TitleVoiceOptions.Length ? TitleVoiceOptions[tv].Value : "game";
        a.Duck = _pcDuck.IsChecked == true;
        a.Night = _pcNight.IsChecked == true;
        return a;
    }

    private string SavePcMods(string dir)
    {
        string audio = AudioSettings.Write(dir, CollectAudio());
        string palette = PaletteSettings.Write(dir, _paletteBox.SelectedIndex);
        return audio + " " + palette;
    }

    private string CurrentDir()
    {
        string dir = _cvInstallDir.Text?.Trim() ?? "";
        if (string.IsNullOrWhiteSpace(dir))
        {
            _status.Text = "Choose install folder first";
            return null;
        }
        if (!Directory.Exists(dir))
        {
            _status.Text = "Install folder does not exist";
            return null;
        }
        return dir;
    }

    private void LoadPortSettings()
    {
        var ps = Port.Read();
        LoadPortSettingsFrom(ps);
    }

    private void LoadPortSettingsFrom(PortSettings ps)
    {
        _cvScale.SelectedIndex = ps.Scale switch { "1" => 1, "2" => 2, "3" => 3, _ => 0 };
        _cvAniso.SelectedIndex = ps.Aniso switch { "0" => 1, "1" => 2, "2" => 3, "3" => 4, "4" => 5, "5" => 6, _ => 0 };
        _cvMsaa.SelectedIndex = ps.Msaa == "1" ? 1 : ps.Msaa == "0" ? 2 : 0;
        _cvDither.SelectedIndex = ps.Dither == "1" ? 1 : ps.Dither == "0" ? 2 : 0;
        _cvFxaa.SelectedIndex = ps.Fxaa switch { "on" => 1, "extreme" => 2, "off" => 3, _ => 0 };
        _cvAsync.IsChecked = ps.AsyncShaders == "1";
        _cvLowMem.IsChecked = ps.LowMemory;
        _cvMnk.IsChecked = ps.MnkMode;
        SelectComboValue(_pRes, ps.Resolution);
        _pFullscreen.IsChecked = ps.Fullscreen;
        _pVsync.IsChecked = ps.Vsync;
        _pSkipIntro.IsChecked = ps.SkipIntro;
        _pQuality.SelectedItem = ps.Quality;
        _pFps.SelectedIndex = ps.GameHz == 60 ? 1 : 0;
        _cvSkipPresses.Text = ps.SkipPresses.ToString();
        _cvSkipGap.Text = ps.SkipGap.ToString();
        _cvSkipDelay.Text = ps.SkipDelay.ToString();
        _cvAudioEnable.IsChecked = ps.AudioEnabled;
        _cvAudioMute.IsChecked = ps.AudioMute;
        _cvAudioNight.IsChecked = ps.AudioNight;
        _cvAudioMaster.SelectedIndex = Math.Clamp((int)Math.Round(ps.AudioMaster / 25.0), 0, 4);
        _cvDlcGolden.IsChecked = ps.DlcGolden;
        _cvDlcMegaXel.IsChecked = ps.DlcMegaXel;
        _cvDlcRay.IsChecked = ps.DlcRay;
        _cvDlcRetro.IsChecked = ps.DlcRetro;
        _cvInstallDir.Text = string.IsNullOrWhiteSpace(ps.InstallDir) ? Port.DefaultManagedRoot : ps.InstallDir;
        _cvInstallStatus.Text = Port.UseManaged(ps) ? "Installed port: " + ps.InstallDir : "No managed port installed yet; Launch port uses the dev hybrid if available.";
        int texIdx = Array.IndexOf(TexCacheValues, ps.TextureCacheMb);
        _cvTexCache.SelectedIndex = texIdx >= 0 ? texIdx : 0;
        _cvTexDetail.SelectedIndex = Math.Clamp(ps.TextureMipSkip, 0, 2);
        _cvStart.SelectedIndex = ps.StartMode switch { "title" => 0, "game" => 2, _ => 1 };
        _cvBoot.SelectedIndex = ps.BootMode switch { "normal" => 1, "title" => 2, "stage" => 3, _ => 0 };
        _cvLicense.Text = ps.LicenseMask;
        _cvUserData.Text = ps.UserDataRoot;
        _selectedCharacter = ps.SelectedCharacter ?? "normal";
        _charStatus.Text = _selectedCharacter switch
        {
            "golden" => "Golden Beck selected — Golden Beck DLC required",
            "megaxel" => "MegaXel Form selected — MegaXel Form DLC required",
            "ray" => "Ray selected — Ray Expansion DLC required",
            "retro" => "Retro Hero selected — Retro Hero DLC required",
            _ => "Normal Beck selected"
        };
        foreach (var kv in _cvKeyButtons)
        {
            string value = ps.Keybinds.TryGetValue(kv.Key, out string? v) ? v : "";
            kv.Value.Content = string.IsNullOrEmpty(value) ? "default" : value;
        }
        foreach (var kv in _cvGamepadButtons)
        {
            string value = ps.Keybinds.TryGetValue(kv.Key, out string? v) ? v : "";
            kv.Value.Content = string.IsNullOrEmpty(value) ? "default" : value;
        }
        _cvContent.Text = Port.ContentStatusFor(ps);
        _cvSave.Text = Port.SaveStatus(ps);
        _cvStatus.Text = Port.StatusLine(ps);
        _cvLog.Text = Port.TailLog(ps);
    }

    private PortSettings CollectPort()
    {
        var s = new PortSettings
        {
            Backend = "rex",
            Quality = _pQuality.SelectedItem as string ?? "high",
            GameHz = _pFps.SelectedIndex == 1 ? 60 : 120,
            Fullscreen = _pFullscreen.IsChecked == true,
            Vsync = _pVsync.IsChecked == true,
            SkipIntro = _pSkipIntro.IsChecked == true,
            StartMode = _cvStart.SelectedIndex switch { 0 => "title", 2 => "game", _ => "menu" },
            BootMode = _cvBoot.SelectedIndex switch { 1 => "normal", 2 => "title", 3 => "stage", _ => "fast" },
            SkipPresses = IntOr(_cvSkipPresses.Text, 2),
            SkipGap = IntOr(_cvSkipGap.Text, 4),
            SkipDelay = IntOr(_cvSkipDelay.Text, 14),
            Scale = _cvScale.SelectedIndex > 0 ? _cvScale.SelectedIndex.ToString() : "",
            Aniso = _cvAniso.SelectedIndex > 0 ? (_cvAniso.SelectedIndex - 1).ToString() : "",
            Msaa = _cvMsaa.SelectedIndex switch { 1 => "1", 2 => "0", _ => "" },
            Dither = _cvDither.SelectedIndex switch { 1 => "1", 2 => "0", _ => "" },
            Fxaa = _cvFxaa.SelectedIndex switch { 1 => "on", 2 => "extreme", 3 => "off", _ => "preset" },
            AsyncShaders = _cvAsync.IsChecked == true ? "1" : "0",
            TextureCacheMb = TexCacheValues[Math.Clamp(_cvTexCache.SelectedIndex, 0, TexCacheValues.Length - 1)],
            TextureMipSkip = Math.Clamp(_cvTexDetail.SelectedIndex, 0, 2),
            LowMemory = _cvLowMem.IsChecked == true,
            MnkMode = _cvMnk.IsChecked == true,
            AudioEnabled = _cvAudioEnable.IsChecked == true || _cvAudioMaster.SelectedIndex < AudioVolumes.Length - 1 || _cvAudioNight.IsChecked == true,
            AudioMaster = AudioVolumes[Math.Clamp(_cvAudioMaster.SelectedIndex, 0, AudioVolumes.Length - 1)],
            AudioMute = _cvAudioMute.IsChecked == true,
            AudioNight = _cvAudioNight.IsChecked == true,
            DlcGolden = _cvDlcGolden.IsChecked == true,
            DlcMegaXel = _cvDlcMegaXel.IsChecked == true,
            DlcRay = _cvDlcRay.IsChecked == true,
            DlcRetro = _cvDlcRetro.IsChecked == true,
            SelectedCharacter = _selectedCharacter,
            InstallDir = _cvInstallDir.Text?.Trim() ?? "",
            ReplaceGame = _cvReplace.IsChecked == true,
            LicenseMask = string.IsNullOrWhiteSpace(_cvLicense.Text) ? "4294967295" : _cvLicense.Text!.Trim(),
            UserDataRoot = _cvUserData.Text?.Trim() ?? "",
        };
        string res = _pRes.SelectedItem as string ?? "960x540";
        string[] wh = res.Split('x');
        if (wh.Length == 2 && int.TryParse(wh[0], out int w) && int.TryParse(wh[1], out int h))
        {
            s.Width = w;
            s.Height = h;
        }
        foreach (var kv in _cvKeyButtons)
        {
            string value = kv.Value.Content?.ToString() ?? "";
            if (value.Length > 0 && value != "default") s.Keybinds[kv.Key] = value;
        }
        foreach (var kv in _cvGamepadButtons)
        {
            string value = kv.Value.Content?.ToString() ?? "";
            if (value.Length > 0 && value != "default") s.Keybinds[kv.Key] = value;
        }
        return s;
    }

    private static int IntOr(string s, int fallback) => int.TryParse(s, out int v) ? v : fallback;

    private void RefreshSaveList()
    {
        string dir = PcDir();
        if (dir.Length == 0 || !Directory.Exists(dir))
        {
            _saveInfo.Text = "Steam game folder not found; no save backups listed.";
            return;
        }
        _saveBackups = Patcher.ListSaveBackups(dir).ToList();
        _saveCombo.ItemsSource = _saveBackups.Select(b => $"{b.Time:yyyy-MM-dd HH:mm} — {b.Files} files, {b.Bytes:N0} bytes").ToList();
        _saveCombo.SelectedIndex = _saveBackups.Count > 0 ? 0 : -1;
        _saveInfo.Text = _saveBackups.Count == 0 ? "No backups in .mn9patcher/saves" : $"{_saveBackups.Count} backup(s) in .mn9patcher/saves (latest first).";
    }

    private static Bitmap LoadAsset(string name)
    {
        try
        {
            using var stream = AssetLoader.Open(new Uri($"avares://Mn9Patcher/assets/{name}"));
            return new Bitmap(stream);
        }
        catch { return null; }
    }

    private static T Tip<T>(T control, string text) where T : Control
    {
        ToolTip.SetTip(control, text);
        return control;
    }

    private static WrapPanel CheckList(params CheckBox[] boxes)
    {
        var panel = new WrapPanel { Orientation = Orientation.Horizontal };
        foreach (var box in boxes) { box.Margin = new Thickness(0, 4, 22, 4); panel.Children.Add(box); }
        return panel;
    }

    private static void SelectComboValue(ComboBox cb, string? value)
    {
        if (string.IsNullOrWhiteSpace(value)) return;
        var items = (cb.ItemsSource as System.Collections.IEnumerable)?
            .Cast<object>().Select(o => o?.ToString() ?? "").ToList() ?? new List<string>();
        if (!items.Contains(value))
        {
            items.Add(value);
            cb.ItemsSource = items;
        }
        cb.SelectedItem = value;
    }

    private static Border Section(string title, params Control[] children)
    {
        var panel = new StackPanel { Spacing = 6 };
        panel.Children.Add(new TextBlock { Text = title, Foreground = new SolidColorBrush(Color.Parse("#5CE8FF")), FontWeight = FontWeight.Bold, Margin = new Thickness(0, 0, 0, 4) });
        foreach (var child in children) panel.Children.Add(child);
        return new Border { BorderBrush = new SolidColorBrush(Color.Parse("#5CE8FF")), Background = new SolidColorBrush(Color.Parse("#B30B1A2A")), BorderThickness = new Thickness(1), CornerRadius = new CornerRadius(8), Padding = new Thickness(12), Margin = new Thickness(0, 0, 0, 12), Child = panel };
    }

    private static StackPanel SimpleRow(string label, Control control, double width = 190) => new() { Orientation = Orientation.Horizontal, Spacing = 10, Children = { new TextBlock { Text = label, Width = width, VerticalAlignment = VerticalAlignment.Center }, control } };

    private static StackPanel SimpleRow(string label1, Control c1, string label2, Control c2, double width = 190) => new() { Orientation = Orientation.Horizontal, Spacing = 10, Children = { new TextBlock { Text = label1, Width = width, VerticalAlignment = VerticalAlignment.Center }, c1, new TextBlock { Text = label2, Width = width, VerticalAlignment = VerticalAlignment.Center }, c2 } };

    // Same category pager the Port tab uses, so both pages share one look:
    // dark card, cyan-outlined buttons, selected button filled cyan.
    private static Control BuildCategoryPager(string[] order, Dictionary<string, Control> contents, string initial)
    {
        string selected = initial;
        var contentHost = new ScrollViewer
        {
            Content = contents[selected],
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        var buttons = new Dictionary<string, Button>();
        var bar = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Center,
            Spacing = 4,
            Margin = new Thickness(0, 8, 0, 12),
        };

        void Select(string cat)
        {
            selected = cat;
            contentHost.Content = contents[cat];
            foreach (var kv in buttons)
            {
                bool isSel = kv.Key == cat;
                kv.Value.Background = isSel
                    ? new SolidColorBrush(Color.Parse("#5CE8FF"))
                    : new SolidColorBrush(Color.Parse("#1A2A3A"));
                kv.Value.Foreground = isSel ? Brushes.Black : Brushes.White;
            }
        }

        foreach (string cat in order)
        {
            var btn = new Button
            {
                Content = cat,
                Padding = new Thickness(16, 8, 16, 8),
                MinWidth = 100,
                Background = new SolidColorBrush(Color.Parse("#1A2A3A")),
                Foreground = Brushes.White,
                BorderBrush = new SolidColorBrush(Color.Parse("#5CE8FF")),
                BorderThickness = new Thickness(1),
            };
            btn.Click += (_, _) => Select(cat);
            buttons[cat] = btn;
            bar.Children.Add(btn);
        }
        Select(initial);

        return new Border
        {
            Background = new SolidColorBrush(Color.Parse("#0B1A2A")),
            BorderBrush = new SolidColorBrush(Color.Parse("#5CE8FF")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Margin = new Thickness(14),
            Padding = new Thickness(0, 12, 0, 0),
            Child = new StackPanel { Spacing = 0, Children = { bar, contentHost } },
        };
    }

    private Control BuildPortTab()
    {
        var installSection = Section("Install the port from XBLA",
            SimpleRow("XBLA package", new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { _cvXbla, browseXbla } }),
            SimpleRow("Install folder", new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { _cvInstallDir, browseInstall } }),
            new TextBlock { Text = "Optional addon packages (DLC). Add each package file or its extracted folder; they are copied into the port's content folder and unlocked with the game.", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap },
            _cvAddonList,
            new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8, Children = { addAddon, removeAddon } },
            CheckList(_cvReplace),
            SimpleRow("License mask", _cvLicense),
            installPort,
            _cvInstallStatus,
            new TextBlock { Text = "The XBLA package is extracted with the bundled STFS tool, the bundled runtime is copied into the install folder, and port.conf is written with license_mask=4294967295 so the game builds as fully unlocked. Existing game data is replaced when the checkbox is on.", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap });

        var quality = Section("Fine image quality",
            SimpleRow("Draw scale", _cvScale, "Anisotropy", _cvAniso),
            SimpleRow("MSAA", _cvMsaa, "Dither", _cvDither),
            SimpleRow("FXAA (port)", _cvFxaa),
            SimpleRow("Texture cache (soft cap)", _cvTexCache),
            SimpleRow("Texture detail (port)", _cvTexDetail),
            _cvAsync);

        var contentRefresh = new Button { Content = "Refresh" };
        var contentOpen = new Button { Content = "Open folder" };
        var contentApply = new Button { Content = "Apply addon selection" };
        contentRefresh.Click += (_, _) => _cvContent.Text = Port.ContentStatusFor(CollectPort());
        contentOpen.Click += (_, _) => Port.OpenFolder(Port.ContentDirFor(CollectPort()));
        contentApply.Click += (_, _) => _cvContent.Text = Port.ApplyContent(CollectPort());

        var charGolden = new Button { Content = "Golden Beck", Width = 160, Tag = "golden" };
        var charMegaXel = new Button { Content = "MegaXel Form", Width = 160, Tag = "megaxel" };
        var charCall = new Button { Content = "Call", Width = 160, Tag = "call" };
        var charRay = new Button { Content = "Ray", Width = 160, Tag = "ray" };
        var charRetro = new Button { Content = "Retro Hero", Width = 160, Tag = "retro" };
        var charNormal = new Button { Content = "Normal Beck", Width = 160, Tag = "normal" };
        foreach (var btn in new[] { charGolden, charMegaXel, charCall, charRay, charRetro, charNormal }) btn.Click += OnCharacterSelectClick;

        _charStatus = new TextBlock { Text = "Select a character to set as default", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap };

        var characterSelect = Section("Character Select (replaces Credits menu)",
            new TextBlock { Text = "Pick a default character. The selection is saved to port.conf as selected_character. The runtime hook that applies it on launch (and swaps the Credits menu entry) is not implemented yet, so this currently only records the preference.", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap },
            new WrapPanel { Orientation = Orientation.Horizontal, Children = { charNormal, charGolden, charMegaXel, charCall, charRay, charRetro } },
            _charStatus);

        var content = Section("Characters & Addon Packs",
            CheckList(_cvDlcGolden, _cvDlcMegaXel, _cvDlcRay, _cvDlcRetro),
            new TextBlock { Text = "Checked packs are copied into the port's DLC folder; unchecked packs are removed. In game, pick the form from the character/Extra menu. The packs keep their own models and movesets (Ray included), so Beck's controller is not forced onto them.", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap },
            new WrapPanel { Orientation = Orientation.Horizontal, Children = { contentApply, contentRefresh, contentOpen } },
            _cvContent);

        var saveBackup = new Button { Content = "Backup now" };
        var saveRestore = new Button { Content = "Restore latest" };
        var saveClear = new Button { Content = "Clear save" };
        var saveOpen = new Button { Content = "Open folder" };
        var saveRefresh = new Button { Content = "Refresh" };
        saveBackup.Click += (_, _) => _cvSave.Text = Port.BackupSave(CollectPort());
        saveRestore.Click += (_, _) => _cvSave.Text = Port.RestoreLatestSave(CollectPort());
        saveClear.Click += (_, _) => _cvSave.Text = Port.ClearSave(CollectPort());
        saveOpen.Click += (_, _) => Port.OpenFolder(Port.SaveRoot(CollectPort()));
        saveRefresh.Click += (_, _) => _cvSave.Text = Port.SaveStatus(CollectPort());

        var saves = Section("Port saves", SimpleRow("Save folder", _cvUserData), new WrapPanel { Orientation = Orientation.Horizontal, Children = { saveBackup, saveRestore, saveClear, saveOpen, saveRefresh } }, _cvSave);

        var logRefresh = new Button { Content = "Refresh log" };
        var cacheClear = new Button { Content = "Clear shader cache" };
        var logOpen = new Button { Content = "Open logs" };
        logRefresh.Click += (_, _) => { _cvLog.Text = Port.TailLog(CollectPort()); _cvStatus.Text = Port.StatusLine(CollectPort()); };
        cacheClear.Click += (_, _) => _cvStatus.Text = Port.ClearShaderCache(CollectPort());
        logOpen.Click += (_, _) => Port.OpenFolder(Port.LogDirFor(CollectPort()));
        var diag = Section("Diagnostics", _cvStatus, new WrapPanel { Orientation = Orientation.Horizontal, Children = { logRefresh, cacheClear, logOpen } }, _cvLog);

        var kbPopup = BuildBindPopup("Keyboard (mnk_mode)", Port.KeybindNames, _cvKeyButtons, true);
        var gpPopup = BuildBindPopup("Gamepad (physical controller)", Port.GamepadBindNames, _cvGamepadButtons, false);

        var kbCategoryBtn = new Button { Content = "Keyboard (mnk_mode) ▼", Width = 220 };
        kbCategoryBtn.Click += (_, _) => kbPopup.IsOpen = true;
        var gpCategoryBtn = new Button { Content = "Gamepad (physical controller) ▼", Width = 220 };
        gpCategoryBtn.Click += (_, _) => gpPopup.IsOpen = true;

        var keyReset = new Button { Content = "Clear all overrides" };
        keyReset.Click += (_, _) => { foreach (var btn in _cvKeyButtons.Values) btn.Content = "default"; foreach (var btn in _cvGamepadButtons.Values) btn.Content = "default"; };

        var keys = Section("Controller & Keyboard Bindings",
            new TextBlock { Text = "Click a category button to open its binds. Click a bind, then press the key/gamepad button. Keyboard binds (keybind_*) are for mnk_mode fallback. Gamepad binds (gamepad_*) remap physical controller buttons. Blank = runtime default.", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 0, 0, 6) },
            new WrapPanel { Orientation = Orientation.Horizontal, Children = { kbCategoryBtn, gpCategoryBtn, keyReset } }, kbPopup, gpPopup);

        var categories = new Dictionary<string, Control>
        {
            ["Setup"] = new StackPanel { Margin = new Thickness(14), Spacing = 6, Children = { 
                new TextBlock { Text = "Port settings are saved to <install>/port.conf; the dev fallback writes ~/Projects/mn9-hybrid/mn9-hybrid.conf. The PC-only tabs are kept for Steam. Use the category buttons to edit install, display, quality, audio, addons, saves, controls and diagnostics.", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 0, 0, 8) }, 
                installSection,
                diag 
            } },
            ["Display"] = new StackPanel { Margin = new Thickness(14), Spacing = 6, Children = { 
                Section("Port Display",
                    SimpleRow("Resolution", _pRes),
                    SimpleRow("Fullscreen", _pFullscreen),
                    SimpleRow("VSync", _pVsync),
                    SimpleRow("Skip intro", _pSkipIntro),
                    SimpleRow("FPS cap", _pFps),
                    SimpleRow("Quality", _pQuality),
                    SimpleRow("Start at", _cvStart),
                    SimpleRow("Boot mode", _cvBoot))
            } },
            ["Quality"] = new StackPanel { Margin = new Thickness(14), Spacing = 6, Children = { quality } },
            ["Audio"] = new StackPanel { Margin = new Thickness(14), Spacing = 6, Children = { Section("Audio (port)", SimpleRow("Master volume", _cvAudioMaster), CheckList(_cvAudioEnable, _cvAudioMute, _cvAudioNight), new TextBlock { Text = "Applies a master gain, mute and a soft peak compressor to the port's 5.1 output. Per-bus music/SFX/voice gains, pitch randomizers, dialog shuffle, ducking and music replacement use the PC OpenAL shim, so they stay on the Audio tab for Steam.", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap }) } },
            ["Gameplay"] = new StackPanel { Margin = new Thickness(14), Spacing = 6, Children = { 
                Section("Port gameplay", new TextBlock { Text = "The port uses Start at / Skip intro screens on Display. Auto-skip cutscenes and disable-dialog are retail PC shims and stay on the Steam page; use the game's own skip button during story scenes.", Foreground = Brushes.Gray, FontSize = 12, TextWrapping = TextWrapping.Wrap }),
                CheckList(_cvMnk, _cvLowMem),
                keys 
            } },
            ["Character"] = new StackPanel { Margin = new Thickness(14), Spacing = 6, Children = { content } },
            ["Char Select"] = new StackPanel { Margin = new Thickness(14), Spacing = 6, Children = { characterSelect } },
            ["Saves"] = new StackPanel { Margin = new Thickness(14), Spacing = 6, Children = { saves } },
        };

        var categoryOrder = new[] { "Setup", "Display", "Quality", "Audio", "Gameplay", "Character", "Char Select", "Saves" };
        string selectedCategory = "Setup";
        var contentHost = new ScrollViewer { Content = categories[selectedCategory], VerticalScrollBarVisibility = ScrollBarVisibility.Auto };

        var categoryButtons = new Dictionary<string, Button>();
        var buttonBar = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Center, Spacing = 4, Margin = new Thickness(0, 8, 0, 12) };

        void SelectCategory(string cat)
        {
            selectedCategory = cat;
            contentHost.Content = categories[cat];
            foreach (var kv in categoryButtons)
            {
                bool isSel = kv.Key == cat;
                kv.Value.Background = isSel ? new SolidColorBrush(Color.Parse("#5CE8FF")) : new SolidColorBrush(Color.Parse("#1A2A3A"));
                kv.Value.Foreground = isSel ? Brushes.Black : Brushes.White;
            }
        }

        foreach (string cat in categoryOrder)
        {
            var btn = new Button { Content = cat, Padding = new Thickness(16, 8, 16, 8), MinWidth = 100, Background = new SolidColorBrush(Color.Parse("#1A2A3A")), Foreground = Brushes.White, BorderBrush = new SolidColorBrush(Color.Parse("#5CE8FF")), BorderThickness = new Thickness(1) };
            btn.Click += (_, _) => SelectCategory(cat);
            categoryButtons[cat] = btn;
            buttonBar.Children.Add(btn);
        }
        SelectCategory("Setup");

        return new Border
        {
            Background = new SolidColorBrush(Color.Parse("#0B1A2A")),
            BorderBrush = new SolidColorBrush(Color.Parse("#5CE8FF")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Margin = new Thickness(14),
            Padding = new Thickness(0, 12, 0, 0),
            Child = new StackPanel { Spacing = 0, Children = { buttonBar, contentHost } },
        };
    }

    private void OnCharacterSelectClick(object sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is string character)
        {
            _selectedCharacter = character;
            _charStatus.Text = character switch
            {
                "golden" => "Golden Beck selected — Golden Beck DLC required",
                "megaxel" => "MegaXel Form selected — MegaXel Form DLC required",
                "call" => "Call selected — native pawn and controller (barrier absorbs Xel)",
                "ray" => "Ray selected — Ray Expansion DLC required",
                "retro" => "Retro Hero selected — Retro Hero DLC required",
                _ => "Normal Beck selected"
            };
        }
    }

    private void OnBindKeyClick(object sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is string key)
        {
            _bindingKey = key; _awaitingKey = true; btn.Content = "Press key..."; btn.Background = Brushes.LightBlue; this.KeyDown += OnKeyPressedForBind;
        }
    }

    private void OnBindGamepadClick(object sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is string key)
        {
            _bindingKey = key; _awaitingGamepad = true; btn.Content = "Press gamepad..."; btn.Background = Brushes.LightGreen; this.KeyDown += OnKeyPressedForBind;
        }
    }

    private static void OnBindKeyClickStatic(MainWindow window, object sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is string key) { window._bindingKey = key; window._awaitingKey = true; btn.Content = "Press key..."; btn.Background = Brushes.LightBlue; window.KeyDown += window.OnKeyPressedForBind; }
    }

    private static void OnBindGamepadClickStatic(MainWindow window, object sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is string key) { window._bindingKey = key; window._awaitingGamepad = true; btn.Content = "Press gamepad..."; btn.Background = Brushes.LightGreen; window.KeyDown += window.OnKeyPressedForBind; }
    }

    private void OnKeyPressedForBind(object sender, KeyEventArgs e)
    {
        if (!_awaitingKey && !_awaitingGamepad) return;
        string keyName = e.Key.ToString();
        if (_awaitingKey)
        {
            if (_cvKeyButtons.TryGetValue(_bindingKey, out var btn)) { btn.Content = keyName; btn.Background = Brushes.Transparent; }
        }
        else if (_awaitingGamepad)
        {
            if (_cvGamepadButtons.TryGetValue(_bindingKey, out var btn)) { btn.Content = keyName; btn.Background = Brushes.Transparent; }
        }
        _awaitingKey = false; _awaitingGamepad = false; _bindingKey = null; this.KeyDown -= OnKeyPressedForBind;
    }

    private Popup BuildBindPopup(string title, string[] bindNames, Dictionary<string, Button> buttons, bool isKeyboard)
    {
        var stack = new StackPanel { Spacing = 2 };
        stack.Children.Add(new TextBlock { Text = title, FontWeight = FontWeight.Bold, Foreground = isKeyboard ? Brushes.LightBlue : Brushes.LightGreen, Margin = new Thickness(8, 8, 8, 4) });
        foreach (string name in bindNames)
        {
            string display = name.Replace(isKeyboard ? "keybind_" : "gamepad_", "");
            var btn = new Button { Content = $"{display}: default", Tag = name, HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Stretch, Margin = new Thickness(8, 2, 8, 2), Padding = new Thickness(8, 4, 8, 4) };
            btn.Click += (_, _) => { var popup = (Popup)btn.Parent!.Parent!.Parent!; popup.IsOpen = false; if (isKeyboard) OnBindKeyClickStatic(this, btn, null); else OnBindGamepadClickStatic(this, btn, null); };
            buttons[name] = btn;
            stack.Children.Add(btn);
        }
        var border = new Border { Child = stack, Background = new SolidColorBrush(Color.Parse("#1A1A2E")), BorderBrush = new SolidColorBrush(Color.Parse("#5CE8FF")), BorderThickness = new Thickness(1), CornerRadius = new CornerRadius(4) };
        return new Popup { Child = border, PlacementMode = PlacementMode.Bottom };
    }
}