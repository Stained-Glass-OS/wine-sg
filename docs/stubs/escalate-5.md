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
