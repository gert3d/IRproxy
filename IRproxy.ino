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
 *  5 Keys on the Motorola remote / Samsung remote are identified, these are re-encoded and re-transmitted for Pioneer 
 * 
 *
 */ 

#include <avr/interrupt.h>
#include <avr/io.h>

// microsecod delays for the Pioneer IR signals
#define PAUSE  576         // micro-seconds duration (.), active 38KHz pulse
#define SHORT  498         // micro-seconds duration (S), as a period between PAUSEs
#define LONG  1547         // micro-seconds duration (L), as a period between PAUSEs
#define INIT  8577         // micro-seconds duration (I), as a period between PAUSEs
#define GO  4217           // micro-seconds duration (G), active 38KHz pulse
#define CONNECT 27000      // micro-seconds duration (C), active 38KHz pulse


//static const char MSG01[] = "Analyze IR Remote";
static const char MSG02[] = "Waiting...for IR input";
static const char MSG03[] = "\nReceiving a new code:";
static const char MSG04[] = " We went high";
static const char MSG05[] = " We went low again";
static const char MSG06[] = " The IR code timed out at data-period: ";
static const char MSG07[] = " The received code is: (ignoring the separating pulses when the detector is active, LOW)";
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

// For the Pioneer remote the following codes will be used:
// vol+:           IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .S.L.S.L.S.S.S.S.L.S.L.S.L.L.L.L .C IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .S.L.S.L.S.S.S.S.L.S.L.S.L.L.L.L .C  135 periods total
// vol-:           IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .L.L.S.L.S.S.S.S.S.S.L.S.L.L.L.L .C IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .L.L.S.L.S.S.S.S.S.S.L.S.L.L.L.L .C
// volx:           IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .S.L.S.S.L.S.S.S.L.S.L.L.S.L.L.L .C IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .S.L.S.S.L.S.S.S.L.S.L.L.S.L.L.L .C
// OnOff:          IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .S.S.L.L.L.S.S.S.L.L.S.S.S.L.L.L .C IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .S.S.L.L.L.S.S.S.L.L.S.S.S.L.L.L .C
// DVD:            IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .L.S.L.S.S.S.S.L.S.L.S.L.L.L.L.S .C IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .L.S.L.S.S.S.S.L.S.L.S.L.L.L.L.S .C
// SAT:            IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .S.S.S.S.L.S.S.S.L.L.L.L.S.L.L.L .C IG .L.S.L.S.S.L.S.L.S.L.S.L.L.S.L.S .S.S.S.S.L.S.S.S.L.L.L.L.S.L.L.L .C
//                 0/1      2 - common - 33                     34 - variable - 65      66 68   70 - same common part - 101     102 -  same variable part - 133 134/135
//
// build this up like this:
 static const String common = "LSLSSLSLSLSLLSLS";       // 32 pulses, each code-bit is preceeded by a pause (this is a common code for all of them: full code will be : IG- common - function - C - IG - common - function - C)
  static const String volUP = "SLSLSSSSLSLSLLLL";       // 32 pulses, each code-bit is preceeded by a pause 
static const String volDOWN = "LLSLSSSSSSLSLLLL";       // 32 pulses, each code-bit is preceeded by a pause
   static const String mute = "SLSSLSSSLSLLSLLL";       // 32 pulses, each code-bit is preceeded by a pause
  static const String OnOff = "SSLLLSSSLLSSSLLL";       // 32 pulses, each code-bit is followed by a pause 
    static const String DVD = "LSLSSSSLSLSLLLLS";       // 32 pulses, each code-bit is followed by a pause 
    static const String SAT = "SSSSLSSSLLLLSLLL";       // 32 pulses, each code-bit is followed by a pause 
    


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
  digitalWrite(PrintModePin, HIGH);     // prevents SerialPrint() statements to be carried out in the sketch, unless Dig10 is connected to GND.
  
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

