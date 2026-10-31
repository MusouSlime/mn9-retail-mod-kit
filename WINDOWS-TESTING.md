# Windows test pass

Target: retail **Mighty No. 9** from Steam, Windows 10/11 x64.

## 0. Setup

- Install the game through Steam (default
  `C:\Program Files (x86)\Steam\steamapps\common\Mighty No. 9`).
- Download `Mn9Patcher-win64.zip` from Releases and extract it.
- Close the game before every Apply/Restore.
- Keep `MN9Game\Logs\Launch.log` after each run.

## 1. Detect / browse

- [ ] **Detect** finds the Steam install.
- [ ] **Browse** works when the game is in a non-default library.
- [ ] Wrong folder is rejected with "Not a Mighty No. 9 install."

## 2. Maximum, fullscreen

- [ ] Select **1920x1080**, tick **Fullscreen**, **VSync**; apply.
- [ ] `MN9Game\Config\MN9SystemSettings.ini` (or
  `SDL2Linux-MN9SystemSettings.ini` on a Linux install) has
  `ResX=1920`, `ResY=1080`, `Fullscreen=True`, `UseVsync=True`.
- [ ] Launch: fullscreen 1920x1080, no black screen, all effects on.
- [ ] Exit and relaunch: settings persist (the game did not overwrite them).

## 3. Native-resolution guard

- [ ] With **Fullscreen** ticked, select **1280x720** and apply.
- [ ] The log says `fullscreen: native display 1920x1080, ignoring preset
  1280x720`, and the INI has `ResX=1920 ResY=1080`.
- [ ] Launch: still fullscreen, no black screen.

## 4. Potato / windowed

- [ ] Select **1280x720**, untick Fullscreen, apply Potato.
- [ ] `DynamicShadows=False`, `ScreenPercentage=75`,
  `MaxSmoothedFrameRate=30`, `PoolSize=96`.
- [ ] Launch: windowed 1280x720, visibly low-end, stable.

## 5. Mouse patch (Windows path)

- [ ] Tick **Free the mouse**, apply: `Binaries\Win64\SDL2.dll.orig` is created.
- [ ] Launch: cursor is not grabbed/hidden.
- [ ] Untick, apply: `SDL2.dll` still exists and the game launches; the revert
  only restores the patched exports.
- [ ] Re-tick and launch again: still works.

## 6. Save backups

- [ ] **Backup saves** creates
  `<game>\.mn9patcher\saves\<timestamp>_manual\manifest.json`.
- [ ] Play a little, exit, **Backup saves** again.
- [ ] **Restore selected** restores the chosen snapshot and first writes a
  `before-restore` snapshot.
- [ ] Manifest hashes match the files (`sha256` field).
- [ ] `SaveData` files are still writable by the game after restore.

## 7. Restore original

- [ ] Apply any profile, then **Restore original**.
- [ ] INIs are back to the baseline; `SystemData.dat` is writable again.
- [ ] Launch: default retail settings.

## 8. Safe Mode

- [ ] Apply, then launch with
  `Mn9Patcher.exe --cli --game-dir "..." --launch`.
- [ ] Kill the game abnormally (Task Manager) → exit marker is written under
  `%USERPROFILE%\.local\state\mn9-safe-mode\crash` (Git Bash) or the Windows
  equivalent used by the build.
- [ ] Next launch restores baseline + Quality and clears the marker.

## 9. Report

Include:

- Windows build number and GPU/driver.
- Game path and whether it is a fresh install.
- Pass/fail for each section above.
- `Launch.log` tail for any failure.
- Screenshots: fullscreen title, fullscreen stage select, windowed potato.
- Any SmartScreen/antivirus prompt (unsigned exe is expected).

## Known non-issues

- Windows SmartScreen warns about the unsigned exe: **More info → Run anyway**.
- MSAA is off by design; the patcher's MSAA checkbox is disabled/broken on
  Linux and should stay off on Windows for now.
- 4:3 HUD-corner/aspect-fill patches are Linux-only and intentionally absent.
