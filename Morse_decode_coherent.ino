/*

  Inspiré de
  MorseCodeDecoder6.ino
  Code by by LINGIB
  https://www.instructables.com/member/lingib/instructables/


*/

// ----- TFT shield definitions
#include <Arduino.h>
#include <OpenAudio_ArduinoLibrary.h>
#include <Audio.h>
#include <SPI.h>
#include <EncoderTool.h>
#include <utility/imxrt_hw.h>
#include <Wire.h>
#include <SD.h>
#include <SerialFlash.h>
#include <output_i2s.h>
#include <input_i2s.h>
#include <AudioStream_F32.h>
#include "ILI9341_t3n.h"
#include "ili9341_t3n_font_Arial.h"
#include <Bounce2.h>
#include "AudioCoherentDemod4x_F32.h"
#include "AudioMixer9_F32.h"


                       // signal/tuning indicator
#define Debug true

unsigned long Start_reference = 100;            // choose value midway between dot and dash at target speed
unsigned long Reference = Start_reference;
unsigned long Leading_edge;                     // used to calculate tone duration
unsigned long Trailing_edge;                    // used to calculate tone duration
unsigned long Tone_duration[6];                 // store tone durations for entire letter here
short Tone_index = 0;
unsigned long Tone_max = 0L;
unsigned long Tone_min = 9999L;
unsigned long Duration;                         // used for calculating rolling dot/dash averages;
unsigned long Average;
unsigned long Letter_start;                     // used for calculating WPM (words per minute)
short Symbol_count = 0;
short WPM;
bool Started = false;                           // decoder logic
bool Measuring = false;
bool Tone = false;

//------------------------------- utilisé par l'affichage
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define num_chars 300
char CodeBuffer[num_chars];
char DisplayLine[num_chars+1];

// Display layout zones
#define DECODE_AREA_Y      0
#define DECODE_AREA_HEIGHT 54       // 3 lines of Arial_14 (~18px each)
#define MENU_AREA_Y        56
#define MENU_LINE_HEIGHT   22
#define VERSION_LINE_Y     222      // bottom line for version

// Oscilloscope display constants
#define SCOPE_AREA_Y       (MENU_AREA_Y + NUM_OUTPUTS * MENU_LINE_HEIGHT)
#define SCOPE_AREA_HEIGHT  (VERSION_LINE_Y - SCOPE_AREA_Y)
#define SCOPE_MS_PER_PIXEL 10
#define SCOPE_WIPE_AHEAD   10
#define SCOPE_X_OFFSET     20
#define SCOPE_WIDTH        (SCREEN_WIDTH - SCOPE_X_OFFSET)
#define PIN_INPUT_1        14
#define PIN_INPUT_2        15
#define SCOPE_SYNC_HIGH    (SCOPE_AREA_Y + 5)
#define SCOPE_SYNC_LOW     (SCOPE_SYNC_HIGH + 10)
#define SCOPE_DETECT_HIGH  (SCOPE_SYNC_LOW + 10)
#define SCOPE_DETECT_LOW   (SCOPE_DETECT_HIGH + 10)
#define SCOPE_POWER_TOP    (SCOPE_DETECT_LOW + 5)
#define SCOPE_POWER_BOT    (VERSION_LINE_Y - 8)

// Decoded text cursor position (within decode area)
int posH = 10;
int posV = DECODE_AREA_Y + 2;

// Oscilloscope state
int scopeX = 0;
unsigned long scopeLastUpdate = 0;
bool scopePin14Prev = false;
bool scopePin14Level = false;
bool scopePin15Level = false;
bool scopeDetectLevel = false;
float scopePowerPrev = 0.0f;
bool scopeActive = false;
bool scopeSerialRequested = false;
bool scopeSerialActive = false;
unsigned long scopeBarFlashTime = 0;

// Serial command buffer
#define CMD_BUF_SIZE 64
char cmdBuffer[CMD_BUF_SIZE];
int cmdIndex = 0;

//-----------------------------utilisé par le décodeur
float Diff = 0.2 ;
float Marge = 0.21 ;           // différence entre bin analysé et bin exterieur (~ noise )
float y = 0.2 ;               // niveau de bruit
float x = 0 ;                 // valeur du bin mesuré_désiré)

 //-------------------------------Utilisé par myFFT
 #define FFT_SIZE 1024                                    // Valeur FFT
 #define WATERFALL_ROW_COUNT 1                           // nombre de lignes

//------------------------------- Menu UI
#define NUM_OUTPUTS 4
#define NUM_MENU_ROWS 5
#define NUM_SOURCES 9

const char* outputNames[NUM_OUTPUTS] = {
  "Out Left",
  "Out Right",
  "Out PWM 1",
  "Out PWM 2"
};

const char* sourceNames[NUM_SOURCES] = {
  "POWER",
  "I_SAMPLES",
  "Q_SAMPLES",
  "PHASE",
  "SUBSAMP",
  "DETECT",
  "UNFILT_P",
  "BESSEL",
  "PRE"
};

