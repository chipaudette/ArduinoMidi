// MIDI Sequencer.  Start a MIDI time clock.  Play in MIDI notes (and other codes)
// in real time.  This records the MIDI codes relative to the MIDI clock
// and plays the notes back out in a loop.
//
// Here's my plan: Listen to select MIDI codes (note on/off, aftertouch, CC, pitch bend)
// and mark when time they arrive (MIDI time).  Save all of the midi note on/off messages (so it's
// polyphonic!) but only keep the first value for the other messages for a given MIDI time
// step (saves memory).
//
// Uses SparkFun MIDI Sheild
// 
// Chip Audette.  http://synthhacker.blogspot.com
// Created June 2016
//

#include "MIDI_Note_Buffer.h"
#include "MIDI_Codes.h"
#include "Button.h"

#define ECHOMIDI (false)
#define ECHOMIDI_BIN (false)
#define ECHOMIDI_CLOCK (false)

// define sequencer limits
#define MAX_N_BEATS (16)    //number of beats (quarter notes) to loop
#define MAX_N_CODES (80)  //make this small enough so that 4*N_STORED_CODES bytes fits in memory

// defines for MIDI Shield components (NOT needed if you just want to echo the MIDI codes)
#define KNOB1  0   //potentiometer on sparkfun MIDI shield
#define KNOB2  1   //potentiometer on sparkfun MIDI shield
#define STAT1  7   //status LED on sparkfun MIDI shield
#define STAT2  6   //status LED on sparkfun MIDI shield

//define variables
byte foo_byte, rx_bytes[3];  //standard 3 byte MIDI transmission
int byte_counter=-1;
byte MIDI_command_buffer[MAX_N_CODES][3]; //this can get big fast!
int MIDI_time_buffer[MAX_N_CODES];
int buffer_counter=-1;
int raw_current_time_index = 0;
int current_time_index = 0;
boolean hasTimeCounterBeenExternallySet = false;
#define MAX_MAX_TIME_INDEX (MAX_N_BEATS*MIDI_PPQN)
int max_time_index = MAX_MAX_TIME_INDEX;
int CC_counter = 0, MODWHEEL_on = 0;
int PB_counter = 0, PB_on = 0;
int is_recording = 0; //record incoming MIDI messages?
int is_playing = 1;   //playback the recorded MIDI messages?

MIDI_Note_Buffer note_buffer = MIDI_Note_Buffer(&Serial);

Button pushbutton[3] = {Button(2), Button(3), Button(4)};  //the MIDI Shield's 3 pushbuttons
Button footswitch[2] = {Button(A2), Button(A3)}; //I've got footswitches attached to these analog inputs

void setup() {
  pinMode(STAT1,OUTPUT); //prepare the LED outputs
  pinMode(STAT2,OUTPUT); //prepare the LED outputs
  
  //initialize the time codes
  clearRecordedMIDI();
  
  //start serial with midi baudrate 31250
  Serial.begin(31250);     
}

void loop () {
  //update status lights
  if (is_recording) { turnOnStatLight(STAT1); } else { turnOffStatLight(STAT1); };
  if (is_playing) { turnOnStatLight(STAT2); } else { turnOffStatLight(STAT2); };
  
  //Are there any MIDI messages?
  if(Serial.available() > 0)  {    
     
    //read the byte
    foo_byte = Serial.read();

    //what kind of message is it?
    if (foo_byte == MIDI_CLOCK) { //MIDI CLOCK
      
      if (ECHOMIDI_CLOCK) Serial.println(foo_byte,HEX);
      incrementTheTimeAndAct();
      
    } else if (foo_byte == MIDI_START) {
      
      if (ECHOMIDI) { Serial.print(foo_byte,HEX); Serial.print(" "); }
      if (ECHOMIDI_BIN) { Serial.write(foo_byte); }
      resetCurTimeIndex();
      hasTimeCounterBeenExternallySet = true;
      
    } else { //not a MIDI Clock
      
      if (ECHOMIDI) { Serial.print(foo_byte,HEX); Serial.print(" "); }
      if (ECHOMIDI_BIN) { Serial.write(foo_byte); }
      receiveMIDIByteAndAct(foo_byte);
      
    } //close if MIDI Clock
    
  } else {    //if Serial.isAvailable()
    
    //service the pushbuttons, knobs, and footswtiches
    serviceUserControls();
    
  } //end if Serial.isAvailable()
} //end loop()


void incrementTheTimeAndAct(void) {
  raw_current_time_index++;
  if (raw_current_time_index >= MAX_MAX_TIME_INDEX) current_time_index = 0;
  current_time_index = raw_current_time_index % max_time_index;
  resetMessageCounters();
  playCodesForThisTimeStep(current_time_index);
}

