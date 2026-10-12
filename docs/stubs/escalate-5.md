# Drafter 5 escalations

- windows.media.mediacontrol: the 8 `@ stub` exports (AudioStateMonitor creators) need a windows.media.audio IDL that Wine lacks; VideoProperties / ImageProperties / CopyFromFileAsync need include/windows.media.idl changes (outside the drafter's DLL group).
- windows.media.devices: DefaultAudioCapture/RenderDeviceChanged are stored but never raised (needs an IMMNotificationClient and event-args objects).
- mfplat: MFCreateMediaBufferWrapper's initial current length (set to the wrapper's length) and MFPluginControl_* (registry-backed plugin store) are unverified against Windows.
- winmm: midiConnect/midiDisconnect, MMIO_GETTEMP, global IO procs left alone (no documented driver contract).
- mf: MFCreateDeviceSource / MFEnumDeviceSources for video capture would need a V4L2 capture source in winegstreamer (not present in Wine 10.0).

## windows.media.speech (agent notes)
> # windows.media.speech: items needing a decision or an engine (drafter 5)
> 
> 1. No speech recognition engine. SpeechRecognizer never produces text: RecognizeAsync/RecognizeWithUIAsync complete with
>    status TimeoutExceeded, the continuous session's ResultGenerated/Completed events and RecognitionQualityDegrading and
>    HypothesisGenerated never fire, AutoStopSilenceTimeout is stored but nothing times out. A real engine (or a
>    deliberate decision to fail Create) is a cross-DLL/design call.
>    Also: SemanticInterpretation of a result is NULL (needs its own class); SpeechContinuousRecognitionCompletedEventArgs and
>    ResultGenerated args classes do not exist, so the session raises no Completed after Stop/Cancel.
> 2. No synthesis engine / no voices. AllVoices is empty, SynthesizeTextToStream* returns an empty stream, DefaultVoice and an
>    unset Voice return HRESULT_FROM_WIN32(ERROR_NOT_FOUND) (Windows always has a default voice). Choice for the integrator:
>    keep the error, or add a stub VoiceInformation. The two remaining todo_wine in tests/speech.c depend on this.
> 3. Supported/SystemSpeechLanguage: system language follows the user's locale although no language is "installed";
>    TrySetSystemSpeechLanguageAsync always reports FALSE.
> 4. Values taken from memory of the docs, unverified: 20 s AutoStopSilenceTimeout default, recognizer timeout defaults
>    (5 s / 150 ms / 0), IsEnabled default TRUE, synthesizer option ranges rejected with E_INVALIDARG,
>    per-owner IAsyncInfo id counters (derived from tests/speech.c expectations).
> 5. recognizer_factory_Create fails when no audio capture device exists (pre-existing); Windows creates the recognizer anyway.
>    The gates SKIP the recognizer part in that case.

## windows.media.* (agent notes)
> # Escalations: windows.media.* (drafter 5, patches 2820-2822)
> 
> 1. windows.media.mediacontrol: the 8 spec-stub exports (CreateCaptureAudioStateMonitor,
>    ...ForCategory, ...ForCategoryAndDeviceId, ...ForCategoryAndDeviceRole, and the four
>    Render variants) are left as `@ stub`. They are the entry points behind the
>    Windows.Media.Audio.AudioStateMonitor statics, but Wine's include/ has no
>    windows.media.audio.idl (no IAudioStateMonitor, no IID, no MediaCategory), and the
>    flat signatures are not documented. Needs the IDL added under include/ (outside my
>    directories) and then an AudioStateMonitor object (SoundLevel = Full, SoundLevelChanged
>    event), plus an activation factory for Windows.Media.Audio.AudioStateMonitor.
> 2. SystemMediaTransportControlsDisplayUpdater::VideoProperties, ::ImageProperties and
>    ::CopyFromFileAsync stay E_NOTIMPL with a FIXME: include/windows.media.idl only
>    forward-declares IVideoDisplayProperties / IImageDisplayProperties (no vtbl, no IID),
>    so they cannot be implemented from the mediacontrol directory. CopyFromFileAsync also
>    needs Windows.Storage file metadata reading (a real subsystem).
> 3. windows.media.devices: DefaultAudioCapture/RenderDeviceChanged handlers are stored and
>    released correctly, but never raised (that needs an IMMNotificationClient on the
>    enumerator and DefaultAudio*ChangedEventArgs objects, Id + Role); not testable in the
>    gate without a second audio endpoint.
> 4. windows.media: the 18 audit items in captions.c were already implemented by patch 1700;
>    only the two remaining FIXMEs (factory QueryInterface, DllGetClassObject) were left.
> 5. Note for the model patch 2800: sg_class_name-style calls there use ARRAY_SIZE() of the
>    RuntimeClass_ WCHAR arrays, which counts the terminating NUL, so the HSTRING length is
>    one too long (the probe compares with wcscmp so it does not notice). Check
>    WindowsGetStringLen there.