// Source index constants (matching #defines)
const int sourceIndex[NUM_SOURCES] = {
  POWER, I_SAMPLES, Q_SAMPLES, PHASE_SAMPLES,
  SUBSAMPLE_TICKS, DETECTION_SAMPLES, UNFILTERED_POWER,
  BESSEL, PRE
};

// Current source selection for each output (index into sourceNames)
int outputSourceSel[NUM_OUTPUTS] = { BESSEL, POWER, SUBSAMPLE_TICKS, UNFILTERED_POWER };

// Menu state
int menuSelectedRow = 0;           // which output function is highlighted (0..3)
bool menuEditMode = false;         // true = editing parameter value for selected row



// ----- binary morse tree
const char Symbol[] =
{
  ' ', '5', ' ', 'H', ' ', '4', ' ', 'S',       // 0
  ' ', ' ', ' ', 'V', ' ', '3', ' ', 'I',       // 8
  ' ', ' ', ' ', 'F', ' ', ' ', ' ', 'U',       // 16
  '?', ' ', '_', ' ', ' ', '2', ' ', 'E',       // 24
  ' ', '&', ' ', 'L', '"', ' ', ' ', 'R',       // 32
  ' ', '+', '.', ' ', ' ', ' ', ' ', 'A',       // 40
  ' ', ' ', ' ', 'P', '@', ' ', ' ', 'W',       // 48
  ' ', ' ', ' ', 'J', '\'', '1', ' ', ' ',      // 56
  ' ', '6', '-', 'B', ' ', '=', ' ', 'D',       // 64
  ' ', '/', ' ', 'X', ' ', ' ', ' ', 'N',       // 72
  ' ', ' ', ' ', 'C', ';', ' ', '!', 'K',       // 80
  ' ', '(', ')', 'Y', ' ', ' ', ' ', 'T',       // 88
  ' ', '7', ' ', 'Z', ' ', ' ', ',', 'G',       // 96
  ' ', ' ', ' ', 'Q', ' ', ' ', ' ', 'M',       // 104
  ':', '8', ' ', ' ', ' ', ' ', ' ', 'O',       // 112
  ' ', '9', ' ', ' ', ' ', '0', ' ', ' '        // 120
};
short Index = 63;                               // point to middle of symbol[] array
short Offset = 32;                              // amount to shift index for first dot/dash
short Count = 6;                                // Height we are up the tree


// attention a ne pas utiliser des lignes utilisées par la platine audio !!!!! ...
#define TFT_DC      2
#define TFT_CS      10
#define TFT_RST     1  
#define TFT_MOSI     11
#define TFT_SCLK    13
#define TFT_MISO    12

ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK );

//-------------------------------gestion encodeur

volatile int8_t Pos_enc = 0;                                    // position encodeur
volatile int8_t old_Pos_enc = 0;

#define  TUNESW      4                              // poussoir de l'encodeur
Button tunesw = Button();

using namespace EncoderTool;                         // switch rotatif
Encoder encoder;

const short LED = 5;

#define VERSION "1.3.1 2026-02-08 14:50"
#define AUTEUR " F1FGV et F1VL"


const int myInput = AUDIO_INPUT_LINEIN;                 // entrée connecteur 10 pins
//const int myInput = AUDIO_INPUT_MIC;                      // entrée connecteur MIC




AudioInputI2S                      I2s1;           //xy=89,393
AudioOutputI2S                     I2s2;           //xy=1288,393
// NOTE: AudioOutputPWM conflicts with AudioOutputI2S (DMA conflict) — cannot use both

AudioCoherentDemod4x_F32           CW_In;      //xy=474,405

AudioConvert_I16toF32              cnvrtI2F0 ;
AudioConvert_F32toI16              cnvrtF2I0 ;    // Out Left converter
AudioConvert_F32toI16              cnvrtF2I1 ;    // Out Right converter

AudioEffectGain_F32                amp1;           // available for future use

// Input: I2s1 ch1 (Line In Right) -> I16-to-F32 converter -> CW_In demodulator
AudioConnection              patchCord2(I2s1, 1, cnvrtI2F0, 0);
AudioConnection_F32          patchCord3(cnvrtI2F0, 0, CW_In, 0);

// amp1 taps the F32 input (available for future use)
AudioConnection_F32          patchCord1a(cnvrtI2F0, 0, amp1, 0);

// 4 output selectors: Out Left, Out Right, Out PWM 1, Out PWM 2
AudioMixer9_F32              outputSelector[NUM_OUTPUTS];

#define POWER 0
#define I_SAMPLES 1
#define Q_SAMPLES 2
#define PHASE_SAMPLES 3
#define SUBSAMPLE_TICKS 4
#define DETECTION_SAMPLES 5
#define UNFILTERED_POWER 6
#define BESSEL 7
#define PRE 8

