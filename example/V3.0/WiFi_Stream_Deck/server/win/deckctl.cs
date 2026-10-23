using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")]
public class MMDeviceEnumerator { }

[Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMMDeviceEnumerator {
    int EnumAudioEndpoints(int flow, int stateMask, out IntPtr devices);
    int GetDefaultAudioEndpoint(int flow, int role, out IMMDevice device);
}

[Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IMMDevice {
    int Activate(ref Guid iid, int clsCtx, IntPtr activationParams,
                 [MarshalAs(UnmanagedType.IUnknown)] out object iface);
    int OpenPropertyStore(int access, out IntPtr store);
    int GetId([MarshalAs(UnmanagedType.LPWStr)] out string id);
    int GetState(out int state);
}

[Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IAudioEndpointVolume {
    int RegisterControlChangeNotify(IntPtr p);
    int UnregisterControlChangeNotify(IntPtr p);
    int GetChannelCount(out uint c);
    int SetMasterVolumeLevel(float l, Guid ctx);
    int SetMasterVolumeLevelScalar(float l, Guid ctx);
    int GetMasterVolumeLevel(out float l);
    int GetMasterVolumeLevelScalar(out float l);
    int SetChannelVolumeLevel(uint ch, float l, Guid ctx);
    int SetChannelVolumeLevelScalar(uint ch, float l, Guid ctx);
    int GetChannelVolumeLevel(uint ch, out float l);
    int GetChannelVolumeLevelScalar(uint ch, out float l);
    int SetMute([MarshalAs(UnmanagedType.Bool)] bool m, Guid ctx);
    int GetMute([MarshalAs(UnmanagedType.Bool)] out bool m);
    int GetVolumeStepInfo(out uint step, out uint stepCount);
    int VolumeStepUp(Guid ctx);
    int VolumeStepDown(Guid ctx);
    int QueryHardwareSupport(out uint mask);
    int GetVolumeRange(out float min, out float max, out float inc);
}

internal static class Program
{
    private const uint KeyUp = 0x0002;
    private const uint Extended = 0x0001;

    [DllImport("user32.dll")]
    private static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);

    private static IAudioEndpointVolume Endpoint(int flow)
    {
        var en = (IMMDeviceEnumerator)(new MMDeviceEnumerator());
        IMMDevice dev;
        if (en.GetDefaultAudioEndpoint(flow, 0, out dev) != 0 || dev == null) return null;
        Guid iid = typeof(IAudioEndpointVolume).GUID;
        object o;
        if (dev.Activate(ref iid, 23, IntPtr.Zero, out o) != 0) return null;
        return o as IAudioEndpointVolume;
    }

    private const int Render = 0;
    private const int Capture = 1;

    private static int Step(float delta)
    {
        var v = Endpoint(Render);
        if (v == null) { Console.WriteLine("no audio endpoint"); return 1; }
        float level;
        if (v.GetMasterVolumeLevelScalar(out level) != 0) { Console.WriteLine("read volume failed"); return 1; }
        float next = level + delta;
        if (next < 0f) next = 0f;
        if (next > 1f) next = 1f;
        Guid ctx = Guid.Empty;
        v.SetMasterVolumeLevelScalar(next, ctx);
        Console.WriteLine("{0:0}%", next * 100f);
        return 0;
    }

    private static int ToggleMute(int flow)
    {
        var v = Endpoint(flow);
        if (v == null) { Console.WriteLine(flow == Capture ? "no microphone" : "no audio endpoint"); return 1; }
        bool muted;
        if (v.GetMute(out muted) != 0) { Console.WriteLine("read mute failed"); return 1; }
        Guid ctx = Guid.Empty;
        v.SetMute(!muted, ctx);
        if (flow == Capture) Console.WriteLine(muted ? "mic on" : "mic muted");
        else Console.WriteLine(muted ? "sound on" : "muted");
        return 0;
    }

    private static void CtrlKey(byte vk)
    {
        keybd_event(0x11, 0, 0, UIntPtr.Zero);            // Ctrl down
        keybd_event(vk, 0, 0, UIntPtr.Zero);
        keybd_event(vk, 0, KeyUp, UIntPtr.Zero);
        keybd_event(0x11, 0, KeyUp, UIntPtr.Zero);        // Ctrl up
    }

    private static void AltKey(byte vk)
    {
        keybd_event(0x12, 0, 0, UIntPtr.Zero);            // Alt down
        keybd_event(vk, 0, 0, UIntPtr.Zero);
        keybd_event(vk, 0, KeyUp, UIntPtr.Zero);
        keybd_event(0x12, 0, KeyUp, UIntPtr.Zero);        // Alt up
    }

    private static void Snip()
    {
        keybd_event(0x5B, 0, Extended, UIntPtr.Zero);     // Win down
        keybd_event(0x10, 0, 0, UIntPtr.Zero);            // Shift down
        keybd_event(0x53, 0, 0, UIntPtr.Zero);            // S
        keybd_event(0x53, 0, KeyUp, UIntPtr.Zero);
        keybd_event(0x10, 0, KeyUp, UIntPtr.Zero);
        keybd_event(0x5B, 0, Extended | KeyUp, UIntPtr.Zero);
    }

    private static void Key(byte vk, bool extended)
    {
        uint flags = extended ? Extended : 0;
        keybd_event(vk, 0, flags, UIntPtr.Zero);
        keybd_event(vk, 0, flags | KeyUp, UIntPtr.Zero);
    }

    private static void ShowDesktop()
    {
        keybd_event(0x5B, 0, Extended, UIntPtr.Zero);
        keybd_event(0x44, 0, 0, UIntPtr.Zero);
        keybd_event(0x44, 0, KeyUp, UIntPtr.Zero);
        keybd_event(0x5B, 0, Extended | KeyUp, UIntPtr.Zero);
    }

    private static void OpenTerminal()
    {
        foreach (var exe in new[] { "wt.exe", "powershell.exe" })
        {
            try
            {
                Process.Start(new ProcessStartInfo(exe) { UseShellExecute = true });
                return;
            }
            catch (Exception) { }
        }
    }

    private static int Main(string[] args)
    {
        if (args.Length == 0) return 2;
        switch (args[0].ToLowerInvariant())
        {
            case "volume_up":
            case "vol_up": return Step(0.02f);
            case "volume_down":
            case "vol_down": return Step(-0.02f);
            case "mute": return ToggleMute(Render);
            case "mic_mute": return ToggleMute(Capture);
            case "esc": Key(0x1B, false); return 0;
            case "copy": CtrlKey(0x43); return 0;
            case "paste": CtrlKey(0x56); return 0;
            case "snip": Snip(); return 0;
            case "start": Key(0x5B, true); return 0;
            case "alt_tab": AltKey(0x09); return 0;
            case "ctrl_tab": CtrlKey(0x09); return 0;
            case "play_pause": Key(0xB3, true); return 0;
            case "next": Key(0xB0, true); return 0;
            case "prev": Key(0xB1, true); return 0;
            case "desktop": ShowDesktop(); return 0;
            case "terminal": OpenTerminal(); return 0;
            default: return 2;
        }
    }
}
