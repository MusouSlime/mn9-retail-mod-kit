using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Mn9Patcher;

/// <summary>
/// Best-effort cross-platform intro skipper for a directly launched port.
/// Linux uses xdotool; Windows uses the Win32 foreground/keybd_event path.
/// </summary>
internal static class IntroSkipper
{
    public static async Task RunAsync(Process process, PortSettings s, int? pressesOverride = null,
                                      int? delayOverride = null)
    {
        try
        {
            int presses = Math.Clamp(pressesOverride ?? s.SkipPresses, 0, 20);
            if (presses <= 0) return;
            IntPtr hwnd = OperatingSystem.IsWindows()
                ? await WaitForWindowsWindowAsync(process, TimeSpan.FromSeconds(60))
                : IntPtr.Zero;

            await Task.Delay(TimeSpan.FromSeconds(Math.Clamp(delayOverride ?? s.SkipDelay, 0, 120)));
            for (int i = 0; i < presses; i++)
            {
                if (OperatingSystem.IsWindows())
                {
                    if (hwnd == IntPtr.Zero)
                        hwnd = await WaitForWindowsWindowAsync(process, TimeSpan.FromSeconds(5));
                    if (hwnd != IntPtr.Zero) PressWindows(hwnd);
                }
                else
                {
                    PressLinux(process.Id);
                }
                await Task.Delay(TimeSpan.FromSeconds(Math.Clamp(s.SkipGap, 1, 30)));
            }
        }
        catch { }
    }

    // --- Linux --------------------------------------------------------------

    private static void PressLinux(int pid)
    {
        // xdotool 4.x on some distros rejects --sync after windowactivate.
        // Try the plain forms first, then the focus fallback.
        if (Tool("xdotool", $"search --pid {pid} windowactivate key space")) return;
        if (Tool("xdotool", $"search --pid {pid} windowfocus key space")) return;
        if (Tool("xdotool", $"search --pid {pid} windowactivate --sync key space")) return;
    }

    private static bool Tool(string exe, string args)
    {
        try
        {
            var psi = new ProcessStartInfo
            {
                FileName = exe,
                Arguments = args,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true,
            };
            using var p = Process.Start(psi);
            if (p == null) return false;
            p.StandardOutput.ReadToEnd();
            p.StandardError.ReadToEnd();
            if (!p.WaitForExit(5000))
            {
                try { p.Kill(); } catch { }
                return false;
            }
            return p.ExitCode == 0;
        }
        catch { return false; }
    }

    // --- Windows ------------------------------------------------------------

    private const uint KeyEventKeyUp = 0x0002;
    private const byte VkSpace = 0x20;

    [DllImport("user32.dll")]
    private static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    private static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);

    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hWnd);

    private static async Task<IntPtr> WaitForWindowsWindowAsync(Process process, TimeSpan timeout)
    {
        DateTime end = DateTime.UtcNow + timeout;
        while (DateTime.UtcNow < end)
        {
            try
            {
                process.Refresh();
                IntPtr handle = process.MainWindowHandle;
                if (handle == IntPtr.Zero) handle = FindWindowForPid((uint)process.Id);
                if (handle != IntPtr.Zero) return handle;
            }
            catch { }
            await Task.Delay(400);
        }
        return IntPtr.Zero;
    }

    private static IntPtr FindWindowForPid(uint pid)
    {
        IntPtr found = IntPtr.Zero;
        EnumWindows((hWnd, _) =>
        {
            GetWindowThreadProcessId(hWnd, out uint windowPid);
            if (windowPid == pid && IsWindowVisible(hWnd))
            {
                found = hWnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    private static void PressWindows(IntPtr hwnd)
    {
        ShowWindow(hwnd, 5); // SW_SHOW
        SetForegroundWindow(hwnd);
        Thread.Sleep(150);
        keybd_event(VkSpace, 0, 0, UIntPtr.Zero);
        Thread.Sleep(180);
        keybd_event(VkSpace, 0, KeyEventKeyUp, UIntPtr.Zero);
    }
}
