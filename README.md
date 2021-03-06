# Teathimble 
Teathimble is a minimalistic firmware intended to control embroidery machines. It runs on most AVR (so arduino as well) microcontrollers and fits into ATmega32 MCU, providing great base for many projects. Teathimble is a trimmed fork of [Teacup](https://github.com/Traumflug/Teacup_Firmware) - lean and efficient firmware for RepRap printers by Triffid Hunter, Traumflug, jakepoz, many others.

## Features
- Minimalistic code.
- Communication via buffered serial and text commands (G-code).
- True acceleration, interrupt based, with look-ahead and jerk calculation planning.
- Integer only math to save cpu cycles for performance boost.
- Up to 4 axis in straight or coreXY kinematics scheme.
- DC motor speed controller.
- Decent performance: can run up to 48'000 evenly spaced steps/second on 20 MHz as mentioned by core developers.

## Work progress
For coil winder machine check this [branch](https://gitlab.com/markol/Coil_winder). Current code is checked on custom made arduino-like board top on ATmega32 to control [simple embroidery machine](https://gitlab.com/markol/embroiderino) and coils winder. It is just a matter of formality to port configurations on other available boards. You might also need [this arduino core](https://github.com/MCUdude/MightyCore) to turn custom board into arduino compatible.
To run inside simple [simulator](https://reprap.org/wiki/SimulAVR) build project with enclosed makefile, this might be come in handy for development.

## Building
Most of the configuration is described and is in *config.h* file.
To disable simulavr info sections, launch make with variable set `SIMULFLAGS= ` 
Example command for flashing Arduino UNO: `make flash MCU=atmega328p SIMULFLAGS= UPLOAD_PORT=/dev/ttyACM0`, assuming that configuration is complete. Makefile contains paths configuration to arduino core or mighty core, they need to be set up correctly. Project does not use arduino libraries at all, it is just compatible with Arduio IDE which should be capable of whole compilation and memory programming task, just open the *.ino* file.

## List of supported G-codes
For now parser is kept simple and not so proof for sophisticated errors in code syntax.

- **G0 X? Y? F?** - liner move - jump, F is an feedrate
- **G1 X? Y? F?** - linear move - stitch
- **G28** - home axes
- **G90** - set to absolute positioning
- **G91** - set to relative positioning
- **M0** - unconditional stop of all movement in progress
- **M112** - kill, stops everything and requires reset
- **M114** - get current position info
- **M119** - get endstop triggers status
- **M222 S?** - sets the motor max speed limit in SPM
- **M301 S? P? I?** - set the kP and kI values, S sets the current motor speed for tuning purposes



## Source files description
Code is written in pure C.

|      Name      |                              Description                                                                                               |
|----------------|:--------------------------------------------------------------------------------------------------------------------------------------:|
| config.h       |  Configuration constants definitions.                                                                                                  |
|debug.c         |  Debugging aids.                                                                                                                       |
|gcode_parser.c  | G-code interpreter, instructions and their actions definitions.                                                                        |
|kinematics.c    |  Separated code for different movement schemes.                                                                                        |
|maths.c         | Calculation stuff, functions, constants, tables.                                                                                       |
|motion_planner.c| Ramping acceleration and lookahead related stuff.                                                                                      |
|motor.c         | A rather complex block of math that figures out when to step each axis according to speed and acceleration profiles and received moves.|
|msg.c           | For numbers parsing.                                                                                                                   |
|pinio.c         |  Initialize all I/O.                                                                                                                   |
|queue.c         | The queue of moves received from the host.                                                                                             |
|sensors_control.c| DC motor speed controller implementation and needle position detection and processing.                                                |
|serial.c        | Serial buffers and communication management.                                                                                           |
|teathimble.ino.c| Code starts here.                                                                                                                      |
|teathimble.ino  | Same as above, allows firmware to be built in Arduino IDE.                                                                             |
|timer-avr.c     | Timer management, used primarily by motor.c for timing steps.                                                                          |