// Connect all 9 demodulator outputs to each of the 4 selectors
AudioConnection_F32          cDS00(CW_In, POWER, outputSelector[0], POWER);
AudioConnection_F32          cDS01(CW_In, I_SAMPLES, outputSelector[0], I_SAMPLES);
AudioConnection_F32          cDS02(CW_In, Q_SAMPLES, outputSelector[0], Q_SAMPLES);
AudioConnection_F32          cDS03(CW_In, PHASE_SAMPLES, outputSelector[0], PHASE_SAMPLES);
AudioConnection_F32          cDS04(CW_In, SUBSAMPLE_TICKS, outputSelector[0], SUBSAMPLE_TICKS);
AudioConnection_F32          cDS05(CW_In, DETECTION_SAMPLES, outputSelector[0], DETECTION_SAMPLES);
AudioConnection_F32          cDS06(CW_In, UNFILTERED_POWER, outputSelector[0], UNFILTERED_POWER);
AudioConnection_F32          cDS07(CW_In, BESSEL, outputSelector[0], BESSEL);
AudioConnection_F32          cDS08(CW_In, PRE, outputSelector[0], PRE);

AudioConnection_F32          cDS10(CW_In, POWER, outputSelector[1], POWER);
AudioConnection_F32          cDS11(CW_In, I_SAMPLES, outputSelector[1], I_SAMPLES);
AudioConnection_F32          cDS12(CW_In, Q_SAMPLES, outputSelector[1], Q_SAMPLES);
AudioConnection_F32          cDS13(CW_In, PHASE_SAMPLES, outputSelector[1], PHASE_SAMPLES);
AudioConnection_F32          cDS14(CW_In, SUBSAMPLE_TICKS, outputSelector[1], SUBSAMPLE_TICKS);
AudioConnection_F32          cDS15(CW_In, DETECTION_SAMPLES, outputSelector[1], DETECTION_SAMPLES);
AudioConnection_F32          cDS16(CW_In, UNFILTERED_POWER, outputSelector[1], UNFILTERED_POWER);
AudioConnection_F32          cDS17(CW_In, BESSEL, outputSelector[1], BESSEL);
AudioConnection_F32          cDS18(CW_In, PRE, outputSelector[1], PRE);

AudioConnection_F32          cDS20(CW_In, POWER, outputSelector[2], POWER);
AudioConnection_F32          cDS21(CW_In, I_SAMPLES, outputSelector[2], I_SAMPLES);
AudioConnection_F32          cDS22(CW_In, Q_SAMPLES, outputSelector[2], Q_SAMPLES);
AudioConnection_F32          cDS23(CW_In, PHASE_SAMPLES, outputSelector[2], PHASE_SAMPLES);
AudioConnection_F32          cDS24(CW_In, SUBSAMPLE_TICKS, outputSelector[2], SUBSAMPLE_TICKS);
AudioConnection_F32          cDS25(CW_In, DETECTION_SAMPLES, outputSelector[2], DETECTION_SAMPLES);
AudioConnection_F32          cDS26(CW_In, UNFILTERED_POWER, outputSelector[2], UNFILTERED_POWER);
AudioConnection_F32          cDS27(CW_In, BESSEL, outputSelector[2], BESSEL);
AudioConnection_F32          cDS28(CW_In, PRE, outputSelector[2], PRE);

AudioConnection_F32          cDS30(CW_In, POWER, outputSelector[3], POWER);
AudioConnection_F32          cDS31(CW_In, I_SAMPLES, outputSelector[3], I_SAMPLES);
AudioConnection_F32          cDS32(CW_In, Q_SAMPLES, outputSelector[3], Q_SAMPLES);
AudioConnection_F32          cDS33(CW_In, PHASE_SAMPLES, outputSelector[3], PHASE_SAMPLES);
AudioConnection_F32          cDS34(CW_In, SUBSAMPLE_TICKS, outputSelector[3], SUBSAMPLE_TICKS);
AudioConnection_F32          cDS35(CW_In, DETECTION_SAMPLES, outputSelector[3], DETECTION_SAMPLES);
AudioConnection_F32          cDS36(CW_In, UNFILTERED_POWER, outputSelector[3], UNFILTERED_POWER);
AudioConnection_F32          cDS37(CW_In, BESSEL, outputSelector[3], BESSEL);
AudioConnection_F32          cDS38(CW_In, PRE, outputSelector[3], PRE);

// Connect selectors 0,1 to I2S output via F32->I16 converters
AudioConnection_F32          connectSel0(outputSelector[0], 0, cnvrtF2I0, 0);
AudioConnection_F32          connectSel1(outputSelector[1], 0, cnvrtF2I1, 0);
AudioConnection              connectConvOutLeft(cnvrtF2I0, 0, I2s2, 0);   // Out Left
AudioConnection              connectConvOutRight(cnvrtF2I1, 0, I2s2, 1);  // Out Right

// NOTE: Out PWM 1/2 (selectors 2,3) disabled — AudioOutputPWM conflicts with AudioOutputI2S

AudioControlSGTL5000              sgtl5000_1;    //xy=503,960
// GUItool: end automatically generated code




//------------------------------- channel selection helpers

void selectOutputChannel(int outputIdx, int newSourceIdx) {
  for (int i = 0; i < 9; i++) {
    outputSelector[outputIdx].gain(i, 0.0f);
  }
  outputSelector[outputIdx].gain(newSourceIdx, 1.0f);
}

