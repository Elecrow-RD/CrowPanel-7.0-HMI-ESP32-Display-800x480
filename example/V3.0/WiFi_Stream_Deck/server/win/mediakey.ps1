<#
    Volume and media control for the stream deck.

    Volume + mute drive the Core Audio endpoint directly. Broadcasting
    WM_APPCOMMAND reached every top-level window, so a toggle such as mute ran
    several times per press (mute then unmute in the same tap) and volume jumped
    by several steps. Core Audio also lets us set an explicit state instead of
    toggling blindly.

    Transport keys (play/pause, next, prev) go out as media keypresses, which the
    OS routes to the active media session.
#>
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("volume_up", "volume_down", "mute", "play_pause", "next", "prev")]
    [string]$Key
)

Add-Type -TypeDefinition @'
using System;
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

public static class DeckAudio {
    static IAudioEndpointVolume Endpoint() {
        var en = (IMMDeviceEnumerator)(new MMDeviceEnumerator());
        IMMDevice dev;
        if (en.GetDefaultAudioEndpoint(0, 0, out dev) != 0 || dev == null) return null;
        Guid iid = typeof(IAudioEndpointVolume).GUID;
        object o;
        if (dev.Activate(ref iid, 23, IntPtr.Zero, out o) != 0) return null;
        return o as IAudioEndpointVolume;
    }

    public static void Step(float delta) {
        var v = Endpoint();
        if (v == null) return;
        float level;
        if (v.GetMasterVolumeLevelScalar(out level) != 0) return;
        float next = level + delta;
        if (next < 0f) next = 0f;
        if (next > 1f) next = 1f;
        Guid ctx = Guid.Empty;
        v.SetMasterVolumeLevelScalar(next, ctx);
    }

    public static void ToggleMute() {
        var v = Endpoint();
        if (v == null) return;
        bool muted;
        if (v.GetMute(out muted) != 0) return;
        Guid ctx = Guid.Empty;
        v.SetMute(!muted, ctx);
    }
}
'@

$mediaKeys = @{ play_pause = 0xB3; next = 0xB0; prev = 0xB1 }
$vkMedia = $mediaKeys[$Key]

if ($null -eq $vkMedia) {
    switch ($Key) {
        "volume_up"   { [DeckAudio]::Step(0.02) }
        "volume_down" { [DeckAudio]::Step(-0.02) }
        "mute"        { [DeckAudio]::ToggleMute() }
    }
} else {
    Add-Type -Namespace Deck -Name Native -MemberDefinition @'
[DllImport("user32.dll")]
public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, System.UIntPtr dwExtraInfo);
'@
    $extended = 0x0001
    $keyUp = 0x0002
    [Deck.Native]::keybd_event([byte]$vkMedia, 0, [uint32]$extended, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 40
    [Deck.Native]::keybd_event([byte]$vkMedia, 0, [uint32]($extended -bor $keyUp), [UIntPtr]::Zero)
}
