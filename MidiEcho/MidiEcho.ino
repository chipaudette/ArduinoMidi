// SparkFun MIDI Sheild and MIDI Breakout test code
// Defines bare-bones routines for sending and receiving MIDI data
// Written 02/16/10
//include <Streaming.h>
//include <String.h>
//include <avr/pgmspace.h>
//include "types.h"
//include "notes.h"
//include "wT128_allVow.h"
//#include "wT128_saw.h"

#define ECHOMIDI (true)

// defines for MIDI Shield components only
#define KNOB1  0
#define KNOB2  1

#define BUTTON1  2
#define BUTTON2  3
#define BUTTON3  4

#define STAT1  7
#define STAT2  6

#define OFF 1
#define ON 2
#define WAIT 3

//define ISR_HIGHLOW_PIN 9

byte byte1;
byte byte2;
byte byte3;
//int noteTranspose = -27;
//int curNote=0;
//int newNote=0;
int FS1,FS2;

//define N_POLY 3
//volatile noteStruct allNotes[N_POLY];
//define HALF_STEP_FRAC (0.05946309)

//#define OUT_PIN 11
//float targSampleRate_Hz = 5000;  //up to 14000...7812.5...if PWM is at 16MHz/256/2, this is 4 PWM periods


void setup() {
  pinMode(STAT1,OUTPUT);
  pinMode(STAT2,OUTPUT);
  
  //start serial with midi baudrate 31250
  Serial.begin(31250);     
}

void loop () {
  static byte incomingByte;
  static int noteNum;
  static int velocity;
  //static int pot;
  //static int gate;

  turnOffStatLight(STAT1);     //turn off the STAT1 light
  
  //read analog inputs
  
  //pot = analogRead(KNOB1);
  //note = pot/8;  // convert value to value 0-127
  //gate = analogRead(KNOB2);
  //if (gate > 256) { gate=HIGH; } else { gate=LOW; };
  //FS1 = analogRead(A0); if (FS1 > 200) { FS1 = LOW; turnOffStatLight(STAT1);} else { FS1 = HIGH; turnOnStatLight(STAT1);}
  //FS2 = analogRead(A1); if (FS2 > 200) { FS2 = LOW; turnOffStatLight(STAT2);} else { FS2 = HIGH; turnOnStatLight(STAT2);}
  
  
  
  //Are there any MIDI messages
  if(Serial.available() > 0)
  {
    turnOnStatLight(STAT1);   //turn on the STAT1 light indicating that it's received some Serial comms
    
    //read the first byte
    byte1 = Serial.read();
    //Serial.write(byte1);
    
    
    if (byte1 == 0xF8) {
      //timing signal, skip
    } else {
      if (ECHOMIDI) Serial.write(byte1);
      
      if ((byte1 == 0x90) | (byte1 == 0x80) | (byte1 == 0xB0)) {
        byte2 = 0xF8;
        while (byte2 == 0xF8) { while (Serial.available() == 0); byte2 = Serial.read(); }//wait and read 2nd byte in message
        noteNum = (int)byte2;
        if (ECHOMIDI) Serial.write(byte2);
        byte3 = 0xF8;
        while (byte3 == 0xF8) { while (Serial.available() == 0); byte3 = Serial.read(); }//wait and read 2nd byte in message
         velocity = (int)byte3;
        if (ECHOMIDI) Serial.write(byte3);
      }
      
      //check message type
      switch (byte1) {
        case 0xF8:
          //timing message. ignore
          break;
        case 0x90:
          //note on message
          turnOnStatLight(STAT2);//turn on STAT2 light indicating that a note is active
          
          //add a note
          //while (Serial.available() == 0); byte2 = Serial.read(); //wait and read 2nd byte in message
          //noteNum = (int)byte2;
          //velocity = (int)byte3;
          //addNote(noteNum,velocity);
          //addNotePair(noteNum,velocity);
          
          break;
      case 0x80:
        turnOffStatLight(STAT2);//turn off light that had been indicating that a note was active
        
        //note off
        //while (Serial.available() == 0); byte2 = Serial.read(); //wait and read 2nd byte in message
        //velocity = (int)byte3;
        //stopNote(noteNum,velocity);
        //if (noteNum == allNotes[0].noteID) {
          //allNotes[0].is_active = false;
          //allNotes[1].is_active = false;
          //stopAllNotes;
        //}
        break;
      case 0xB0:
        //CC changes (mod wheel, footswitch, etc)
        //waveIndex = constrain((int)byte3 >> 2,0,N_WAVES -1);
        break;
      }
    }
    
  } else {
    delay(1);
  }
}


void turnOnStatLight(int pin) {
  digitalWrite(pin,LOW);
}
void turnOffStatLight(int pin) {
  digitalWrite(pin,HIGH);
}

