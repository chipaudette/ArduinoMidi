/*
 Created: Chip Audette, Feb/Mar 2016
 Purpose: Sense Reisitive Ribbon and Output MIDI notes

 This code is aimed toward the Sequential Prophet 6 synthesizer.
 Specifically, it automatically changes the synth's bend range to +/- 1 octave.

 Physical Setup:
     Using old Sparkfun MIDI shield
     Using Ribbon connected to A3 (wiper) and Gnd

 */

#include <Biquad.h>   //http://www.earlevel.com/main/2012/11/26/biquad-c-source-code/


//debugging info
#define PRINT_TEXT (0)
#define PRINT_BEND_TEXT (0)
#define PRINT_CAL_TEXT (0)
#define PRINT_RAW_RIBBON_VALUE (0)
#define WRITE_MIDI (1)

#define USE_ARDUINO  1
//#define USE_TEENSY  1
//#define Serial_t (Serial1)  //for MIDI for Arduino Micro or for TEENSY
#define Serial_t (Serial) //for MIDI for Arduino UNO

 
// initialize variables for analogIn
const int ribbonPin = A3;
const int ribbonVccPin = A2;
const int ribbonGnd = A1;
//int drivePin = 4; //Digital pin #4
//int drivePin_state = HIGH;
const int pushButton1_pin = 2;   // Push button, Digital Pin
const int pushButton2_pin = 3;   // Push button, Digital Pin
const int pushButton3_pin = 3;   // Push button, Digital Pin
const int LED1_pin = 6; //on Sparkfun MIDI shield
const int LED2_pin = 7; //on Sparkfun MIDI shield
const int POT1_pin = A0;
//const int comparePin = A4;
//const int comparePinGnd = A5;

//sampling
volatile boolean flag_update_system = true;
volatile boolean flag_cannot_keep_up = false;
#define SAMPLE_RATE_HZ (600.0)

//filtering
#define DO_NOTCH_FILTERING (1)
#define NOTCH_FREQ_HZ (60.0)
#define NOTCH_Q (0.707) //higher is sharper.  0.707 is crtically damped (butterworth)
#define FILTER_PEAK_GAIN_DB (0.0) //doesn't matter for Lowpass, highpass, notch, or bandpass
Biquad notch_filter(bq_type_notch,NOTCH_FREQ_HZ / SAMPLE_RATE_HZ, NOTCH_Q, FILTER_PEAK_GAIN_DB); //one for each channel because the object maintains the filter states
Biquad notch_filter2(bq_type_notch,(2.0*NOTCH_FREQ_HZ) / SAMPLE_RATE_HZ, NOTCH_Q, FILTER_PEAK_GAIN_DB); //one for each channel because the object maintains the filter states

//decimation
#define PROCESSING_DECIMATION (12)
#define PROCESSING_SAMPLE_RATE_HZ (SAMPLE_RATE_HZ/PROCESSING_DECIMATION)
#define LP_Q (0.707) //higher is sharper.  0.707 is crtically damped (butterworth)
Biquad lp_filter(bq_type_lowpass,PROCESSING_SAMPLE_RATE_HZ/SAMPLE_RATE_HZ, LP_Q, FILTER_PEAK_GAIN_DB); //one for each channel because the object maintains the filter states
volatile int decimation_counter = 0;

//Define ribbon parameters
#define RIBBON_BY_VCC 1
#define RIBBON_BY_WIPER 2
const int ribbon_mode = RIBBON_BY_WIPER;
#define RIBBON_SPAN (1023)
int ribbon_max_val = 339,ribbon_min_val = 13; //for my Arduino UNO
//int ribbon_max_val = 520,ribbon_min_val = 22; //for my Arduino UNO, External 3.3V ref
//int ribbon_max_val = 330,ribbon_min_val = 10; //for my Arduino Micro, 5V pullup
//int ribbon_max_val = 548,ribbon_min_val = 0; //for my Arduino Micro, 15K external pullup
//int ribbon_max_val = 344,ribbon_min_val = 0; //for my Teensy
//int ribbon_max_val = 592,ribbon_min_val = 0; //for my Teensy with double pullups
//int ribbon_max_val = 585,ribbon_min_val = 0; //for my Teensy 15k pullup
//int ribbon_max_val = 545, ribbon_min_val = 0;  //Teensy plus opamp w/ 33K pullup
//int ribbon_max_val = 842, ribbon_min_val = 0;  //Teensy plus opamp w/ 15K pullup
float R_pullup_A3_kOhm = 0.0;
float R_ribbon_min_kOhm = 0.0;
const float R_ribbon_max_kOhm = 17.45;
const float ribbon_span_half_steps_float = 36;
const float ribbon_span_extra_half_steps_float = 0.5;
const int max_allowed_jump_steps = 5;

