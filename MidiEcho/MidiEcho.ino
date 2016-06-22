// SparkFun MIDI Sheild and MIDI Breakout test code
// Defines bare-bones routines for sending and receiving MIDI data
// 
// Chip Audette.  http://synthhacker.blogspot.com
//
// Updated June 2016


#define ECHOMIDI (true)
#define ECHOMIDI_CLOCK (false)

// defines for MIDI Shield components (NOT needed if you just want to echo the MIDI codes)
#define KNOB1  0   //potentiometer on sparkfun MIDI shield
#define KNOB2  1   //potentiometer on sparkfun MIDI shield
#define BUTTON1  2  //pushbutton on sparkfun MIDI shield
#define BUTTON2  3  //pushbutton on sparkfun MIDI shield
#define BUTTON3  4  //pushbutton on sparkfun MIDI shield
#define STAT1  7   //status LED on sparkfun MIDI shield
#define STAT2  6   //status LED on sparkfun MIDI shield

//other variables
byte byte1, byte2, byte3;  //standard 3 byte MIDI transmission
//int FS1,FS2; //foot swtiches connected to the analog inputs


void setup() {
  pinMode(STAT1,OUTPUT); //prepare the LED outputs
  pinMode(STAT2,OUTPUT); //prepare the LED outputs
  turnOffStatLight(STAT1);     //turn off the STAT1 light
  turnOffStatLight(STAT2);     //turn off the STAT1 light

  
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
 
  //Are there any MIDI messages
  if(Serial.available() > 0)
  {
    turnOnStatLight(STAT1);   //turn on the STAT1 light indicating that it's received some Serial comms

    //read the first byte
    byte1 = Serial.read();

    
    if (byte1 == 0xF8) {
      //timing signal, skip
      if (ECHOMIDI_CLOCK) Serial.write(byte1);
    } else {
      if (ECHOMIDI) Serial.write(byte1);

      //none of the rest is needed if you just want to echo the MIDI codes
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
          //while (Serial.available() == 0); byte2 = Serial.read(); //wait and read 2nd byte in message
          
          break;
      case 0x80:
        //note off message
        turnOffStatLight(STAT2);//turn off light that had been indicating that a note was active
        //while (Serial.available() == 0); byte2 = Serial.read(); //wait and read 2nd byte in message

        break;
      case 0xB0:
        //CC changes (mod wheel, footswitch, etc)
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

