Another morse decoder

The goal is to be able to decode a morse signal in a -20db snr environment.

This is work in progress.

## Signal connections on Teensy hardware

The input is on the line connector right channel, the one opposite to the earphone plug.
Input sensitivity: `lineInLevel(0)` = 3.12Vpp max range.

Debug inputs:
* pin 14 sync start
* pin 15 tone

Debug outputs:
* line left channel
* line right channel

## User Interface

The TFT display (320x240, ILI9341) is divided into three zones:
- **Top**: Decoded Morse text output (3 lines)
- **Middle**: Menu with 4 function rows + scope bar
- **Bottom**: Software version

### Menu Functions

| Row | Function  | Parameter Values |
|-----|-----------|------------------|
| 0   | Out Left  | 11 sources: POWER, I_SAMPLES, Q_SAMPLES, PHASE, SUBSAMP, DETECT, UNFILT_P, BESSEL, PRE, INPUT, SIG INT |
| 1   | Out Right | Same 11 sources |
| 2   | Gain      | 0.1, 1.0, 10.0 (controls I2S output gain) |
| 3   | Sig Gen   | 1.0, 0.0, -1.0, 0.9Hz, 9Hz, 90Hz, 900Hz (internal signal generator) |
| 4   | Marge     | Detection threshold, 0.005 increments (calls `set_threshold()`) |
| 5   | Scope     | Button triggers serial CSV output |

A rotary encoder navigates the menu; pushing it enters/exits parameter edit mode.

### Audio Architecture

All-F32 audio path using OpenAudio_ArduinoLibrary:
- `AudioInputI2S_F32` → `AudioCoherentDemod4x_F32` (9 demodulator outputs)
- Signal sources routed through `AudioMixer11_F32` (11-input mixer) to `AudioOutputI2S_F32`
- Additional sources: INPUT (raw Line In), SIG INT (internal signal generator)
- Output gain controlled via `AudioOutputI2S_F32.setGain()`
- `AudioCoherentDemodSegmented4x_F32`: alternative demodulator using Hann-windowed overlap analysis for cleaner I/Q extraction — same 9-output interface, drop-in compatible

### Oscilloscope Display

Between the menu and the version line, a 4-trace oscilloscope view shows signal levels vs time:
- **Pin 14 sync** (green) and **Pin 15 tone** (cyan): superposed digital traces sharing the same vertical band. A rising edge on pin 14 triggers the sweep.
- **Detection** (magenta): binary trace from `get_last_detection()`.
- **Power** (yellow): analog trace (0.0–1.0) from `get_last_power()`.
- Sweep rate: 10ms/pixel, 300 pixels wide = 3.0 seconds.
- A 5th menu row controls the scope: pressing the button outputs one sweep of tab-separated serial data (`Time Sync Tone Detect Power`).

### Serial Command Language

Connect at 115200 baud. Available commands:
- `help` — list commands, source names, and signal generator modes
- `status` — show current settings (outputs, gain, siggen, marge)
- `set out<N> <source>` — assign source to output 0-1 (e.g. `set out0 INPUT`)
- `set gain <value>` — set output gain (0.1, 1.0, or 10.0)
- `set siggen <mode>` — set signal generator mode 0-6
- `set marge <value>` — set detection threshold (e.g. `set marge 0.3`)
- `scope` — output next oscilloscope sweep as tab-separated CSV

### Teensy 4.0 Pin usage

| Pin | Function | Notes |
|-----|----------|-------|
| 0 | Encoder B | Rotary encoder rotation |
| 1 | TFT RST | ILI9341 display reset |
| 2 | TFT DC | ILI9341 data/command |
| 3 | Encoder A | Rotary encoder rotation |
| 4 | TUNESW | Encoder push button (INPUT_PULLUP) |
| 5 | LED | Signal/tuning indicator |
| 6 | (reserved) | Audio Shield SPI flash MEMCS |
| 7 | I2S RX | Audio Shield SGTL5000 |
| 8 | I2S TX | Audio Shield SGTL5000 |
| 9 | (reserved) | Audio Shield SD card CS |
| 10 | TFT CS | ILI9341 SPI chip select |
| 11 | SPI MOSI | ILI9341 display |
| 12 | SPI MISO | ILI9341 display |
| 13 | SPI SCLK | ILI9341 display |
| 14 | Digital input 1 | **Available** |
| 15 | Digital input 2 | **Available** |
| 16 | — | Available |
| 17 | — | Available |
| 18 | I2C SDA | SGTL5000 control (Wire) |
| 19 | I2C SCL | SGTL5000 control (Wire) |
| 20 | I2S LRCLK | Audio Shield SGTL5000 |
| 21 | I2S BCLK | Audio Shield SGTL5000 |
| 22 | — | Available |
| 23 | I2S MCLK | Audio Shield SGTL5000 |

## Signal Processing

Processing in python seems to work, see below 10WPM @ -20db snr, transmit data is : "ios" (.. --- ...)

![figure 1](pict/decodage_1.png)
![figure 2](pict/decodage_2.png)
![figure 3](pict/decodage_3.png)