# In-game option rows (retail GFx)

Status: **working**. The Other page has a real **Cursor Hold** row
(default **ON**), the original **Lives Remaining** row is intact, and the
title menu no longer crashes on the missing DLC title-call packages.

## Cursor Hold

- **ON** — cursor held and hidden (the state the game starts in).
- **OFF** — window grab released and cursor shown.

The toggle is applied immediately in-game through a native callback. It is
**session-only**: the row is not part of `TOtherOptionData`, so it resets to ON
when the Options movie is reopened or the game restarts.

## What the UI actually is

- Package: `MN9Game.upk`
- GFx object: `SwfMovie Option` (also `SwfMovie GameOption` for the pause menu)
- Extract with:

```sh
umodel -export -path=<cooked> -out=/tmp/swf_opt -3rdparty -obj=Option MN9Game.upk
# -> /tmp/swf_opt/MN9Game/SwfMovie/Option.gfx
```

- FFDec script export:

```sh
java -jar ffdec-cli.jar -export script /tmp/opt_scripts Option.gfx
```

- The Other page is built in `scripts/frame_6/DoAction.as` → `InitOtherData()`.
- Rows are attached at runtime by `InitOptionData()` in
  `scripts/frame_1/DoAction.as`:

```as
OptionNameMC = in_ParentMC.attachMovie("buttonLine", MCName, depth * 2);
```

so a new `MCName` needs no timeline clip, only ActionScript.

## Final row setup

`InitOtherData()` keeps the retail rows (Calibration, Language, Voice
Language, Decide/Cancel, **RestNum/Lives Remaining**) and appends one row:

```as
_loc2_ = AddOptionDataOnOff(_loc1_.length, "mn9mouse",
   {bOn:true, bOnDefault:true, Name:"Cursor Hold", GuideInfoArray:undefined});
_loc2_.UpdateTextFunction = UpdateTextFunctionOnOff;
_loc1_.push(_loc2_);
```

`etc3` (Lives Remaining) is left as the original list row. Do **not** rename
or reuse it: a row whose `MCName` matches an authored clip reuses that clip as
`OptionNameMC`, while the on/off value clip is attached under the same name and
overwrites it (that produced the earlier garbled label).

### Label

`buttonLine`'s text field contains a placeholder glyph in the SWF. The game's
C++ only fills the labels it knows (`window.etc.etc1..4`,
`window.etc.calibration`), so a new row has to fill its own text in AS.

In `InitOptionData()`, right after `InitFunction()`:

```as
if(_loc1_.MCName == "mn9mouse" && _loc1_.OptionNameMC != undefined)
{
   _loc1_.OptionNameMC.text.text.text = "Cursor Hold";
}
```

The triple `.text` matters: `OptionNameMC.text.text` is the `TextField`
(the same object `CommonSetFont()` receives); assigning to `.text.text`
replaces the field object and leaves the placeholder visible.

No `LocalizedText*` string patch is needed. `LocalizedTextRestNum[0]` stays
`"Lives Remaining"`.

## Native callback

### Why free-form names do not work

`flash.external.ExternalInterface.call("Mn9FreeMouseOn")` is silently dropped:
Scaleform only forwards names that exist as **UFunctions** on the movie player.
The host callback never sees the call (verified by logging every name that
reaches `FGFxExternalInterface::Callback`). Names such as `GetInitData`,
`ASSetText`, `PlaySECursor` do reach it.

The row therefore calls two harmless `MN9GFxMoviePlayer` wrappers that are
registered as UFunctions:

| AS call | Native action |
| --- | --- |
| `ASSetText` | hold: `SDL_SetWindowGrab(win, 1)`, `SDL_ShowCursor(0)` |
| `ASInputSetInputEnable` | free: `SDL_SetWindowGrab(win, 0)`, `SDL_ShowCursor(1)` |

The original wrapper actions are skipped (the dispatcher returns "not found").
No AS code calls these two wrapper names as host callbacks, so the UI is
unaffected.

### Code cave

`patch_free_mouse.py` replaces the `UObject::FindFunction` call at
`0x1b24387` inside `FGFxExternalInterface::Callback` with a jump to a code cave
in the 0x2c8-byte zero gap between the RX and RW LOAD segments
(VA `0x2b8e628`). The RX segment is extended to cover it; no file bytes are
shifted.

The cave:

1. `strcmp`s the callback name against `ASSetText` / `ASInputSetInputEnable`
2. performs the SDL grab/cursor calls (or falls through to the original
   `FindFunction` for every other name)
3. prints `[mn9] Cursor Hold: on/off` to stdout for diagnostics

**Stack alignment:** the cave does `push rdi; push rsi; sub $8, rsp` so every
call is ABI-aligned. Calling `SDL_SetWindowGrab()` without the `sub $8` crashes
inside `XSync` (`xcb_wait_for_reply`, SIGSEGV) even though the surrounding code
looks fine.

