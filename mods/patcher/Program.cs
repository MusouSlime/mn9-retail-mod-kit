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
    private readonly TextBox _path = new() { Width = 470 };
    private readonly ComboBox _res = new() { Width = 140 };
    private readonly ComboBox _quality = new()
    {
        Width = 150,
        ItemsSource = Port.Qualities,
        SelectedIndex = 1,   // "high" — smooth native-resolution preset
    };
    private readonly CheckBox _skipIntro = new() { Content = "Skip intro screens (port)", IsChecked = true };
    private readonly ComboBox _character = new() { Width = 180 };

    // ReXGlue port controls
    private readonly ComboBox _cvBackend = new()
    {
        Width = 190,
        ItemsSource = new[] { "ReXGlue port (Vulkan)", "PC build (Steam)" },
        SelectedIndex = 0,
    };
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
    private readonly CheckBox _cvAsync = new() { Content = "Async shader compilation (may skip frames)" };
    // Soft texture-cache cap in MB. The runtime doubles it for the hard cap, so
    // 4096 MB is the highest input that is fully accepted (hard = 8192 MB).
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
    private readonly Dictionary<string, TextBox> _cvKeys = new();
    private static readonly int[] FpsValues = { 30, 60, 62, 72, 90, 120, 144, 240 };
    private readonly ComboBox _fps = new()
    {
        Width = 210,
        ItemsSource = new[]
        {
            "30 — low power",
            "60 — standard",
            "62 — physics-safe (default)",
            "72 — high refresh",
            "90 — high refresh",
            "120 — high refresh",
            "144 — high refresh",
            "240 — maximum",
        },
        SelectedIndex = 2,
    };

    private static readonly int[] PoolValues = { 64, 128, 256, 384, 512, 768, 1024 };
    private readonly ComboBox _pool = new()
    {
        Width = 210,
        ItemsSource = new[]
        {
            "64 MB — minimum",
            "128 MB — low",
            "256 MB — default",
            "384 MB — balanced",
            "512 MB — high",
            "768 MB — very high",
            "1024 MB — maximum",
        },
        SelectedIndex = 2,
    };

    private static readonly int[] PhysicsValues = { 1, 2, 3, 4, 5, 6, 8, 10 };
    private readonly ComboBox _difficulty = new()
    {
        Width = 210,
        ItemsSource = new[] { "Normal", "Hard", "Hyper", "Maniac" },
        SelectedIndex = 3,
    };
    private readonly ComboBox _physics = new()
    {
        Width = 210,
        ItemsSource = new[]
        {
            "1 — fastest (least stable)",
            "2 — low",
            "3 — balanced",
            "4 — smooth",
            "5 — default",
            "6 — high",
            "8 — very high",
            "10 — maximum",
        },
        SelectedIndex = 4,
    };

    private readonly CheckBox _shadows = new() { Content = "Shadows", IsChecked = true };
    private readonly CheckBox _bloom = new() { Content = "Bloom / FXAA", IsChecked = true };
    private readonly CheckBox _motion = new() { Content = "Motion blur", IsChecked = true };
    private readonly CheckBox _shafts = new() { Content = "Light shafts", IsChecked = true };
    private readonly CheckBox _distortion = new() { Content = "Distortion", IsChecked = true };
    private readonly CheckBox _flares = new() { Content = "Lens flares", IsChecked = true };
    private readonly CheckBox _ao = new() { Content = "Ambient occlusion", IsChecked = true };
    private readonly CheckBox _mlaa = new() { Content = "MLAA (post AA)" };
    private readonly CheckBox _msaa = new() { Content = "MSAA (broken on this build)" };
    private readonly CheckBox _retroTex = new() { Content = "Retro textures (point filter, 256px mips)" };
    private readonly CheckBox _fxaa = new() { Content = "FXAA (smooth edges)", IsChecked = true };

    private readonly ComboBox _auMusic = new()
    {
        Width = 240,
        ItemsSource = new[] { "Game music", "Mute music", "Random from folder", "Playlist (folder order)", "Shuffle (no repeat)" },
        SelectedIndex = 2,
    };
    private readonly TextBox _auDir = new() { Width = 420, Watermark = "/path/to/music" };
    private readonly TextBox _auMap = new() { Width = 420, Watermark = ".mn9charsel/music.map" };
    private readonly ComboBox _auSfxPitch = new()
    {
        Width = 240,
        ItemsSource = new[] { "Off", "Subtle (±5%)", "Medium (±10%)", "Chaos (±20%)" },
        SelectedIndex = 0,
    };
    private readonly ComboBox _auVoice = new()
    {
        Width = 300,
        ItemsSource = new[] { "Game voice", "Mute voice (keep subtitles)" },
        SelectedIndex = 0,
    };
    private readonly ComboBox _auMusicGain = GainBox();
    private readonly ComboBox _auSfxGain = GainBox();
    private readonly ComboBox _auVoiceGain = GainBox();
    private readonly CheckBox _auDuck = new() { Content = "Duck music under dialog", IsChecked = true };
    private readonly CheckBox _auNight = new() { Content = "Night mode (compress peaks)" };
    private readonly CheckBox _fullscreen = new() { Content = "Fullscreen" };
    private readonly CheckBox _vsync = new() { Content = "VSync", IsChecked = true };
    private readonly CheckBox _voice = new() { Content = "Steam voice" };
    private readonly CheckBox _movies = new() { Content = "Play intro movies" };
    private readonly CheckBox _mouse = new() { Content = "Free the mouse (external patch)" };
    private readonly CheckBox _laa = new() { Content = "Large Address Aware (32-bit game, up to 4 GB)" };
    private readonly CheckBox _lock = new() { Content = "Lock config (stop game-side saving)", IsChecked = true };

    private readonly TextBlock _status = new()
    {
        Text = "",
        TextWrapping = TextWrapping.Wrap,
        Margin = new Thickness(0, 10, 0, 0),
    };

    private readonly CheckBox _skipCutscenes = new() { Content = "Auto skip cutscenes" };
    private readonly CheckBox _disableDialog = new() { Content = "Disable dialog" };
    private readonly ComboBox _postShader = new()
    {
        Width = 230,
        ItemsSource = new[] { "Off", "Retro — Soft", "Retro — 8-bit", "Retro — CRT", "Noir", "Dot Matrix", "Cinematic", "Posterize", "Metallic — smooth" },
        SelectedIndex = 8,
    };

    private static readonly int[] TurboRateValues = { 6, 8, 10, 12, 15, 20 };
    private static readonly int[] TurboButtonValues = { 2, 0, 1, 3 };   // X, A, B, Y
    private static readonly int[] TurboPadValues = { -1, 8 };           // none, R3
    private readonly CheckBox _turboEnabled = new() { Content = "Enable turbo auto-fire" };
    private readonly ComboBox _turboButton = new()
    {
        Width = 160,
        ItemsSource = new[] { "X (default)", "A", "B", "Y" },
        SelectedIndex = 0,
    };
    private readonly ComboBox _turboRate = new()
    {
        Width = 120,
        ItemsSource = new[] { "6 Hz", "8 Hz", "10 Hz", "12 Hz", "15 Hz", "20 Hz" },
        SelectedIndex = 2,
    };
    private readonly ComboBox _turboPad = new()
    {
        Width = 190,
        ItemsSource = new[] { "None", "R3 (Direct Change 3)" },
        SelectedIndex = 0,
    };
    private readonly TextBox _turboKey = new() { Width = 110, Watermark = "blank = always on" };

    private readonly TextBlock _characterNote = new()
    {
        Text = "Linux: uses the LD_PRELOAD spawn override. Ray currently uses Beck's controller/moveset while his DLC controller is being fixed.",
        Foreground = Brushes.Gray,
        TextWrapping = TextWrapping.Wrap,
        FontSize = 12,
    };

    private readonly Bitmap _beckSheet = LoadAsset("models/beck_sheet.png");
    private readonly Bitmap _goldSheet = LoadAsset("models/gold_sheet.png");
    private readonly Bitmap _callSheet = LoadAsset("models/call_sheet.png");
    private readonly Bitmap _raySheet = LoadAsset("models/ray_sheet.png");
    private readonly Bitmap _retroSheet = LoadAsset("models/retro_sheet.png");
    private readonly Image _model = new()
    {
        Stretch = Stretch.Uniform,
        Width = 260,
        Height = 260,
        Margin = new Thickness(10),
    };
    private readonly TextBlock _modelName = new()
    {
        Text = "BECK",
        Foreground = new SolidColorBrush(Color.Parse("#5CE8FF")),
        FontWeight = FontWeight.Bold,
        HorizontalAlignment = HorizontalAlignment.Center,
        Margin = new Thickness(0, 6, 0, 0),
    };
    private readonly Dictionary<string, IImage[]> _modelFrames = new();
    private readonly DispatcherTimer _modelTimer;
    private int _modelFrame;

    private readonly ComboBox _saveCombo = new() { Width = 340 };
    private readonly TextBlock _saveInfo = new()
    {
        Text = "No save backups yet.",
        Foreground = Brushes.Gray,
        TextWrapping = TextWrapping.Wrap,
    };
    private List<SaveBackupInfo> _saveBackups = new();

    public MainWindow()
    {
        Title = Environment.Is64BitProcess
            ? "Xel Launcher 1.0.2 — Mighty No. 9 (64-bit)"
            : "Xel Launcher 1.0.2 — Mighty No. 9 (32-bit)";
        Width = 980;
        Height = 740;
        MinWidth = 760;
        MinHeight = 560;
        CanResize = true;
        WindowStartupLocation = WindowStartupLocation.CenterScreen;
        Background = new SolidColorBrush(Color.Parse("#081522"));

        _res.ItemsSource = new[]
        {
            "640x360",   // 16:9
            "960x540",   // 16:9
            "1024x768",  // 4:3
            "1280x720",  // 16:9
            "1280x800",  // 16:10
            "1280x960",  // 4:3
            "1440x1080", // 4:3
            "1680x1050", // 16:10
            "1920x1080", // 16:9
            "1920x1200", // 16:10
        };
        _res.SelectedIndex = 8;   // 1920x1080 — avoids shrinking the game by default

        _character.ItemsSource = Patcher.CharacterNames;
        _character.SelectedIndex = 0;

        var browse = new Button { Content = "Browse..." };
        browse.Click += async (_, _) => await Browse();

        var detect = new Button { Content = "Detect" };
        detect.Click += (_, _) =>
        {
            string dir = Patcher.FindGameDir();
            if (dir != null) _path.Text = dir;
            _status.Text = dir == null ? "Game not found." : "Game found.";
            if (dir != null) RefreshSaveBackups(dir);
        };

        var apply = new Button { Content = "Apply settings", MinWidth = 150 };
        apply.Click += (_, _) => Apply();

        var restore = new Button { Content = "Restore original" };
        restore.Click += (_, _) =>
        {
            string dir = CurrentDir();
            if (dir == null) return;
            _status.Text = Patcher.Restore(dir);
        };

        var launch = new Button { Content = "Launch port" };
        launch.Click += async (_, _) =>
        {
            SaveAudio();
            _status.Text = "Launching the ReXGlue port...";
            WindowState = WindowState.Minimized;
            string msg = await Task.Run(() => Port.Launch(CollectPort()));
            WindowState = WindowState.Normal;
            _status.Text = msg;
        };

        var launchSteam = new Button { Content = "Launch Steam version" };
        launchSteam.Click += (_, _) =>
        {
            SaveAudio();
            _status.Text = "Applying PC settings and launching Steam...";
            Apply();
            string msg = Port.LaunchPc();
            _status.Text = msg;
            if (msg.Contains("Launched", StringComparison.OrdinalIgnoreCase))
                WindowState = WindowState.Minimized;
        };

        var setChar = new Button { Content = "Set character" };
        setChar.Click += (_, _) =>
        {
            string dir = CurrentDir();
            if (dir == null) return;
            SaveOptions(dir);
            string setMessage = Patcher.SetCharacter(dir, SelectedCharacter());
            _status.Text = setMessage + " " + Patcher.InstallModFiles(dir);
        };

        var launchChar = new Button { Content = "Launch with character" };
        launchChar.Click += (_, _) =>
        {
            string dir = CurrentDir();
            if (dir == null) return;
            SaveOptions(dir);
            _status.Text = "Launching...";
            string msg = Patcher.LaunchCharacter(dir, SelectedCharacter());
            _status.Text = msg;
            if (msg.Contains("Launched", StringComparison.OrdinalIgnoreCase))
                WindowState = WindowState.Minimized;
        };

        apply.Background = new SolidColorBrush(Color.Parse("#5CE8FF"));
        apply.Foreground = new SolidColorBrush(Color.Parse("#06121F"));
        apply.FontWeight = FontWeight.Bold;
        apply.CornerRadius = new CornerRadius(6);
        foreach (var b in new[] { restore, launch, launchSteam, setChar, launchChar, browse, detect })
        {
            b.Background = new SolidColorBrush(Color.Parse("#14283D"));
            b.Foreground = Brushes.White;
            b.CornerRadius = new CornerRadius(6);
        }
        var backupSaves = new Button { Content = "Backup saves" };
        backupSaves.Click += (_, _) =>
        {
            string dir = CurrentDir();
            if (dir == null) return;
            _status.Text = Patcher.BackupSaves(dir, "manual");
            RefreshSaveBackups(dir);
        };

        var restoreSaves = new Button { Content = "Restore selected" };
        restoreSaves.Click += (_, _) =>
        {
            string dir = CurrentDir();
            if (dir == null) return;
            if (_saveCombo.SelectedIndex < 0 || _saveCombo.SelectedIndex >= _saveBackups.Count)
            {
                _status.Text = "Pick a save backup first.";
                return;
            }
            string id = _saveBackups[_saveCombo.SelectedIndex].Id;
            _status.Text = Patcher.RestoreSaves(dir, id);
            RefreshSaveBackups(dir);
        };
        foreach (var b in new[] { backupSaves, restoreSaves })
        {
            b.Background = new SolidColorBrush(Color.Parse("#14283D"));
            b.Foreground = Brushes.White;
            b.CornerRadius = new CornerRadius(6);
        }

        var pathRow = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 8,
            Children = { new TextBlock { Text = "Game:", VerticalAlignment = VerticalAlignment.Center }, _path, browse, detect },
        };

        Tip(_path, "Path to the Mighty No. 9 install folder.");
        Tip(browse, "Pick the game folder manually.");
        Tip(detect, "Find the game folder from Steam.");

        Tip(_res, "Render resolution written to the port and the PC INIs.");
        Tip(_quality, "Port image quality: max = 2x supersampling (heavy), high = native 8x anisotropy + MSAA (smooth), balanced/performance = lighter.");
        Tip(_skipIntro, "Focus the port window and send A to skip the title screen, first-run dialog and opening story slides.");
        Tip(_cvTexCache, "Soft texture-cache cap. The runtime doubles it for the hard cap: 64 MB minimum, 4096 MB maximum (hard 8192 MB). Values outside that range are rejected; Auto keeps the low-memory 128/256 MB defaults.");
        Tip(_fps, "Frame cap written to MaxSmoothedFrameRate. 62 is the physics-safe default; higher values can change movement and pacing.");
        Tip(_pool, "Texture streaming pool in MB (PoolSize). Higher keeps more textures resident; lower saves VRAM.");
        Tip(_physics, "Physics substeps per frame (MaxPhysicsSubsteps). 5 is the game default; higher is more stable but slower.");
        Tip(_difficulty, "PC difficulty written to GameDifficulty: 0 Normal, 1 Hard, 2 Hyper, 3 Maniac (Expert).");
        Tip(_shadows, "Dynamic character and world shadows.");
        Tip(_bloom, "Bloom glow and the game's FXAA anti-aliasing.");
        Tip(_motion, "Motion blur and pause-menu blur.");
        Tip(_shafts, "Volumetric light shafts from bright lights.");
        Tip(_distortion, "Heat-haze and screen distortion effects.");
        Tip(_flares, "Camera lens-flare sprites.");
        Tip(_ao, "Screen-space ambient occlusion (SSAO).");
        Tip(_mlaa, "Post-process morphological anti-aliasing.");
        Tip(_msaa, "Hardware MSAA. Broken on this retail build.");
        Tip(_fullscreen, "Run fullscreen instead of a window.");
        Tip(_vsync, "Synchronize to the display refresh rate.");
        Tip(_voice, "Enable Steam voice-chat support.");
        Tip(_movies, "Play the startup/intro movies.");
        Tip(_mouse, "External patch that releases the mouse cursor from the game window.");
        Tip(_laa, "Allow the 32-bit executable to use up to 4 GB of address space.");
        Tip(_lock, "Make the config read-only after applying so the game cannot overwrite it.");
        Tip(_skipCutscenes, "Automatically trigger the game's skip action for Kismet story cutscenes.");
        Tip(_disableDialog, "Hide dialog popups and mute dialog voice.");
        Tip(_postShader, "Post-process style: Metallic (smooth, default), retro (pixelation/quantisation), Noir, Dot Matrix, Cinematic or Posterize.");
        Tip(_retroTex, "Texture-level retro look: nearest-neighbour filtering and 256px max mips (also lowers VRAM).");
        Tip(_fxaa, "Post-process FXAA. Smooths model and texture edges; works with the selected post style. Costs 8 texture taps.");
        Tip(_turboEnabled, "Auto-fire for the Linux PC build: holding the fire button keeps shooting.");
        Tip(_turboButton, "Controller button used for fire. X is the game's default attack button.");
        Tip(_turboRate, "Auto-fire taps per second. 10 Hz is a good default; too high can drop inputs.");
        Tip(_turboPad, "Use R3 as the auto-fire toggle. R3 stops performing Direct Change 3 while selected.");
        Tip(_turboKey, "Optional keyboard key that turns auto-fire on/off while playing. Leave blank for always on.");
        Tip(_character, "Choose the character used by the spawn override.");
        Tip(setChar, "Save the selection without launching.");
        Tip(launchChar, "Save the selection, install the Steam shim, and launch through Steam.");
        Tip(apply, "Write the selected settings to the ReXGlue port config and the PC game config (with a backup).");
        Tip(restore, "Restore the pre-patch PC config and the original game binary.");
        Tip(launch, "Write the port settings and launch the ReXGlue Xbox 360 port (Vulkan).");
        Tip(launchSteam, "Apply the PC build settings and launch the retail Steam version (app 314710).");
        Tip(_saveCombo, "Select a save backup to restore.");
        Tip(backupSaves, "Create a timestamped backup of the current save files.");
        Tip(restoreSaves, "Replace the current saves with the selected backup.");

        var physicsRow = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10,
            Children =
            {
                new TextBlock
                {
                    Text = "Physics substeps (PC)",
                    Width = 170,
                    VerticalAlignment = VerticalAlignment.Center,
                },
                _physics,
            },
        };

        var portDisplay = Section("Display & launch",
            Row("Resolution", _res, "Quality preset", _quality),
            CheckList(_fullscreen, _vsync, _skipIntro),
            Row("Start at", _cvStart, "License mask", _cvLicense));

        var pcDisplay = Section("PC build display (Steam only)",
            Row("Frame cap (PC)", _fps, "Texture pool (PC)", _pool),
            physicsRow,
            SimpleRow("Difficulty (PC)", _difficulty));

        var pcNote = new TextBlock
        {
            Text = "These options write the retail PC build's UE3 INIs. They do not " +
                   "affect the ReXGlue port, which renders the Xbox 360 build with the " +
                   "quality preset on the Port tab.",
            Foreground = Brushes.Gray,
            TextWrapping = TextWrapping.Wrap,
            FontSize = 12,
            Margin = new Thickness(0, 0, 0, 8),
        };

        var postRow = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10,
            Margin = new Thickness(0, 4, 0, 0),
            Children =
            {
                new TextBlock { Text = "Post-process:", VerticalAlignment = VerticalAlignment.Center },
                _postShader,
            },
        };

        var turbo = Section("Turbo auto-fire (Linux PC build)",
            CheckList(_turboEnabled),
            Row("Fire button", _turboButton, "Rate", _turboRate),
            SimpleRow("Pad toggle", _turboPad),
            SimpleRow("Toggle key", _turboKey),
            new TextBlock
            {
                Text = "Holding the fire button taps it at the selected rate. The left mouse button is handled automatically. " +
                       "Pad toggle R3 stops doing Direct Change 3 and turns auto-fire on/off instead. " +
                       "The optional toggle key does the same; blank means always on. Linux PC build only.",
                Foreground = Brushes.Gray,
                FontSize = 12,
                TextWrapping = TextWrapping.Wrap,
            });

        var effects = Section("Effects",
            CheckList(_shadows, _bloom, _motion, _shafts, _distortion, _flares, _ao, _mlaa, _msaa),
            postRow,
            CheckList(_retroTex, _fxaa));

        var graphics = new StackPanel
        {
            Margin = new Thickness(14),
            Children = { pcNote, pcDisplay, turbo, effects },
        };

        var gameplay = new StackPanel
        {
            Margin = new Thickness(14),
            Children =
            {
                Section("Cutscenes & dialog",
                    _skipCutscenes,
                    _disableDialog,
                    new TextBlock
                    {
                        Text = "These options are written to .mn9charsel/options and applied when you launch through the Character tab.",
                        Foreground = Brushes.Gray,
                        TextWrapping = TextWrapping.Wrap,
                        FontSize = 12,
                        Margin = new Thickness(0, 6, 0, 0),
                    }),
                Section("Runtime notes",
                    new TextBlock
                    {
                        Text = "The spawn override only runs when the game is launched through the Character tab. " +
                               "Steam Input and controllers stay active because the game is started by Steam.",
                        Foreground = Brushes.Gray,
                        TextWrapping = TextWrapping.Wrap,
                        FontSize = 12,
                    }),
            },
        };

        var modelBorder = new Border
        {
            Width = 260,
            Height = 260,
            CornerRadius = new CornerRadius(12),
            BorderBrush = new SolidColorBrush(Color.Parse("#5CE8FF")),
            BorderThickness = new Thickness(2),
            ClipToBounds = true,
            Background = new SolidColorBrush(Color.Parse("#0A1626")),
            Child = _model,
        };

        var portraitPanel = new StackPanel
        {
            Spacing = 4,
            Children = { modelBorder, _modelName },
        };

        var characterControls = new StackPanel
        {
            Spacing = 10,
            Children =
            {
                new StackPanel
                {
                    Orientation = Orientation.Horizontal,
                    Spacing = 10,
                    Children =
                    {
                        new TextBlock { Text = "Character:", VerticalAlignment = VerticalAlignment.Center },
                        _character,
                        setChar,
                        launchChar,
                    },
                },
                _characterNote,
            },
        };

        var characterLayout = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
            ColumnSpacing = 18,
            Children = { portraitPanel, characterControls },
        };
        Grid.SetColumn(portraitPanel, 0);
        Grid.SetColumn(characterControls, 1);

        var character = new StackPanel
        {
            Margin = new Thickness(14),
            Children =
            {
                Section("Character", characterLayout),
            },
        };

        var saves = new StackPanel
        {
            Margin = new Thickness(14),
            Children =
            {
                Section("Save backups",
                    new StackPanel
                    {
                        Orientation = Orientation.Horizontal,
                        Spacing = 10,
                        Children = { _saveCombo, backupSaves, restoreSaves },
                    },
                    _saveInfo),
            },
        };

        var advanced = new StackPanel
        {
            Margin = new Thickness(14),
            Children =
            {
                Section("Advanced",
                    CheckList(_voice, _mouse, _laa, _lock, _movies)),
            },
        };

        var portTab = BuildPortTab(portDisplay);
        var tabs = new TabControl
        {
            Margin = new Thickness(18, 10, 18, 0),
            Items =
            {
                new TabItem { Header = "Port", Content = portTab },
                new TabItem { Header = "PC build", Content = new ScrollViewer { Content = graphics } },
                new TabItem { Header = "Audio", Content = BuildAudioTab() },
                new TabItem { Header = "Gameplay", Content = new ScrollViewer { Content = gameplay } },
                new TabItem { Header = "Character", Content = new ScrollViewer { Content = character } },
                new TabItem { Header = "Saves", Content = new ScrollViewer { Content = saves } },
                new TabItem { Header = "Advanced", Content = new ScrollViewer { Content = advanced } },
            },
        };

        var buttons = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10,
            Children = { apply, restore, launch, launchSteam },
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
                new TextBlock { Text = "Xel Launcher", FontSize = 20, FontWeight = FontWeight.Bold },
                new TextBlock
                {
                    Text = "Mighty No. 9 mod launcher. Tabs group the options; hover any option for a description.",
                    Foreground = new SolidColorBrush(Color.Parse("#B8D8E8")),
                    TextWrapping = TextWrapping.Wrap,
                },
                new TextBlock
                {
                    Text = $"{(Environment.Is64BitProcess ? "64-bit" : "32-bit")} · " +
                           $"{RuntimeInformation.ProcessArchitecture} · " +
                           $"{RuntimeInformation.OSDescription}",
                    Foreground = new SolidColorBrush(Color.Parse("#7FA8BB")),
                    FontSize = 12,
                },
            },
        };

        var header = new Grid
        {
            Margin = new Thickness(18, 12, 18, 0),
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
            ColumnSpacing = 16,
            Children = { logo, headerText },
        };
        Grid.SetColumn(logo, 0);
        Grid.SetColumn(headerText, 1);

        pathRow.Margin = new Thickness(18, 10, 18, 0);
        var headerPanel = new StackPanel { Children = { header, pathRow } };

        var root = new DockPanel();
        DockPanel.SetDock(headerPanel, Dock.Top);
        DockPanel.SetDock(bottom, Dock.Bottom);
        root.Children.Add(headerPanel);
        root.Children.Add(bottom);
        root.Children.Add(tabs);
        LoadPortSettings();

        var bgImage = new Image
        {
            Source = LoadAsset("background.png"),
            Stretch = Stretch.UniformToFill,
            Opacity = 0.65,
        };
        var bgOverlay = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#D9081522")),
        };
        var bgMark = new Image
        {
            Source = LoadAsset("mark.png"),
            Width = 340,
            Opacity = 0.07,
            Stretch = Stretch.Uniform,
            HorizontalAlignment = HorizontalAlignment.Right,
            VerticalAlignment = VerticalAlignment.Top,
            Margin = new Thickness(0, 40, 30, 0),
            IsHitTestVisible = false,
        };
        var bgGrid = new Grid { Children = { bgImage, bgOverlay, bgMark } };
        var shell = new Grid { Children = { bgGrid, root } };
        Content = shell;
        if (int.TryParse(Environment.GetEnvironmentVariable("MN9_TAB"), out int tabIndex))
            tabs.SelectedIndex = Math.Clamp(tabIndex, 0, 4);
        KeyDown += (_, e) =>
        {
            if (e.Key == Key.Enter && e.KeyModifiers.HasFlag(KeyModifiers.Control))
                Apply();
        };
        AddModelFrames("beck", _beckSheet);
        AddModelFrames("gold", _goldSheet);
        AddModelFrames("call", _callSheet);
        AddModelFrames("ray", _raySheet);
        AddModelFrames("retro", _retroSheet);
        _modelTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(110) };
        _modelTimer.Tick += (_, _) => AdvanceModel();
        _modelTimer.Start();

        _path.Text = Environment.GetEnvironmentVariable("MN9_GAME_DIR") ?? Patcher.FindGameDir() ?? "";
        _path.TextChanged += (_, _) =>
        {
            RefreshSaveBackups(_path.Text?.Trim());
            SyncCharacter(_path.Text?.Trim());
            SyncPcTuning(_path.Text?.Trim());
            SyncTurbo(_path.Text?.Trim());
            LoadAudioSettings();
        };
        _character.SelectionChanged += (_, _) => UpdateModel();
        _postShader.SelectionChanged += (_, _) =>
        {
            string dir = _path.Text?.Trim();
            if (string.IsNullOrEmpty(dir) ||
                !Directory.Exists(Path.Combine(dir, "MN9Game", "Config"))) return;
            SaveOptions(dir);
            _status.Text = Patcher.InstallModFiles(dir);
        };
        _fxaa.IsCheckedChanged += (_, _) =>
        {
            string dir = _path.Text?.Trim();
            if (string.IsNullOrEmpty(dir) ||
                !Directory.Exists(Path.Combine(dir, "MN9Game", "Config"))) return;
            SaveOptions(dir);
            _status.Text = Patcher.InstallModFiles(dir);
        };
        RefreshSaveBackups(_path.Text?.Trim());
        SyncCharacter(_path.Text?.Trim());
        SyncPcTuning(_path.Text?.Trim());
        SyncTurbo(_path.Text?.Trim());
        LoadAudioSettings();
    }

    private static ComboBox GainBox() => new()
    {
        Width = 110,
        ItemsSource = new[] { "100%", "75%", "50%", "25%", "0%" },
        SelectedIndex = 0,
    };

    private static int GainIndex(string value) => value switch
    {
        "0.75" => 1,
        "0.5" => 2,
        "0.25" => 3,
        "0" => 4,
        _ => 0,
    };

    private static string GainValue(int index) => index switch
    {
        1 => "0.75",
        2 => "0.5",
        3 => "0.25",
        4 => "0",
        _ => "1",
    };

    private static int NearestIndex(int[] values, int value)    {
        int best = 0;
        for (int i = 1; i < values.Length; i++)
            if (Math.Abs(values[i] - value) < Math.Abs(values[best] - value)) best = i;
        return best;
    }

    private void SyncPcTuning(string dir)
    {
        var (fps, pool, physics) = Patcher.ReadPcTuning(dir ?? "");
        _fps.SelectedIndex = NearestIndex(FpsValues, fps);
        _pool.SelectedIndex = NearestIndex(PoolValues, pool);
        _physics.SelectedIndex = NearestIndex(PhysicsValues, physics);
    }

    private void SyncTurbo(string dir)
    {
        var (enabled, rate, button, key, pad) = Patcher.ReadTurbo(dir ?? "");
        _turboEnabled.IsChecked = enabled;
        _turboRate.SelectedIndex = NearestIndex(TurboRateValues, (int)Math.Round(rate));
        int bi = Array.IndexOf(TurboButtonValues, button);
        _turboButton.SelectedIndex = bi >= 0 ? bi : 0;
        int pi = Array.IndexOf(TurboPadValues, pad);
        _turboPad.SelectedIndex = pi >= 0 ? pi : 0;
        _turboKey.Text = key;
    }

    private void SyncCharacter(string dir)
    {
        int index = Array.IndexOf(Patcher.CharacterIds, Patcher.ReadCharacter(dir ?? ""));
        _character.SelectedIndex = index >= 0 ? index : 0;
        var (skip, dialog, cel, strength, fxaa) = Patcher.ReadOptions(dir ?? "");
        _skipCutscenes.IsChecked = skip;
        _disableDialog.IsChecked = dialog;
        _fxaa.IsChecked = fxaa;
        _postShader.SelectedIndex = !cel ? 0 : strength switch
        {
            "subtle" => 1,
            "bold" => 2,
            "comic" => 3,
            "noir" => 4,
            "dotmatrix" => 5,
            "cinematic" => 6,
            "posterize" => 7,
            "metallic" => 8,
            _ => 8,
        };
        UpdateModel();
    }

    private void AddModelFrames(string id, Bitmap sheet)
    {
        if (sheet == null) return;
        var frames = new IImage[16];
        for (int i = 0; i < frames.Length; ++i)
        {
            frames[i] = new CroppedBitmap(sheet,
                new PixelRect((i % 4) * 256, (i / 4) * 256, 256, 256));
        }
        _modelFrames[id] = frames;
    }

    private void AdvanceModel()
    {
        string id = SelectedCharacter();
        if (!_modelFrames.TryGetValue(id, out var frames) || frames.Length == 0) return;
        _modelFrame = (_modelFrame + 1) % frames.Length;
        _model.Source = frames[_modelFrame];
    }

    private void UpdateModel()
    {
        string id = SelectedCharacter();
        _modelFrame = 0;
        _model.Source = _modelFrames.TryGetValue(id, out var frames) && frames.Length > 0
            ? frames[0]
            : null;
        _modelName.Text = id switch
        {
            "gold" => "GOLDEN BECK",
            "retro" => "RETRO HERO",
            "call" => "CALL",
            "ray" => "RAY",
            _ => "BECK",
        };
    }

    private void SaveOptions(string dir)
    {
        Patcher.WriteOptions(dir,
            _skipCutscenes.IsChecked == true,
            _disableDialog.IsChecked == true,
            _postShader.SelectedIndex != 0,
            _postShader.SelectedIndex switch { 1 => "subtle", 2 => "bold", 3 => "comic", 4 => "noir", 5 => "dotmatrix", 6 => "cinematic", 7 => "posterize", 8 => "metallic", _ => "metallic" },
            _fxaa.IsChecked == true);
    }

    private string SelectedCharacter()
    {
        int index = _character.SelectedIndex;
        return index >= 0 && index < Patcher.CharacterIds.Length
            ? Patcher.CharacterIds[index]
            : "beck";
    }

    private static StackPanel Row(string a, Control av, string b, Control bv) => new()
    {
        Orientation = Orientation.Horizontal,
        Spacing = 10,
        Children =
        {
            new TextBlock { Text = a, Width = 110, VerticalAlignment = VerticalAlignment.Center }, av,
            new TextBlock { Text = b, Width = 110, VerticalAlignment = VerticalAlignment.Center }, bv,
        },
    };

    private void RefreshSaveBackups(string dir)
    {
        _saveBackups = new List<SaveBackupInfo>();
        if (!string.IsNullOrWhiteSpace(dir) &&
            Directory.Exists(Path.Combine(dir, "MN9Game", "SaveData")))
        {
            _saveBackups = Patcher.ListSaveBackups(dir).ToList();
        }

        _saveCombo.ItemsSource = _saveBackups
            .Select(b => $"{b.Time.LocalDateTime:yyyy-MM-dd HH:mm}  {b.Files} files  {FormatSize(b.Bytes)}")
            .ToList();
        _saveCombo.SelectedIndex = _saveBackups.Count > 0 ? 0 : -1;
        _saveInfo.Text = _saveBackups.Count == 0
            ? "No save backups yet."
            : $"{_saveBackups.Count} backup(s) in .mn9patcher/saves (latest first).";
    }

    private static string FormatSize(long bytes) => bytes switch
    {
        < 1024 => $"{bytes} B",
        < 1024 * 1024 => $"{bytes / 1024.0:0.0} KB",
        _ => $"{bytes / (1024.0 * 1024.0):0.0} MB",
    };

    private string CurrentDir()
    {
        string dir = _path.Text?.Trim();
        if (string.IsNullOrEmpty(dir) || !Directory.Exists(Path.Combine(dir, "MN9Game", "Config")))
        {
            _status.Text = "Choose the Mighty No. 9 folder first.";
            return null;
        }
        return dir;
    }

    private async Task Browse()
    {
        string? picked = await PickPath(_path.Text ?? "", false);
        if (picked != null)
        {
            _path.Text = picked;
            RefreshSaveBackups(_path.Text);
        }
    }

    private void Apply()
    {
        string dir = CurrentDir();
        if (dir == null) return;

        var s = new Settings
        {
            Shadows = _shadows.IsChecked == true,
            Bloom = _bloom.IsChecked == true,
            MotionBlur = _motion.IsChecked == true,
            LightShafts = _shafts.IsChecked == true,
            Distortion = _distortion.IsChecked == true,
            LensFlares = _flares.IsChecked == true,
            AmbientOcclusion = _ao.IsChecked == true,
            Mlaa = _mlaa.IsChecked == true,
            Msaa = _msaa.IsChecked == true,
            Fullscreen = _fullscreen.IsChecked == true,
            Vsync = _vsync.IsChecked == true,
            Voice = _voice.IsChecked == true,
            Movies = _movies.IsChecked == true,
            MouseFix = _mouse.IsChecked == true,
            Laa = _laa.IsChecked == true,
            LockConfig = _lock.IsChecked == true,
            RetroTextures = _retroTex.IsChecked == true,
            Difficulty = Math.Clamp(_difficulty.SelectedIndex, 0, 3),
            Fps = FpsValues[Math.Clamp(_fps.SelectedIndex, 0, FpsValues.Length - 1)],
            Pool = PoolValues[Math.Clamp(_pool.SelectedIndex, 0, PoolValues.Length - 1)],
            Physics = PhysicsValues[Math.Clamp(_physics.SelectedIndex, 0, PhysicsValues.Length - 1)],
        };

        string res = _res.SelectedItem as string ?? "1280x720";
        string[] wh = res.Split('x');
        s.Width = int.Parse(wh[0]);
        s.Height = int.Parse(wh[1]);

        _status.Text = "Applying...";
        string applied = Patcher.Apply(dir, s);
        string portApplied = Port.Apply(CollectPort());
        string character = Patcher.SetCharacter(dir, SelectedCharacter());
        SaveOptions(dir);
        int turboRate = TurboRateValues[Math.Clamp(_turboRate.SelectedIndex, 0, TurboRateValues.Length - 1)];
        int turboButton = TurboButtonValues[Math.Clamp(_turboButton.SelectedIndex, 0, TurboButtonValues.Length - 1)];
        int turboPad = TurboPadValues[Math.Clamp(_turboPad.SelectedIndex, 0, TurboPadValues.Length - 1)];
        string turboSaved = Patcher.WriteTurbo(dir, _turboEnabled.IsChecked == true,
            turboRate, turboButton, _turboKey.Text ?? "", turboPad);
        string install = Patcher.InstallModFiles(dir);
        _status.Text = applied + Environment.NewLine + portApplied + Environment.NewLine +
                       character + Environment.NewLine + turboSaved + Environment.NewLine + install;
    }

    private Control BuildAudioTab()
    {
        var browseMusic = new Button { Content = "Browse" };
        browseMusic.Click += async (_, _) => await BrowseMusic();
        var browseMap = new Button { Content = "Browse" };
        browseMap.Click += async (_, _) => await BrowseMap();
        var mapTemplate = new Button { Content = "Create template" };
        mapTemplate.Click += (_, _) => CreateMapTemplate();
        var saveAudio = new Button { Content = "Save audio settings", MinWidth = 160 };
        saveAudio.Click += (_, _) => SaveAudio();

        var dirRow = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10,
            Children = { _auDir, browseMusic },
        };
        var mapRow = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10,
            Children = { _auMap, browseMap, mapTemplate },
        };
        var musicNote = new TextBlock
        {
            Text = "A track is assigned per stage/BGM and kept for that stage: it loops until the stage changes, " +
                   "then Random picks a new one, Playlist walks the folder in name order, and Shuffle uses a no-repeat bag. " +
                   "OGG, WAV, FLAC and MP3 are supported. The game's own BGM is muted while a replacement plays.",
            Foreground = Brushes.Gray,
            FontSize = 12,
            TextWrapping = TextWrapping.Wrap,
        };
        var mapNote = new TextBlock
        {
            Text = "Stage map: one line per map, e.g. Map04_MiB=/music/map04.ogg or Map05_WWB=/music/water/ " +
                   "(a folder picks one track for that stage). Use default= for the fallback. Patterns match the loaded map name. " +
                   "The assigned track loops and is only replaced when the map changes.",
            Foreground = Brushes.Gray,
            FontSize = 12,
            TextWrapping = TextWrapping.Wrap,
        };
        var voiceNote = new TextBlock
        {
            Text = "Mute voice uses the OpenAL buffer length to leave subtitles intact. " +
                   "The PC build's Disable dialog checkbox mutes voice and hides text; use one or the other.",
            Foreground = Brushes.Gray,
            FontSize = 12,
            TextWrapping = TextWrapping.Wrap,
        };

        var root = new StackPanel
        {
            Margin = new Thickness(14),
            Spacing = 8,
            Children =
            {
                Section("Music",
                    SimpleRow("Mode", _auMusic),
                    SimpleRow("Folder", dirRow),
                    SimpleRow("Stage map", mapRow),
                    musicNote,
                    mapNote),
                Section("SFX",
                    SimpleRow("Pitch randomizer", _auSfxPitch),
                    CheckList(_auNight)),
                Section("Dialog",
                    SimpleRow("Voice", _auVoice),
                    CheckList(_auDuck),
                    voiceNote),
                Section("Levels",
                    Row("Music", _auMusicGain, "SFX", _auSfxGain),
                    SimpleRow("Voice", _auVoiceGain)),
                saveAudio,
            },
        };
        return new ScrollViewer { Content = root };
    }

    private async Task BrowseMusic()
    {
        string? picked = await PickPath(_auDir.Text ?? AudioSettings.DefaultMusicDir, false);
        if (picked != null) _auDir.Text = picked;
    }

    private async Task BrowseMap()
    {
        string? picked = await PickPath(_auMap.Text ?? AudioSettings.DefaultMusicDir, true);
        if (picked != null) _auMap.Text = picked;
    }

    // In-app path picker. The native XDG/Dolphin dialog can freeze on KDE, so
    // this dialog only uses Directory.GetDirectories/GetFiles.
    private async Task<string?> PickPath(string start, bool files)
    {
        string current = Directory.Exists(start) ? start
            : File.Exists(start) ? (Path.GetDirectoryName(start) ?? "/")
            : (Environment.GetFolderPath(Environment.SpecialFolder.UserProfile) is string h && h.Length > 0 ? h : "/");

        var win = new Window
        {
            Title = files ? "Select a map file" : "Select a music folder",
            Width = 780,
            Height = 560,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            CanResize = true,
        };
        var pathBox = new TextBox { Width = 660, Text = current };
        var list = new ListBox { Height = 390 };
        var status = new TextBlock
        {
            Foreground = Brushes.Gray,
            FontSize = 12,
            TextWrapping = TextWrapping.Wrap,
        };

        void Refresh()
        {
            var items = new List<string>();
            try
            {
                items.AddRange(Directory.GetDirectories(current)
                    .OrderBy(x => x, StringComparer.OrdinalIgnoreCase));
                if (files)
                    items.AddRange(Directory.GetFiles(current)
                        .Where(f => f.EndsWith(".map", StringComparison.OrdinalIgnoreCase) ||
                                    f.EndsWith(".txt", StringComparison.OrdinalIgnoreCase))
                        .OrderBy(x => x, StringComparer.OrdinalIgnoreCase));
                status.Text = items.Count + " item(s)";
            }
            catch (Exception ex)
            {
                status.Text = ex.Message;
            }
            list.ItemsSource = items;
            pathBox.Text = current;
        }

        var go = new Button { Content = "Go" };
        go.Click += (_, _) =>
        {
            if (Directory.Exists(pathBox.Text)) { current = pathBox.Text!; Refresh(); }
        };
        var up = new Button { Content = "Up" };
        up.Click += (_, _) =>
        {
            string? p = Path.GetDirectoryName(current);
            if (!string.IsNullOrEmpty(p)) { current = p; Refresh(); }
        };
        var home = new Button { Content = "Home" };
        home.Click += (_, _) =>
        {
            current = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            Refresh();
        };
        var music = new Button { Content = "Music" };
        music.Click += (_, _) =>
        {
            current = AudioSettings.DefaultMusicDir;
            Refresh();
        };
        var use = new Button { Content = files ? "Use selected file" : "Use this folder" };
        use.Click += (_, _) =>
        {
            if (files)
            {
                if (list.SelectedItem is string sel && File.Exists(sel)) { win.Close(sel); return; }
                if (File.Exists(pathBox.Text)) { win.Close(pathBox.Text); return; }
                status.Text = "Select a .map or .txt file.";
                return;
            }
            string p = pathBox.Text ?? current;
            if (Directory.Exists(p)) win.Close(p);
            else status.Text = "Folder does not exist.";
        };
        var cancel = new Button { Content = "Cancel" };
        cancel.Click += (_, _) => win.Close(null);

        list.SelectionChanged += (_, _) =>
        {
            if (list.SelectedItem is string s) pathBox.Text = s;
        };
        list.DoubleTapped += (_, _) =>
        {
            if (list.SelectedItem is not string s) return;
            if (Directory.Exists(s)) { current = s; Refresh(); }
            else if (files && File.Exists(s)) win.Close(s);
        };

        var top = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 8,
            Children = { pathBox, go },
        };
        var nav = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 8,
            Children = { up, home, music, use, cancel },
        };
        var root = new DockPanel { Margin = new Thickness(14) };
        DockPanel.SetDock(top, Dock.Top);
        root.Children.Add(top);
        DockPanel.SetDock(nav, Dock.Bottom);
        root.Children.Add(nav);
        DockPanel.SetDock(status, Dock.Bottom);
        root.Children.Add(status);
        root.Children.Add(list);
        win.Content = root;
        Refresh();
        return await win.ShowDialog<string?>(this);
    }

    private void CreateMapTemplate()
    {
        string dir = CurrentDir();
        if (dir == null) return;
        string path = _auMap.Text?.Trim() ?? "";
        if (string.IsNullOrEmpty(path)) path = Path.Combine(dir, ".mn9charsel", "music.map");
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        File.WriteAllText(path,
            "# Mighty No. 9 stage music map\n" +
            "# pattern = file or folder. Patterns match the loaded map name.\n" +
            "# Map04_MiB=/home/user/Music/map04.ogg\n" +
            "# Map05_WWB=/home/user/Music/water/\n" +
            "# default=/home/user/Music/fallback.ogg\n\n" +
            "Map10_Tutorial=\n" +
            "OpDemo=\n" +
            "Map04_MiB=\n" +
            "Map05_WWB=\n" +
            "Map06_EGP=\n" +
            "Map07_Mine=\n" +
            "Map09_RAT=\n" +
            "Map11_OIL=\n" +
            "Map12_WH=\n" +
            "Map13_HighWay=\n" +
            "Map14_Factory=\n" +
            "Map15_COLO=\n" +
            "default=\n");
        _auMap.Text = path;
        _status.Text = "Music map template written to " + path;
    }

    private void LoadAudioSettings()
    {
        if (_auMusic == null) return;
        var a = AudioSettings.Read(_path.Text?.Trim() ?? "");
        _auMusic.SelectedIndex = a.Music switch { "mute" => 1, "random" => 2, "playlist" => 3, "shuffle" => 4, _ => 0 };
        _auDir.Text = string.IsNullOrEmpty(a.MusicDir) ? AudioSettings.DefaultMusicDir : a.MusicDir;
        _auMap.Text = a.MusicMap;
        _auDuck.IsChecked = a.Duck;
        _auNight.IsChecked = a.Night;
        _auSfxPitch.SelectedIndex = a.SfxPitch switch { "0.05" => 1, "0.10" => 2, "0.20" => 3, _ => 0 };
        _auVoice.SelectedIndex = a.Voice == "mute" ? 1 : 0;
        _auMusicGain.SelectedIndex = GainIndex(a.MusicGain);
        _auSfxGain.SelectedIndex = GainIndex(a.SfxGain);
        _auVoiceGain.SelectedIndex = GainIndex(a.VoiceGain);
    }

    private void SaveAudio()
    {
        string dir = CurrentDir();
        if (dir == null) return;
        var a = new AudioSettings
        {
            Music = _auMusic.SelectedIndex switch { 1 => "mute", 2 => "random", 3 => "playlist", 4 => "shuffle", _ => "game" },
            MusicDir = string.IsNullOrWhiteSpace(_auDir.Text) ? AudioSettings.DefaultMusicDir : _auDir.Text!.Trim(),
            MusicMap = _auMap.Text?.Trim() ?? "",
            SfxPitch = _auSfxPitch.SelectedIndex switch { 1 => "0.05", 2 => "0.10", 3 => "0.20", _ => "0" },
            Voice = _auVoice.SelectedIndex == 1 ? "mute" : "game",
            MusicGain = GainValue(_auMusicGain.SelectedIndex),
            SfxGain = GainValue(_auSfxGain.SelectedIndex),
            VoiceGain = GainValue(_auVoiceGain.SelectedIndex),
            Duck = _auDuck.IsChecked == true,
            Night = _auNight.IsChecked == true,
        };
        _status.Text = AudioSettings.Write(dir, a) + " " + Patcher.InstallModFiles(dir);
    }

    private Control BuildPortTab(Control displaySection)
    {
        var backendRow = SimpleRow("Backend", _cvBackend);

        var quality = Section("Fine image quality",
            Row("Draw scale", _cvScale, "Anisotropy", _cvAniso),
            Row("MSAA", _cvMsaa, "Dither", _cvDither),
            SimpleRow("Texture cache (soft cap)", _cvTexCache),
            _cvAsync);

        var contentRefresh = new Button { Content = "Refresh" };
        var contentOpen = new Button { Content = "Open folder" };
        contentRefresh.Click += (_, _) => _cvContent.Text = Port.ContentStatus();
        contentOpen.Click += (_, _) => Port.OpenFolder(Port.ContentDir);
        var content = Section("Content & unlock",
            new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Spacing = 10,
                Children = { contentRefresh, contentOpen },
            },
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
        var saves = Section("Port saves",
            SimpleRow("Save folder", _cvUserData),
            new WrapPanel
            {
                Orientation = Orientation.Horizontal,
                Children = { saveBackup, saveRestore, saveClear, saveOpen, saveRefresh },
            },
            _cvSave);

        var logRefresh = new Button { Content = "Refresh log" };
        var cacheClear = new Button { Content = "Clear shader cache" };
        var logOpen = new Button { Content = "Open logs" };
        logRefresh.Click += (_, _) =>
        {
            _cvLog.Text = Port.TailLog();
            _cvStatus.Text = Port.StatusLine(CollectPort());
        };
        cacheClear.Click += (_, _) => _cvStatus.Text = Port.ClearShaderCache(CollectPort());
        logOpen.Click += (_, _) => Port.OpenFolder(Port.LogDir);
        var diag = Section("Diagnostics",
            _cvStatus,
            new WrapPanel
            {
                Orientation = Orientation.Horizontal,
                Children = { logRefresh, cacheClear, logOpen },
            },
            _cvLog);

        var keyPanel = new WrapPanel { Orientation = Orientation.Horizontal };
        foreach (string name in Port.KeybindNames)
        {
            var tb = new TextBox { Width = 140, Watermark = "default" };
            _cvKeys[name] = tb;
            keyPanel.Children.Add(new StackPanel
            {
                Margin = new Thickness(0, 0, 12, 8),
                Children =
                {
                    new TextBlock
                    {
                        Text = name.Replace("keybind_", ""),
                        FontSize = 11,
                        Foreground = Brushes.Gray,
                    },
                    tb,
                },
            });
        }
        var keyReset = new Button { Content = "Clear overrides" };
        keyReset.Click += (_, _) =>
        {
            foreach (var tb in _cvKeys.Values) tb.Text = "";
        };
        var keys = Section("Keyboard bindings (port)",
            new TextBlock
            {
                Text = "The game's own Game Options → Controls menu remaps keyboard and gamepad buttons. " +
                       "These fields only set the ReXGlue keyboard-as-pad fallback. " +
                       "Blank = runtime default. Examples: Space, Backspace, Enter, Shift+Up, MouseLeft.",
                Foreground = Brushes.Gray,
                FontSize = 12,
                TextWrapping = TextWrapping.Wrap,
                Margin = new Thickness(0, 0, 0, 6),
            },
            keyPanel,
            keyReset);

        var note = new TextBlock
        {
            Text = "Launch runs the ReXGlue Xbox 360 port. Settings are written to " +
                   "~/Projects/mn9-hybrid/mn9-hybrid.conf; the PC-only tabs are kept for Steam.",
            Foreground = Brushes.Gray,
            FontSize = 12,
            TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 8),
        };

        var root = new StackPanel
        {
            Margin = new Thickness(14),
            Spacing = 6,
            Children = { note, backendRow, displaySection, quality, content, saves, diag, keys },
        };
        return new ScrollViewer { Content = root };
    }

    private static StackPanel SimpleRow(string label, Control control, double width = 190) => new()
    {
        Orientation = Orientation.Horizontal,
        Spacing = 10,
        Children =
        {
            new TextBlock { Text = label, Width = width, VerticalAlignment = VerticalAlignment.Center },
            control,
        },
    };

    private void LoadPortSettings()
    {
        var ps = Port.Read();
        _cvBackend.SelectedIndex = ps.Backend == "pc" ? 1 : 0;
        _cvScale.SelectedIndex = ps.Scale switch { "1" => 1, "2" => 2, "3" => 3, _ => 0 };
        _cvAniso.SelectedIndex = ps.Aniso switch
        {
            "0" => 1, "1" => 2, "2" => 3, "3" => 4, "4" => 5, "5" => 6, _ => 0,
        };
        _cvMsaa.SelectedIndex = ps.Msaa == "1" ? 1 : ps.Msaa == "0" ? 2 : 0;
        _cvDither.SelectedIndex = ps.Dither == "1" ? 1 : ps.Dither == "0" ? 2 : 0;
        _cvAsync.IsChecked = ps.AsyncShaders == "1";
        int texIdx = Array.IndexOf(TexCacheValues, ps.TextureCacheMb);
        _cvTexCache.SelectedIndex = texIdx >= 0 ? texIdx : 0;
        _cvStart.SelectedIndex = ps.StartMode switch { "title" => 0, "game" => 2, _ => 1 };
        _cvLicense.Text = ps.LicenseMask;
        _cvUserData.Text = ps.UserDataRoot;
        _quality.SelectedItem = ps.Quality;
        _res.SelectedItem = ps.Resolution;
        _fullscreen.IsChecked = ps.Fullscreen;
        _vsync.IsChecked = ps.Vsync;
        _skipIntro.IsChecked = ps.SkipIntro;
        foreach (var kv in _cvKeys)
            kv.Value.Text = ps.Keybinds.TryGetValue(kv.Key, out string? v) ? v : "";
        _cvContent.Text = Port.ContentStatus();
        _cvSave.Text = Port.SaveStatus(ps);
        _cvStatus.Text = Port.StatusLine(ps);
        _cvLog.Text = Port.TailLog();
    }

    private PortSettings CollectPort()
    {
        var s = new PortSettings
        {
            Backend = _cvBackend.SelectedIndex == 1 ? "pc" : "rex",
            Quality = _quality.SelectedItem as string ?? "high",
            Fullscreen = _fullscreen.IsChecked == true,
            Vsync = _vsync.IsChecked == true,
            SkipIntro = _skipIntro.IsChecked == true,
            StartMode = _cvStart.SelectedIndex switch { 0 => "title", 2 => "game", _ => "menu" },
            Scale = _cvScale.SelectedIndex > 0 ? _cvScale.SelectedIndex.ToString() : "",
            Aniso = _cvAniso.SelectedIndex > 0 ? (_cvAniso.SelectedIndex - 1).ToString() : "",
            Msaa = _cvMsaa.SelectedIndex switch { 1 => "1", 2 => "0", _ => "" },
            Dither = _cvDither.SelectedIndex switch { 1 => "1", 2 => "0", _ => "" },
            AsyncShaders = _cvAsync.IsChecked == true ? "1" : "0",
            TextureCacheMb = TexCacheValues[Math.Clamp(_cvTexCache.SelectedIndex, 0, TexCacheValues.Length - 1)],
            LicenseMask = string.IsNullOrWhiteSpace(_cvLicense.Text)
                ? "4294967295" : _cvLicense.Text!.Trim(),
            UserDataRoot = _cvUserData.Text?.Trim() ?? "",
        };
        string res = _res.SelectedItem as string ?? "960x540";
        string[] wh = res.Split('x');
        if (wh.Length == 2 && int.TryParse(wh[0], out int w) && int.TryParse(wh[1], out int h))
        {
            s.Width = w;
            s.Height = h;
        }
        foreach (var kv in _cvKeys)
        {
            string value = kv.Value.Text?.Trim() ?? "";
            if (value.Length > 0) s.Keybinds[kv.Key] = value;
        }
        return s;
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
        foreach (var box in boxes)
        {
            box.Margin = new Thickness(0, 4, 22, 4);
            panel.Children.Add(box);
        }
        return panel;
    }

    private static Border Section(string title, params Control[] children)
    {
        var panel = new StackPanel { Spacing = 6 };
        panel.Children.Add(new TextBlock
        {
            Text = title,
            Foreground = new SolidColorBrush(Color.Parse("#5CE8FF")),
            FontWeight = FontWeight.Bold,
            Margin = new Thickness(0, 0, 0, 4),
        });
        foreach (var child in children) panel.Children.Add(child);
        return new Border
        {
            BorderBrush = new SolidColorBrush(Color.Parse("#5CE8FF")),
            Background = new SolidColorBrush(Color.Parse("#B30B1A2A")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Padding = new Thickness(12),
            Margin = new Thickness(0, 0, 0, 12),
            Child = panel,
        };
    }
}
