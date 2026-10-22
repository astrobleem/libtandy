# JAWS PSG clip samples

These two example clips port only the pulse-2 register periods and volume
levels observed in the local `jaws-native` Milestone 2 source and captures.
They are separate GPL-3.0-only data assets; `COPYING` carries that license.
The `lt_sfx` player and libtandy audio modules remain MIT licensed.

The source reference is JAWS commit
`b3660e0f5ccad401601bcb1ec30aae367b25b191`, files `src/jaws_sfx.c/.h` and
`private/generated/event_sfx.h`. The provenance notes are
`docs/M2-AUDIO.md`, `docs/M2-EVENT-SFX.md`, and
`private/generated/event_sfx.provenance.json` in that source tree. Cue `$0B`
is the harpoon shot, triggered at `$9C66`; cue `$15` is the successful conch
pickup at `$A674`. Both use the source's special voice 4, route 1, which writes
the NES pulse-2 state. The native clips use PSG tone channel 1.

The source tick is one 60.0988 Hz game frame, stored exactly as the reduced
rational rate `150247/2500` in each clip. Both have provenance ID one so an
editor round trip can retain a simple source-derived notice. The harpoon clip includes the
11 output frames owned by the cue, beginning one frame after the cue call;
the repeated final period is represented by holding its previous event. The
conch clip covers the 65 owned frames from source frames 1373 through 1437.
Its 32 period/level changes occur every two frames, with attenuation levels
1, 5, 9, and 13 across the four phrases. The PSG period is the captured NES
timer plus one under the documented 3,579,546 Hz PSG clock assumption. The
numeric attenuation mapping is not a loudness-equivalence claim.

The conch sample data derives from the separately GPL-marked JAWS event table;
its GPL notice is retained here. The canonical binary presets
`harpoon.ltsfx` and `conch.ltsfx` can be loaded directly by a clip editor;
`export_ltsfx.c` rebuilds them from these C arrays using libtandy's checked
serializer. Edits retain provenance ID one and the same source rate unless
the editor author changes them. No ROM, emulator trace, game graphics, or
audio recording is included. The host fixture compares output bytes and tick
boundaries with literal expectations for both source-owned windows.

The canonical files are 80 bytes (`harpoon.ltsfx`, SHA-256
`1c8b0abce3eaf6f19c78d9006027389bd9de9b6c89a894421f0a5cb5ce394a75`) and
212 bytes (`conch.ltsfx`, SHA-256
`4872bd0c006ba39f1bf75a7a1453233b5dc54a35b1653aaa608259f41f583fe5`).
