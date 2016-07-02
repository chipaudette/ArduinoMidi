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


#define ECHOMIDI (false)
#define ECHOMIDI_BIN (false)
#define ECHOMIDI_CLOCK (false)

// define sequencer limits
#define MAX_N_BEATS (16)    //number of beats (quarter notes) to loop
#define MAX_N_CODES (50)  //make this small enough so that 4*N_STORED_CODES bytes fits in memory

// MIDI Code information
#define MIDI_PPQN  (24)   //MIDI clock "pulses per quarter note"
#define MIDI_CHAN  (0x00) //0x00 is omni 
#define MIDI_CLOCK (0xF8) //Code for MIDI Clock
#define NOTE_ON    (0x90+MIDI_CHAN) //Code for MIDI Note On
#define NOTE_OFF   (0x80+MIDI_CHAN) //Code for MIDI Note Off
#define MIDI_CC    (0xB0+MIDI_CHAN) //Code for MIDI CC
//define MIDI_AT   (0b11010000+MIDI_CHAN) //Code for Per-Channel Aftertouch
#define MIDI_PB    (0xE0+MIDI_CHAN) //pitch bend
#define MODWHEEL_CC     (0x01)  //CC code for the mod wheel (for my prophet 6)

// defines for MIDI Shield components (NOT needed if you just want to echo the MIDI codes)
#define KNOB1  0   //potentiometer on sparkfun MIDI shield
#define KNOB2  1   //potentiometer on sparkfun MIDI shield
#define STAT1  7   //status LED on sparkfun MIDI shield
#define STAT2  6   //status LED on sparkfun MIDI shield


//define variables
byte foo_byte, rx_bytes[3];  //standard 3 byte MIDI transmission
int byte_counter=-1;
byte MIDI_command_buffer[MAX_N_CODES][3]; //this can get big fast!
int buffer_counter=-1;
int MIDI_time_buffer[MAX_N_CODES];
int current_time_index = 0;
int max_time_index = MAX_N_BEATS*MIDI_PPQN;
int CC_counter = 0, MODWHEEL_on = 0;
int PB_counter = 0, PB_on = 0;
int is_recording = 0; //record incoming MIDI messages?
int is_playing = 1;   //playback the recorded MIDI messages?

#define MAX_N_NOTES 120
typedef struct MIDI_Note_t {
    boolean isActive;
    int timeOn;
    int timeOff;
    byte noteNum;
    byte onVel;
    byte offVel;
};
MIDI_Note_t MIDI_note_buffer[MAX_N_NOTES];
int current_MIDI_Note_Index=0;


class Button {
  private:
    int pin;
    
  public:
    int state;
    int has_state_changed;
    Button(int p) {
      pin = p;
      state = 0;
      has_state_changed=0;

      pinMode(pin, INPUT_PULLUP);  //prepare the input
    }
    
    int update(void) {
      int raw_val = !digitalRead(pin);
      if (raw_val != state) {
        state = raw_val;
        has_state_changed=1;
      } else {
        has_state_changed = 0;
      }
      return state;
    }
};

Button pushbutton[3] = {Button(2), Button(3), Button(4)};  //the MIDI Shield's 3 pushbuttons
Button footswitch[2] = {Button(A2), Button(A3)}; //I've got footswitches attached to these analog inputs