boolean flag_bend_range_is_for_ribbon = false;
#define BEND_DEFAULT 0
#define BEND_FOR_RIBBON 1

boolean flag_pitch_is_bent = false;
float time_for_recentering_sec = 2.5;
long int time_for_recentering_counts = 0;
long int counts_until_recentering = 0;

#ifdef USE_TEENSY
IntervalTimer sampleTimer; 
#endif

#define MIDI_note_bottom (43-7)  //Note C1

#define MIDI_CHANNEL (0x01)
#define NOTE_ON (0x90+MIDI_CHANNEL)
#define NOTE_OFF (0x80+MIDI_CHANNEL)
#define NOTE_ON_VEL (127)  //0x7F
#define NOTE_OFF_VEL (0)  //0x7F
#define PITCH_BEND (0xE0+MIDI_CHANNEL)
#define MIDI_CC (0b10110000 + MIDI_CHANNEL)
#define BRIGHTNESS_CC ((byte)74)

#define synth_bend_half_steps (12)  //what is the synth's pitch wheel set to?

//interrupt service routine...format of function call changes depending upon platform
#ifdef USE_TEENSY
void serviceTimer(void)     //Teensy convention
#else
ISR(TIMER1_COMPA_vect)  //arduino convention
#endif
{
  if (flag_update_system) {
    flag_cannot_keep_up = true;
  }
  
  read_ribbon();
  decimation_counter++;
  if (decimation_counter >= PROCESSING_DECIMATION) {
    decimation_counter = 0;
    flag_update_system = true;
  }
}


//setup routine
void setup() {
  //setup the ribbon pins and the analogRead reference
  //analogReference(INTERNAL);
  //analogReference(EXTERNAL);
  pinMode(ribbonVccPin, INPUT); //set to high impedance
  pinMode(ribbonPin, INPUT_PULLUP);
  pinMode(ribbonGnd,OUTPUT); digitalWrite(ribbonGnd,LOW);  //use as ground
  pinMode(POT1_pin,INPUT);

  //setup the LEDs
  pinMode(LED1_pin, OUTPUT); digitalWrite(LED1_pin, HIGH); //HIGH is off for he Sparkfun shield
  pinMode(LED2_pin, OUTPUT); digitalWrite(LED2_pin, HIGH); //HIGH is off for he Sparkfun shield

  // initialize the MIDI communications:
  Serial.begin(115200*2);  //for Arduino Micro only
  if (WRITE_MIDI) Serial_t.begin(31250);

  //set scaling for ribbon
  R_pullup_A3_kOhm = R_ribbon_max_kOhm * ( ((float)RIBBON_SPAN) / ((float)ribbon_max_val) - 1.0 );
  R_ribbon_min_kOhm = ((float)ribbon_min_val) / ((float)ribbon_max_val) * R_ribbon_max_kOhm;

  //setup the timer
  float dt_sec_sample = 1.0 / SAMPLE_RATE_HZ;//arduino can run up to 1200Hz as of Mar 5, 2016

  #ifdef USE_TEENSY
  int timer_loop_usec = (int)(dt_sec_sample*1000000.0); //microseconds
  sampleTimer.begin(serviceTimer, timer_loop_usec);
  #else
  int timer_clock_divider = 64;  //see which divider I'm using in setupTimer()
  int timer_value = (int)( (16000000L) / ((long)timer_clock_divider) / ((long)SAMPLE_RATE_HZ) );
  setupTimer(timer_value);
  #endif

  //decide how long to wait until pitch bend is recentered
  float dt_sec_processing = 1.0 / PROCESSING_SAMPLE_RATE_HZ;
  time_for_recentering_counts = (long int)(time_for_recentering_sec / dt_sec_processing + 0.5); //time steps to count off
}



