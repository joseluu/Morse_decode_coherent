# Done Changes

## 1.2 2026-02-07 23:40 - Oscilloscope improvement (feature 2.1)

### 3rd trace
- Added detection trace (magenta) showing the Tone/DETECTION_SAMPLES state
- Rebalanced vertical layout: 3 traces × 10px with 15px gaps, 9px top/bottom padding

### Scope bar (5th menu item)
- Extended menu navigation to 5 rows (NUM_MENU_ROWS = 5)
- Row 5 controls the oscilloscope: white bar (20px) on the left of the scope area when selected
- Button press on row 5 flashes the bar yellow and requests serial output for the next sweep

### Serial CSV output
- After pressing scope button, the next sweep outputs tab-separated data on serial
- Header: `Time\tSync\tTone\tDetect`
- One line per 10ms with: timestamp, pin 14 (0/1), pin 15 (0/1), detection (0/1)
- Output stops at end of sweep (one interval only)

### Display offset
- Scope traces shifted 20px right (x=20..319) to make room for the scope bar

## 1.1.2 2026-02-07 23:18 - Sweep rate changed to 10ms/pixel

- Changed SCOPE_MS_PER_PIXEL from 20 to 10 (full sweep = 3.2s)

## 1.1.1 2026-02-07 23:09 - Oscilloscope-like display for pins 14 & 15

### Scope display
- Added oscilloscope-style trace display in the TFT area between the 4-output menu (y=144) and the version line (y=222)
- Pin 14 displayed in green, pin 15 displayed in cyan
- Each signal shows HIGH and LOW levels separated by 10 pixels, with 15px gap between the two traces
- Triggered on rising edge of pin 14, sweep resets to x=0
- Sweep rate: 20ms per pixel (320px = 6.4s full sweep) (changed to 10ms in 1.1.2)
- 10-pixel wipe-ahead clears old data in front of current position
- At right edge, sweep stops and waits for next trigger
- Vertical transition lines drawn when signal level changes between samples

### Pins used
- **Pin 14**: Digital input 1 (trigger + trace)
- **Pin 15**: Digital input 2 (trace)

## 1.1 2026-02-01 16:03 - Redo user interface

### Display layout
- Top 3 lines (0-54px) reserved for decoded Morse output text
- Middle area (56px+) used for the menu selection area
- Bottom line (222px) displays the software version string

### Menu system
- 4 output functions listed vertically on the left: Out Left, Out Right, Out PWM 1, Out PWM 2
- Rotary encoder rotation navigates between functions (highlighted with reverse video)
- Push button toggles parameter edit mode for the selected function
- In edit mode, encoder rotation cycles through 9 source signals as a circular list
- Source is applied immediately on each rotation step
- Second push exits edit mode

### Audio outputs
- Added 2 PWM output channels (AudioOutputPWM) in addition to existing I2S Left/Right
- PWM output uses Teensy 4.0 pins: **pin 22** (Out PWM 1) and **pin 23** (Out PWM 2)
- Default pins 3/4 were avoided because they conflict with the rotary encoder (pin 3) and push button (pin 4)
- Each of the 4 outputs has its own AudioMixer9_F32 selector
- All 9 demodulator outputs (POWER, I_SAMPLES, Q_SAMPLES, PHASE_SAMPLES, SUBSAMPLE_TICKS, DETECTION_SAMPLES, UNFILTERED_POWER, BESSEL, PRE) can be routed to any output

### Removed
- Old encoder step/margin control logic
- Old displayStatus/displayBottom functions
- Unused variables (Etat, oldStep, Step, selectedChannel1)
