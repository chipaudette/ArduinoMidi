
//Chip Audette
//May 1, 2013
//Assumes use of MIDI Shield from Sparkfun
//  -- Instead of pots, has dangling 1/8 jacks wired in.
//Input is MIDI signal with the MIDI pulse
//Output is HIGH/LOW signal on A0/A2 (pulse high) and A1/A3 (pulse low)
//Output is 1/16th note on A0/A1, 1/8th note on A2/A3
//Output is also an echo of all incoming MIDI traffice to the Serial Out (ie, MIDI out)

typedef unsigned long micros_t;
typedef unsigned long millis_t;

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

//define analog input pins for reading the analog inputs
#define N_INPUTS (2)
int inputPin[] = {A0, A1};
#define N_PUSHBUTTONS (3)
int pushbuttonPin[] = {2,3,4};
int prevPushbuttonState[N_PUSHBUTTONS];
boolean flag_middlePushbutton = false;

//define the midi timing info
#define MIDI_PPQN 24  //how many MIDI time pulses per quarter note...24 is standard
micros_t lastMessageReceived_micros=0;
micros_t pulseTimeout_micros = 2000000UL;

//define the outputs
#define OUTPIN_MODE  OUTPUT  //INPUT or OUTPUT?  INPUT gives more protection
#define N_OUTPUTS (2)   //how many outputs are attached
#define N_COUNTERS (N_OUTPUTS+1)  //the extra counter is for the LED
int pulseCountsPerOutput[N_COUNTERS]; //how many midi pulses before issuing a trigger command
int pulse_counter[N_COUNTERS];
int outputPin_high[] = {A2, A4, STAT1};
int outputPin_low[] = {A3, A5, 8}; //the last one can be any pin at all that is not yet used


#define ANALOG_UPDATE_MILLIS 200  //update analog inputs after this period of milliseconds

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
    
  //set pin modes for the lights
  pinMode(STAT1,OUTPUT);
  pinMode(STAT2,OUTPUT);
  
  //set pin modes for the analog inputs
  for (int I=0;I<N_INPUTS;I++) {
    pinMode(inputPin[I],INPUT);
  }

  //set pin modes for the pushbuttons
  for (int I=0;I<N_PUSHBUTTONS;I++) {
    pinMode(pushbuttonPin[I],INPUT_PULLUP);
    prevPushbuttonState[I] = HIGH; //initialize
  } 
  
  //attach interrupts to the pin
  attachInterrupt(1,serviceMiddlePushbutton, RISING);
    
  //initialize output-related variables
  for (int I=0; I<N_COUNTERS; I++) {
    pinMode(outputPin_high[I],OUTPIN_MODE);
    pinMode(outputPin_low[I],OUTPIN_MODE);
    pulseCountsPerOutput[I] = MIDI_PPQN; //init to 16th notes
  }
  
  //reset the counters
  resetCounters();
  
  //start serial with midi baudrate 31250
  Serial.begin(31250);     
}

