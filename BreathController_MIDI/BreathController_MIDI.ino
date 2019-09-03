/*
  BreathController_MIDI

  Creates a MIDI breath controller by reads an analog pressure sensor and generating
  relevant MIDI messages.  Assumes the use of a Sparkfun MIDI shield.

  Creare: Chip Audette, OpenAudio, Sept 2019

  MIT License, Use at your own risk
*/

#define DEBUG false   //set to true to debug via Serial Monitor on PC, or to do calibration.  Set to false for MIDI

// define MIDI Code information
// https://www.midi.org/specifications-old/item/table-1-summary-of-midi-message
#define MIDI_CHAN  (0x04)                 //0x00 is "Channel 1", 0x01 is "Channel 2", etc.
#define MIDI_AT    (0b11010000+MIDI_CHAN) //MIDI Code for Aftertouch (Channel)
#define MIDI_PB    (0xE0+MIDI_CHAN)       //MIDI Code for a pitch bend (Channel)
#define MIDI_CC    (0xB0+MIDI_CHAN)       //MIDI Code for MIDI CC message

// Specific CC codes for different functions
// https://www.midi.org/specifications-old/item/table-3-control-change-messages-data-bytes-2
#define CC_MODWHEEL     (0x01)          //CC code for the mod wheel
#define CC_BREATH       (0x02)
#define CC_FOOT         (0x04)
#define CC_EXPRESSION   (0x0B)
#define CC_LOWPASS_CUTOFF (102)         //for my prophet 6

#define CC_TO_USE      CC_MODWHEEL   //change this to one of the entries above

// the setup routine runs once when you press reset
void setup() {
  // initialize serial communication depending upon debug vs actual MIDI
  #if (DEBUG == true)
    Serial.begin(115200); //debugging
  #else
    Serial.begin(31250); //MIDI standard speed
  #endif
}

int readPressureSensor(void) {
  int sensorValue = analogRead(A2) - 1023 / 2;
  int sensorValue2 = analogRead(A3) - 1023 / 2;
  return sensorValue - sensorValue2;
}

// the loop routine runs over and over again forever:
float raw_value = 0.0, scaled_value = 0.0;
int count = 0;
int n_ave = 10;
float calibration__value_at_zero = -10.4; //with no breath, what is the typical raw reading?
float calibration__max_value = 55.0; //with max breath, what is the typical max raw reading?
void loop() {

  //copy any incoming time-related MIDI message back to the output (so that this is MIDI thru)
  while (Serial.available()) {
    byte foo = Serial.read();
    if (foo >= 0b11111000) Serial.write(foo);//echo the byte only if a MIDI timing-related message
  }

  //read sensor
  raw_value = raw_value + (float)readPressureSensor(); //should be 0 to 1023
  count++;

  //do we do anything with the value, yet?
  if (count >= n_ave) {
    
    //compute the average value, remove offset, and do initial scaling
    raw_value = raw_value / ((float)n_ave);
    scaled_value = raw_value - calibration__value_at_zero; //remove offset (must manually obtain this offset value!
    scaled_value = scaled_value / calibration__max_value; //should be approximately -1.0 to 1.0

    //scale and remove offset 
    float pot_scale_fac = (1.0 - ((float)analogRead(A0)) / ((float)1023)) * 3.0; //should be zero to 2.0
    scaled_value = scaled_value * pot_scale_fac;
    scaled_value = max(min(scaled_value, 1.0), -1.0); //should now again be limited to -1.0 to 1.0

    //what kind of MIDI message to send?  Chose based on second potentiometer
    float modePotValue = ((float)(1023 - analogRead(A1)))/1023.0; //why are the pots backwards?
    int transmit_threshold = 1;
    if (modePotValue < (0.33 * 0.75)) {
      sendAftertouchMessage(scaled_value,transmit_threshold);
    } else if (modePotValue < (0.33 * 1.25)) {
      sendAftertouchMessage(scaled_value,transmit_threshold);
      sendPitchBendMessage(scaled_value,transmit_threshold);
    } else if (modePotValue < (0.33 * 1.75)) {
      sendPitchBendMessage(scaled_value,transmit_threshold);
    } else if (modePotValue < (0.33 * 2.25)) {
      sendPitchBendMessage(scaled_value,transmit_threshold);
      sendCCMessage(scaled_value,transmit_threshold,CC_TO_USE);
    } else {
      sendCCMessage(scaled_value,transmit_threshold,CC_TO_USE);
    }

    //reset values for next iteration
    raw_value = 0.0f; count = 0;
  }
  delay(1);        // delay in between reads for stability
}

