/*
 *  File....... IRanalyzer.pde 
 *  Purpose.... Records up to 192 signal changes and prints in format for use with TransmitIRSignal 
 *  Author..... Walter Anderson, modified by Tom Lauwers
 *  E-mail..... wandrson@walteranderson.us/tlauwers@birdbraintechnologies.com
 *  Started.... 18 May 2007 
 *  Updated.... 21 Dec 2011
 *  Original at: http://www.arduino.cc/playground/Code/InfraredReceivers
 * 
 *  Modified by Gert de Vries (specific goal) sept 2013
 *  38KHz IR detector : pin1 = DIG2, pin2 = GND, pin3 = 5V
 *  Goal: decode Samsung remote, and send out Pioneer code's for the same functions. 
 *  Decoding works ok. Make sure not to waste time - like Serial.print() - in the timing sections, since that messes things up. 
 *  5 Keys on the Motorola remote are identified, these now need to be encoded and transmitted for Pioneer 
 *
 */ 

#include <avr/interrupt.h>
#include <avr/io.h>

// microsecod delays for the Pioneer IR signals
#define PAUSE  576         // micro-seconds duration
#define SHORT  498         // micro-seconds duration
#define LONG  1547         // micro-seconds duration
#define INIT  8577         // micro-seconds duration
#define GO  4217           // micro-seconds duration


//static const char MSG01[] = "Analyze IR Remote";
static const char MSG02[] = "Waiting...for IR input";
static const char MSG03[] = "\nReceiving a new code:";
static const char MSG04[] = " We went high";
static const char MSG05[] = " We went low again";
static const char MSG06[] = " The IR code timed out at data-period: ";
static const char MSG07[] = " The received code is: ";
static const char MSG17[] = "free RAM ";

static const int IRdetectPin = 2;               // Sets the pin the receiver is on
static const int LEDpin = 13;              // indicate signal reception
static const int IRsendPin = 11;           // Sets the pin the IR LED is on
static const int PrintModePin = 10;         // set HIGH using software, but when set LOW (jumper), the SerialPrint() statements in the sketch will NOT be carried out

char DetectorLevel;            // Keeps track of detector mode (LOW = 38KHz pulse = pause, HIGH = no 38KHz pulse = data or timeout)
int SignalNumber = 0;               // number all the level changes of the IR pulses (this is 1 more than the number of periods recorded)
unsigned short time;                     // period in u-seconds of the pulses and pauses
int timeout = 0;              // Times out if no signal detected for a long period
boolean timedOut = false;
String IRcode = "";      // String for the received IR code
int decoded_signal;
boolean toggleLED = true;     // Flag to determine if we need to turn LED on or off

enum Pulse
{
  Init,
  Long,
  Short,
};


static const int ReceiveBufferLength = 256;
static const char ReceiveBuffer[ReceiveBufferLength] = {};
static int ReceiveBufferIndex = 0;

void ClearCharArray(char charArray[], int length)
{
  for(int i = 0; i < length; i++)
  {
    charArray[i] = 0x00;
  }
}

// For the Pioneer remote the following codes will be used:
// vol+:           IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. S.L.S.L.S.S.S.S.L.S.L.S.L.L.L.L. C IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. S.L.S.L.S.S.S.S.L.S.L.S.L.L.L.L.  135 periods total
// vol-:           IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. L.L.S.L.S.S.S.S.S.S.L.S.L.L.L.L. C IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. L.L.S.L.S.S.S.S.S.S.L.S.L.L.L.L.
// volx:           IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. S.L.S.S.L.S.S.S.L.S.L.L.S.L.L.L. C IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. S.L.S.S.L.S.S.S.L.S.L.L.S.L.L.L.
// OnOff:          IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. S.S.L.L.L.S.S.S.L.L.S.S.S.L.L.L. C IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. S.S.L.L.L.S.S.S.L.L.S.S.S.L.L.L.
// DVD:            IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. L.S.L.S.S.S.S.L.S.L.S.L.L.L.L.S. C IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. L.S.L.S.S.S.S.L.S.L.S.L.L.L.L.S.
// SAT:            IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. S.S.S.S.L.S.S.S.L.L.L.L.S.L.L.L. C IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S. S.S.S.S.L.S.S.S.L.L.L.L.S.L.L.L.
//                         0 - common - 34                     35 - variable - 66      67     68 - same common part - 102     103 -  same variable part - 134
//
static const String volUP = "S.L.S.L.S.S.S.S.L.S.L.S.L.L.L.L.";       // 32 pulses  (including separating pauses) 
static const String volDOWN = "L.L.S.L.S.S.S.S.S.S.L.S.L.L.L.L.";       // 32 pulses
static const String mute = "S.L.S.S.L.S.S.S.L.S.L.L.S.L.L.L.";       // 32 pulses
static const String common = "IG.L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S.";    // 35 pulses (this is a common code for all of them: full code will be : common - function - common - function)
static const String OnOff = "S.S.L.L.L.S.S.S.L.L.S.S.S.L.L.L.";
static const String DVD = "L.S.L.S.S.S.S.L.S.L.S.L.L.L.L.S.";
//String SAT = "S.S.S.S.L.S.S.S.L.L.L.L.S.L.L.L.";


