# Done Changes

## 1.6.1 2026-02-15 15:32 - Switch to segmented demodulator

- Replaced `AudioCoherentDemod4x_F32 CW_In` with `AudioCoherentDemodSegmented4x_F32 CW_In(2500, 0.8f)`
- Parameters: segment_length=2500, overlap_factor=0.8 (hop=500, segment rate ~86.4 Hz)
- All existing audio connections and API calls remain unchanged (drop-in compatible)

## 1.6 2026-02-15 14:46 - Segmented coherent demodulator (feature 4)

### New class: AudioCoherentDemodSegmented4x_F32
- Hann-windowed overlap-analysis I/Q extraction (replaces 4x phase-counter trick)
- Based on Python `coherent_demodulate_segmented` algorithm
- Constructor: `AudioCoherentDemodSegmented4x_F32(segment_length, overlap_factor)`
- Same 9-output interface as `AudioCoherentDemod4x_F32` for drop-in compatibility
- Per-segment I/Q at rate `fs/hop` (~34.56 Hz for segment_length=2500, overlap=0.5)
- 3 independent LP filters for I, Q, and power at segment rate (3 Hz cutoff, bilinear transform)
- Reuses pre-filter (Butterworth 4th @ 1800 Hz) and Bessel filter (4th @ 900 Hz) coefficients
- Static cos/sin LO lookup tables (48 entries, period = fs/fc)
- Dynamic memory: ~20 KB for segment_length=2500 (ring buffer + Hann window)
- New files: `AudioCoherentDemodSegmented4x_F32.h`, `AudioCoherentDemodSegmented4x_F32.cpp`

## 1.5.1 2026-02-14 17:25 - Adjustments (feature 3.4)

- Changed Marge increment from 0.005 to 0.01 per encoder step

## 1.5 2026-02-11 22:08 - Marge threshold menu (feature 3.3)

- Reduced decoded text area from 3 lines to 2 (DECODE_AREA_HEIGHT 54→36, MENU_AREA_Y 56→38)
- Added "Marge" as 5th menu row (row 4) with 0.005 increment steps
- Marge value displayed with 2 decimal places, calls `CW_In.set_threshold()` on change
- Scope bar moved to row 5 (was row 4), NUM_MENU_ROWS 5→6
- SCOPE_AREA_Y adjusted for the extra menu row

## 1.4.1 2026-02-11 21:28 - Signal management fixes

- Fixed signal generator DC modes: swapped 1.0 and -1.0 values to match labels (I2S output inverts)
- Swapped I2S Left/Right channel assignments to match physical outputs
- Reduced input sensitivity: `lineInLevel(0)` (3.12Vpp max range)

## 1.4 2026-02-11 20:34 - Signal management (feature 3.2)

### F32 I/O migration
- Replaced `AudioInputI2S` + `AudioOutputI2S` (I16) with `AudioInputI2S_F32` + `AudioOutputI2S_F32`
- Eliminated I16↔F32 converters (`cnvrtI2F0`, `cnvrtF2I0`, `cnvrtF2I1`)
- Direct F32 path: `I2s1` → `CW_In` → `outputSelector` → `I2s2`
- Removed `amp1` (AudioEffectGain_F32), gain now via `I2s2.setGain()`

### PWM outputs removed
- Removed all PWM output code (`pwmPeak2/3`, `analogWrite`, pins 22/23 setup)
- Removed `Out PWM 1` and `Out PWM 2` menu items
- Reduced from 4 output selectors to 2 (Out Left, Out Right only)

### New AudioMixer11_F32 class
- Extended `AudioMixer9_F32` from 9 to 11 inputs
- New files: `AudioMixer11_F32.h`, `AudioMixer11_F32.cpp`

### Internal signal generator
- New class `AudioSignalGenerator_F32` (header-only: `AudioSignalGenerator_F32.h`)
- 7 modes: DC {1.0, 0.0, -1.0}, Sine {0.9, 9, 90, 900} Hz
- Phase accumulator using `arm_sin_f32`

### New source parameters
- Added `INPUT` (raw audio from Line In Right) and `SIG INT` (signal generator) to output selectors
- `NUM_SOURCES` increased from 9 to 11

### Menu restructure
- Row 0: Out Left (11 sources), Row 1: Out Right (11 sources)
- Row 2: Gain (0.1, 1.0, 10.0) — controls `AudioOutputI2S_F32.setGain()`
- Row 3: Sig Gen (7 modes) — controls signal generator
- Row 4: Scope bar (unchanged)

### Serial commands updated
- `set out<N>`: restricted to 0-1, new sources INPUT and SIG_INT available
- `set gain <value>`: new command (0.1, 1.0, 10.0)
- `set siggen <mode>`: new command (0-6)
- `help` and `status` updated for new menu structure

## 1.3.2 2026-02-10 18:00 - Fix I2S output (DMA conflict)

### Root cause
- `AudioOutputPWM` and `AudioOutputI2S` cannot coexist — both instantiated causes DMA conflict that silently kills I2S output

### Audio I/O changes
- Removed `AudioOutputPWM pwmOut(22, 23)` — root cause of no output on Line Out
- Reverted to standard I16 I2S types: `AudioInputI2S` + `AudioOutputI2S`
- F32 processing via converters: `AudioInputI2S` → `cnvrtI2F0` → F32 → `cnvrtF2I0/1` → `AudioOutputI2S`
- `setI2SFreq()` now sets both SAI1 and SAI2 clock registers (matching old working version)

### Demodulator additions
- Added `get_last_power()` and `get_last_detection()` getters to `AudioCoherentDemod4x_F32`
- Added `current_detection` member variable for caching detection state

### PWM outputs disabled
- Out PWM 1/2 menu items remain but are non-functional (selectors 2/3 have no output connection)
- Future: needs alternative PWM approach (e.g. `analogWrite()` or direct timer control)

## 1.3.1 2026-02-08 14:50 - Oscilloscope changes (feature 3.1)

### Superposed sync/tone traces
- Pin 14 (green) and Pin 15 (cyan) now share the same vertical band (SCOPE_SYNC_HIGH/LOW)
- Both traces drawn at the same y positions, overlapping when both HIGH or both LOW

### Detection trace from demodulator
- Third trace now reads `CW_In.get_last_detection()` instead of the `Tone` variable
- Still displayed as binary HIGH/LOW in magenta

### Power trace (new, analog)
- Fourth trace displays `CW_In.get_last_power()` (float 0.0–1.0) in yellow
- Mapped to a 30px vertical range with continuous line drawing

### Marge linked to demodulator threshold
- `set marge` command now also calls `CW_In.set_threshold()` to sync the demodulator

### Serial CSV updated
- Added Power column: header is now `Time\tSync\tTone\tDetect\tPower`

## 1.3 2026-02-07 23:56 - Serial command language (feature 3)

- Added serial command interface at 115200 baud
- Commands: `help`, `status`, `set out<N> <source>`, `set marge <value>`, `scope`
- `help` prints available commands and source names
- `status` prints all 4 output assignments and current Marge threshold
- `set out0 BESSEL` sets Out Left to BESSEL, updates display immediately
- `set marge 0.3` changes detection threshold
- `scope` triggers CSV serial output on next sweep (same as button press on row 5)
- Non-blocking line reader processes serial input without impacting audio

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