void applyAllOutputSelections() {
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    selectOutputChannel(i, outputSourceSel[i]);
  }
}

//------------------------------------------------------------ setup()

void setup() {

  // -------------------------------------------------------------------- configure serial port
  Serial.begin(115200);
  //---------------------------------------------------------configure paramètres SGTL5000
  sgtl5000_1.enable();
  AudioMemory(100);                                       // pour le 16 bits
  AudioMemory_F32(200);                                   // pour le 32 bits
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);             // or AUDIO_INPUT_MIC
  sgtl5000_1.volume(0.6);
  sgtl5000_1.lineInLevel (15);                            // 0 à 15   15: 0.24Vpp    11: 0.48Vpp  0: 2.8Vpp (1V rms) // avoid saturation !
  sgtl5000_1.lineOutLevel (13);                           // 13 to 31     13 = maximum

  setI2SFreq((int)CW_In.get_f_sampling());

  amp1.setGain(1.0f);
  applyAllOutputSelections();

  encoder.begin(3,0);                                          // deux fils de l'encodeur partie rotative

  tunesw.attach(TUNESW,INPUT_PULLUP);                           // gestion poussoir switch rotatif
  tunesw.interval(5);
  tunesw.setPressedState(LOW);

  // ---------------------------------------------------------- configure TFT display
  tft.begin();
  tft.setRotation( 1);
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(30, 7);
  tft.setTextColor(ILI9341_ORANGE);
  tft.setFont(Arial_14);
  tft.print("decodeur Morse ");
  tft.setCursor(30,50);
  tft.print(VERSION);
  tft.setCursor(30, 93);
  tft.print(AUTEUR);
  delay(3000);
  tft.fillScreen(ILI9341_BLACK);
  drawMenu();
  displayVersion();

  // ------------------------------------------------------------ configure LED
  pinMode(LED, OUTPUT);                               // signal/tuning indicator
  digitalWrite(LED, LOW);

  // ------------------------------------------------------------ configure scope inputs
  pinMode(PIN_INPUT_1, INPUT);
  pinMode(PIN_INPUT_2, INPUT);
 //-------------------------------------------------------------- pour le décodage
  Started = false;
  Measuring = false;
}

// ----------------------------
// updateOscilloscope()
// ----------------------------
void updateOscilloscope() {
  // Refresh scope bar (yellow flash timeout)
  drawScopeBar();

  // Read pin 14 and detect rising edge (trigger)
  bool pin14Now = digitalRead(PIN_INPUT_1);
  if (pin14Now && !scopePin14Prev) {
    // Rising edge on pin 14: reset sweep
    scopeX = 0;
    scopeActive = true;
    scopeLastUpdate = millis();
    scopePin14Level = pin14Now;
    scopePin15Level = digitalRead(PIN_INPUT_2);
    scopeDetectLevel = (CW_In.get_last_detection() > 0.5f);
    scopePowerPrev = CW_In.get_last_power();
    // Start serial output if requested
    if (scopeSerialRequested) {
      scopeSerialRequested = false;
      scopeSerialActive = true;
      Serial.println("Time\tSync\tTone\tDetect\tPower");
    }
  }
  scopePin14Prev = pin14Now;

  if (!scopeActive) return;
  if (scopeX >= SCOPE_WIDTH) {
    scopeSerialActive = false;
    return;  // wait for next trigger
  }

  unsigned long now = millis();
  if (now - scopeLastUpdate < SCOPE_MS_PER_PIXEL) return;
  scopeLastUpdate = now;

  // Read current levels
  bool newPin14 = digitalRead(PIN_INPUT_1);
  bool newPin15 = digitalRead(PIN_INPUT_2);
  bool newDetect = (CW_In.get_last_detection() > 0.5f);
  float newPower = CW_In.get_last_power();

  // Serial output if active
  if (scopeSerialActive) {
    Serial.print(now);
    Serial.print('\t');
    Serial.print(newPin14 ? 1 : 0);
    Serial.print('\t');
    Serial.print(newPin15 ? 1 : 0);
    Serial.print('\t');
    Serial.print(newDetect ? 1 : 0);
    Serial.print('\t');
    Serial.println(newPower, 3);
  }

  // Wipe ahead: clear columns in front of current position
  int screenX = scopeX + SCOPE_X_OFFSET;
  int wipeEnd = min(screenX + SCOPE_WIPE_AHEAD, SCREEN_WIDTH);
  if (screenX < wipeEnd) {
    tft.fillRect(screenX, SCOPE_AREA_Y, wipeEnd - screenX, SCOPE_AREA_HEIGHT, ILI9341_BLACK);
  }

  // Draw pin 14 sync trace (green) — superposed with pin 15
  int y14 = newPin14 ? SCOPE_SYNC_HIGH : SCOPE_SYNC_LOW;
  tft.drawPixel(screenX, y14, ILI9341_GREEN);
  if (newPin14 != scopePin14Level) {
    tft.drawFastVLine(screenX, SCOPE_SYNC_HIGH, SCOPE_SYNC_LOW - SCOPE_SYNC_HIGH + 1, ILI9341_GREEN);
  }

  // Draw pin 15 tone trace (cyan) — superposed with pin 14
  int y15 = newPin15 ? SCOPE_SYNC_HIGH : SCOPE_SYNC_LOW;
  tft.drawPixel(screenX, y15, ILI9341_CYAN);
  if (newPin15 != scopePin15Level) {
    tft.drawFastVLine(screenX, SCOPE_SYNC_HIGH, SCOPE_SYNC_LOW - SCOPE_SYNC_HIGH + 1, ILI9341_CYAN);
  }

  // Draw detection trace (magenta) — binary from get_last_detection
  int yDet = newDetect ? SCOPE_DETECT_HIGH : SCOPE_DETECT_LOW;
  tft.drawPixel(screenX, yDet, ILI9341_MAGENTA);
  if (newDetect != scopeDetectLevel) {
    tft.drawFastVLine(screenX, SCOPE_DETECT_HIGH, SCOPE_DETECT_LOW - SCOPE_DETECT_HIGH + 1, ILI9341_MAGENTA);
  }

  // Draw power trace (yellow) — analog from get_last_power
  int powerRange = SCOPE_POWER_BOT - SCOPE_POWER_TOP;
  int yPow = SCOPE_POWER_BOT - (int)(newPower * powerRange);
  yPow = constrain(yPow, SCOPE_POWER_TOP, SCOPE_POWER_BOT);
  int yPowPrev = SCOPE_POWER_BOT - (int)(scopePowerPrev * powerRange);
  yPowPrev = constrain(yPowPrev, SCOPE_POWER_TOP, SCOPE_POWER_BOT);
  tft.drawPixel(screenX, yPow, ILI9341_YELLOW);
  if (yPow != yPowPrev) {
    int yMin = min(yPow, yPowPrev);
    int yMax = max(yPow, yPowPrev);
    tft.drawFastVLine(screenX, yMin, yMax - yMin + 1, ILI9341_YELLOW);
  }

  // Update stored levels
  scopePin14Level = newPin14;
  scopePin15Level = newPin15;
  scopeDetectLevel = newDetect;
  scopePowerPrev = newPower;

  scopeX++;
}

