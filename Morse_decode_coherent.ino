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
#define num_chars 300
char CodeBuffer[num_chars];
char DisplayLine[num_chars+1];
int posH = 10;            // debut de la chaine de caratères horizontal
int posV = 10;            // debut de la chaine de caratères vertical

//-----------------------------utilisé par le décodeur 
float Diff = 0.2 ;
float Marge = 0.21 ;           // différence entre bin analysé et bin exterieur (~ noise )
float y = 0.2 ;               // niveau de bruit 
float x = 0 ;                 // valeur du bin mesuré_désiré)

 //-------------------------------Utilisé par myFFT
 #define FFT_SIZE 1024                                    // Valeur FFT
 #define WATERFALL_ROW_COUNT 1                           // nombre de lignes 


 

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

boolean Etat ;             // memoire poussoir encodeur
volatile int oldStep;
volatile int8_t Step;
volatile int8_t Pos_enc = 0;                                    // position encodeur
volatile int8_t old_Pos_enc = 0;

#define  TUNESW      4                              // poussoir de l'encodeur
Button tunesw = Button();

using namespace EncoderTool;                         // switch rotatif
Encoder encoder;

const short LED = 5;     

#define VERSION "decodeur CW coherent"
#define DATE "02 01 2026"
#define AUTEUR " F1FGV et F1VL"


const int myInput = AUDIO_INPUT_LINEIN;                 // entrée connecteur 10 pins
//const int myInput = AUDIO_INPUT_MIC;                      // entrée connecteur MIC




// GUItool: begin automatically generated code
AudioInputI2S                      I2s1;           //xy=89,393
//AudioAnalyzeFFT1024_IQ_F32         myFFT;          //xy=336,140
AudioCoherentDemod4x_F32           CW_In;      //xy=474,405
AudioAnalyzeNoteFrequency          notefreq1 ;
AudioConvert_I16toF32              cnvrtI2F0 ;
AudioConvert_F32toI16              cnvrtF2I0 ;
AudioConvert_F32toI16              cnvrtF2I1 ;
AudioOutputI2S                     I2s2;           //xy=1288,393
AudioConnection              patchCord1(I2s1, 1, notefreq1,0);                // en 16 bits utilisé pour test 
AudioConnection              patchCord2(I2s1, 1, cnvrtI2F0,0);                   // 16 bits vers Float32

AudioConnection_F32          patchCord3(cnvrtI2F0, 0, CW_In, 0);                 // en 32 bits décodage de la tonalité

//AudioConnection_F32          patchCord4(cnvrtI2F0, 0, myFFT, 0);                 // en 32 bits
//AudioConnection_F32          patchCord5(cnvrtI2F0, 0, myFFT, 1);                 // en 32 bits


// connections to output channels
#if Debug
  // 0 power (lp filtered)
  // 1 prefilter (after)
  // 2 besselfilter (after)
  // 3 I subsampled by decimation_factor
  // 4 Q subsampled by decimation_factor
  // 5 instant phase (subsampled by decimation factor)
AudioConnection_F32          patchCord4(CW_In, 3, cnvrtF2I0, 0);      // debug by sending internal taps to output 
AudioConnection_F32          patchCord5(CW_In, 4, cnvrtF2I1, 0); 
#else // monitoring
AudioConnection_F32          patchCord4(cnvrtI2F0, 0, cnvrtF2I0, 0);  // audio monitor by sending input directly to output  
AudioConnection_F32          patchCord5(cnvrtI2F0, 0, cnvrtF2I1, 0);   
#endif

AudioConnection              patchCord8(cnvrtF2I0, 0, I2s2, 0);
AudioConnection              patchCord9(cnvrtF2I1, 0, I2s2, 1); 

AudioControlSGTL5000              sgtl5000_1;    //xy=503,960
// GUItool: end automatically generated code




//------------------------------------------------------------ setup()

