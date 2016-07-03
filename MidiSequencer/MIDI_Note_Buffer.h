
#ifndef MIDI_Note_Buffer_h
#define MIDI_Note_Buffer_h

#include <arduino.h>
#include "MIDI_Codes.h"

typedef struct MIDI_Note_t {
    boolean isActive;
    int timeOn;
    int timeOff;
    byte noteNum;
    byte onVel;
    byte offVel;
};

#define MAX_N_NOTES 120
class MIDI_Note_Buffer {
  private:
    MIDI_Note_t note_buffer[MAX_N_NOTES];
    const int max_n_notes = MAX_N_NOTES;
    int current_note_buffer_index=0;
    Stream *serial = &Serial;

  public:
    boolean is_recording = false;
    MIDI_Note_Buffer(HardwareSerial *s) { 
      serial = s;
      clear();
    }
    
    void clear(void) {
      stopPlayedNotes();
      for (int i=0; i < max_n_notes; i++) { 
        if (note_buffer[i].isActive) sendNoteOffMessage(i);
        note_buffer[i].timeOn = -1; 
        note_buffer[i].timeOff = -1; 
      };
      current_note_buffer_index = 0;
    }
    
    void addNoteOffForOnNotes(int cur_time_ind) {
      //step through array and see if any notes are open;
      for (int i=0; i < max_n_notes; i++) {
        if ((note_buffer[i].timeOn > -1) & (note_buffer[i].timeOff < 0)) {
          byte new_message[] = {(byte)NOTE_OFF, (byte)note_buffer[i].noteNum, (byte)64};
          saveThisNoteOffMessage(cur_time_ind,new_message[1],new_message[2]);
          serial->write(new_message,3); //and write the new message to turn off the note
        }
      }
    }

    
    int saveThisNoteOnMessage(const int &cur_time_ind, const byte &noteNum, const byte &vel) {
      if (is_recording) {
        incrementNoteBufferIndex();
    
        //save the note information
        note_buffer[current_note_buffer_index].timeOn = cur_time_ind;
        note_buffer[current_note_buffer_index].timeOff = -1;
        note_buffer[current_note_buffer_index].noteNum = noteNum;
        note_buffer[current_note_buffer_index].onVel = vel;
        return current_note_buffer_index;
      }
      return -1;
    }
    
    void incrementNoteBufferIndex(void) {
      current_note_buffer_index++; 
      if (current_note_buffer_index >= max_n_notes) {
        current_note_buffer_index=0;  //overwrite the beginning, if necessary
        if (note_buffer[current_note_buffer_index].isActive) { //before overwriting the note, should we turn it off first?
          if (note_buffer[current_note_buffer_index].timeOff > -1) { //it is a valid noteOff message
            //write the Note Off message
            sendNoteOffMessage(current_note_buffer_index);
          }
        }
      }
    }
    
    void sendNoteOffMessage(const int &ind) {
      serial->write(NOTE_OFF);
      serial->write(note_buffer[ind].noteNum);
      serial->write(note_buffer[ind].offVel);
      note_buffer[ind].isActive=0;
    }
    
    void sendNoteOnMessage(const int &ind) {
      serial->write(NOTE_ON);
      serial->write(note_buffer[ind].noteNum);
      serial->write(note_buffer[ind].onVel);
      note_buffer[ind].isActive=1;
    }
    
    int saveThisNoteOffMessage(const int &cur_time_ind, const byte &noteNum, const byte &vel) {
      if (is_recording) {
        int ind = findNoteInBuffer(noteNum);
        if (ind < 0) return ind;
    
        //save the note information
        note_buffer[ind].timeOff = cur_time_ind;
        note_buffer[ind].offVel = vel;
        return ind;
      }
      return -1;
    }
    
    int findNoteInBuffer(const byte &noteNum) {
      int ind = -1;
      for (int i=0; i<max_n_notes; i++)  {
        if (note_buffer[i].noteNum == noteNum) {
          if (note_buffer[i].timeOff < 0) {
            return i;
          }
        }
      }
      return ind;
    }
        
    void playCodesForThisTimeStep(const int &cur_time_ind, const int &max_time_ind) {
      static int prev_time_ind = -1;
      if (prev_time_ind >= max_time_ind) prev_time_ind = -1;
      
      //look through the note buffer
      for (int i=0; i < max_n_notes; i++) {
        if (note_buffer[i].timeOn == cur_time_ind) sendNoteOnMessage(i); //turn on notes
        if ((note_buffer[i].timeOff > prev_time_ind) && (note_buffer[i].timeOff <= cur_time_ind)) sendNoteOffMessage(i); //turn off note
      }

      //now, capture cases where we're not playing the full buffer's worth of time (ie, max_time_ind < MAX_MAX_TIME_IND)
      //such that notes might get turned on during valid time, but the note_off messages are beyond the end of valid
      //time.  If they're beyond the end, turn them off just before we wrap around in time.
      if (cur_time_ind == (max_time_ind-1)) { //is it the last time index?
        for (int i=0; i < max_n_notes; i++) { //loop over each note in the buffer
          if (note_buffer[i].timeOff > cur_time_ind) { //is the noteOff past the end of valid time
            if (note_buffer[i].isActive == 1) { //is the note currently active?
              sendNoteOffMessage(i); //turn off the note
            }
          }
        }
      }

      if (cur_time_ind < (max_time_ind-1)) {
        prev_time_ind = cur_time_ind; 
      } else {
        prev_time_ind = -1;
      }
    }



    
    void stopPlayedNotes(void) {
      for (int i=0; i < max_n_notes; i++ ) {
        if (note_buffer[i].isActive) sendNoteOffMessage(i);
      }
    }
    

};

#endif