// ----------------------------
// Serial command language
// ----------------------------

int findSourceIndex(const char* name) {
  for (int i = 0; i < NUM_SOURCES; i++) {
    if (strcmp(name, sourceNames[i]) == 0) return i;
  }
  return -1;
}

void cmdHelp() {
  Serial.println("Commands:");
  Serial.println("  help                  - show this help");
  Serial.println("  status                - show current settings");
  Serial.println("  set out<N> <source>   - set output (0-3) to source");
  Serial.println("  set marge <value>     - set detection threshold");
  Serial.println("  scope                 - output next sweep as CSV");
  Serial.print("Sources:");
  for (int i = 0; i < NUM_SOURCES; i++) {
    Serial.print(' ');
    Serial.print(sourceNames[i]);
  }
  Serial.println();
}

void cmdStatus() {
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    Serial.print(outputNames[i]);
    Serial.print(": ");
    Serial.println(sourceNames[outputSourceSel[i]]);
  }
  Serial.print("Marge: ");
  Serial.println(Marge, 3);
}

void executeCommand(char* cmd) {
  // Skip leading spaces
  while (*cmd == ' ') cmd++;
  // Remove trailing spaces/newlines
  int len = strlen(cmd);
  while (len > 0 && (cmd[len-1] == ' ' || cmd[len-1] == '\r' || cmd[len-1] == '\n')) {
    cmd[--len] = '\0';
  }
  if (len == 0) return;

  if (strcmp(cmd, "help") == 0) {
    cmdHelp();
  } else if (strcmp(cmd, "status") == 0) {
    cmdStatus();
  } else if (strcmp(cmd, "scope") == 0) {
    scopeSerialRequested = true;
    Serial.println("Scope serial requested");
  } else if (strncmp(cmd, "set ", 4) == 0) {
    char* arg = cmd + 4;
    while (*arg == ' ') arg++;

    if (strncmp(arg, "out", 3) == 0) {
      // Parse: out<N> <source>
      int outIdx = arg[3] - '0';
      if (outIdx < 0 || outIdx >= NUM_OUTPUTS) {
        Serial.println("Error: output index 0-3");
        return;
      }
      char* srcName = arg + 4;
      while (*srcName == ' ') srcName++;
      int srcIdx = findSourceIndex(srcName);
      if (srcIdx < 0) {
        Serial.print("Error: unknown source '");
        Serial.print(srcName);
        Serial.println("'");
        return;
      }
      outputSourceSel[outIdx] = srcIdx;
      selectOutputChannel(outIdx, srcIdx);
      drawMenu();
      Serial.print(outputNames[outIdx]);
      Serial.print(" -> ");
      Serial.println(sourceNames[srcIdx]);
    } else if (strncmp(arg, "marge ", 6) == 0) {
      float val = atof(arg + 6);
      if (val > 0.0f) {
        Marge = val;
        CW_In.set_threshold(val);
        Serial.print("Marge = ");
        Serial.println(Marge, 3);
      } else {
        Serial.println("Error: invalid value");
      }
    } else {
      Serial.println("Error: unknown set parameter");
    }
  } else {
    Serial.print("Unknown command: ");
    Serial.println(cmd);
    Serial.println("Type 'help' for commands");
  }
}

void processSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdIndex > 0) {
        cmdBuffer[cmdIndex] = '\0';
        executeCommand(cmdBuffer);
        cmdIndex = 0;
      }
    } else {
      if (cmdIndex < CMD_BUF_SIZE - 1) {
        cmdBuffer[cmdIndex++] = c;
      } else {
        cmdIndex = 0;  // overflow: discard
      }
    }
  }
}

// ----------------------------
// loop()
// ----------------------------
void loop() {
  sample();                                                  // ----- check for tone

  if (encoder.valueChanged()) {
    Pos_enc = encoder.getValue();
    int direction = (Pos_enc > old_Pos_enc) ? 1 : -1;
    old_Pos_enc = Pos_enc;
    handleEncoder(direction);
  }

  tunesw.update();
  if (tunesw.pressed()) {
    handleButton();
  }
  updateOscilloscope();
  processSerialInput();
  decode();
}


/***********************************gestion de l'encodeur et menu*****************************************************/

void handleEncoder(int direction) {
  if (!menuEditMode) {
    // Navigation mode: rotate selects which row is highlighted
    menuSelectedRow += direction;
    if (menuSelectedRow < 0) menuSelectedRow = NUM_MENU_ROWS - 1;
    if (menuSelectedRow >= NUM_MENU_ROWS) menuSelectedRow = 0;
  } else {
    // Edit mode: rotate changes the source for the selected output (circular)
    int src = outputSourceSel[menuSelectedRow] + direction;
    if (src < 0) src = NUM_SOURCES - 1;
    if (src >= NUM_SOURCES) src = 0;
    outputSourceSel[menuSelectedRow] = src;
    selectOutputChannel(menuSelectedRow, src);
  }
  drawMenu();
  delay(80);
}

void handleButton() {
  if (menuSelectedRow == 4) {
    // Scope serial output: request one sweep of serial data
    scopeSerialRequested = true;
    scopeBarFlashTime = millis();
    drawScopeBar();
    if (Debug) {
      Serial.println("Scope serial requested");
    }
    return;
  }
  menuEditMode = !menuEditMode;
  drawMenu();
  if (Debug) {
    Serial.print("Menu edit mode: ");
    Serial.println(menuEditMode ? "ON" : "OFF");
  }
}

//------------------------------------- gestion de l'affichage

void drawMenuRow(int row) {
  int y = MENU_AREA_Y + row * MENU_LINE_HEIGHT;
  bool isSelected = (row == menuSelectedRow);

  tft.setFont(Arial_14);

  // Clear the row
  tft.fillRect(0, y, SCREEN_WIDTH, MENU_LINE_HEIGHT, ILI9341_BLACK);

  // Draw function name on the left
  if (isSelected) {
    // Reverse video for selected row
    tft.fillRect(0, y, 100, MENU_LINE_HEIGHT,
                 menuEditMode ? ILI9341_YELLOW : ILI9341_WHITE);
    tft.setTextColor(ILI9341_BLACK);
  } else {
    tft.setTextColor(ILI9341_ORANGE, ILI9341_BLACK);
  }
  tft.setCursor(4, y + 3);
  tft.print(outputNames[row]);

  // Draw current source value on the same line, right side
  if (isSelected && menuEditMode) {
    tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  } else {
    tft.setTextColor(ILI9341_GREEN, ILI9341_BLACK);
  }
  tft.setCursor(110, y + 3);
  tft.print(sourceNames[outputSourceSel[row]]);
}

void drawMenu() {
  for (int i = 0; i < NUM_OUTPUTS; i++) {
    drawMenuRow(i);
  }
  drawScopeBar();
}

void drawScopeBar() {
  if (menuSelectedRow == 4) {
    uint16_t color = (millis() - scopeBarFlashTime < 200) ? ILI9341_YELLOW : ILI9341_WHITE;
    tft.fillRect(0, SCOPE_AREA_Y, SCOPE_X_OFFSET, SCOPE_AREA_HEIGHT, color);
  } else {
    tft.fillRect(0, SCOPE_AREA_Y, SCOPE_X_OFFSET, SCOPE_AREA_HEIGHT, ILI9341_BLACK);
  }
}

void displayVersion() {
  tft.setFont(Arial_14);
  tft.fillRect(0, VERSION_LINE_Y, SCREEN_WIDTH, 18, ILI9341_BLACK);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK);
  tft.setCursor(4, VERSION_LINE_Y);
  tft.print("CW decod. ");
  tft.print(VERSION);
}