// other variables
//int FS1,FS2; //foot swtiches connected to the analog inputs

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
  if(Serial.available() > 0)
  {     
    //read the byte
    foo_byte = Serial.read();

    //what kind of message is it?
    if (foo_byte == MIDI_CLOCK) { //MIDI CLOCK
      //if (ECHOMIDI_CLOCK) Serial.println(foo_byte,HEX);
      current_time_index++;
      if (current_time_index >= max_time_index) current_time_index = 0;
      resetMessageCounters();
      playCodesForThisTimeStep(current_time_index);
      
    } else { //not a MIDI Clock
      if (ECHOMIDI) { Serial.print(foo_byte,HEX); Serial.print(" "); }
      if (ECHOMIDI_BIN | is_recording) { Serial.write(foo_byte); }
      if (foo_byte & (0b10000000+MIDI_CHAN)) { //bitwise compare to detect start of MIDI_Message (for this channel)
        turnOnStatLight(STAT1);   //turn on the STAT1 light indicating that it's received some Serial comms
        if (byte_counter > 0) saveMessage(current_time_index,rx_bytes);  //save the previously started message
        clear_rx_bytes();  byte_counter = -1; //clear the temporary storage
        if ((foo_byte == NOTE_ON) || (foo_byte == NOTE_OFF) || (foo_byte == MIDI_CC) || (foo_byte == MIDI_PB)) {
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
    } //close if MIDI Clock
    
  } else {    //if Serial.isAvailable()
    delay(1); //just to keep from looping too fast

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
    if (analogRead(KNOB1) < 511) {  //the wheel turns backwards!  smaller values mean the knob is more clockwise
      max_time_index = MAX_N_BEATS*MIDI_PPQN;
    } else {
      max_time_index = MAX_N_BEATS*MIDI_PPQN/2;
    }
    
  } //end if Serial.isAvailable()
} //end loop()

void resetMessageCounters(void) {
  CC_counter = 0;
  PB_counter = 0;
}

void setRecordingState(int state, int cur_time_ind) {
  if (is_recording && (state==0)) {
    //turning OFF recording...
    addNoteOffMidiCodes(cur_time_ind);       //if needed, create NOTE_OFF for any open notes

    //recenter the pitch and mod wheels
    addPitchAndModCenterCodes(cur_time_ind); //if needed, create centering coes for pitch and mod wheels
  } else {
    //turning on recording...
    MODWHEEL_on = 0;
    PB_on = 0;
  }
  clear_rx_bytes();
  is_recording = state;
}

void togglePlaybackState(void) {
  stopPlayedNotes();
  is_playing = !is_playing;
}

void stopAllNotes(void) {  //http://www.music-software-development.com/midi-tutorial.html
  Serial.write(0b10110000+MIDI_CHAN); //CC
  Serial.write((byte)123); //all notes off
  Serial.write((byte)0);
}

void clearRecordedMIDI(void) {
  stopPlayedNotes();
  for (int i=0; i < MAX_N_CODES; i++) MIDI_time_buffer[i] = -1;
  for (int i=0; i < MAX_N_NOTES; i++) { 
    if (MIDI_note_buffer[i].isActive) sendNoteOffMessage(i);
    MIDI_note_buffer[i].timeOn = -1; 
    MIDI_note_buffer[i].timeOff = -1; 
  };
  resetMessageCounters();
}

void stopPlayedNotes(void) {
  //step through all the codes and stop those those that had been played
  //for (int i=0; i < MAX_N_CODES; i++) { //step through ALL time because they may be out of order (from layer-on-layer recording)
  //  if (MIDI_time_buffer[i] > -1) {
  //    if (MIDI_command_buffer[i][0] == NOTE_ON) {
  //      Serial.write(NOTE_OFF);
  //      Serial.write(MIDI_command_buffer[i][1]);
  //      Serial.write(MIDI_command_buffer[i][2]);
  //    }
  //  }
  //}
  for (int i=0; i < MAX_N_NOTES; i++ ) {
    if (MIDI_note_buffer[i].isActive) sendNoteOffMessage(i);
  }
}

void addNoteOffMidiCodes(int cur_time_ind) {
  //step through array and see if any notes are open;
//  for (int i=0; i < MAX_N_CODES; i++) {
//    if (MIDI_time_buffer[i] > -1) { //is it a valid entry?
//      if (MIDI_command_buffer[i][0] == NOTE_ON) { //is this a NOTE_ON message
//        byte note_num = MIDI_command_buffer[i][1];
//        
//        //this is a note on message, count how many note on and note off for this note number
//        int noteOn_count = countCodesEqualTo(NOTE_ON,(2-1),note_num);  //note_num is the 2nd byte in the MIDI message
//        int noteOff_count = countCodesEqualTo(NOTE_OFF,(2-1),note_num);//note_num is the 2nd byte in the MIDI message
//
//        //If there is an imbalance of NOTE_ON and NOTE_OFF, make NOTE_OFF messages
//        if (noteOn_count > noteOff_count) {
//          //create NOTE_OFF message
//          byte new_message[] = {(byte)NOTE_OFF, (byte)note_num, (byte)64};
//
//          //add the NOTE_OFF mesage to the MIDI command buffer
//          for (int j=noteOff_count; j < noteOn_count; j++) { //add a notoff for every extra note on
//            saveMessage(cur_time_ind,new_message);
//            Serial.write(new_message,3); //and write the new message to turn off the note
//          }
//        }
//      }
//    }
//  }
  for (int i=0; i < MAX_N_NOTES; i++) {
    if ((MIDI_note_buffer[i].timeOn > -1) & (MIDI_note_buffer[i].timeOff < 0)) {
      byte new_message[] = {(byte)NOTE_OFF, (byte)MIDI_note_buffer[i].noteNum, (byte)64};
      saveThisNoteOffMessage(cur_time_ind,new_message[1],new_message[2]);
      Serial.write(new_message,3); //and write the new message to turn off the note
    }
  }
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

int countCodesEqualTo(const byte &MIDI_code,const int &byte_ind, const byte &test_value) {
  int count=0;
  byte stored_value;
  for (int i=0; i < MAX_N_CODES; i++) { //loop over all entries
    if (MIDI_time_buffer[i] > -1) { //is it a valid entry?
      if (MIDI_command_buffer[i][0] == MIDI_code) {
        if (MIDI_command_buffer[i][byte_ind] == test_value) count++;
      }
    }
  }
  return count;
}

void playCodesForThisTimeStep(int cur_time_ind) {
  //step through all the codes and play those at this timestep
  if (is_playing) {
    //look through the note buffer
    for (int i=0; i < MAX_N_NOTES; i++) {
      if (MIDI_note_buffer[i].timeOn == cur_time_ind) sendNoteOnMessage(i);
      if (MIDI_note_buffer[i].timeOff == cur_time_ind) sendNoteOffMessage(i);
    }
    
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

void saveMessage(const int &cur_time_ind, byte given_bytes[]) {
  if (is_recording) {
    //Serial.print("saveMess "); Serial.print(rx_bytes[0],HEX);  Serial.print(" "); Serial.println(cur_time_ind);
    switch (given_bytes[0]) {
      case (NOTE_ON):
        saveThisNoteOnMessage(cur_time_ind, given_bytes[1],given_bytes[2]);
        break;
      case (NOTE_OFF):
        saveThisNoteOffMessage(cur_time_ind, given_bytes[1],given_bytes[2]);
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

void saveThisMessage(const int &cur_time_ind, byte given_bytes[]) {
  if (is_recording) {
    buffer_counter++; 
    if (buffer_counter >= MAX_N_CODES) buffer_counter=0;  //overwrite the beginning, if necessary
    MIDI_time_buffer[buffer_counter] = cur_time_ind;
    MIDI_command_buffer[buffer_counter][0] = given_bytes[0];
    MIDI_command_buffer[buffer_counter][1] = given_bytes[1];
    MIDI_command_buffer[buffer_counter][2] = given_bytes[2];
  }
}

void saveThisNoteOnMessage(const int &cur_time_ind, const byte &noteNum, const byte &vel) {
  if (is_recording) {
    incrementNoteCounter();

    //save the note information
    MIDI_note_buffer[current_MIDI_Note_Index].timeOn = cur_time_ind;
    MIDI_note_buffer[current_MIDI_Note_Index].timeOff = -1;
    MIDI_note_buffer[current_MIDI_Note_Index].noteNum = noteNum;
    MIDI_note_buffer[current_MIDI_Note_Index].onVel = vel;
  }
}

void incrementNoteCounter(void) {
  current_MIDI_Note_Index++; 
  if (current_MIDI_Note_Index >= MAX_N_NOTES) {
    current_MIDI_Note_Index=0;  //overwrite the beginning, if necessary
    if (MIDI_note_buffer[current_MIDI_Note_Index].isActive) { //before overwriting the note, should we turn it off first?
      if (MIDI_note_buffer[current_MIDI_Note_Index].timeOff > -1) { //it is a valid noteOff message
        //write the Note Off message
        sendNoteOffMessage(current_MIDI_Note_Index);
      }
    }
  }
}

void sendNoteOffMessage(const int &ind) {
  Serial.write(NOTE_OFF);
  Serial.write(MIDI_note_buffer[ind].noteNum);
  Serial.write(MIDI_note_buffer[ind].offVel);
  MIDI_note_buffer[ind].isActive=0;
}

void sendNoteOnMessage(const int &ind) {
  Serial.write(NOTE_ON);
  Serial.write(MIDI_note_buffer[ind].noteNum);
  Serial.write(MIDI_note_buffer[ind].onVel);
  MIDI_note_buffer[ind].isActive=1;
}

void saveThisNoteOffMessage(const int &cur_time_ind, const byte &noteNum, const byte &vel) {
  if (is_recording) {
    int ind = findNoteInBuffer(noteNum);
    if (ind < 0) return;

    //save the note information
    MIDI_note_buffer[ind].timeOff = cur_time_ind;
    MIDI_note_buffer[ind].offVel = vel;
  }
}

int findNoteInBuffer(const byte &noteNum) {
  int ind = -1;
  for (int i=0; i<MAX_N_NOTES; i++)  {
    if (MIDI_note_buffer[i].noteNum == noteNum) {
      if (MIDI_note_buffer[i].timeOff < 0) {
        return i;
      }
    }
  }
  return ind;
}

void turnOnStatLight(const int &pin) {
  digitalWrite(pin,LOW);
}
void turnOffStatLight(const int &pin) {
  digitalWrite(pin,HIGH);
}