// ///////////////////////////

int calc_MIDI_value(float scaled_value, const int bipolar_mode) {
  int MIDI_value = 0;
  switch (bipolar_mode) {
    case 0: 
        //send just positive values
        MIDI_value = max(min((int)(scaled_value * 127.0), 127), 0);
        break;
    case 1:
        //let's try negative values (vaccum) as having same effect as positive (blowing)
        MIDI_value = max(min((int)(abs(scaled_value) * 127.0), 127), 0);
        break;
    case 2:
        //send both positive and negative values
        MIDI_value = max(min((int)(scaled_value * 63.0 + 63.0), 127), 0); //should be 0 to 127
        break;
  }
  return MIDI_value;
}

void sendAftertouchMessage(float scaled_value, int transmit_threshold) {
  static int prev_MIDI_value = -100; //this is remembered from call-to-call.  Init to large neg number.
  
  const int bipolar_mode = 1; // choose 0 for pos only, 1 for pos and neg becoming pos, and 2 for full bipolar
  int MIDI_value = calc_MIDI_value(scaled_value, bipolar_mode); 
 
  //decide whether to transmit MIDI message
  if (abs(MIDI_value - prev_MIDI_value) > transmit_threshold) {
    #if (DEBUG == false)
    Serial.write(MIDI_AT);
    //Serial.write(MIDI_AT);
    Serial.write(MIDI_value);
    #else
    //debugging
    Serial.print(raw_value);
    Serial.print(", "); Serial.print(scaled_value);
    Serial.print(", "); Serial.print(MIDI_value);
    Serial.println();
    #endif
    
    prev_MIDI_value = MIDI_value;
  }
}

void sendPitchBendMessage(float scaled_value, int transmit_threshold) {
  static int prev_MIDI_value = -100; //this is remembered from call-to-call. Init to large neg number.
  int MIDI_value;
  
   //pitch bend mode...14-bit number centered on 0x2000 (ie 8192)
  int MIDI_value_7bit = max(min((int)(scaled_value * 63.0 + 63.0), 127), 0); //should be 0 to 127
  uint16_t MIDI_value_14bit = max(min((int)(scaled_value * 8192.0 + 8192.0 + 0.5),2*8192-1),0);
  //decide whether to transmit MIDI message
  if (abs(MIDI_value_7bit - prev_MIDI_value) > transmit_threshold) {
    #if (DEBUG == false)
      Serial.write(MIDI_PB);
      Serial.write((byte) (MIDI_value_14bit & 0x7F)); //LSB
      Serial.write((byte) ((MIDI_value_14bit >> 7) & 0x7F)); //MSB
    #else
      //debugging
      Serial.print(raw_value);
      Serial.print(", "); Serial.print(scaled_value);
      Serial.print(", "); Serial.print(MIDI_value_7bit);
      Serial.print(", "); Serial.print(MIDI_value_14bit);
      Serial.print(", "); Serial.print(MIDI_value_14bit,BIN);
      Serial.print(", "); Serial.print((byte) (MIDI_value_14bit & 0x7F),BIN);
      Serial.print(", "); Serial.print((byte) ((MIDI_value_14bit >> 7) & 0x7F),BIN);
      
      Serial.println();
    #endif
    
    prev_MIDI_value = MIDI_value_7bit;
  }
}

void sendCCMessage(float scaled_value, int transmit_threshold, const byte CC_ID) {
  static int prev_MIDI_value = -100; //this is remembered from call-to-call.  Init to large neg number.
  
  const int bipolar_mode = 1; // choose 0 for pos only, 1 for pos and neg becoming pos, and 2 for full bipolar
  int MIDI_value = calc_MIDI_value(scaled_value, bipolar_mode); 
 
  //decide whether to transmit MIDI message
  if (abs(MIDI_value - prev_MIDI_value) > transmit_threshold) {
    #if (DEBUG == false)
    Serial.write(MIDI_CC);
    Serial.write(CC_ID);
    Serial.write(MIDI_value);
    #else
    //debugging
    Serial.print(raw_value);
    Serial.print(", "); Serial.print(scaled_value);
    Serial.print(", "); Serial.print(MIDI_value);
    Serial.println();
    #endif
    
    prev_MIDI_value = MIDI_value;
  }
}
