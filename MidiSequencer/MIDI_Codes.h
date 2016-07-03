
#ifndef MIDI_Codes_h
#define MIDI_Codes_h

// MIDI Code information
#define MIDI_PPQN  (24)   //MIDI clock "pulses per quarter note"
#define MIDI_CHAN  (0x00) //0x00 is omni 
#define MIDI_CLOCK (0xF8) //Code for MIDI Clock
#define MIDI_START (0xFA)  //start clock...is this also a clock code?
#define NOTE_ON    (0x90+MIDI_CHAN) //Code for MIDI Note On
#define NOTE_OFF   (0x80+MIDI_CHAN) //Code for MIDI Note Off
#define MIDI_CC    (0xB0+MIDI_CHAN) //Code for MIDI CC
#define MIDI_AT   (0b11010000+MIDI_CHAN) //Code for Per-Channel Aftertouch
#define MIDI_PB    (0xE0+MIDI_CHAN) //pitch bend
#define MODWHEEL_CC     (0x01)  //CC code for the mod wheel (for my prophet 6)

#endif MIDI_Codes_h