void receiveMIDIByteAndAct(const byte &foo_byte) {       
  if (foo_byte & (0b10000000+MIDI_CHAN)) { //bitwise compare to detect start of MIDI_Message (for this channel)
    turnOnStatLight(STAT1);   //turn on the STAT1 light indicating that it's received some Serial comms
    if (byte_counter > 0) saveMessage(current_time_index,rx_bytes);  //save the previously started message
    clear_rx_bytes();  byte_counter = -1; //clear the temporary storage
    if ((foo_byte == NOTE_ON) || (foo_byte == NOTE_OFF) || (foo_byte == MIDI_CC) || (foo_byte == MIDI_PB)) { 
    //if ((foo_byte == MIDI_CC) || (foo_byte == MIDI_PB)) {
      byte_counter++; 
      //Serial.print("start, byte_count "); Serial.println(byte_counter);
      rx_bytes[byte_counter] = foo_byte; //only store these certain types of messages
    } else {
      //Serial.println("start, rejected");
    }
  
  } else if (byte_counter >= 0) {  //other bytes in the alrady started MIDI Message 
    //Serial.print("mid, byte_count "); Serial.println(byte_counter);
    byte_counter++; rx_bytes[byte_counter] = foo_byte;
  }
  //if we've gotten all three bytes, save the message
  if (byte_counter == (3-1)) { saveMessage(current_time_index,rx_bytes);  clear_rx_bytes();  byte_counter = -1; }
}

void serviceUserControls(void) {
  //check the pushbuttons and footswitches
  for (int i=0; i<3; i++) pushbutton[i].update();
  for (int i=0; i<2; i++) footswitch[i].update();

  //act on the footswitches
  if (footswitch[0].has_state_changed) setRecordingState(footswitch[0].state,current_time_index);

  //act on the pushbuttons (once released)
  if ((pushbutton[1].state == 0) && (pushbutton[1].has_state_changed)) togglePlaybackState();
  if ((pushbutton[2].state == 0) && (pushbutton[2].has_state_changed)) stopAllNotes();
  
  //act on combo-presses foe the push buttons...clear the MIDI recording?
  if ((pushbutton[0].state==1) & (pushbutton[2].state==1)) clearRecordedMIDI();  //combo to clear memory

  //check potentiometer to set max length of loop
  int knob_val = analogRead(KNOB1);
  updateMaxTimeIndex(analogRead(KNOB1));
}

void resetMessageCounters(void) {
  CC_counter = 0;
  PB_counter = 0;
}

void updateMaxTimeIndex(const int &knob_val) {
  static const int n_table = 8;
  static int value_bounds[n_table] = {-1, 511-50, 767-40, 895-30, 959-20, 991-10, 1007, 5000}; //end with some very large value
  static int max_times[n_table] = {MAX_MAX_TIME_INDEX, 8*MIDI_PPQN, 4*MIDI_PPQN, 2*MIDI_PPQN, MIDI_PPQN, MIDI_PPQN/2, MIDI_PPQN/4, MIDI_PPQN/8}; //end with some value that doesn't matter
  int prev_max_time_index = max_time_index;

  //search the table of values
  int ind = 1;
  while ((knob_val > value_bounds[ind]) && (ind < n_table)) ind++;
  ind--;

  //set the new max time index
  max_time_index = max_times[ind];

  //if it has changed, do something
  //if (max_time_index != prev_max_time_index) { 
  //}
}

void setRecordingState(int state, int cur_time_ind) {
  if (is_recording && (state==0)) {
    //turning OFF recording...
    note_buffer.addNoteOffForOnNotes(cur_time_ind);       //if needed, create NOTE_OFF for any open notes

    //recenter the pitch and mod wheels
    addPitchAndModCenterCodes(cur_time_ind); //if needed, create centering coes for pitch and mod wheels
  } else {
    //turning on recording...
    MODWHEEL_on = 0;
    PB_on = 0;
  }
  clear_rx_bytes();
  is_recording = state;
  note_buffer.is_recording = state;
}

void togglePlaybackState(void) {
  note_buffer.stopPlayedNotes();
  is_playing = !is_playing;
}

void stopAllNotes(void) {  //http://www.music-software-development.com/midi-tutorial.html
  Serial.write(0b10110000+MIDI_CHAN); //CC
  Serial.write((byte)123); //all notes off
  Serial.write((byte)0);
}

