# New `TOtherOptionData` field (Cursor Hold)

Status: **attempted, blocked at the cooked linker**. The property itself can be
added and the package loads; as soon as cooked bytecode references the new
property, `ULinkerLoad::CreateExport` crashes while preloading `MN9Game.upk`.
The retail install has been restored to the working Cursor Hold build.

What was verified:

- a fifth `UBoolProperty` can be linked after `bDecideCancelButtonNormal`
  (offset 9, struct size still 16),
- `ConvertOtherOptionData` / `CreateOtherOptionDataObj` can be patched to
  reference it and decompile correctly,
- a no-op bytecode insertion boots,
- duplicating the existing `bDecideCancelButtonNormal` assignment block also
  crashes at load, so the failure is in the cooked bytecode/linker preload, not
  in the new property name or field itself.

The next viable route is the sidecar/config field or a linker-order workaround
(e.g. pre-creating the property before the UI class is loaded).

## Why this is the template

The in-game Other rows currently use only the four retail fields:

| `TOtherOptionData` | type | in-memory offset |
| --- | --- | --- |
| `Language` | int32 | 0 |
| `VoiceLanguage` | int32 | 4 |
| `bDecideCancelButtonNormal` | bool | 8 |
| *(padding)* | | 9–11 |
| `RestNum` | int32 | 12 |

`sizeof(TOtherOptionData)` is **16** bytes. The property chain order is
`Language → VoiceLanguage → bDecideCancelButtonNormal → RestNum`.

The native thunk confirms the ABI:

```
execApplyOtherOptionData:
    sub $0x10,%rsp          ; 16-byte local
    ... StepCompiledIn ...
    call ApplyOtherOptionData(FTOtherOptionData)  ; rsi/rdx = 16 bytes
```

Adding a bool **after `bDecideCancelButtonNormal`** puts it at offset 9, inside
the existing padding. `RestNum` stays at 12 and the struct size stays 16, so:

- no UFunction parameter-size change,
- no native `FTOtherOptionData` ABI change,
- no change to the offsets of existing fields.

That is the key to making this safe.

## Required edits

1. **Property export**
   - clone the `bDecideCancelButtonNormal` `UBoolProperty` export
     (`ArrayDim=1`, same property flags),
   - outer `TOtherOptionData`,
   - `NextField = RestNum`,
   - link `bDecideCancelButtonNormal.NextField` to the new property,
   - keep `PropertiesSize = 16` (the bool fits in the padding).
   - Use a unique existing FName (or append one) so tagged serialization does
     not collide with another `Other` field.
2. **AS conversion**
   - `MN9UIOption.ConvertOtherOptionData`: add
     `OtherOptionData.bCursorHold = OtherOptionDataObj.GetBool("bCursorHold");`
   - `MN9UIOption.CreateOtherOptionDataObj`: add
     `SetBool("bCursorHold", OtherOptionData.bCursorHold);`
   - Both are cooked UScript bytecode; insert a cloned bool-property block and
     resize the function exports (the same export-shifting machinery as
     `pack_option.py`).
3. **AS row**
   - `InitOtherData`: `bOn: !_loc3_.bCursorHold` (or `bOn: _loc3_.bCursorHold`
     depending on polarity),
   - apply object: `bCursorHold = !bOn`,
   - keep the native SDL callback as the immediate apply action.
4. **Native persistence**
   - `ApplyOtherOptionData` / `LoadOtherOptionDataFromIniFile` already copy the
     16-byte struct, so byte 9 travels with it,
   - only patch them if they zero/reinitialize the struct; otherwise the byte
     survives the INI load/save round-trip.

## Tooling needed

- `add_other_field.py`: append a `UBoolProperty` export + optional name,
  link it after `bDecideCancelButtonNormal`, and update `ExportOffset`-relative
  offsets.
- `patch_bool_member.py`: clone the bool-member bytecode block in
  `ConvertOtherOptionData` and `CreateOtherOptionDataObj`.
- Reuse `pack_option.py` / `pack_swf.py` offset-shifting logic.

## Backups

Working install backed up before any surgery:

- `work/backups/MN9Game_cursorhold_working.upk`
- `work/backups/MN9Game_cursorhold_working.bin`
- `work/backups/BootGame_cursorhold_working.upk`
- `work/backups/SystemData_cursorhold_working.dat`

## Next step

Build `add_other_field.py` against a **copy** of `MN9Game.upk` and verify with
UELib that `TOtherOptionData` now has five fields in the order
`Language, VoiceLanguage, bDecideCancelButtonNormal, bCursorHold, RestNum`
with `PropertiesSize` still 16. Do not install until the copy loads in umodel
and the struct round-trips.
