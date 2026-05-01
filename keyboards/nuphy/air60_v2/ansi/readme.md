# NuPhy Air60 V2

*NuPhy Air60 V2 is a standard 60 key keyboard.*
![NuPhy Air60 V2](https://i.imgur.com/R7jS2JC.jpeg)

* Keyboard Maintainer: [nuphy](https://github.com/nuphy-src)
* Hardware Supported: NuPhy Air60 V2 PCB
* Hardware Availability: Private

Custom branch for this Air60 V2 QoL port:

    grgas-air60v2

Compile example for this keyboard (after setting up your build environment):

    qmk compile -kb nuphy/air60_v2/ansi -km via

Flashing example for this keyboard:

    qmk flash -kb nuphy/air60_v2/ansi -km via

Console example for this keyboard:

    qmk console

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [QMK CLI instructions](https://docs.qmk.fm/cli) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader:

* **Firmware reboot**: Hold `FN + M + Esc` to trigger `QK_REBOOT`; keep `Esc` held to remain in bootloader for flashing

* **Bootmagic reset**: Hold down the key at (0,0) in the matrix (usually the top left key or Escape) and plug in the keyboard

* **Hardware reset**: Remove the capslock keycap, hold the little button beneath and plug in the keyboard.

## Debug / Console

* **Debug toggle**: `FN + M + 1`
* **Live logs**: Run `qmk console` after enabling debug to see matrix scan rate and settings messages

## Customizations and Fixes

* [customizations.md](customizations.md)
* [Air60 V2 custom firmware notes](../readme.md)
