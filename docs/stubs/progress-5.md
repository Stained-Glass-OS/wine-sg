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

## 2026-10-10
Added: 2876-2878 (dinput JoyConfig8, effect files, HID/mouse FIXMEs), 2802-2804 (winmm mmio parse + MCI parser, mixer callbacks + midi streams; dsound effects/format/status/channels).
Left (outside winmm/dsound code or no contract): mciwave/mciavi32/mcicda todo_wine groups, midiConnect/midiDisconnect, mf sequencer source, MFPluginControl, dmscript, dmstyle playback, quartz AddSourceFilter/collections, wm_reader async parts, wmvcore system profiles/encoder.

- 2805-2806 dmscript: script object (ActiveScript engines, routines, variables, errors) and script track (play/clone/params); gate test/dmscript-gate.sh, 13 mutants caught; dmscript_test 0 failures, todos removed.
- 2964-2966 mfplat: VideoInfoHeader(2) init, MFVideoFormat/AMMediaType conversion, other-major media buffers + legacy Lock; 3 gates, 14 mutants caught, mfplat_test 0 failures, 65 todo_wine removed. Remaining mfplat stubs have no test ground truth (see escalate-5.md).
- 2807 quartz: Video Renderer get/put_FullScreenMode return E_NOTIMPL as the conformance tests show (invented fullscreen code removed, todo_wine dropped); gate test/quartz-fullscreen-gate.sh, 2 mutants caught.
- 2945 evr: seven spec stubs now forward to the mfplat implementations (DXVA color info, FP16 arrays, bitmap/VIH2 media types, uncompressed format, InitVideoFormat); gate ~285 checks, 6 mutants caught. MFCreateVideoOTA / MFCreateVideoPresenter2 still need an engine.
- 2808-2809 quartz: AmpFactorToDB/DBToAmpFactor, RegisterFilter version handling, graph SetTimeFormat/ConvertTimeFormat, EC_REPAINT default handling; gate test/quartz-misc-gate.sh (60 checks), 6 mutants caught, 10 todo_wine removed, filtergraph 0 failures. Assumptions (documentation only): amp units (dB in 1/100 dB, amp in percent), E_INVALIDARG for unsupported REGFILTER2 version, EC_REPAINT paused/stopped fallbacks not value-checked.
- 2946-2949 wmp: idle-state media/playlist/cdrom collections, dvd, playerApplication, buffering/download progress FIXMEs; one gate test/wmp-collections-gate.sh (needs all four), 7 mutants caught, media+oleobj pass. Assumptions (documentation only): S_FALSE vs error for absent items, dvd domain 'stop', NS_E_DVD_CANNOT_JUMP, add/newPlaylist return unstored objects.
- 2967-2969 mfplat: IMFPluginControl (registry-backed), Serialize/DeserializePresentationDescriptor, IMFVideoMediaType::GetVideoRepresentation + interlace modes; 3 gates, 7 mutants caught, mfplat_test 0 failures. Assumptions: registry layout and serialized byte layout are private, not Windows-compatible. Remaining mfplat @stub exports undocumented (listed in notes).
- 2867-2869 wmvcore: seven audio-only v8 system profiles (index list), writer idle statistics/time, input settings; gate test/wmvw-sysprof-gate.sh, 3 mutants caught, wmvcore_test passes. Assumptions: profile ID GUIDs unknown so LoadProfileByID/SetProfileByID return NS_E_INVALIDPROFILE; sample rates and codec format guessed; video/VBR profiles skipped.
- 2973-2974 mf: IMFSequencerSource queue + idle media source + topology provider, quality manager notifications; gate test/mf-sequencer-gate.sh, 6 mutants caught. Assumptions in notes/2973.txt (ids from 1, MF_E_* choices). mf.c failure count unchanged vs 2834 note (not re-baselined).
- 2982-2984 windows.media.mediacontrol: the 8 AudioStateMonitor creator exports (Win32 audiostatemonitorapi, not WinRT); local audio.idl, idle monitor (SoundLevel Full, unique callback handles); gate test/mc-audiostate-gate.sh (156 checks), 4 mutants caught, mediacontrol_test 0 failures. Assumption: IAudioStateMonitor IID 63bd8738-e30d-4c77-bf5c-834e87c657e2 from memory; error codes for NULL args guessed.
- 2976-2977 winmm: midiConnect/midiDisconnect connection table with thru forwarding (midiInOpen now routes callbacks via an internal forwarder), MCI_OPEN element+element-id = MCIERR_FLAGS_NOT_COMPATIBLE; gate test/winmmthru-gate.sh, 5 mutants caught, midi test failures same lines as baseline (31). Assumptions: tests have no midiConnect coverage; error codes from documentation. Input->output data flow untested (no MIDI input here); mci test not baselined (217 failures in this env).
- 2954 dmstyle: style/motif track Play emits DMUS_NOTE_PMSG/DMUS_CURVE_PMSG (pattern selection by command, variation choice, music-time conversion, chord-based pitch); gate test/dmcontent-play-gate.sh (~45 checks), 11 mutants caught, dmstyle_test 0 failures. Assumptions (not from docs, see notes/2954.txt): default chord/scale, music value bit layout, pitch base, variation eligibility/order, seeding. Audition Load/Save, motif Load, chord-map methods and dmcompos Compose* remain.
- 2913-2917 dmime: MIDIToMusic/MusicToMIDI for non-fixed modes, Stop with future time/resolution flags, segment Compose, PlaySegmentEx flags/from/AUTOTRANSITION; gate test/dmime-conv-gate.sh (~134 checks), 13 mutants caught, dmime_test 1 failure (dmime.c:4558, same without patches). Assumptions (no Windows reference) in notes/2913-2917.txt: music value layout, result codes, tie-breaking, resolution order. Left: InitAudio flags, parameter control DMOs, segment trigger motifs, SetGraph tool cleanup.
- 2918 dmcompos: composer engine (ComposeTemplateFromShape/ComposeSegmentFromTemplate/ComposeSegmentFromShape/ComposeTransition/AutoTransition, signpost track Compose); gate test/dmcompose-gate.sh (~95 checks), 14 mutants caught, dmcompos_test 0 failures. Algorithm choices (signpost spacing, activity->chord rate, transition lengths, error codes) are assumptions: docs give interfaces, not the algorithm.
- 2931-2934 quartz: additive include/control.idl (IAMCollection, IMediaTypeInfo, IFilterInfo, IRegFilterInfo, IPinInfo), filter/pin/media-type/reg-filter objects, IMediaControl::AddSourceFilter/get_FilterCollection/get_RegFilterCollection; gate test/quartz-collections-gate.sh (609 checks), 11 mutants caught, filtergraph test 0 failures (other quartz tests not rerun). Assumptions: zero-based Item, IsFileSource OATRUE, widl-assigned DISPIDs for new methods (not native ids), guessed error codes. Touches shared include/ (patch 2931, additive).
- 2996 evr: unknown CLSID in DllGetClassObject logs WARN (LockServer already done by 2837); gate test/evr-classfactory-gate.sh. MFCreateVideoPresenter2/MFCreateVideoOTA remain @stub: no known signature, a guessed stdcall signature would corrupt the caller's stack (escalated).
- 2955-2956 winegstreamer/wmvcore: sync reader SetStreamsSelected values/CLEANPOINT_ONLY, SetRange(start,0), async reader Pause/Resume/rate/StartAtPosition(frame, timecode)/Accelerator/TypeNegotiation (2956 edits dlls/wmvcore/async_reader.c); gate test/wgasync-gate.sh (68 checks), 8 mutants caught, existing wmv gates pass; wmvcore_test produced an empty .ok (read as 0 failures, no baseline). Assumptions: Pause/Resume HRESULTs, rate rules, playlist offset error (from memory of docs). Left: InitPlaylistBurn, compressed output formats, reverse playback, DRM.
- 2879 dinput: top-level HID usages multi-axis/simulation/game controls enumerate with a DI8DEVTYPE_* (table dlls/dinput/hid_usage_type.h); gate test/dinput-hidusage-gate.sh checks the table (32 checks, 4 mutants via SG_UT_DEFS) but the try_open dispatch is NOT exercised end to end (no virtual device with these usages); joystick8 142 failures patched = baseline, device8 baseline hung (inconclusive). Mapping rows other than gamepad/joystick are my choice (not documented). i386 build clean.