void loop() {
  static int idle_counter = 0;
  
  if (flag_update_system) {
    flag_update_system = false;
    updateTheSystem();
    
    //check to see if we should turn off the bend commands
    if (flag_pitch_is_bent) { 
      counts_until_recentering--;  //decrement counter
      if (counts_until_recentering <= 0) {
        #if WRITE_MIDI
        recenterMidiPitchBend();  //remove any bend commands still active
        setBendRange(BEND_DEFAULT); //turn the bend range back to the default
        transmitBrightness(0);
        #endif
      }
    }

  } else {
    idle_counter++;  //kill time without being stuck in an empty loop
  }
}


//static int prev_FS2_val;
int prev_but_val[] = {HIGH, HIGH, HIGH};
int raw_ribbon_value = 0;
int ribbon_value = 0;
float ribbon_value_float = 0.0;
int note_num = 0;
int closest_note_num = 0;
float note_num_float = 0.0;

int prev_note_num = 0;
int prev_closest_note_num = 0;
int prev_MIDI_note_num = 0;
boolean prev_note_on = false;
float prev_ribbon_val_float = 1023.0;
unsigned int prev_bend_int = 0;
byte prev_pitch_bend_MSB = 63;
byte prev_pitch_bend_LSB = 63;

float foo_float, ribbon_frac, ribbon_R_kOhm, ribbon_span_frac;
float brightness_frac;

boolean note_on;
int MIDI_note_num;
boolean note_change;
float partial_note_num_float;
float partial_bend_float ;
#define MIDI_MAX_BEND  (8192)  //can go up and down by this amount
unsigned int bend_int;
byte pitch_bend_LSB, pitch_bend_MSB;
void updateTheSystem() {

  //read input
  //read_ribbon(); //this is now done in the interrupt service routine!

  //process input
  process_ribbon();

  //is the note on?
  note_on = false;
  if (ribbon_value < (RIBBON_SPAN-10)) {
    if ((note_num >= 0) && (note_num <= ribbon_span_half_steps_float)) note_on = true;
    if (abs(note_num - prev_note_num) > max(max_allowed_jump_steps,synth_bend_half_steps)) note_on = false;
  } else {
    note_num = prev_note_num; //just in case there was a bad value
  }

  //calc MIDI note number
  note_num = constrain(note_num, 0, ribbon_span_half_steps_float);
  int MIDI_note_num = note_num + MIDI_note_bottom;
  boolean note_change = (note_on != prev_note_on) | (note_num != prev_note_num);

  //compute the amount of bend
  partial_note_num_float = note_num_float - ((float)note_num);
  partial_bend_float = partial_note_num_float / ((float)synth_bend_half_steps);
  partial_bend_float = constrain(partial_bend_float, -1.0, 1.0);
  #define MIDI_MAX_BEND  (8192)  //can go up and down by this amount
  bend_int = (unsigned int)(MIDI_MAX_BEND + (partial_bend_float * ((float)MIDI_MAX_BEND)));
  pitch_bend_LSB = ((byte)bend_int) & 0b01111111;  //get lower 7 bits, ensure first bit to zero
  pitch_bend_MSB = ((byte)(bend_int >> 7)) & 0b01111111;  //get upper 7 bits, set first bit to zero

  //compute the brightness
  #define BIT_SHIFT (10-7)  //analog read is 10 bits.  MIDI message is 7 bis
  int pot_val = map(analogRead(POT1_pin),0,1023,127,0);  //reverse direction and fit within 0-127
  float max_bright_val = (float)pot_val;  
  byte brightness_val = (byte)(brightness_frac * max_bright_val + 0.5); //0-127.  The 0.5 is so that it rounds
  //brightness_val = 127 - brightness_val;  //reverse the direction because the potentiometer is backwards

  //light LED if note is on
  digitalWrite(LED1_pin, !note_on); //if note is on (ie HIGH) turn on the LED (LOW for the Sparkfun shield)

  //read putshbutton
  boolean but_val = (digitalRead(pushButton1_pin) == LOW);  //pressed makes it LOW...so set the value as TRUE
  boolean but2_val = (digitalRead(pushButton2_pin) == LOW); //pressed makes it LOW...so set the value as TRUE
  //boolean but3_val = (digitalRead(pushButton3_pin) == LOW); //pressed makes it LOW...so set the value as TRUE

  //print messages
  if (note_change) {

    digitalWrite(LED2_pin, LOW);  //LOW is "on" for the sparkfun MIDI shield
    if (PRINT_TEXT) {
      Serial.print("ribbon value = ");
      Serial.print(ribbon_value);
      Serial.print(", R = ");
      Serial.print(ribbon_R_kOhm);
      Serial.print(" kOhm, Frac MAX = ");
      Serial.print( ribbon_span_frac );

      Serial.print(", Note On = ");
      Serial.print(note_on);
      Serial.print(", Note Float = ");
      Serial.print(note_num_float);
      Serial.print(", Note Num = ");
      Serial.print(note_num);
      Serial.print(", MIDI Note = ");
      Serial.println(MIDI_note_num);
    } //else {
      if (note_on) {
        #if WRITE_MIDI
        //ensure that the bend range has been set
        if (flag_bend_range_is_for_ribbon==false) setBendRange(BEND_FOR_RIBBON);

        //turn on new note
        Serial_t.write((byte)NOTE_ON);
        Serial_t.write((byte)MIDI_note_num);
        Serial_t.write((byte)NOTE_ON_VEL);  //velocity

        //send pitch bend info
        transmitPitchBend();

        //send the brightness info
        transmitBrightness(brightness_val);

        //turn off the old note?
        if (prev_note_on == true) {
          //previous note was on...so assume legato...turn off old note now that new one is already started
          Serial_t.write((byte)NOTE_OFF);
          Serial_t.write((byte)prev_MIDI_note_num);
          Serial_t.write((byte)NOTE_OFF_VEL);
        }
        #endif
        
      } else if ((note_on == false) && (prev_note_on == true)) {
        //no other notes are active.  turn note off
        #if WRITE_MIDI
        Serial_t.write((byte)NOTE_OFF);
        Serial_t.write((byte)prev_MIDI_note_num);
        Serial_t.write((byte)NOTE_OFF_VEL);
        #endif
      }
    //}
    digitalWrite(LED2_pin, HIGH); //HIGH is "on" for the sparkfun MIDI shield
  } else if (1) {
    //not changing notes...but we should still update the pitch bend info
    if (note_on == true) {
      if (bend_int != prev_bend_int) { //but only if it has changed
        #if WRITE_MIDI
        transmitPitchBend();
        transmitBrightness(brightness_val);
        #endif
      }
    }
  }

  //save state
  prev_note_num = note_num;
  prev_closest_note_num = closest_note_num;
  prev_MIDI_note_num = MIDI_note_num;
  prev_note_on = note_on;
  prev_but_val[0] = but_val;
  prev_but_val[1] = but2_val;
  prev_bend_int = bend_int;
  prev_pitch_bend_MSB = pitch_bend_MSB;
  prev_pitch_bend_LSB = pitch_bend_LSB;
  prev_ribbon_val_float = ribbon_value_float;
}

