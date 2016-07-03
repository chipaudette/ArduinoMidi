#ifndef Button_h
#define Button_h

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

#endif Button_h
