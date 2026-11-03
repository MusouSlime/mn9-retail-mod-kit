using System.Globalization;

namespace Mn9Patcher;

/// <summary>Settings for the optional OpenAL audio shim (music/SFX/voice).</summary>
public sealed class AudioSettings
{
    public bool Enabled = true;
    public string Music = "random";     // game | mute | random | playlist | shuffle
    public string MusicDir = DefaultMusicDir;
    public string MusicMap = "";
    public string SfxPitch = "0";       // 0 | 0.05 | 0.10 | 0.20
    public string Voice = "game";       // game | mute
    public string MusicGain = "1";      // 0..1
    public string SfxGain = "1";
    public string VoiceGain = "1";
    public bool Duck = true;
    public bool Night = false;

    public static string DefaultMusicDir
    {
        get
        {
            string m = Environment.GetFolderPath(Environment.SpecialFolder.MyMusic);
            if (string.IsNullOrEmpty(m))
                m = System.IO.Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Music");
            return m;
        }
    }

    public static string Path(string gameDir) =>
        System.IO.Path.Combine(gameDir, ".mn9charsel", "audio.conf");

    public static AudioSettings Read(string gameDir)
    {
        var s = new AudioSettings();
        try
        {
            string path = Path(gameDir);
            if (!File.Exists(path)) return s;
            foreach (string raw in File.ReadAllLines(path))
            {
                string line = raw.Trim();
                if (line.Length == 0 || line.StartsWith('#')) continue;
                int eq = line.IndexOf('=');
                if (eq <= 0) continue;
                string key = line[..eq].Trim(), value = line[(eq + 1)..].Trim();
                switch (key)
                {
                    case "enabled": s.Enabled = value != "0"; break;
                    case "music" when value is "game" or "mute" or "random" or "playlist" or "shuffle": s.Music = value; break;
                    case "music_dir": s.MusicDir = value; break;
                    case "music_map": s.MusicMap = value; break;
                    case "sfx_pitch" when double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out _): s.SfxPitch = value; break;
                    case "voice" when value is "game" or "mute": s.Voice = value; break;
                    case "music_gain": s.MusicGain = value; break;
                    case "sfx_gain": s.SfxGain = value; break;
                    case "voice_gain": s.VoiceGain = value; break;
                    case "duck": s.Duck = value != "0"; break;
                    case "night": s.Night = value != "0"; break;
                }
            }
        }
        catch { }
        return s;
    }

    public static string Write(string gameDir, AudioSettings s)
    {
        try
        {
            string path = Path(gameDir);
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(path)!);
            s.Enabled = s.Music != "game" || s.SfxPitch != "0" || s.Voice != "game" ||
                        s.MusicGain != "1" || s.SfxGain != "1" || s.VoiceGain != "1" ||
                        s.Night;
            File.WriteAllText(path,
                $"enabled={(s.Enabled ? 1 : 0)}{Environment.NewLine}" +
                $"music={s.Music}{Environment.NewLine}" +
                $"music_dir={s.MusicDir}{Environment.NewLine}" +
                $"music_map={s.MusicMap}{Environment.NewLine}" +
                $"sfx_pitch={s.SfxPitch}{Environment.NewLine}" +
                $"voice={s.Voice}{Environment.NewLine}" +
                $"music_gain={s.MusicGain}{Environment.NewLine}" +
                $"sfx_gain={s.SfxGain}{Environment.NewLine}" +
                $"voice_gain={s.VoiceGain}{Environment.NewLine}" +
                $"duck={(s.Duck ? 1 : 0)}{Environment.NewLine}" +
                $"night={(s.Night ? 1 : 0)}{Environment.NewLine}");
            return s.Enabled ? "Audio settings saved (shim enabled)." : "Audio settings saved (shim inactive).";
        }
        catch (Exception ex)
        {
            return "Audio settings failed: " + ex.Message;
        }
    }
}