int toggle_counter = 0;
const int filt_reset_thresh = ribbon_max_val + max(1,(RIBBON_SPAN - ribbon_max_val)/6);
void read_ribbon(void) {
  static int prev_raw_ribbon_value = 0;

  //read the ribbon value
  raw_ribbon_value = analogRead(ribbonPin);
  ribbon_value = raw_ribbon_value;
  ribbon_value_float = (float)ribbon_value;
  //if (drivePin_state == LOW) ribbon_value_float = RIBBON_SPAN - ribbon_value_float;

//  //invert the drive pin
//  toggle_counter++;
//  if (toggle_counter >= 1) {
//    toggle_counter = 0;
//    drivePin_state = !drivePin_state;
//    digitalWrite(drivePin,drivePin_state);    
//  }
  
  //if the ribbon value is above some limit (ie, not being touched), don't apply the filters.  
  //This is to prevent the internal filter states getting spun up onto these non-real values.  
  //Hopefully, this'll improve the pitch transitions on short, sharp, stacato notes.
  if (ribbon_value < filt_reset_thresh) {
    //apply notch filtering
    if (DO_NOTCH_FILTERING) {
      //apply filtering
      ribbon_value_float = notch_filter.process(ribbon_value_float); //60 Hz
      //ribbon_value_float = notch_filter2.process(ribbon_value_float); //120 Hz
    }
  
    //apply low-pass filtering (for decimation)
    if (PROCESSING_DECIMATION > 1) {
      ribbon_value_float = lp_filter.process(ribbon_value_float);
    }

    //if previously the note was off, force the system to update now for max responsiveness
    if (prev_raw_ribbon_value >= filt_reset_thresh) {
      decimation_counter = PROCESSING_DECIMATION;
    }
  } else {
    //if note was previously on, now it's off, so force the systme to update now for max responsiveness
    if (prev_raw_ribbon_value <= ribbon_max_val) {
      //this is the note turning off.  Force the system to update now
      decimation_counter = PROCESSING_DECIMATION;
    }
  }
  prev_raw_ribbon_value = raw_ribbon_value;

  //print some debugging info
  if (PRINT_RAW_RIBBON_VALUE) {
    if (flag_cannot_keep_up) {
      Serial.print(-ribbon_value); //enable this to get value at top and bottom of ribbon to calibrate
    } else {
      Serial.print(ribbon_value); //enable this to get value at top and bottom of ribbon to calibrate
    }
    Serial.print(", "); //enable this to get value at top and bottom of ribbon to calibrate
    Serial.print((int)ribbon_value_float); //enable this to get value at top and bottom of ribbon to calibrate
    //Serial.print(compare_value); //enable this to get value at top and bottom of ribbon to calibrate
    Serial.println();
    flag_cannot_keep_up = false;
  }

  //save for next time
  prev_raw_ribbon_value = raw_ribbon_value;
}
void process_ribbon(void) {

  //process ribbon value
  foo_float = 1.0 / (((float)RIBBON_SPAN) / ribbon_value_float - 1.0);
  ribbon_frac = (R_pullup_A3_kOhm / R_ribbon_max_kOhm) * foo_float;
  ribbon_R_kOhm = R_pullup_A3_kOhm * foo_float;
  ribbon_span_frac = (ribbon_R_kOhm - R_ribbon_min_kOhm) / (R_ribbon_max_kOhm - R_ribbon_min_kOhm);
  ribbon_span_frac = max(0.0, ribbon_span_frac);
  note_num_float = ribbon_span_frac * (ribbon_span_half_steps_float + ribbon_span_extra_half_steps_float);
  note_num_float = note_num_float - ribbon_span_extra_half_steps_float/2.0;



  //apply a calibration (of sorts) to better linearize the response
  if (1) { 
    float bottom_transition = 1.5;
    float first_transition = 8.0;
    float second_transition = -2.0; //-10
    float third_transition = 1.0; //-5

    if (PRINT_CAL_TEXT) {
      if ((note_num_float > -10.0) & (note_num_float < 38.0)) {
        Serial.print("Applying Cal: raw_ribbon_val = ");
        Serial.print(raw_ribbon_value);
        Serial.print(", note_num_float = ");
        Serial.print(note_num_float);
      }
    }

    //float cal_amount = 1.0; // Uno
    float cal_amount = 1.0; // micro
    if (note_num_float > bottom_transition) {
      if (note_num_float < first_transition) {
        note_num_float += (cal_amount*((note_num_float-bottom_transition)/(first_transition-bottom_transition)));
      } else if (note_num_float < (ribbon_span_half_steps_float+second_transition)) {
        note_num_float += cal_amount;
      } else if (note_num_float < (ribbon_span_half_steps_float+third_transition)) {
        float foo = note_num_float - (ribbon_span_half_steps_float+second_transition); //how many above the transition
        float full_segment = -second_transition + third_transition; //positive number
        note_num_float += cal_amount*(1.0 - foo/full_segment);
      }
    }

    if (PRINT_CAL_TEXT) {
      if ((note_num_float > -10.0) & (note_num_float < ribbon_span_half_steps_float+2.0)) { 
        Serial.print(", final note_num_float = ");
        Serial.println(note_num_float);
      }
    }
  }
  note_num = ((int)(note_num_float + 0.5)); //round

  //look for special case...decide to change note or just bend a lot
  closest_note_num = note_num;
  if (0) {
    //add some hysteresis to prevent fluttering between two notes
    //if you keep you finger on the line between them
    if (abs(note_num - prev_note_num) == 1) {  //is it a neighboring note?
      if (note_num > prev_note_num) {
        note_num = ((int)(note_num_float + 0.35)); //need extra to get up to next note
      } else if (note_num < prev_note_num) {
        note_num = ((int)(note_num_float + 0.65)); //next extra to get down to next note
      }
    }
  } else {
    //if it can bend the note, just let it bend the note
    if ( ((prev_note_on==true) && (abs(note_num_float - ((float)prev_note_num)) < (synth_bend_half_steps-0.5))) ||  //bend if we can
         ((prev_note_on==false) && (abs(note_num - prev_note_num) < min(2,synth_bend_half_steps))) ) { //if new note, only bend if we're off by one note
      //don't change note, step back to original note...the rest of the code will just bend more
      note_num = prev_note_num;
    }
  }

  //calc the desired brightness CC value (as a value of 0.0 to 1.0)
  brightness_frac = min(1.0,max(0.0,note_num_float / ribbon_span_half_steps_float));
}