void setup() {
  Serial.begin(57600);
  Serial.println();
  Serial.println(F("Analyze IR Remote"));                    // "Analyze IR Remote";
  Serial.print(MSG17);                      // "free RAM ";
  Serial.println(freeRam()); 
  Serial.println(MSG02);                    //  "Waiting...for IR input";
  TCCR1A = 0x00;          // COM1A1=0, COM1A0=0 => Disconnect Pin OC1 from Timer/Counter 1 -- PWM11=0,PWM10=0 => PWM Operation disabled
  // ICNC1=0 => Capture Noise Canceler disabled -- ICES1=0 => Input Capture Edge Select (not used) -- CTC1=0 => Clear Timer/Counter 1 on Compare/Match
  // CS12=0 CS11=1 CS10=1 => Set prescaler to clock/64
  TCCR1B = 0x03;          // 16MHz clock with prescaler means TCNT1 increments every 4uS
  // ICIE1=0 => Timer/Counter 1, Input Capture Interrupt Enable -- OCIE1A=0 => Output Compare A Match Interrupt Enable -- OCIE1B=0 => Output Compare B Match Interrupt Enable
  // TOIE1=0 => Timer 1 Overflow Interrupt Enable
  TIMSK1 = 0x00;          
  pinMode(IRdetectPin, INPUT);
  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin, LOW);
  pinMode(PrintModePin, INPUT);
  digitalWrite(PrintModePin, HIGH);     // prevents SerialPrint() statements to be carried out in the sketch, unless there is a resistor from Dig10 to GND.
  
  // for the broadcast of IR code's:
  pinMode(IRsendPin, OUTPUT);    
  // PWM Magic - we directly set atmega registers to 50% duty cycle,
  // variable frequency mode. Currently set to 38 KHz.
  // Thanks http://arduino.cc/en/Tutorial/SecretsOfArduinoPWM .
  // also see http://arduinodiy.wordpress.com/2012/02/28/timer-interrupts/ for info
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  //set prescalar to 16MHz clock for now
  TCCR2B = _BV(WGM22) | _BV(CS20);
  // To get a 38KHz wave, we need OCR2A = (16000000/76000)-1 = 209
  // for 36KhZ this would be: 16M/72K - 1 = 221
  // and for 40KHz: 16M/80K -1 = 199
  // OCRA2A = 140 for 56KHz operation
  OCR2A = 209; 

// periods in microseconds
// Init (I): -8577
// GO (G): +4217
// Pause (.): -576
// Long (L): +1547
// Short (S): +498
// Connect (C): 27130 (apart invoegen

//common = String("IG.H.S.H.S.S.H.S.H.S.H.S.H.H.S.H.S.");
//volUP = String("S.H.S.H.S.S.S.S.H.S.H.S.H.H.H.H.");
//volDOWN = String("H.H.S.H.S.S.S.S.S.S.H.S.H.H.H.H.");
//mute = String("S.H.S.S.H.S.S.S.H.S.H.H.S.H.H.H.");
//OnOff = String("S.S.H.H.H.S.S.S.H.H.S.S.S.H.H.H.");
//DVD = String("H.S.H.S.S.S.S.H.S.H.S.H.H.H.H.S.");
//SAT = String("S.S.S.S.H.S.S.S.H.H.H.H.S.H.H.H.");

