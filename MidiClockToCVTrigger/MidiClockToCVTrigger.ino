
//Chip Audette
//May 1, 2013
//Assumes use of MIDI Shield from Sparkfun
//  -- Instead of pots, has dangling 1/8 jacks wired in.
//Input is MIDI signal with the MIDI pulse
//Output is HIGH/LOW signal on A0/A2 (pulse high) and A1/A3 (pulse low)
//Output is 1/16th note on A0/A1, 1/8th note on A2/A3
//Output is also an echo of all incoming MIDI traffice to the Serial Out (ie, MIDI out)

typedef unsigned long micros_t;

#define ECHOMIDI (true)

// defines for MIDI Shield components only
//define KNOB1  0
//define KNOB2  1

//define BUTTON1  2
//define BUTTON2  3
//define BUTTON3  4

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
//int FS1,FS2;

#define OUTPIN_MODE  OUTPUT  //INPUT or OUTPUT?  INPUT gives more protection
#define OUTPIN_16H  A0
#define OUTPIN_16L  A1
#define OUTPIN_8H  A2
#define OUTPIN_8L  A3

#define MIDI_PPQN 24
int pulsesPer16th = 0;
int pulsesPer8th = 0;
int pulsesPerTriggerActive = 0;
micros_t lastMessageReceived_micros=0;
micros_t pulseTimeout_micros = 2000000UL;

void turnOnStatLight(int pin) {
  digitalWrite(pin,LOW);
}
void turnOffStatLight(int pin) {
  digitalWrite(pin,HIGH);
}
void activateTrigger(int pinToHigh, int pinToLow, int LEDpin) {
    digitalWrite(pinToHigh,HIGH);
    digitalWrite(pinToLow,LOW);
    if (LEDpin > 0) turnOnStatLight(LEDpin);
}
void deactivateTrigger(int pinToLow, int pinToHigh, int LEDpin) {
    digitalWrite(pinToHigh,HIGH);
    digitalWrite(pinToLow,LOW);
    if (LEDpin > 0) turnOffStatLight(LEDpin);
}
void setup() {
  
  //set pin modes
  pinMode(STAT1,OUTPUT);
  pinMode(STAT2,OUTPUT);
  pinMode(OUTPIN_16H,OUTPIN_MODE);
  pinMode(OUTPIN_16L,OUTPIN_MODE);
  pinMode(OUTPIN_8H,OUTPIN_MODE);
  pinMode(OUTPIN_8L,OUTPIN_MODE);
  
  //initialize variables
  pulsesPer16th = MIDI_PPQN / 4;
  pulsesPer8th = MIDI_PPQN / 2;
  pulsesPerTriggerActive = pulsesPer16th/2;
  
  //start serial with midi baudrate 31250
  Serial.begin(31250);     
}

void loop () {
  //static byte incomingByte;
  //static int noteNum;
  //static int velocity;
  static int pulse_counter = -1;
  micros_t currentTime_micros=0;
  //static int pot;
  //static int gate;
  int rem8, rem16;
  static boolean MIDI_LED_state = false;
  static micros_t lastMIDImessage_millis=0;
  unsigned long loopCount=0;

  //begin code
  
  //check to see if there's been any MIDI traffic...if not, shut off the LED
  loopCount++;
  if (loopCount > 10000) {
    loopCount=0;
    micros_t curTime_millis = millis();
    if (curTime_millis < lastMIDImessage_millis) lastMIDImessage_millis = 0; //simplistic reset
    if ((curTime_millis - lastMIDImessage_millis) > 1000) turnOffStatLight(STAT2); //turn off after 1 second
  }

  //Are there any MIDI messages
  if(Serial.available() > 0)
  {
    //read the byte
    byte1 = Serial.read();
    
    //echo the byte
    Serial.write(byte1);
    
    //act on the byte
    switch (byte1) {
      case 0xF8:  //MIDI Clock Pulse
        //increment the pulse counter
        pulse_counter++;
        if (pulse_counter >= MIDI_PPQN) pulse_counter = 0;
        currentTime_micros = micros();
        
        //check to see if enough time has passed to reset
        if (currentTime_micros < lastMessageReceived_micros) {
          //it wrapped around
          lastMessageReceived_micros = 0; //simplistic assumption
        }
        if ((currentTime_micros - lastMessageReceived_micros) > pulseTimeout_micros) {
          //reset the counter!
          pulse_counter = 0; 
        }
        lastMessageReceived_micros = currentTime_micros;
       
        //act upon the counter
        rem16 = pulse_counter % pulsesPer16th;
        if (rem16 == 0) {
          activateTrigger(OUTPIN_16H,OUTPIN_16L,0);
        //} else if (rem16 >= pulsesPerTriggerActive) {
        } else if (rem16 >= 1) {
          deactivateTrigger(OUTPIN_16H,OUTPIN_16L,0);
        }
        rem8 = pulse_counter % pulsesPer8th;
        if (rem8 == 0) {
          activateTrigger(OUTPIN_8H,OUTPIN_8L,0);
        } else if (rem8 >= pulsesPerTriggerActive) {
          deactivateTrigger(OUTPIN_8H,OUTPIN_8L,0);
        }
        if (pulse_counter == 0) {
          //quarter note
          turnOnStatLight(STAT1);
        } else if (pulse_counter >= pulsesPerTriggerActive) {
          turnOffStatLight(STAT1);
        }
        break;
      case 0xFA:  //MIDI Clock Start
        //restart the counter
        pulse_counter = -1;
        
        //reset the two triggers
        //deactivateTrigger(OUTPIN_16H,OUTPIN_16L,STAT1);
        //deactivateTrigger(OUTPIN_8H,OUTPIN_8L,STAT2);
        break;
    }
    
    //toggle the LED indicating serial traffic
    MIDI_LED_state = !MIDI_LED_state;
    if (MIDI_LED_state) {
      turnOnStatLight(STAT2);   //turn on the STAT light indicating that it's received some Serial comms
    } else {
      turnOffStatLight(STAT2);   //turn on the STAT light indicating that it's received some Serial comms
    }
    lastMIDImessage_millis = millis();    
    
  } else {
    delay(1);
  }
}