void setBendRange(int type) {
  //for Prophet 6.  From user manual Page 71 "Received NRPN Messages"
  //Pitch Bend Range is parameter 31.  Values are 0-24.
  byte CC_byte = 0b10110000+MIDI_CHANNEL;  //Control change

  Serial_t.write(CC_byte); //control change
  Serial_t.write(0b01100011); //NRPN parameter number MSB CC
  Serial_t.write((byte)0); //Parameter number MSB

  Serial_t.write(CC_byte); //control change
  Serial_t.write(0b01100010); //NRPN parameter number LSB CC
  Serial_t.write((byte)31); //Parameter number LSB
  
  Serial_t.write(CC_byte); //control change
  Serial_t.write(0b00000110); //NRPN parameter value MSB CC
  Serial_t.write((byte)0); //Parameter value MSB
        
  Serial_t.write(CC_byte); //control change
  Serial_t.write(0b00100110); //NRPN parameter value LSB CC
  if (type == BEND_FOR_RIBBON) {
    Serial_t.write((byte)synth_bend_half_steps); //12 semitones
    flag_bend_range_is_for_ribbon = true;
  } else { 
    Serial_t.write((byte)2); //assume default is 2 semitones
    flag_bend_range_is_for_ribbon = false;
  } 
}

void transmitBrightness(byte value) {
  Serial_t.write((byte)MIDI_CC);
  Serial_t.write((byte)BRIGHTNESS_CC);
  Serial_t.write(0b01111111 & value);  //be sure that first bit is zero
}

