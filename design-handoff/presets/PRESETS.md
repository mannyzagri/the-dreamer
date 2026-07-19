# PRESETS.md — factory preset bank ownership & loading (processor layer)

Presets are PARAMETER-VALUE SETS. They belong to the AudioProcessor /
state layer — NOT to the GUI (a GUI rewrite must not be able to lose them)
and NOT to the DSP algorithm code (filters/oscillators don't know what a
preset is). This doc tells the DSP/VM Claude to make the processor own the
factory bank so it ships inside the plugin and survives any GUI change.

## Asset
- `presets.json` — 47 factory presets (bank A 23 + bank B 24), each a full
  map of parameter-ID -> value using the canonical IDs from PARAMETERS.md /
  DSP_BUILD.md §9. Per-tone params suffixed _a.._d. ~220 params per preset.
  Values are normalized 0..1 unless PARAMETERS.md gives the param a real
  range (e.g. fine -50..+50c, hit_pitchtrim -24..+24) — in that case the
  stored value is already in that range. Booleans stored as true/false;
  combos as integer indices.

## Build-time embedding
- Add presets.json to the JUCE target as BinaryData (juce_add_binary_data),
  so it compiles into the plugin — no external file at runtime.
- Do NOT commit generated headers; commit presets.json as the source asset.

## Processor implementation
On the AudioProcessor:
1. At construction, parse BinaryData::presets_json into an in-memory
   vector<Preset> { juce::String name, category; std::map<String,var> values; }.
2. Implement the standard program interface so hosts see the bank too:
     int getNumPrograms()            -> presets.size() (min 1)
     int getCurrentProgram()         -> currentProgram
     void setCurrentProgram(int i)   -> applyPreset(i); currentProgram=i
     const String getProgramName(int)-> presets[i].name
3. applyPreset(int i): for each (id,value) in presets[i].values, write into
   the APVTS:
     - float/normalized params: set via
       apvts.getParameter(id)->setValueNotifyingHost(normalized) OR set the
       APVTS value and let listeners update. Use the param's range to
       convert if the stored value is in real units.
     - choice/bool params: set the choice index / bool.
   Wrap in a single undo-transaction / suspend audio if needed; changing
   ~220 params at once should be one atomic state swap. Simplest robust
   path: build a ValueTree matching apvts state and call
   apvts.replaceState(tree) — one call, host learns all changes.
4. Expose applyPreset(index) and getPresetList() publicly so ANY editor
   (the JS GUI, a future JUCE editor, or the host program menu) can drive
   preset changes through the processor. The GUI becomes a viewer: it asks
   the processor for names and calls setCurrentProgram — it does NOT hold
   the preset data.

## User presets (optional, recommended)
- Keep the same JSON shape for user presets saved to a user directory
  (e.g. userAppData/The Dreamer/Presets/*.json). Factory bank is read-only
  (BinaryData); user bank is read/write. getNumPrograms can concatenate
  factory + user if you want them in the host menu, or keep user presets to
  a browser only. Author's choice; not required for v1.

## Why this fixes the reported problem
The bank previously lived only as JS arrays in app.js, so a GUI rebuild
could overwrite it and a non-JS editor couldn't see it at all. Moving the
data to presets.json (embedded, processor-owned) makes presets independent
of the GUI implementation. The GUI references presets by index/name only.

## Validation
- Load each of the 47 presets in turn offline; assert no NaN/Inf in output
  and that every parameter ID in the preset exists in the APVTS (a missing
  ID = a preset/param drift bug — fail loudly, don't silently ignore).
- Round-trip: getStateInformation after applyPreset(i), reload, compare.
