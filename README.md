# a2c
Apple //c Emulator

This software attempts to emulate the [Apple IIc](https://en.wikipedia.org/wiki/Apple_IIc) computer in a Linux terminal.

Features:
* 40 and 80 column text modes supported through curses.
* LoRes graphics modes also supported with color, using a 48x80 terminal window.
* Automatic detection of DOS or ProDOS interleaved floppy disk images.
* Integrated Woz Machine (IWM) emulated and disk drive stepper motor simulated.
* Apple IIc ROM versions FF, 00, 03 and 04 supported.
* WDC 65C02 CPU emulated, despite more limited NCR 65C02 used in the actual hardware.
* All soft switches emulated, internal ROM diagnostics should pass.
* Debugger with CPU trace and memory dumping facilities available.
* Second ACIA serial chip can be redirected to a real TTY on the host.
* Graphical (SDL) window with HiRes graphics output can run in parallel.

Known issues and missing features:
* Floppy write not implemented, meaning all IWM data writes will be ignored.
* HiRes graphics are recognized and displayed in curses but too blocky to be useful.
* Second external floppy drive not supported.
* IRQ handling is missing.
* No sound or game (joystick) input.
* Apple IIc specific RAM expansion not supported.

Tips:
* Use Ctrl+C to enter the debugger, then enter the 'q' command to quit the emulator.
* F1 is mapped to the "RESET" key, use it to enter Applesoft BASIC if no floppy disk image is loaded.
* Type "PR#3" to enable 80 column text mode and "PR#0" to go back to 40 columns.
* Type "GR" to henter LoRes graphics mode and "TEXT" to go back.