void transmitPitchBend(void) {
  //send pitch bend
  if (PRINT_BEND_TEXT) {
    //human readable
    Serial.print("Pitch Bend: ");
    Serial.print(", Note Num: ");
    Serial.print(note_num) ;
    Serial.print(", Frac Bent = ");
    Serial.print(partial_note_num_float);
    Serial.print(", Bend Int = ");
    Serial.print(bend_int);
    Serial.print(", Bend MIDI M/S = ");
    Serial.print(pitch_bend_MSB);
    Serial.print(" ");
    Serial.println(pitch_bend_LSB);
  } 
  //else {
    //write MIDI
    Serial_t.write((byte)PITCH_BEND);
    Serial_t.write((byte)pitch_bend_LSB);
    Serial_t.write((byte)pitch_bend_MSB);
  //}
  flag_pitch_is_bent = true;
  counts_until_recentering = time_for_recentering_counts;
}

void recenterMidiPitchBend(void) {
  Serial_t.write((byte)PITCH_BEND);
  Serial_t.write((byte)0x00);  //center,LSB
  Serial_t.write((byte)64);  //center,MSB}
  flag_pitch_is_bent = false;
};

#ifdef USE_ARDUINO
////Code from: http://letsmakerobots.com/node/28278
void setupTimer(const int &match_value_counts) {
  noInterrupts();           // disable all interrupts
  //we're going to use timer1
  TCCR1A = 0;  //enable normal operation (mode 0)
  TCCR1B = 0;
  TCNT1  = 0; //reset the counter

  // compare match register
  //  OCR1A = 0x7a12;           // compare match register...16MHz/prescaler/sample_rate_hz
  OCR1A = match_value_counts;
  TCCR1B |= (1 << WGM12);   // CTC mode
  //TCCR1B |= (1 << CS10);    // prescaler  (CS10=1)
  //TCCR1B |= (1 << CS11);    // prescaler  (CS11=8)
  TCCR1B |= ((1 << CS11) | (1 << CS10));  //CS11+CS10 = 64
  //TCCR1B |= ((1 << CS12) | (1 << CS10));  //CS12+CS10 = 1024
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();             // enable all interrupts
}
#endif 

