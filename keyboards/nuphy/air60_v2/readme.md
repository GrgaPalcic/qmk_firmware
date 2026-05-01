# NuPhy Air60 V2 Custom Firmware Notes

This Air60 V2 tree keeps the Air60-specific hardware layout, matrix, RGB driver, and custom VIA identity while porting quality-of-life improvements from [`keyboards/nuphy/air75_v2/ansi`](../air75_v2/ansi).

## Branch

The maintained working branch for this Air60 V2 port is created from the current `my-nuphy-keyboards` work:

```bash
git checkout -b grgas-air60v2
```

## Build, Flash, and Console

Use the existing custom VIA target:

```bash
qmk compile -kb nuphy/air60_v2/ansi -km via
qmk flash -kb nuphy/air60_v2/ansi -km via
qmk console
```

Successful builds produce:

```text
nuphy_air60_v2_ansi_via.bin
```

## Entering Bootloader

- `FN + M + Esc` triggers `QK_REBOOT`. Keep `Esc` held to stay in bootloader for flashing.
- Hardware fallback: remove the Caps Lock keycap, hold the button underneath, then plug in USB.

## Debug

- `FN + M + 1` toggles debug because `DB_TOGG` is on the `1` key in `M_LAYER`.
- Run `qmk console` after enabling debug to see matrix scan rate and settings logs.

## Commit Reference

Suggested branch and commit flow:

```bash
git checkout -b grgas-air60v2
git add keyboards/nuphy/air60_v2
git commit -m "Port adi4086's Air75 V2 QoL firmware to Air60 V2 ANSI" \
  -m "Preserve the Air60 V2 hardware layout, matrix, RGB driver, and custom VIA contract while porting the newer Air75 V2 quality-of-life behavior." \
  -m "This includes RF wake-delay handling, USB-only RF DFU hold behavior, wake buffering, sleep and LED power fixes, and updated Air60 documentation for build, flash, console, and debug workflows." \
  -m "Credit: original Air75 V2 QoL implementation by adi4086; this Air60 V2 work adapts those changes to the Air60 ANSI target."
```

## Credit

The original Air75 V2 QoL implementation was done by [adi4086](https://github.com/adi4086). This Air60 V2 port adapts that work to the Air60 ANSI hardware target while preserving Air60-specific constraints.

## References

- [ANSI target README](./ansi/readme.md)
- [Customizations and fixes](./ansi/customizations.md)