// this is how an example signal looks like:
// microseconds                        4800                  600             550          1500           550
//  ------------------------------|____________________|---------------|___________|-----------------|_________|----- //  ---------------------- Detector value
//  Start                         0                    1               2           3                 4         5                                 SignalNumber
//                                L                    H               L           H                 L         H                                 DetectorLevel
//                                0                  4800             5400        5950             7450       8000                               TimerValue[SignalNumber]
//                                      START                SHORT          PAUSE         LONG          PAUSE                                     ->  timeout

  

  while(digitalRead(IRdetectPin) == HIGH) {}                  // Initial waiting loop - do nothing until IR receiver goes low (indicating signal detected) a 38KHz puls train has begun, so let's decode it
                                              // YES, at this point a signal is being received, prepare to decode it
  TCNT1 = 0;                                  // Resets the TCNT1 counter to 0   
  time = 0;                                   // record start-time of the first 38KHz pulse 
  DetectorLevel = 'L';                        // L indicates that IR detector went LOW


  ReceiveIR();
  
  if (SignalNumber >= 4) {          // avoid false signals
    if (digitalRead(PrintModePin) == LOW) {
      Serial.println(MSG03);
      Serial.print(MSG06);
      Serial.println(SignalNumber-1);  
      Serial.println(MSG07);            // The received code is:
      Serial.print(IRcode.substring(0,17));
      Serial.print(" ");
      Serial.print(IRcode.substring(17,30));
      Serial.print(" ");
      Serial.println(IRcode.substring(30,SignalNumber-1));      
//      Serial.println(IRcode);
    }
    
    DecodeIR();
    if (digitalRead(PrintModePin) == LOW) Serial.println(decoded_signal);
    
    SendIR();   
   
//    if (digitalRead(PrintModePin) == LOW) { 
      Serial.println();
      Serial.print(MSG17);                      // "free RAM ";
      Serial.println(freeRam());   
//    }
  }
}

// ------------------------------------------------------- Subroutines ----------------------------------------------------------------------------------------------------------------------------

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


void DecodeIR() {                                                                              // this routine looks into the received pulse-code-string and constructs the pulse-code-string to be sent out again
     if (IRcode.substring(17,30) == "LLLSSSSSSSSLL") decoded_signal = 1;                       // Motorola/Samsung remote sends "Volume UP"
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
   switch (decoded_signal) {    // first construct the IRcode to send out
     case 0:
       IRcode = "";
       break;
     case 1:
       IRcode = "G" + common + volUP + "CG" + common + volUP + "C";
       break;
     case 2:
       IRcode = "G" + common + volDOWN + "CG" + common + volDOWN + "C";
       break;
     case 3:
       IRcode = "G" + common + mute + "CG" + common + mute + "C";
       break;
     case 4:
       IRcode = "G" + common + OnOff + "CG" + common + OnOff + "C";
       break;
     case 5:
       IRcode = "G" + common + DVD + "CG" + common + DVD + "C";
       break;
   }
   
   if (digitalRead(PrintModePin) == LOW) {
     Serial.print("IRcode = ");
     Serial.println(IRcode);
     Serial.print("Length IRcode = ");
     Serial.println(IRcode.length());
   }
}



void SendIR() {             // see : http://forum.arduino.cc/index.php?topic=45499.0  

   digitalWrite(LEDpin, HIGH);                        // turn on the indicator LED 
   timeout = IRcode.length();

   for (int j = 0 ; j < timeout ; j++) {

     if (IRcode[j] == 'S') time = SHORT;
       else {
         if (IRcode[j] == 'L') time = LONG;
         else {
           if (IRcode[j] == 'G') time = GO;
           else {
             if (IRcode[j] == 'C') time = CONNECT;
           }
         }
       }
       
// Connect pin to signal, this turns on the IR LED (in a PWM mode, see value OCR2A)
       TCCR2A |=  _BV(COM2A0);         // Fast PWM Mode, see: http://arduino.cc/en/Tutorial/SecretsOfArduinoPWM
// Duration depends of separating code-bit, only in case of GO, it needs to be INIT long, otherwise use PAUSE (see Pioneer IR code)
       if (time == GO) delayMicroseconds(INIT);
       else delayMicroseconds(PAUSE);
       
       TCCR2A &= (0xFF - _BV(COM2A0)); // disconnect pin from signal
       delayMicroseconds(time);        // duration of the coding bit
       if (digitalRead(PrintModePin) == LOW) {
         Serial.print(time);
         Serial.print(" ");
       }
   }
   digitalWrite(IRsendPin,LOW);              // turn pin low in case it's still high
   digitalWrite(LEDpin, LOW);                // turn off indicator LED
}