## dsound
- IKsPropertySet::Set (DirectSound private set) cannot be reached from a client: CLSID_DirectSoundPrivate is not registered in Wine, so the returned E_PROP_ID_UNSUPPORTED is untested.

## (resolved) 2970-2972 were unfinished at the first session limit; they now have gates and mutants and are in patches/series (2972: the bundled FAudio has no wave support yet, so PrepareInMemoryWave/PrepareStreamingWave return E_FAIL)
Old note:
- patches/sg/2970-dmsynth-stubs, 2971-dmusic-ports, 2972-xact-cue-wave: written, apply in order on a pristine tree, conformance tests pass (dmsynth, dmusic, xact3: 0 failures), but only 2970 has a gate and NO mutants were run for any of the three. They are left out of patches/series until gates and mutants exist (see patches/notes/2970-2972.txt and /var/tmp/drafter-5/agent-dmusic-ESCALATE.md on the drafter machine).

## Round 5 (2026-10-10)
- dmstyle: Play for style/motif/audition tracks needs a pattern-playback engine; audition/motif Load formats undocumented; chord-map legacy methods stay E_NOTIMPL.
- dmcompos: ComposeSegmentFromTemplate/FromShape, ComposeTransition, AutoTransition, ComposeTemplateFromShape, signpost Compose need a chord-progression engine; template Load undocumented.
- evr: nine `@ stub` exports (MFConvertColorInfoFromDXVA, FP16 array converters, MFCreateVideoMediaTypeFrom*, MFCreateVideoOTA, MFCreateVideoPresenter2, MFGetUncompressedVideoFormat, MFInitVideoFormat) have no test ground truth.
- wmp: collection/application getters and buffering/download progress have no test ground truth.
- quartz: VideoWindow_put_FullScreenMode FIXME sits on an implemented path (drop FIXME); several filtergraph/filtermapper FIXME candidates untested; AddSourceFilter/collections need control.idl interfaces.
- mfplat: remaining `@ stub` exports and MFPluginControl have no tests.
- dinput: top-level HID usages other than gamepad/joystick are ignored (no test shows expected enumeration/type). 2878's MOUSE_VIRTUAL_DESKTOP absolute-motion code rests on documentation only (no test or gate; Wine never emits absolute raw mouse); integrator may keep or revert to FIXME.
- quartz: all 17 remaining suites run clean with no stale todo_wine; remaining todo_wine need new features (IPersistPropertyBag/IDirectSound3DBuffer on DSoundRender, IMediaPosition on AVI splitter output pin, splitter seeking, EC_COMPLETE timing, VMR7/9).
- dmscript: AutoImp*/SourceCodeLoader helper classes: no method names/DISPIDs/IIDs available; create_unimpl_instance keeps its FIXME.
- dmcompos: chord map chdt/chpl/spsq layouts undocumented (progressions are diatonic triads from the chord map scale); chord-map track Compose stays E_NOTIMPL (test says so); template object Load undocumented.
- evr: MFCreateVideoPresenter2 / MFCreateVideoOTA need a signature from an allowed source (wrapping the existing presenter creation is small once known).