void setup() {

  // -------------------------------------------------------------------- configure serial port
  Serial.begin(115200);
  SetI2SFreq(CW_In.get_f_sampling());
  //---------------------------------------------------------configure paramètres SGTL5000
  sgtl5000_1.enable();
  AudioMemory(100);                                       // pour le 16 bits 
  AudioMemory_F32(200);                                   // pour le 32 bits 
  sgtl5000_1.inputSelect(myInput );
  sgtl5000_1.volume(0.6);
  sgtl5000_1.lineInLevel (11);                            // 0 à 15   15: 0.24Vpp    11: 0.48Vpp   // avoid saturation !
  sgtl5000_1.lineOutLevel (13);                           // 13 to 31     13 = maximum
  // myFFT.setOutputType(0);                                 // type de donnée FFT
  // myFFT.setXAxis(0X01);                                   // sens de lecture 
  // myFFT.setNAverage(5);
  // myFFT.windowFunction(AudioWindowHanning1024);

  notefreq1.begin(0.08f);
  
  encoder.begin(3,0);                                          // deux fils de l'encodeur partie rotative

  tunesw.attach(TUNESW,INPUT_PULLUP);                           // gestion poussoir switch rotatif
  tunesw.interval(5);
  tunesw.setPressedState(LOW);

// initialize encoder
  Step = Marge * 100;
  oldStep = Step;

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
  tft.print(DATE);
  tft.setCursor(30, 136);
  tft.print(AUTEUR);
  delay(3000);
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(100, 10);
  tft.setTextColor(ILI9341_ORANGE);
  displayStatus();

  // ------------------------------------------------------------ configure LED
  pinMode(LED, OUTPUT);                               // signal/tuning indicator
  digitalWrite(LED, LOW);
 //-------------------------------------------------------------- pour le décodage        
  Started = false;
  Measuring = false;
}

// ----------------------------
// loop()
// ----------------------------
void loop() {
  sample();                                                  // ----- check for tone

  if (encoder.valueChanged()) {                                   // l'encodeur a été tourné ?
    if (Debug) {
      Serial.println("encodeur tourne") ;
    }
    Pas();                                                      // OK On change 
  }

  tunesw.update();
  if(tunesw.pressed()){
    updateStatus ();
  }
  decode();
}


/***********************************gestion de l'encodeur*****************************************************/

void Pas() 
{
  Pos_enc = (encoder.getValue());
  if (Debug) {
    Serial.println (Pos_enc);
  }
  if ( Pos_enc > old_Pos_enc ) { 
    Step = Step+1;
    oldStep=Step-1;
    old_Pos_enc= Pos_enc;
    if (Step >= 100) {                              // valeur max encodeur
      Step = 100 ;
    }
    set_Choix_Rot();    
  } else {
    Step = Step - 1;
    oldStep=Step+1;
    old_Pos_enc= Pos_enc;
    if (Step <= 1) {                                                //valeur min encodeur
      Step = 1;
    }
    set_Choix_Rot();
  }
}

void set_Choix_Rot()
{
  Marge = Step/100.0;
  displayBottom();
  delay(100);                              // Pour éviter les appuis multiples 
}

//------------------------------------- gestion de l'affichage                                           
char  statsMessage[40];


void  updateStatus() 
{
  sprintf(statsMessage,"freq offset: %7.2f", CW_In.getFrequencyOffsetHz());
  displayStatus();
} 


void sendToTFT1(char character)                   
{
  #define MAX_TEXT_V 180
  posH = posH+15;
  if(posH > SCREEN_WIDTH - 20) // next line
  {
    //tft.fillScreen(ILI9341_BLACK);
    updateStatus();
    posH=10;
    posV = posV+20;
  }
  tft.setCursor(posH,posV);
  tft.print(Symbol[Index]);
  if(posV > MAX_TEXT_V){
    posH=10;
    posV=10;
    tft.fillScreen(ILI9341_BLACK);
    displayBottom();
  }
}

void displayStatus(){
  tft.setFont(Arial_14);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK); 
  tft.setCursor(20, 200);
  tft.print(statsMessage);
}

void displayBottom(){
  displayStatus();
  tft.setFont(Arial_14);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_BLACK); 
  tft.setCursor(20, 220);
  tft.print("Marge ") ;
  tft.print(Marge) ;
  tft.setCursor(160, 220);
  tft.print(Step);
  tft.setCursor(210, 220);
  tft.print("filter ") ;
  tft.print(CW_In.get_lowpass_cutoff());
  tft.print(" Hz") ; 
}

// -------------------------------------------------- sample()  Décodage d'un élément de caractère


boolean sample() {
  // ----- Locals
  int no_tone_ = 0;                                     // Integrator
  int tone_ = 0;                                        // Integrator

  // -------------------------------------------------- check for tone/no_tone

  while (1) {
    if (!CW_In.is_power_value_available()){
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

    if (toneValue > Marge) {                               //  trouvé assez grand  ??                                                       // note au dessus du bruit + Marge ( S/N en fait )?? dans le cas de détection par valeur de bin .
      tone_++;   
      Tone = true; 
      digitalWrite(LED, HIGH)  ;                                  // LED glows when signal present
      return true;

    } else {                                                       // ----- no_tone
      tone_ = 0;                                                // empty opposite integrator
      no_tone_++;  
      Tone = false;                                             // integrate
      digitalWrite(LED, LOW)  ;   
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
int SetI2SFreq(int freq) {
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