```sh
python3 patch_free_mouse.py check   # original / patched
python3 patch_free_mouse.py apply
python3 patch_free_mouse.py revert
```

The cave binary is `free_mouse_cave.bin`, built from `free_mouse_cave.S`.

## Rebuild the Option GFX

The two full frame scripts (`Option_frame_1_free_mouse.as`,
`Option_frame_6_free_mouse.as`) are decompiled from the retail movie and stay
in the personal build; they are not committed. Only `prototype.as` (the
authored row snippets) is in this repo. Start from the retail `Option.gfx` and
your local copies of both scripts:

```sh
java -jar ffdec-cli.jar -replace Option.gfx /tmp/Option_1.gfx \
  /frame_6/DoAction Option_frame_6_free_mouse.as
java -jar ffdec-cli.jar -replace /tmp/Option_1.gfx Option_free_mouse.gfx \
  /frame_1/DoAction Option_frame_1_free_mouse.as
```

The leading slash in `/frame_6/DoAction` is required.

The checked-in `Option_free_mouse.gfx` is the exact payload packed into
`MN9Game.upk` (verified by extracting it back with umodel).

## Packing the GFX into the UPK

The `Option` export is a `UGFxRawData` tagged property:

```
<header> <int32 RawData array size> "GFX\x0f" <gfx bytes> <trailing props>
```

- RawData array size = inner GFX size + 4
- inner GFX size at `magic - 4`
- RawData array size at `magic - 12`
- GFX payload starts at export + 157 in the retail package

A larger GFX shifts every later export, so `pack_option.py` rewrites the file
and all later export offsets/sizes:

```sh
Mn9Exports MN9Game.upk /tmp/exports.jsonl
python3 pack_option.py MN9Game.upk /tmp/exports.jsonl Option_free_mouse.gfx /tmp/MN9Game.new.upk
```

`Mn9Exports` is `tools/ue3/Mn9Exports` (UELib). Verify with:

```sh
umodel -export -path=/tmp -out=/tmp/verify -3rdparty -obj=Option MN9Game.new.upk
cmp /tmp/verify/MN9Game/SwfMovie/Option.gfx Option_free_mouse.gfx
```

`install.sh` runs the manifest dump, pack, and title-call patch in one step.

## Title-menu crash (DLC title calls)

`MN9UITitleMenu.LoadDLC()` is compiled as three no-argument calls:

```as
CheckDLCGoldenHero();
CheckDLCRetroHero();
AddDLCTitleCall();
```

`AddDLCTitleCall()` loops `DLCTitleCallPl03..Pl07` and calls `LoadPackageFile`.
A retail install only ships Pl03, and loading Pl06 hard-crashes the game about
20 s after boot (SIGSEGV in `ULinkerLoad::CreateExport`).

Patch the third call's name index from `AddDLCTitleCall` (456) to the empty
function `CheckDLCRay` (8010):

```sh
python3 patch_titlecall.py apply MN9Game.upk
```

The call becomes a no-op, no export offsets change, and `AddDLCTitleCall`
keeps its original body. This also skips the title-call voice clips.

Note: an earlier attempt replaced the first bytes of `AddDLCTitleCall` with
`return;`. The loader still parses the whole serialized script and crashed in
`CreateExport`; do not short-circuit the function body in place.

## Boot movie skip

`BootGame.upk` is a fully-compressed package containing `SwfMovie Boot`, the
publisher/comcept/Inti/UE/Scaleform/NVIDIA/notice/caution logo timeline
(~19 s before the title).

`PackageUnpack` decompresses it, then `boot_skip_frame_1.as` adds a one-second
timer to frame 1 that calls `ExternalInterface.call("EndPlay")`:

```as
Mn9BootTimer = setInterval(this, "Mn9BootEnd", 1000);
```

`EndPlay` switches to the title level. Title entry drops from ~19 s to ~14 s.
A direct call in frame 1 is too early and stalls the boot; keep the 1 s delay.

Rebuild `BootGame.upk` with `pack_swf.py`:

```sh
decompress BootGame.upk -> work/upk/BootGame.upk
Mn9Exports work/upk/BootGame.upk /tmp/boot_exports.jsonl
python3 pack_swf.py work/upk/BootGame.upk /tmp/boot_exports.jsonl Boot_skip.gfx \
  work/upk/BootGame_skip.upk Boot
```

The retail-derived `BootGame.upk` stays local under `work/`.

## Movies / boot

- `bForceNoMovies=True` in both engine INIs.
- `LoadMapMovies=Loading.Loading` removed from both engine INIs, so the
  loading-screen movie is skipped. Map streaming still takes the same time.
- The `OpDemo` intro **stage** is intentionally untouched.

## Remaining

- Cursor Hold is session-only. Persisting it needs a new `TOtherOptionData`
  field or a config write from the native callback.
- `ApplyNewOtherOptionData` still ignores the row; no crash, but nothing is
  saved.