void clearRecordedMIDI(void) {
  note_buffer.clear();
  for (int i=0; i < MAX_N_CODES; i++) MIDI_time_buffer[i] = -1;
  resetMessageCounters();
  hasTimeCounterBeenExternallySet = false;
}

void addPitchAndModCenterCodes(int cur_time_ind) {
  if (MODWHEEL_on) {
    byte new_message[] = {MIDI_CC, MODWHEEL_CC, (byte)0x00};
    saveMessage(cur_time_ind, new_message);
    MODWHEEL_on = 0; //flag MODWHEEL as off
  }
  if (PB_on) {
    byte new_message[] = {MIDI_PB, 0x00, 0x40}; //this is PB centered on my Prophet 6
    saveMessage(cur_time_ind,new_message);
    PB_on = 0;  //flag PitchBend as off
  }
}

void playCodesForThisTimeStep(int cur_time_ind) {
  //step through all the codes and play those at this timestep
  if (is_playing) {
    note_buffer.playCodesForThisTimeStep(cur_time_ind,max_time_index);
    
    //look through the other MIDI code buffer
    for (int i=0; i < MAX_N_CODES; i++) { //step through ALL time because they may be out of order (from layer-on-layer recording)
      if (MIDI_time_buffer[i] == cur_time_ind) Serial.write(MIDI_command_buffer[i],3); //send the three byte MIDI message
    }
  }
}

void clear_rx_bytes(void) {
  rx_bytes[0]=0;
  rx_bytes[1]=0;
  rx_bytes[2]=0;
}

void saveMessage(int cur_time_ind, byte given_bytes[]) {
  int ind;
  if (is_recording) {
    //Serial.print("saveMess "); Serial.print(rx_bytes[0],HEX);  Serial.print(" "); Serial.println(cur_time_ind);

    if (hasTimeCounterBeenExternallySet==false) {
      cur_time_ind = resetCurTimeIndex();
      hasTimeCounterBeenExternallySet = true;
    }
    
    switch (given_bytes[0]) {
      case (NOTE_ON):
        ind = note_buffer.saveThisNoteOnMessage(cur_time_ind, given_bytes[1],given_bytes[2]);
        if ((ind > -1) && (!ECHOMIDI_BIN)) note_buffer.sendNoteOnMessage(ind); //echo the note on message
        break;
      case (NOTE_OFF):
        ind = note_buffer.saveThisNoteOffMessage(cur_time_ind, given_bytes[1],given_bytes[2]);
        if ((ind > -1) && (!ECHOMIDI_BIN)) note_buffer.sendNoteOffMessage(ind); //echo the note off message
        break;
      case (MIDI_CC):
        if (CC_counter == 0) { //save only the first of this message type
          saveThisMessage(cur_time_ind, given_bytes); 
          CC_counter++;
          if (given_bytes[1] == MODWHEEL_CC) {
            MODWHEEL_on = 1;
            if (given_bytes[2] == 0x00) MODWHEEL_on = 0;
          }
        }
        break;
      case (MIDI_PB):
        if (PB_counter == 0) { //save only the first of this message type
          saveThisMessage(cur_time_ind, given_bytes); 
          PB_counter++;
          PB_on = 1; if ((given_bytes[1] == 0x00) && (given_bytes[2] == 0x40)) PB_on = 0;
        }
        break;
    }
    
  }
}

int saveThisMessage(const int &cur_time_ind, byte given_bytes[]) {
  //should only be used for pitch bend and CC messages
  if (is_recording) {
    if ((buffer_counter == 0) || 
        !(((MIDI_time_buffer[buffer_counter]>>1) == (cur_time_ind>>1)) && //only allow a new code every other MIDI time code
          (MIDI_command_buffer[buffer_counter][0] == given_bytes[0]) &&
          (MIDI_command_buffer[buffer_counter][1] == given_bytes[1]))) {
          
          buffer_counter++; 
          if (buffer_counter >= MAX_N_CODES) buffer_counter=0;  //overwrite the beginning, if necessary
          MIDI_time_buffer[buffer_counter] = cur_time_ind;
          MIDI_command_buffer[buffer_counter][0] = given_bytes[0];
          MIDI_command_buffer[buffer_counter][1] = given_bytes[1];
          MIDI_command_buffer[buffer_counter][2] = given_bytes[2];
          return buffer_counter;
    }
  }
  return -1;
}

int resetCurTimeIndex(void) {
  current_time_index = 0; 
  raw_current_time_index = 0;
  return current_time_index;
}

void turnOnStatLight(const int &pin) {
  digitalWrite(pin,LOW);
}
void turnOffStatLight(const int &pin) {
  digitalWrite(pin,HIGH);
}