## 2026-10-12 (queue audit5-drafter5.json, fast mode, range 4000-4499)
Group 1: patches 4000-4030, one per DLL, gates test/*-smoke-gate.sh sourcing test/media-smoke.sh (probe test/media-smoke-probe.c, one section per DLL, 64+32-bit, own Xvfb).
- 4000 dsdmo (all 9 effect DMOs: params, envelopes, DSP), 4001 qcap (AVI mux audio/seek/config, audio capture, capture graph builder, v4l WoW64 pUnk crash with a real webcam), 4002 dxva2 (monitor config API, processor ranges), 4003 devenum (moniker persistence), 4004 qasf (seeking restart), 4005 mmdevapi (spatial audio objects), 4006 winealsa (device position), 4007 gamebar, 4008 mediacontrol (+windows.media.idl), 4009 mfplat (opaque AM formats), 4010 mf (audio capture source, tee nodes, PMP session, transcode profile; +mfidl.idl), 4011 mfreadwrite (writer chains/Finalize, reader transforms), 4012 mfsrcsnk, 4013 mfmediaengine, 4014 dmime, 4015 dinput, 4016 qdvd (GetDiscID), 4017 wmvcore, 4018 msdmo, 4019 dxdiagn, 4020 wmp, 4021 mciwave (Save no longer deletes the original), 4022 mciqtz32, 4023 mciavi32, 4024 wmphoto, 4025 dpwsockx, 4026 xinputuap, 4027 msacm32, 4028 amstream, 4029 avifil32 (SetInfo), 4030 msvfw32 (MCIWNDM_NEW).
- Judged correct, no change (tests or docs say so; details in the notes): winegstreamer MFT E_NOTIMPLs, evr, quartz fullscreen/IAMDirectSound/filter mapper instance methods, amstream pin methods, dm* track Compose/Join/notification and the dmusic/dmsynth clock Advise* (dmusic test expects E_NOTIMPL), WinRT static-class ActivateInstance, qedit GetCurrentSample.
- Conformance (64/32, this host): dsdmo, qcap, dxva2, qasf, mfreadwrite, mfsrcsnk, msacm32, msdmo, wmvcore, qdvd, wmp, dxdiagn, spatialaudio, amstream, avifil32, msvfw32, winmm mci/wave 0 failures (~120 todo_wine removed, incl. winmm mci.c:796 by 4021). Same failures with the base sources (A/B by swapping the DLL's files back to the series state): mmdevapi capture/render (second shared open of the ALSA device fails, no dmix here), devenum 10, mf transform.c:5127 h264 concat (host GStreamer gives 120 samples). Also environmental: mfplat.c:11222 (D3D11 sharing), dinput hid/device8 todos, dmime.c:4558 (see 2913 note), mf.c:4564/6476 (SAR release / session shutdown timing; 4564 also with the base session.c). Stock Wine 10.0 built from the bare tarball crashes in wineboot on this host, so "base" = tarball + series without these patches.
- Caught by the smoke probes and fixed before commit: mfplat opaque formats broke the wmv decoder DMO type checks (transform.c:6744/6749), mciwave Save deleted the original file, qcap 32-bit crash with a real webcam (WoW64 pUnk).