//common[] = (8577, 4217,576, 1547, 576);
//
}

void loop() {
//  Serial.flush();                 // Reset counters and flags
  IRcode = "";
  SignalNumber = 1;      
  timeout = 0;
  timedOut = false;
  digitalWrite(LEDpin, LOW);
  
  TCCR1A = 0x00;
  TCCR1B = 0x03;
  TIMSK1 = 0x00;
  
// microseconds                        500          600             1500
//  ------------------------------|____________|---------------|_________________|-------------------------------------------- Detector value
//  Start                         0            1               2                 3                                             SignalNumber
//                                L            H               L                 H                                             DetectorLevel
//                                0           500             1100              2600                                           TimerValue[SignalNumber]
//                                    time           time            time                                  ->  timeout

  

  while(digitalRead(IRdetectPin) == HIGH) {}                  // Initial waiting loop - do nothing until IR receiver goes low (indicating signal detected) a 38KHz puls train has begun, so let's decode it
                                               // YES, signal received, prepare to receive it
  TCNT1 = 0;                                  // Resets the TCNT1 counter to 0   
  time = 0;                                    // record start-time of the first 38KHz pulse 
  DetectorLevel = 'L';                         // L indicates that IR detector went LOW


  ReceiveIR();
  
  if (SignalNumber >= 4) {          // avoid false signals
    if (digitalRead(PrintModePin) == LOW) {
      Serial.println(MSG03);
      Serial.print(MSG06);
      Serial.println(SignalNumber-1);  
      Serial.println(MSG07);            // The received code is:
//      Serial.print(IRcode[17]);Serial.print(IRcode[18]);Serial.print(IRcode[19]);Serial.print(IRcode[20]);Serial.print(IRcode[21]);Serial.print(IRcode[22]);Serial.print(IRcode[23]);Serial.print(IRcode[24]);Serial.print(IRcode[25]);Serial.print(IRcode[26]);Serial.print(IRcode[27]);Serial.print(IRcode[28]);Serial.println(IRcode[29]);
      Serial.print(IRcode.substring(0,17));
      Serial.print(" ");
      Serial.print(IRcode.substring(17,30));
      Serial.print(" ");
      Serial.println(IRcode.substring(30,SignalNumber-1));      
      Serial.println(IRcode);
    }
    
    DecodeIR();
    if (digitalRead(PrintModePin) == LOW) Serial.println(decoded_signal);
    
    toggleLED = true;
    SendIR();   
   
    if (digitalRead(PrintModePin) == LOW) { 
      Serial.println();
      Serial.print(MSG17);                      // "free RAM ";
      Serial.println(freeRam());   
    }
  }
}

// example vol+
// 0         0         0         0   33
// XLLLSSSSSLLLSSSSSLLLSSSSSSSSLLLLLX
//                  HHHSSSSSSSSHH
//	            17 ------- 29

// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int freeRam () {                                   // see: http://playground.arduino.cc/Code/AvailableMemory
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}




