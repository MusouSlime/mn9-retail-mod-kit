using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using System.Runtime.InteropServices;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Input;
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
    private readonly NumericUpDown _fps = new() { Minimum = 24, Maximum = 240, Value = 62, Width = 150 };
    private readonly NumericUpDown _pool = new() { Minimum = 64, Maximum = 1024, Increment = 32, Value = 256, Width = 150 };
    private readonly NumericUpDown _physics = new() { Minimum = 1, Maximum = 10, Value = 5, Width = 150 };

    private readonly CheckBox _shadows = new() { Content = "Shadows", IsChecked = true };
    private readonly CheckBox _bloom = new() { Content = "Bloom / FXAA", IsChecked = true };
    private readonly CheckBox _motion = new() { Content = "Motion blur", IsChecked = true };
    private readonly CheckBox _shafts = new() { Content = "Light shafts", IsChecked = true };
    private readonly CheckBox _distortion = new() { Content = "Distortion", IsChecked = true };
    private readonly CheckBox _flares = new() { Content = "Lens flares", IsChecked = true };
    private readonly CheckBox _ao = new() { Content = "Ambient occlusion", IsChecked = true };
    private readonly CheckBox _mlaa = new() { Content = "MLAA (post AA)" };
    private readonly CheckBox _msaa = new() { Content = "MSAA (broken on this build)" };
    private readonly CheckBox _fullscreen = new() { Content = "Fullscreen" };
    private readonly CheckBox _vsync = new() { Content = "VSync", IsChecked = true };
    private readonly CheckBox _voice = new() { Content = "Steam voice" };
    private readonly CheckBox _movies = new() { Content = "Play intro movies" };
    private readonly CheckBox _mouse = new() { Content = "Free the mouse (external patch)" };
    private readonly CheckBox _lock = new() { Content = "Lock config (stop game-side saving)", IsChecked = true };

    private readonly TextBlock _status = new()
    {
        Text = "",
        TextWrapping = TextWrapping.Wrap,
        Margin = new Thickness(0, 10, 0, 0),
    };

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
            ? "Mighty No. 9 Patcher — 64-bit"
            : "Mighty No. 9 Patcher — 32-bit";
        Width = 980;
        Height = 700;
        MinWidth = 760;
        MinHeight = 520;
        CanResize = true;
        WindowStartupLocation = WindowStartupLocation.CenterScreen;

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

        var launch = new Button { Content = "Launch with Safe Mode" };
        launch.Click += async (_, _) =>
        {
            string dir = CurrentDir();
            if (dir == null) return;
            _status.Text = "Launching...";
            string msg = await Task.Run(() => Patcher.LaunchSafe(dir));
            _status.Text = msg;
        };

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

        var pathRow = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 8,
            Children = { new TextBlock { Text = "Game:", VerticalAlignment = VerticalAlignment.Center }, _path, browse, detect },
        };

        var resRow = Row("Resolution", _res, "Frame cap", _fps);
        var advanced = Row("Texture pool (MB)", _pool, "Physics substeps", _physics);

        var checks = new WrapPanel { Orientation = Orientation.Horizontal };
        foreach (var c in new[] { _shadows, _bloom, _motion, _shafts, _distortion, _flares, _ao, _mlaa, _msaa, _fullscreen, _vsync, _voice, _movies, _mouse, _lock })
        {
            c.Margin = new Thickness(0, 4, 18, 4);
            checks.Children.Add(c);
        }

        var buttons = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10,
            Margin = new Thickness(0, 16, 0, 0),
            Children = { apply, restore, launch },
        };

        var body = new StackPanel
        {
            Margin = new Thickness(18),
            Spacing = 10,
            Children =
            {
                new TextBlock { Text = "Mighty No. 9 Patcher", FontSize = 24, FontWeight = FontWeight.Bold },
                new TextBlock
                {
                    Text = "Pick what you want, then press Apply settings. Config is backed up and locked read-only.",
                    Foreground = Brushes.Gray,
                    TextWrapping = TextWrapping.Wrap,
                },
                new TextBlock
                {
                    Text = $"{(Environment.Is64BitProcess ? "64-bit" : "32-bit")} · " +
                           $"{RuntimeInformation.ProcessArchitecture} · " +
                           $"{RuntimeInformation.OSDescription}",
                    Foreground = Brushes.Gray,
                    FontSize = 12,
                },
                pathRow,
                resRow,
                advanced,
                new Border
                {
                    BorderBrush = Brushes.DimGray,
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(6),
                    Padding = new Thickness(12),
                    Child = checks,
                },
                buttons,
                new TextBlock
                {
                    Text = "Save backups",
                    FontWeight = FontWeight.Bold,
                    Margin = new Thickness(0, 16, 0, 0),
                },
                new StackPanel
                {
                    Orientation = Orientation.Horizontal,
                    Spacing = 10,
                    Children = { _saveCombo, backupSaves, restoreSaves },
                },
                _saveInfo,
                _status,
            },
        };

        Content = new ScrollViewer
        {
            Content = body,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        KeyDown += (_, e) =>
        {
            if (e.Key == Key.Enter && e.KeyModifiers.HasFlag(KeyModifiers.Control))
                Apply();
        };
        _path.Text = Environment.GetEnvironmentVariable("MN9_GAME_DIR") ?? Patcher.FindGameDir() ?? "";
        _path.TextChanged += (_, _) => RefreshSaveBackups(_path.Text?.Trim());
        RefreshSaveBackups(_path.Text?.Trim());
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
        var folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = "Select Mighty No. 9",
            AllowMultiple = false,
        });
        if (folders.Count > 0)
        {
            _path.Text = folders[0].Path.LocalPath;
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
            LockConfig = _lock.IsChecked == true,
            Fps = (int)(_fps.Value ?? 62),
            Pool = (int)(_pool.Value ?? 256),
            Physics = (int)(_physics.Value ?? 5),
        };

        string res = _res.SelectedItem as string ?? "1280x720";
        string[] wh = res.Split('x');
        s.Width = int.Parse(wh[0]);
        s.Height = int.Parse(wh[1]);

        _status.Text = "Applying...";
        _status.Text = Patcher.Apply(dir, s);
    }
}
