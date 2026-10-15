# Drafter 7 progress

## Round 4: UIA (branch stubs-7d, from origin/fast-b)

Patch 5800 (uiautomationcore, one patch because the pieces share the property tables, the pattern call protocol and the test provider): notes in patches/notes/5800.txt, verdicts in docs/stubs/triage-7r4.tsv.

Done (implemented, with the new conformance unit dlls/uiautomationcore/tests/patterns.c and mutants):
- client COM objects for every pattern, current and cached, including the text family and text ranges;
- typed properties for all ids, element and element-array properties across threads, default values (GetCurrent/CachedPropertyValueEx), substring conditions;
- IUIAutomation6 proxy factory entries and mapping, programmatic names, PollForPotentialSupported*, ShowContextMenu, GetCurrentMetadataValue, UiaLookupId for text attribute/annotation/style/changes GUIDs;
- UiaRaiseActiveTextPositionChangedEvent and the COM handler, per-window UiaEventRemoveWindow, nested node cloning and win-event forward, node stand-in SetFocus/BoundingRectangle, UiaGetUpdatedCache scope validation;
- the IAccessible bridge's IDispatch (GetTypeInfo, GetIDsOfNames, Invoke through oleacc's type library) and enumerator Clone;
- a dangling CLSID pointer in the class factory (CoCreateInstance of CUIAutomation could fail with E_NOINTERFACE).

Triaged no-ground-truth (reason in the tsv): MSAA role/state to pattern mapping (msaa_provider_GetPatternProvider), contents of the default proxy factory table, the landmark GUIDs of UiaLookupId, property conditions on Double/Point/Rect/Element values, UiaProviderFromIAccessible undefined flag bits.

Conformance: full uiautomationcore suite (patterns 1636 checks, uiautomation 8278 checks), 64- and 32-bit, no new failures against the unmodified base (outputs compared with cmpsuite.py).
