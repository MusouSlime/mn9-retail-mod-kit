# 4:3 / 16:10 aspect fix

Status: **4:3 works**. The engine's `DefaultAspectRatio` is patched to the
window aspect and the Scaleform movie scale mode is forced to
`SM_ExactFit`, so a 1280x960 window fills completely and the menu is not
cropped.

**16:10 is still letterboxed**: the game treats anything in the "wide" bucket
(>= 1.5) as 16:9 here, so a 1280x800 window still gets two 40 px bars. 4:3 is
in the non-wide bucket and works.

## What was found

- `Default__MN9GamePlayerCamera.DefaultAspectRatio` is `1.77778` (16:9).
  Patching it to `1.33333` makes a 4:3 window use the full viewport.
- The title/options/stage-select screens are Scaleform. Their movie scale
  mode defaults to `SM_ShowAll` (letterbox), and the game never calls
  `SetViewScaleMode`.
- `Scaleform::GFx::MovieImpl::SetViewport` stores the scale mode at
  `MovieImpl+0xe8`; forcing it to `SM_ExactFit` (2) makes the GFx movie fill
  the viewport without cropping. `SM_NoBorder` (3) also fills but crops the
  overflowing edges (the 4:3 title screen lost the copyright line).
- The 4:3 stretch is mild because the UI is nearly square-safe; the menus stay
  fully visible.

## Usage

Close the game first.

```sh
# 4:3
python3 mods/aspect_fix/apply_aspect.py \
  --game-dir "/path/to/Mighty No. 9" --width 1280 --height 960

# back to 16:9
python3 mods/aspect_fix/apply_aspect.py \
  --game-dir "/path/to/Mighty No. 9" --width 1920 --height 1080 --no-fill

# check / apply / revert the binary half directly
python3 mods/aspect_fix/patch_movie_scale.py check|apply|revert
```

`apply_aspect.py`:

1. patches `Default__MN9GamePlayerCamera.DefaultAspectRatio` in `MN9Game.upk`
   to `width / height` (fixed offset `0x78E292`, with a scan fallback),
2. applies `patch_movie_scale.py` for non-16:9 (or `--fill`),
3. reverts it for 16:9 unless `--fill` is explicitly requested.

The **patcher applies this automatically** from its resolution preset:
4:3 and 5:4 selections set the aspect and install the `SM_ExactFit` cave,
16:9 reverts the cave. `Restore settings` puts the upk back to 16:9 and removes
the cave. A mouse-patch revert no longer touches unrelated binary bytes.

`patch_movie_scale.py` is Linux-x86_64 only. On Windows the UI will still
letterbox until the PE equivalent is added.

## Notes

- `SM_ExactFit` stretches the 16:9 UI to the window aspect. To fill + crop
  instead, change `$2` to `$3` in `movie_scale_cave.S`, rebuild
  `movie_scale_cave.bin`, and re-apply.
- `patch_gfx_scale.py` (the earlier `UGFxMoviePlayer::SetViewport` stub) turned
  out not to be the active path; `patch_movie_scale.py` is the one that fills
  the screen. Keep it only if you are experimenting.
