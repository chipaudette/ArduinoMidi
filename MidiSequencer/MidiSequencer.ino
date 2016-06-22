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


#define ECHOMIDI (false)
#define ECHOMIDI_CLOCK (false)

// define sequencer limits
#define MAX_N_BEATS (8)    //number of beats (quarter notes) to loop
#define MAX_N_CODES (200)  //make this small enough so that 4*N_STORED_CODES bytes fits in memory

// MIDI Code information
#define MIDI_PPQN  (24)   //MIDI clock "pulses per quarter note"
#define MIDI_CHAN  (0x00) //0x00 is omni 
#define MIDI_CLOCK (0xF8) //Code for MIDI Clock
#define NOTE_ON    (0x90+MIDI_CHAN) //Code for MIDI Note On
#define NOTE_OFF   (0x80+MIDI_CHAN) //Code for MIDI Note Off
#define MIDI_CC    (0b10110000+MIDI_CHAN) //Code for MIDI CC
#define MIDI_AT    (0b11010000+MIDI_CHAN) //Code for Per-Channel Aftertouch
#define MIDI_PB    (0b11100000+MIDI_CHAN) //pitch bend

// defines for MIDI Shield components (NOT needed if you just want to echo the MIDI codes)
#define KNOB1  0   //potentiometer on sparkfun MIDI shield
#define KNOB2  1   //potentiometer on sparkfun MIDI shield
#define BUTTON1  2  //pushbutton on sparkfun MIDI shield
#define BUTTON2  3  //pushbutton on sparkfun MIDI shield
#define BUTTON3  4  //pushbutton on sparkfun MIDI shield
#define STAT1  7   //status LED on sparkfun MIDI shield
#define STAT2  6   //status LED on sparkfun MIDI shield

//define variables
byte foo_byte, rx_bytes[3];  //standard 3 byte MIDI transmission
int byte_counter=-1;
byte MIDI_command_buffer[MAX_N_CODES][3]; //this can get big fast!
int buffer_counter=-1;
int MIDI_time_buffer[MAX_N_CODES];
int current_time_index = 1;
int max_time_index = MAX_N_BEATS*MIDI_PPQN;
int CC_counter = 0;
int PB_counter = 0;
int AT_counter = 0;

// other variables
//int FS1,FS2; //foot swtiches connected to the analog inputs

void setup() {
  pinMode(STAT1,OUTPUT); //prepare the LED outputs
  pinMode(STAT2,OUTPUT); //prepare the LED outputs

  //initialize the time codes
  for (int i=0; i < MAX_N_CODES; i++) MIDI_time_buffer[i] = -1;
  
  //start serial with midi baudrate 31250
  Serial.begin(31250);     
}

void loop () {
  turnOffStatLight(STAT1);     //turn off the STAT1 light
  
  //Are there any MIDI messages?
  if(Serial.available() > 0)
  {    
    //read the byte
    foo_byte = Serial.read();

    //what kind of message is it?
    if (foo_byte == MIDI_CLOCK) { //MIDI CLOCK
      if (ECHOMIDI_CLOCK) Serial.println(foo_byte,HEX);
      current_time_index++;
      if (current_time_index >= max_time_index) current_time_index = 0;
      resetMessageCounters();
      playCodesForThisTimeStep(current_time_index);
      
    } else { //not a MIDI Clock
      if (ECHOMIDI) { Serial.print(foo_byte,HEX); Serial.print(" "); }
      if (foo_byte & (0b10000000+MIDI_CHAN)) { //start of MIDI_Message (for this channel)
        turnOnStatLight(STAT1);   //turn on the STAT1 light indicating that it's received some Serial comms
        if (byte_counter > 0) saveMessage(current_time_index);  //save the previously started message
        clear_rx_bytes();  byte_counter = -1; //clear the temporary storage
        if ((foo_byte == NOTE_ON) || (foo_byte == NOTE_OFF) || (foo_byte == MIDI_CC) || (foo_byte == MIDI_PB) || (foo_byte == MIDI_AT)) {
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
      if (byte_counter == (3-1)) { saveMessage(current_time_index);  clear_rx_bytes();  byte_counter = -1; }
    } //close if MIDI Clock
  } else {    //if Serial.isAvailable()
    delay(1);
  }
}

void resetMessageCounters(void) {
  CC_counter = 0;
  PB_counter = 0;
  AT_counter = 0;
}

void playCodesForThisTimeStep(int cur_time_ind) {
  //Serial.print("playCodes "); Serial.println(cur_time_ind);
  //step through all the codes and play those at this timestep
  for (int i=0; i < MAX_N_CODES; i++) { //step through ALL time because they may be out of order (from layer-on-layer recording)
    if (MIDI_time_buffer[i] == cur_time_ind) {
      //Serial.print("playing! "); Serial.print(cur_time_ind); Serial.print(" ");Serial.println(MIDI_command_buffer[i][0],HEX);
      turnOnStatLight(STAT2);   //turn on the STAT1 light indicating that it's received some Serial comms
      Serial.write(MIDI_command_buffer[i][0]);
      Serial.write(MIDI_command_buffer[i][1]);
      Serial.write(MIDI_command_buffer[i][2]);
    }
  }
  turnOffStatLight(STAT2);   //turn on the STAT1 light indicating that it's received some Serial comms
}

void clear_rx_bytes(void) {
  rx_bytes[0]=0;
  rx_bytes[1]=0;
  rx_bytes[2]=0;
}

void saveMessage(int cur_time_ind) {
  //Serial.print("saveMess "); Serial.print(rx_bytes[0],HEX);  Serial.print(" "); Serial.println(cur_time_ind);
  switch (rx_bytes[0]) {
    case (NOTE_ON):
      saveThisMessage(cur_time_ind); //save all of this type of message
      break;
    case (NOTE_OFF):
      saveThisMessage(cur_time_ind); //save all of this type of message
      break;
    case (MIDI_CC):
      if (CC_counter == 0) { //save only the first of this message type
        saveThisMessage(cur_time_ind); 
        CC_counter++;
      }
      break;
    case (MIDI_PB):
      if (PB_counter == 0) { //save only the first of this message type
        saveThisMessage(cur_time_ind); 
        PB_counter++;
      }
      break;
    case (MIDI_AT):
      if (AT_counter == 0) { //save only the first of this message type
        saveThisMessage(cur_time_ind); 
        AT_counter++;
      }
      break;  
  }
}

void saveThisMessage(int cur_time_ind) {
  buffer_counter++; 
  //Serial.print("saveThisMess "); Serial.print(rx_bytes[0],HEX); Serial.print(" ");Serial.println(buffer_counter);
  if (buffer_counter >= MAX_N_CODES) buffer_counter=0;  //overwrite the beginning, if necessary
  MIDI_time_buffer[buffer_counter] = cur_time_ind;
  MIDI_command_buffer[buffer_counter][0] = rx_bytes[0];
  MIDI_command_buffer[buffer_counter][1] = rx_bytes[1];
  MIDI_command_buffer[buffer_counter][2] = rx_bytes[2];
}


void turnOnStatLight(int pin) {
  digitalWrite(pin,LOW);
}
void turnOffStatLight(int pin) {
  digitalWrite(pin,HIGH);
}

