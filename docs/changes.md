## 0. context
### Hardware
This program runs on an Teensy V4.0 with a 320x240 TFT display using a ILI9341
display controler
The display is connected via SPI
There is a Teensyaudio module connected
### development framework and compilation process
Use the arduino-cli to compile and upload
In order to use the arduino-cli source ~/hobby_w/arduino-cli-setup
Local arduino setup is documented in this file:
/c/Users/josel/hobby_w/ESP32/where_are_Ou_sont_les_librairies.txt
arduino-cli compile should use the config-file "C:\Users\josel\.arduinoIDE\arduino-cli.yaml"
### rotary encoder usage for menu selection
There is a pushable rotary encoder for the user to change parameters
The rotation of the encoder is connected to pin 0 and 3
The push switch of the encoder is connected to pin 4


### version numbering

When altering version number, use a subnumber so that the version string changes
at each uploaded version. As for the date which goes alongs the version string,
add the time with hours and minutes
In the version numbering string, do not use V at the begining
use the date command for accurate timestamps include date and time hh:mm in the version string
Display the software version on the bottom line of the display

### change records
For each requested modfication summarize your changes by adding a paragraphe into the file docs/done_changes.md
and update the project README.md where necessary

### accessing the serial output while in git bash

Reading serial output from Teensy (COM8 in this example) in Git Bash
Use PowerShell's System.IO.Ports.SerialPort class invoked from bash. The key is to use single quotes around the PowerShell command so bash doesn't consume $ signs.

to Read N lines from COM8 at 115200 baud
```
powershell.exe -Command 'try { $port = New-Object System.IO.Ports.SerialPort "COM8", 115200, "None", 8, "One"; $port.ReadTimeout = 10000; $port.Open(); for($i=0; $i -lt 3; $i++) { Write-Output $port.ReadLine() }; $port.Close() } catch { Write-Output $_.Exception.Message }'
```
## 1. redo user interface
* Reserve the first 3 lines of the display for the decoded output.
* Reserve the last line of the display for displaying the program version
* The middle of the display is the menu selectio area where:

List the top level functions vertically on the left side of the display
Use rotation to select one among the functions.
Show selection by reversing video of the function name selected
A single push on the push button allows to change the parameter value of the function
A another push on the push button gets out of the parameter value change
In the parameter change mode, the possible values are listed below,
the values should be considered as a circular list, each rotation of the
rotary encoder selects the next or previous value.
The currently selected value should be on the same line as its function name
It should change with each rotation step and be definitly selected and
as soon as a new parameter value is displayed assign its value to the software.

### List of top level functions:
* Out Left
* Out Right
* Out PWM 1
* Out PWM 2

Where each function is actually an output audio channel, Out Left is the left channel of line output of the Teensy Audio board, Out Right is the right channel of line output of the Teensy Audio board, Out PWM 1 correspond to a suitable pin on the teensy V4.0 board used through AudioOutputPWM, Out PWM 2 correspond to another teensy board pin.

### List of parameter values
* for all out top level functions, the parameter selects which internal source is funnelled into said output among:
    - POWER
    - I_SAMPLES
    - Q_SAMPLES
    - PHASE_SAMPLES
    - SUBSAMPLE_TICKS
    - DETECTION_SAMPLES
    - UNFILTERED_POWER
    - BESSEL
    - PRE

Where the sources correspond to waveforms output by AudioCoherentDemod4x_F32