void sendToTFT1(char character)
{
  #define MAX_DECODE_H (SCREEN_WIDTH - 20)
  #define MAX_DECODE_V (DECODE_AREA_Y + DECODE_AREA_HEIGHT - 20)
  posH = posH + 15;
  if (posH > MAX_DECODE_H) {
    posH = 10;
    posV = posV + 20;
  }
  if (posV > MAX_DECODE_V) {
    // Wrap around: clear decode area
    posH = 10;
    posV = DECODE_AREA_Y + 2;
    tft.fillRect(0, DECODE_AREA_Y, SCREEN_WIDTH, DECODE_AREA_HEIGHT, ILI9341_BLACK);
  }
  tft.setFont(Arial_14);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setCursor(posH, posV);
  tft.print(Symbol[Index]);
}

// -------------------------------------------------- sample()  Décodage d'un élément de caractère


boolean sample() {
  // ----- Locals
  int no_tone_ = 0;                                     // Integrator
  int tone_ = 0;                                        // Integrator

  // -------------------------------------------------- check for tone/no_tone

  while (1) {
    if (!CW_In.has_power_value_available()){
      delay(1);
      continue;
    }
    float toneValue = CW_In.get_power_value();
    if (false && Debug) {
      Serial.print(toneValue);                          // décodage de la note par CW_in
      Serial.print("\t");
      Serial.print(Marge);
      Serial.print("\t");
      Serial.println(toneValue > Marge);
    }

    if (toneValue > Marge) {
      tone_++;
      Tone = true;
      digitalWrite(LED, HIGH);                                  // LED glows when signal present
      return true;

    } else {                                                       // ----- no_tone
      tone_ = 0;                                                // empty opposite integrator
      no_tone_++;
      Tone = false;                                             // integrate
      digitalWrite(LED, LOW);
      return false;
    }
  }
}


void decode() {
  /////////////////////////////////////////////////
  //  Wait for initial leading_edge
  /////////////////////////////////////////////////

  // ----- Wait for initial leading_edge
  if (!Started && !Measuring && Tone) {                         // initial leading edge detected
    Leading_edge = millis();
    Letter_start = Leading_edge;                                // record first edge... used to detect timeout

    // ----- insert word space
   

    if ((millis() - Trailing_edge) > Reference * 3) {           // detect word end
      if (Debug) {
      Serial.print(' ');  
      }                                      // if so insert a space
      //sendToTFT1(' ');
    }

    // ----- now look for a trailing edge
    Started = true;
    Measuring = true;
  }

  /////////////////////////////////////////////////
  //  Wait for trailing_edge
  /////////////////////////////////////////////////

  // ----- Wait for trailing leading_edge
  if (Started && Measuring && !Tone) {                          // trailing edge detected

    // ----- record time of last trailing edge
    Trailing_edge = millis();                                   // needed for detecting word gap

    // ----- calculate tone duration
    Duration = Trailing_edge - Leading_edge;

    // ----- use default reference until we have received both a dot and a dash
    if ((Tone_min < 9999) && (Tone_max > 0) && (Tone_min != Tone_max)) {

      // ---- perform rolling dot/dash averages
      if (Duration < Reference) {

        // ----- we have a dot
        Tone_min = (Tone_min + Duration) / 2;
        Tone_duration[Tone_index] = Tone_min;                    // record average Tone_min tone duration
        Tone_index++;
        Reference = (Tone_min + Tone_max) / 2;

      } else {

        // ----- we have a dash
        Tone_max = (Tone_max + Duration) / 2;
        Tone_duration[Tone_index] = Tone_max;                    // record average Tone_min tone duration
        Tone_index++;
        Reference = (Tone_min + Tone_max) / 2;
      }

    } else {

      // ----- save initial (raw) tones to array

      Tone_duration[Tone_index] = Duration;                       // record tone duration
      Tone_min = min(Tone_min, Tone_duration[Tone_index]);        // needed to calculate reference when decoding
      Tone_max = max(Tone_max, Tone_duration[Tone_index]);
      Tone_index++;
    }

    // ----- now look for a leading edge
    Started = true;
    Measuring = false;
  }

  /////////////////////////////////////////////////
  //  Wait for second and subsequent leading edges
  /////////////////////////////////////////////////


  // ----- measure space duration
  if (Started && !Measuring & Tone) {

    // ----- a leading edge has arrived
    if ((millis() - Trailing_edge) <  Reference) {          // detects gaps between letter elements
      Leading_edge = millis();

      // ----- now look for a trailing edge
      Started = true;
      Measuring = true;
    }
  }

  /////////////////////////////////////////////////
  //  Timeout
  /////////////////////////////////////////////////


  // ----- a leading edge hasn't arrived
  if (Started && !Measuring & !Tone) {

    // ----- timeout
    if ((millis() - Trailing_edge) >  Reference) {

      // ----- calculate new reference
      if (Tone_max != Tone_min) {                          // use old reference if letter is all dots / dashes
        Reference = (Tone_max + Tone_min) / 2;
      }

      // ----- move the pointers
      for (short i = 0; i < Tone_index; i++) {
        (Tone_duration[i] < Reference) ? dot() : dash();
      }

      // ----- print letter
      if (Debug) {
        Serial.print(Symbol[Index]); 
      } 
      if(Symbol[Index] > 0x20)                       // print letter to Serial Monitor
      {
        sendToTFT1(Symbol[Index]); 
      }
                                 // print character to TFT display

      // ----- calculate WPM
      WPM = ((Symbol_count - 1) * 1200) / (Trailing_edge - Letter_start);

      // ----- reset pointers
      Index = 63;                                          // reset binary pointers
      Offset = 32;
      Count = 6;

      Tone_index = 0;                                     // reset decoder
      Symbol_count = 0;

      // ----- point to start
      Started = false;
      Measuring = false;
    }
  }
}