## 2026-10-12 (group 1, 4000-4030)
- avifil32: AVIFILE_SaveFile rewrites the header assuming AVI_HEADERSIZE of room before 'movi'; saving an existing AVI with a smaller header (any write to a RW-opened file) produced a file starting with idx1. 4029's SetInfo therefore does not mark the file dirty. Needs a save path that moves 'movi' or pads with JUNK.
- winegstreamer media_sink.c sink_class_factory_create_media_sink writes *out without a NULL check (crashes on NULL out).
- IIDs written from memory, unverified against SDK metadata: IImageDisplayProperties/IVideoDisplayProperties(2) (4008), IMFTranscodeProfile/IMFPMPServer (4010).
- qdvd GetDiscID follows the publicly described CRC-64 disc id scheme; equality with Windows' value for a real disc unverified.
- Not done (need engines, dialogs or undocumented signatures): mf ASF*/transcode topology/aggregate source/credential cache/proxy locator, dxva2 DXVAHD_CreateDevice, msacm32 acmFilterChoose, mciavi32 Configure, avifil32 Delete/clipboard/format changes, msvfw32 OPENINTERFACE/preview pane/ICMThunk/StretchDIB, mfplay FrameStep, sapi line ids, dplayx DPSYS_SECUREMESSAGE, mf3216, winmm mmioInstallIOProc16, dmusic32 legacy exports, dmsynth articulation extensions.
- winmm:mci 64-bit hung 3 times after mci.c:1274 (auto-open / mpegvideo part) under heavy parallel load with group 1 applied; 6 later runs passed (0 hangs in 4 base runs, run without load). Cause not found; watch it in the integrator's runs (notes/4021.txt).

### Review of the group 1 escalations (2026-10-12, reviewer pass by the drafter)
- avifil32 save: RESOLVED by 4031. Root cause was dwNextFramePos = 0 after loading (any write or save of an existing AVI went to offset 0), plus no room handling for a grown header. Open question left: the move needed mmioFlush(MMIO_EMPTYBUF) around it, or stale mmio buffer bytes were written back over the moved data; a winmm mmio buffering bug is possible and not investigated.
- winegstreamer NULL out: RESOLVED by 4032.
- IIDs: RESOLVED, all five verified: IImageDisplayProperties cd0bc7ef-54e7-411f-9933-f0e98b0a96d2, IVideoDisplayProperties 5609fdb1-5d2d-4872-8170-45dee5bc2f5c, IVideoDisplayProperties2 b410e1ce-ab52-41ab-a486-cc10fab152f9 (windows-rs Windows/Media/mod.rs), IMFPMPServer 994e23af-1cc2-493c-b9fa-46f1cb040fa4 (windows-rs metadata/win32/mfidl.rdl), IMFTranscodeProfile 4adfdba3-7ab0-4953-a62b-461e7ff3da1e (also in third-party MF bindings).
- qdvd GetDiscID: ACCEPTED as is. A stable non-zero id is what callers use it for (caching per disc); E_NOTIMPL broke them. Equality with Windows' value stays unverified (notes/4016.txt).
- winmm:mci hang: ACCEPTED (not attributable to 4021). Load A/B, 3 parallel runs each: base 0 hangs, 4021 0 hangs; both sides fail differently depending on which run gets the one ALSA device (base: open MCIERR_INTERNAL / record DEVICE_OPEN; 4021: play fails at once, then pause NONAPPLICABLE). Quiet runs with 4021: 6/6 pass. 4021 only changes Save (copy instead of rename) and adds a flag; the earlier hangs were all in the auto-open/play part under load. Keep watching winmm:mci in integration.
- "Not done" list: ACCEPTED as escalations; none is a small gap (dialogs, engines, undocumented signatures).

## 2026-10-12 evening (round 3)
- msvcp140 conformance: the std::filesystem tests fail 12 (x64) / 16 (x86) on this host with or without round 3 (wine_test_dir not removable after the symlink checks, CreateSymbolicLinkW failing in 32-bit). Likely our series' symlink support or the /var/tmp filesystem; not investigated.
- My test runner ran Wine tests with the shell's working directory, which is the person's project folder (/var/lib/stained-glass/.../Stained-Glass-OS) in this session; tests that create files in the current directory (winmm mci tempfile.wav, msvcp filesystem) may have created and removed files there during round 2 (folder mtime 13:36, nothing left behind). Fixed: tests now run in a scratch work directory. Gates were not affected (they cd into their own temp dir).
- __GetPlatformExceptionInfo returns no WinRT information for C++/CX exceptions (layout undocumented); 4501 FIXMEs then.
