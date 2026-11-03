# Mega Man 1: Beck — WIP bonus

A WIP bonus release for the **MN9 Patcher** community: a Mega Man 1 (NES)
romhack that replaces Mega Man with **Beck** from *Mighty No. 9*.

This is a romhack patch for **Mega Man 1**, not a patch for Mighty No. 9.
It is not part of the MN9 Patcher and does not modify the retail PC game.

## Apply

Base ROM: **Mega Man (USA).nes**, 16-byte iNES header, 128 KiB PRG.

- pristine US dump SHA1: `2f88381557339a14c20428455f6991c1eb902c99`
- MMLC dump SHA1: `e4c579ad2a74ee320e55fe5e3480bae2a822c082`

Apply `MM1-Beck-v2.43.ips` with Flips, Lunar IPS, or any standard IPS
patcher.

Patched SHA1:

- pristine: `620a9286a7938c7d52805a1b1ff68bb503816ac1`
- MMLC: `8b75dbc432a69a90414e6aff2764493d76d6432d`

IPS SHA256: `e07c0d32b0293d5cdae3ef1cb9ef0b6e665c2cc9eca3dca0ccba89df0108e06f`

## Features

- Beck player sprites from the Mighty Gunvolt 8-bit sheet, 32x32 OAM.
- Down + Jump slide/dash, chainable. Crouched ground pose and two-frame air
  dash.
- Ladder climb animation, pause-menu face, white armour palette, boss-flash
  fix.
- Dash absorbs weakened enemies and projectiles. Heal scales with the
  enemy's pellet-hit count.
- Mega Buster pellets weaken regular enemies instead of killing them:
  they stop at 1 HP, freeze, and expire after roughly four seconds. Dash
  into a weakened enemy to absorb it.
- Bosses: each hit builds a 6-hit charge. The 6th hit freezes the boss for
  about one second. The charged dash does 6 damage normally (five cycles)
  or 10 with the boss's weakness (three cycles).
- Cut Man's screen-0/1 drop-room Guts Blocks are removed so the landing is
  clear.

## WIP caveats

- This is a WIP bonus, not the main patcher release.
- Tested with Mesen (RetroArch). Save states are not used.
- The weakenable-enemy table covers the first 31 enemy IDs; a few rarer
  variants are killed normally instead of weakening.
- No game data, ROMs, or commercial assets are included. The IPS patch is
  the only payload.
