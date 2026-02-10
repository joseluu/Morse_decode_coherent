Another morse decoder

The goal is to be able to decode a morse signal in a -20db snr environment.

This is work in progress.

## Signal connections on Teensy hardware

The input is on the line connector right channel, the one opposite to the earphone plug

Debug inputs:
* pin 14 sync start
* pin 15 tone

Debug outputs:
* line left channel
* line right chennel
* pin 22 out PWM 2
* pin 23 out PWM 2

## User Interface

The TFT display (320x240, ILI9341) is divided into three zones:
- **Top**: Decoded Morse text output (3 lines)
- **Middle**: Menu with 4 output channel selectors (Out Left, Out Right, Out PWM 1, Out PWM 2)
- **Bottom**: Software version

A rotary encoder navigates the menu; pushing it enters/exits parameter edit mode where each output can be assigned one of 9 internal signal sources from the coherent demodulator.

### Oscilloscope Display

Between the menu and the version line, a 4-trace oscilloscope view shows signal levels vs time:
- **Pin 14 sync** (green) and **Pin 15 tone** (cyan): superposed digital traces sharing the same vertical band. A rising edge on pin 14 triggers the sweep.
- **Detection** (magenta): binary trace from `get_last_detection()`.
- **Power** (yellow): analog trace (0.0–1.0) from `get_last_power()`.
- Sweep rate: 10ms/pixel, 300 pixels wide = 3.0 seconds.
- A 5th menu row controls the scope: pressing the button outputs one sweep of tab-separated serial data (`Time Sync Tone Detect Power`).

### Serial Command Language

Connect at 115200 baud. Available commands:
- `help` — list commands and valid source names
- `status` — show current output assignments and detection threshold
- `set out<N> <source>` — assign source to output (e.g. `set out0 BESSEL`)
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
| 22 | PWM Out 1 | AudioOutputPWM channel 1 |
| 23 | PWM Out 2 / MCLK | AudioOutputPWM channel 2 + I2S MCLK |

## Signal Processing

Processing in python seems to work, see below 10WPM @ -20db snr, transmit data is : "ios" (.. --- ...)

![figure 1](pict/decodage_1.png)
![figure 2](pict/decodage_2.png)
![figure 3](pict/decodage_3.png)