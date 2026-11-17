# Production Pal: FL Studio bridge script

`Production Pal/device_ProductionPal.py` lets Production Pal read FL Studio's state about once a second:

- tempo, playing / recording, the focused window
- the Master and the selected mixer track: name, fader, pan, mute, peak, and the effects in its slots
- the busiest named mixer tracks

It only **reports**. It never changes your project.

## Install

The installer copies it for you. To do it by hand, copy the whole `Production Pal` folder to:

```
~/Documents/Image-Line/FL Studio/Settings/Hardware/Production Pal/device_ProductionPal.py
```

## Connect it (Production Pal must be running)

1. FL Studio → **Options → MIDI settings**.
2. **Input:** select **Production Pal**, click **Enable**, set **Controller type** to **Production Pal (user)** and **Port** to **10**.
3. **Output:** select **Production Pal**, set **Port** to **10** (the same number).
4. The Production Pal window shows **FL Studio connected** in green.

## How it talks

Each update is one SysEx message `F0 7D 50 50 <ASCII JSON> F7` sent to the "Production Pal" virtual MIDI port that the app creates. Every FL API call is wrapped, so if a function doesn't exist in your FL version, that field is just left out.

Test it without FL Studio: `python3 tests/test_fl_script.py`.