// ------------------------------
//   dot()
// ------------------------------
void dot() {

  // ----- we have a dot
  Index -=  Offset;                                       // move the pointer left
  Offset /= 2;                                            // prepare for next move
  Count--;
  Symbol_count += 2;                                      // 1*dot + 1*space

  // ----- error check
  if (Count < 0) {

    Index = 63;                                          // reset  binary pointers
    Offset = 32;
    Count = 6;

    Reference = Start_reference;                         // reset decoder
    Tone_min = 9999L;
    Tone_max = 0L;
    Tone_index = 0;
    Symbol_count = 0;
    if (Debug) {
      Serial.println("error in dot");
    }
    // ----- point to start
    Started = false;
    Measuring = false;
  }
}

// ------------------------------
//   dash()
// ------------------------------
void dash() {

  // ----- we have a dash
  Index +=  Offset;                                       // move pointer right
  Offset /= 2;                                            // prepare for next move
  Count--;
  Symbol_count += 4;                                      // 3*dots + 1*space

  // ----- error check
  if (Count < 0) {

    Index = 63;                                          // reset  binary pointers
    Offset = 32;
    Count = 6;

    Reference = Start_reference;                         // reset decoder
    Tone_min = 9999L;
    Tone_max = 0L;
    Tone_index = 0;
    Symbol_count = 0;
    if (Debug) {
      Serial.println("error in dash");
    }
    // ----- point to start
    Started = false;
    Measuring = false;
  }
}

// Teensy 4.0, 4.1
/*****
  Purpose: To set the I2S frequency

  Parameter list:
    int freq        the frequency to set

  Return value:
    int             the frequency or 0 if too large

*****/
#if 1
int setI2SFreq(int freq) {
  int n1;
  int n2;
  int c0;
  int c2;
  int c1;
  double C;

  if (Debug) {
    Serial.printf("Setting sampling freq to %d\n", freq);
  }
  // PLL between 27*24 = 648MHz und 54*24=1296MHz
  // Fudge to handle 8kHz - El Supremo
  if (freq > 8000) {
    n1 = 4;  //SAI prescaler 4 => (n1*n2) = multiple of 4
  } else {
    n1 = 8;
  }
  n2 = 1 + (24000000 * 27) / (freq * 256 * n1);
  if (n2 > 63) {
    // n2 must fit into a 6-bit field
#ifdef DEBUG
    Serial.printf("ERROR: n2 exceeds 63 - %d\n", n2);
#endif
    return 0;
  }
  C = ((double)freq * 256 * n1 * n2) / 24000000;
  c0 = C;
  c2 = 10000;
  c1 = C * c2 - (c0 * c2);
  set_audioClock(c0, c1, c2, true);
  CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
               | CCM_CS1CDR_SAI1_CLK_PRED(n1 - 1)   // &0x07
               | CCM_CS1CDR_SAI1_CLK_PODF(n2 - 1);  // &0x3f

  CCM_CS2CDR = (CCM_CS2CDR & ~(CCM_CS2CDR_SAI2_CLK_PRED_MASK | CCM_CS2CDR_SAI2_CLK_PODF_MASK))
               | CCM_CS2CDR_SAI2_CLK_PRED(n1 - 1)   // &0x07
               | CCM_CS2CDR_SAI2_CLK_PODF(n2 - 1);  // &0x3f)
  return freq;
}
#else
void setI2SFreq(int freq1)                                // merci Franck B.
{
  // PLL between 27*24 = 648MHz und 54*24=1296MHz
  int n1 = 4; //SAI prescaler 4 => (n1*n2) = multiple of 4
  int n2 = 1 + (24000000 * 27) / (freq1 * 256 * n1);
  double C = ((double)freq1 * 256 * n1 * n2) / 24000000;
  int c0 = C;
  int c2 = 10000;
  int c1 = C * c2 - (c0 * c2);
  set_audioClock(c0, c1, c2, true);
  CCM_CS1CDR = (CCM_CS1CDR & ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK))
       | CCM_CS1CDR_SAI1_CLK_PRED(n1-1)                                // &0x07
       | CCM_CS1CDR_SAI1_CLK_PODF(n2-1);                               // &0x3f
  Serial.printf("SetI2SFreq(%d)\n",freq1);
}
#endif