# Single-controller fix (Steam Input duplicate device)

## Symptom

The game log shows two joysticks at startup:

```
Input created joystick device
Input created joystick device
Device Plugged In [0]
Game Controller Setup: mapping[03000000de280000ff11000001000000,Valve Streaming Gamepad,...]
Device Plugged In [1]
Game Controller Setup: mapping[00000000000000000000000000000000,XInput Controller,...]
```

Steam Input is exposing a **virtual Valve pad** alongside the physical
controller. UE3 binds the devices to separate local players, so the physical
pad's D-pad/buttons can appear dead while the virtual pad works (or input is
duplicated).

## Fix A — disable Steam Input for this game (recommended)

Steam → **Mighty No. 9** → Properties → **Controller** → **Disable Steam
Input**. With Steam Input off, only the physical controller is enumerated.

## Fix B — launch wrapper

Use the wrapper if you want to keep Steam Input enabled for other games:

```sh
chmod +x mods/controller_fix/mn9_single_controller.sh
```

Steam → Mighty No. 9 → Properties → Launch Options:

```sh
bash "/absolute/path/to/mods/controller_fix/mn9_single_controller.sh" %command%
```

It sets:

```sh
SDL_GAMECONTROLLER_IGNORE_DEVICES=0x28de,0x11ff
```

which hides the Valve virtual pad and leaves the physical XInput controller.

## Steam Controller / Steam Deck

Do **not** use the wrapper if the virtual pad is the only controller Steam
exposes (Steam Controller, Deck controls, some streaming clients). In that case
leave Steam Input on and either ignore the physical duplicate or disable the
extra device at the Steam level.

## Verify

The log should show a single `Input created joystick device` / `Device Plugged
In [0]` entry after the change.