void loop () {

  micros_t currentTime_micros=0;
  static boolean MIDI_LED_state = false;
  static millis_t curTime_millis=0;
  static millis_t lastMIDImessage_millis=0;
  static millis_t lastAnalogInputs_millis=0;
  static unsigned long loopCount=0;
 
   //service interrupts
   if (flag_middlePushbutton) {
     resetCounters();
     flag_middlePushbutton=false;
   }
     
  //check some time-based things
  loopCount++;
  if (loopCount > 25) {
    //Serial.println("loop count achieved");
    loopCount=0;
    curTime_millis = millis();
    
    //check to see if there's been any MIDI traffic...if not, shut off the LED
    if (curTime_millis < lastMIDImessage_millis) lastMIDImessage_millis = 0; //simplistic reset
    if ((curTime_millis - lastMIDImessage_millis) > 1000) turnOffStatLight(STAT2); //turn off after 1 second
    
    //see if it's time to check the user analog inputs
    if (curTime_millis < lastAnalogInputs_millis) {
      //Serial.println("reseting lastAnalogInputs_millis");
      lastAnalogInputs_millis = 0; //simplistic reset
    }
    if ((curTime_millis - lastAnalogInputs_millis) > ANALOG_UPDATE_MILLIS) {
      lastAnalogInputs_millis = curTime_millis;
      
      //check the potentiometers
      for (int I=0;I<N_INPUTS;I++) {
        //read analog input and decide what the MIDI clock divider should be
        int val = analogRead(inputPin[I]);
        pulseCountsPerOutput[I] = convertAnalogReadToPulseDivider(val);
//        Serial.print("pulseCounterPerOutput ");Serial.print(I);Serial.print(": ");
//        Serial.print(val); Serial.print(", ");
//        Serial.println(pulseCountsPerOutput[I]);
      }
      
      //check the pushbuttons
      for (int I=0;I<N_PUSHBUTTONS;I++) {
        if (I != 1) { //don't service the middle pushbutton because it's on an interrupt
          int val = digitalRead(pushbuttonPin[I]);
          switch (I) {
            case 0:
              if ((val==HIGH) && (prevPushbuttonState[I]==LOW)) stepCountersBack();
              break;
  //          case 1:
  //            if ((val==HIGH) && (prevPushbuttonState[I]==LOW)) resetCounters();
  //            break;
            case 2:
              if ((val==HIGH) && (prevPushbuttonState[I]==LOW)) stepCountersBack();
              break;          
          }
          prevPushbuttonState[I] = val;
        }
      }
    }
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
      
        //maybe a lot of time has passed because the cable was unplugged.
        //if so, ...check to see if enough time has passed to reset the counter
        currentTime_micros = micros();
        if (currentTime_micros < lastMessageReceived_micros) lastMessageReceived_micros = 0; //it wrapped around, simplistic reset
        if ((currentTime_micros - lastMessageReceived_micros) > pulseTimeout_micros) resetCounters(); //reset the counters!
        lastMessageReceived_micros = currentTime_micros;
      
        //loop over each channel
        for (int Icounter=0;Icounter<N_COUNTERS;Icounter++) {
          //increment the pulse counter
          pulse_counter[Icounter]++;
          
          //fit within allowed expanse for this channel
          //pulse_counter[Icounter] %= pulseCountsPerOutput[Icounter];
          int val = pulse_counter[Icounter];
          while (val > (pulseCountsPerOutput[Icounter]-1)) val -= pulseCountsPerOutput[Icounter];  //faster than mod operator
          while (val < 0)  val += pulseCountsPerOutput[Icounter]; //faster than mod operator
          pulse_counter[Icounter] = val;

          //act upon the counter
          if (pulse_counter[Icounter] == 0) {
            activateTrigger(outputPin_high[Icounter],outputPin_low[Icounter],0);
          } else if (pulse_counter[Icounter] >= min(pulseCountsPerOutput[Icounter]/2,MIDI_PPQN/2)) {
            deactivateTrigger(outputPin_high[Icounter],outputPin_low[Icounter],0);
          }
        }
        break;
      case 0xFA:  //MIDI Clock Start
        //restart the counter
        resetCounters();
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

void resetCounters(void) {
  for (int I=0;I<N_COUNTERS;I++) {
    //set so that the next one will be zero
    pulse_counter[I]=-1;  
    
    //deactivate the Trigger so that it is ready to activate on the next MIDI pulse
    deactivateTrigger(outputPin_high[I],outputPin_low[I],0);
  }
}

void stepCountersBack(void) {
  for (int I=0;I<N_COUNTERS;I++) {
    pulse_counter[I] -= 1;    
    //pulse_counter[Icounter] %= pulseCountsPerOutput[Icounter]; //fit within allowed expanse for this channel
  }
}
#define MAX_ANALOG_READ_COUNTS 1023
//#define N_PULSE_OPTIONS 12
//static int choices_pulsesDivider[] = {3,    4,     6,     8,   12,    16,   24,    32,      48,    96,   144,   192};
//#define N_PULSE_OPTIONS 12
//static int choices_pulsesDivider[] =  {3,            6,     8,    12,   16,   24,    32,      48,    72,    96,    144,   192};  //assumed 24 PPQN
#define N_PULSE_OPTIONS 12
static int choices_pulsesDivider[] =  {3,            6,     8,    12,   16,   24,        36,  48,    72,    96,    144,   192};//assumed 24 PPQN
int convertAnalogReadToPulseDivider(int analogVal) {
  static int COUNTS_PER_INDEX = MAX_ANALOG_READ_COUNTS / N_PULSE_OPTIONS;
  int index = analogVal / COUNTS_PER_INDEX;  //rounds down
  index = constrain(index,0,N_PULSE_OPTIONS-1);
  return choices_pulsesDivider[index];
}
  
void serviceMiddlePushbutton(void) {
  flag_middlePushbutton=true;
}

