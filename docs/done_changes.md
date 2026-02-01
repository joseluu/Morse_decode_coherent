# Done Changes

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
