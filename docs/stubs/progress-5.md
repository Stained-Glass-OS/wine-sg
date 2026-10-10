# Drafter 5 progress (media, speech, games)

## 2026-10-09
Build host: 8 cores, 15 GB (tree /var/tmp/drafter-5/build on the drafter's machine); gh logged in with the machine's git credentials; draft PR #3 (stubs-5 -> main).

| patch | DLL | what | gate | mutants caught |
|---|---|---|---|---|
| 2800 | windows.gaming.input | IInspectable stubs on 12 files, provider/controller/raw controller/Gamepad2/racing wheel members, RegisterCustomFactoryForHardwareId (117 audit items) | wgi-stubs, wgi-device (uinput virtual gamepad) | 8 |
| 2820 | windows.media.mediacontrol | IInspectable, SMTC properties, display updater, music properties, Genres vector | wmc-stubs | 6 |
| 2821 | windows.media | leftover FIXMEs (the rest was patch 1700) | wmedia-stubs | 2 |
| 2822 | windows.media.devices | factory IInspectable, selectors, events | wmdev-stubs | 5 |
| 2830 | mfplat | MFCreateMediaBufferWrapper, MFCalculateBitmapImageSize, FP16 arrays, legacy buffer offset, Copy2DTo, file byte stream SetLength/Flush/Close, MFCreateMFByteStreamOnStreamEx, property store Commit | mfplat-buffers, mfplat-bytestream | 9 |
| 2832 | winmm | mixer TARGETTYPE, MCI element ids, MIDI cache calls, mmio memory file growth | winmm-misc | 3 |

| 2810 | windows.media.speech | IInspectable on all objects, async ids/handlers, list constraint, session timeout/events | wmspeech | 6 |
| 2811 | windows.media.speech | recognizer timeouts/UI options/languages/Recognize/Close/StateChanged; synthesizer Options/Voice/Close | wmspeech2 | 6 |
| 2833 | dsound | Restore, AcquireResources, capture Initialize/GetObjectInPath/GetFXStatus, LockServer, KsPropertySet::Set | dsound-stubs | 3 |

| 2834 | mf, mfreadwrite | IMFAudioPolicy + OnClockSetRate of the audio renderer, video capture enumeration, EVR stream GetService, LockServer | mf-misc | 4 |
| 2835 | xinput1_3/1_4 | XInputGetDSoundAudioDeviceGuids, XInputGetAudioDeviceIds | xinput-audio (uinput pad) | 1 |
| 2836 | dinput | device Initialize/RunControlPanel/GetImageInfo, IDirectInput7::FindDevice, LockServer | dinput-misc | 3 |
| 2837 | qdvd, xaudio2_7, evr | class factory LockServer | lockserver | 1 |
| 2838 | mfplat | MFCreateVideoMediaTypeFromVideoInfoHeader(2), ...BitMapInfoHeader(Ex) | mfplat-mediatype | 4 |
| 2840-2845 | sapi | streams, audio formats, resource manager, tokens/categories, voice + events, mmaudio, connection points, XML (helper agent) | sapi-stream/token/voice/mmaudio | 47 |
| 2860, 2861 | winegstreamer | WM reader profile/stream config; MPEG audio decoder settings (helper agent) | wgst-wmreader, wgst-mpegaudio | 11 |
| 2870-2872 | mfmediaengine, mfplay, mfsrcsnk | engine/player/wave sink members (helper agent) | mfme, mfplay, mfsrcsnk | 18 |
| 2880-2882 | winegstreamer | IMediaObject side of resampler, color converter, WMA decoder (helper agent) | wgdmo-* | 21 |
| 2890-2892 | evr | mixer processor/bitmap/mapper/quality, presenter display control/rate/quality, filter config (helper agent) | evr-mixer/presenter/filter | 19 |
| 2900 | qdvd | IDvdControl2/IDvdInfo2/graph builder for a navigator with no disc (helper agent) | qdvd-nav | 4 |

Still being drafted by helper agents: wmp/wmvcore (2850-2859), winegstreamer video decoder/encoder/aac/processor (2910-2919) and wm_reader rest/media sink/source (2920-2929).
CI: the repository runs no checks on the draft PR (gh pr checks: none); every patch was built for x86_64 and i386 locally and applied in order to a pristine tree.
Ground truth rule used throughout: dlls/*/tests encode real behaviour; todo_wine marks that now pass were removed in the same patch.
CI: not checked yet (PR #3 is a draft; first CI run pending).

Notes for the integrator:
- Makefile test-* targets are not added (shared .PHONY line conflicts across drafters): wgi-stubs wgi-device wmc-stubs wmedia-stubs wmdev-stubs mfplat-buffers mfplat-bytestream winmm-misc.
- wgi-device-gate needs /dev/uinput writable and a readable /dev/input node for the virtual pad; it SKIPs (77) otherwise.
- Many audit "notimpl" items are documented answers, not stubs (IMFAsyncCallback::GetParameters E_NOTIMPL, MFT stream-id methods on fixed-stream transforms): left alone.

## End of session (2026-10-09, evening)
Integrated in this branch on top of origin/main (series order): 2831, 2836, 2838, 2840-2845, 2850-2857, 2862, 2870-2872, 2880-2882, 2890-2892, 2900, 2910-2912, 2920-2922, 2930, 2940, 2950-2953, 2960, 2980-2981, 2985-2988, 2990-2995.
Not in the series (written, not fully verified): 2970-2972 (dmsynth, dmusic, xact: conformance passes, gates/mutants missing).
Not done (details in escalate-5.md): dmband, dmcompos, dmscript, most of dmstyle style.c; quartz filtermapper/dsoundrender/videorenderer/window.c; mf sequencer source, MFPluginControl and the other mf/mfplat/mfreadwrite leftovers; wmvcore writer; wm_reader async reader parts; dmusic32.
Notes: 2862 and 2960 had only part of their mutants run before the session ended (see their notes); dmime tests were rerun clean (0 failures).

## 2026-10-09 night: second pass (integrator merged up to 10.0-399 meanwhile)
Not yet integrated (this branch on top of origin/main 6e16d8e): 2839 (dinput Escape/SendDeviceData), 2873/2874 (mf, mfreadwrite todo_wine ground truth), 2875 (dinput device types/names/properties from the conformance tests), 2941-2944 (quartz filter mapper, dsound renderer, video overlay, video window), 2961-2963 (dmband, dmstyle style object, dmcompos), 2970-2972 (dmsynth, dmusic, xact).
Still open (escalate-5.md): dmscript, dmstyle playback, quartz AddSourceFilter/collections, wmvcore writer (a helper may deliver 2863+), mf sequencer source, MFPluginControl, dinput JoyConfig8, wm_reader async parts.