void ReceiveIR() {
  while (!timedOut) {                   // This is the loop where the pulses (pause) and data-periods are evaluated, get out after a timeout
                                                                 
    if (DetectorLevel == 'L') {              // First test whether we are currently in a 38KHz pulse (since then our last recorded DetectorLevel = HIGH)
      while(digitalRead(IRdetectPin) == LOW) {}                // Yes, so wait until the 38KHz pulse ends (if NO see: else - loop)
        time = TCNT1;                          // mark the start-time of the IR detector change
        DetectorLevel = 'H';                   // IR detector went HIGH, so a data-period now starts
    }      
                                              
    else {                                         // If the signal went HIGH, (DetectorLevel[SignalNumber] == 'L') wait until the pin goes low or until it times out check for a timeout
      timeout = 0;
      while(digitalRead(IRdetectPin) == HIGH && !timedOut) {          // We only need to check for timeout if the pin is high because receivers are high when not receiving signal        
        timeout++;                                    // we use the waiting period for a next low (new pulse) to determine whether this was the last pulse
        delayMicroseconds(2);                        
        if(timeout > 20000) {                        // If the IR DetectorLevel is inactive for more than 4 x 2000 useconds (8 msec) then end of the bit stream is reached: 
          timedOut = true; 
//          Serial.print(MSG06);
//          Serial.print(SignalNumber);
//         break;                                   // timed out, get out of the loop: while(digitalRead(IRdetectPin) == HIGH)
        }
      }
      time = (TCNT1 - time)*4;                   // Analyse the code as transmitted by Samsung - like remote
        if (time >= 450 && time <= 650) IRcode += 'S';           // S = SHORT signal, move 'S' into the code variable
        else { 
           if (time >= 1450 && time <= 1800)  IRcode += 'L';      // L = LONG signal, move 'L' into the code variable
            else IRcode += 'X'; // no valid signal
         }
      SignalNumber++;
      DetectorLevel = 'L';                   // IR detector went LOW, so a PAUSE period now starts
    }
  }
}


void DecodeIR() {
     if (IRcode.substring(17,30) == "LLLSSSSSSSSLL") decoded_signal = 1;                       // Motorola remote sends "Volume UP"
   else {
     if (IRcode.substring(17,30) == "LLSLSSSSSSLSL") decoded_signal = 2;                       // "Volume DOWN"
     else {
       if (IRcode.substring(17,30) == "LLLLSSSSSSSSL") decoded_signal = 3;                      // "MUTE"
       else {
         if (IRcode.substring(17,30) == "SLSSSSSSLSLLL") decoded_signal = 4;                    // "ON / OFF"
         else {
           if (IRcode.substring(17,30) == "SLSSLSSSLSLLS") decoded_signal = 5;                  // "Return (use for signal Input select: SAT or DVD)"
           else decoded_signal = 0;                                                            // "no corresponding signal"
         }
       }
     }
   }
}




void SendIR() {             // see : http://forum.arduino.cc/index.php?topic=45499.0  

   switch (decoded_signal) {    // first construct the IRcode to send out
     case 0:
       IRcode = "";
       break;
     case 1:
       IRcode = common + volUP + common + volUP;
       break;
     case 2:
       IRcode = common + volDOWN + common + volDOWN;
       break;
     case 3:
       IRcode = common + mute + common + mute;
       break;
     case 4:
       IRcode = common + OnOff + common + OnOff;
       break;
     case 5:
       IRcode = common + DVD + common + DVD;
       break;
   }
   
   if (digitalRead(PrintModePin) == LOW) Serial.println(IRcode);
  
   digitalWrite(LEDpin, HIGH);                        // next start sending the code
   for (int j = 0 ; j < IRcode.length() ; j++) {
// Turn the LED on or off (first time through LED is turned on, since toggleLED is initialised as TRUE)
       if(toggleLED) {
// Connect pin to signal, this turns on the LED (in a PWM mode, see value OCR2A)
           TCCR2A |=  _BV(COM2A0);         // Fast PWM Mode, see: http://arduino.cc/en/Tutorial/SecretsOfArduinoPWM
           toggleLED = false;
       } 
       else {
           TCCR2A &= (0xFF - _BV(COM2A0)); // disconnect pin from signal
           digitalWrite(LEDpin,LOW); // turn pin low in case it's still high
           toggleLED = true;
       } 
// Delay depends on the char in string IRcode

       switch(IRcode[j])
       {
         case '.':
           time = PAUSE;
           break;
         case 'S':
           time = SHORT;
           break;
         case 'L':
           time = LONG;
           break;
         case 'I':
           time = INIT;
           break;
         case 'G':
           time = GO;
           break;
       }

       if (digitalRead(PrintModePin) == LOW) {
          Serial.print(time);
          Serial.print(" ");
       }
       delayMicroseconds(time); 
    }
// Turn off the signal after we end the loop
    TCCR2A &= (0xFF - _BV(COM2A0)); // disconnect pin from signal
    digitalWrite(11,LOW); // turn pin low in case it's high
    toggleLED = true;  
}
