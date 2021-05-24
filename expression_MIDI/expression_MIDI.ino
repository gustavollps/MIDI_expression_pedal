const bool DEBUG = false;

// Other Values
const int int_ExpMinChange = 1;  
const int int_DelayMS = 10;      // time between samples, and between MIDI writes.
const int AIn_Exp = A0;

// Variables
int int_ExpMidi = 0;         // 0 to 127
int int_ExpMidiPrevious = 0;  // previously SENT VALUE of expression position

// Functions
void MidiPC(int pr_num);            // function for performing midi program changes
void MidiCC(int c_num, int c_val);  // function for performing midi control changes
int ConvertRawToMidi(int int_RawVal);

void setup() {
  // Serial connection
  if (DEBUG) {
    Serial.begin(115200);
  } else {
    Serial.begin(115200);
  }
}

uint64_t time_old = 0;
uint64_t time_now = 0;
uint64_t T = 10;

uint64_t time_pressed = 0;
uint64_t time_released = 0;
uint64_t state_changed_at = 0;

#define T_MIN_PRESS_FIRST 50
#define T_MAX_RELEASE_WAIT 500
#define T_MIN_EXPRESSION_ON_OFF_WAIT 10000
#define T_MAX_WAIT_PRESS_SECOND 500
#define T_MIN_PRESS_SECOND 1000
#define T_COOLDOWN 1500


bool button_state = false;

enum state {
  WAIT_PRESS,
  CALCULATE_FIRST_PRESS,
  WAIT_RELEASE,
  WAIT_FIRST_INTERVAL,
  COOLDOWN
};

state press_state;
uint64_t expression_on_off_timeout = 10000;
uint64_t start_program = millis();
bool expression_on_off = true;
bool change_expression_on_off = false;

void loop() {

  // Pedal will have a limit to how quickly it can respond to repeated MIDI messages
  time_now = millis();
  if ((time_now - time_old) > T) {
    //Serial.println(press_state);
    time_old = time_now;

    int_ExpMidi = ConvertRawToMidi(analogRead(AIn_Exp));
    if (int_ExpMidi > 122) {
      button_state = true;
    }
    else {
      button_state = false;
    }

    if (press_state == WAIT_PRESS) { //waiting for press
      if (button_state) {
        state_changed_at = millis();
        press_state = CALCULATE_FIRST_PRESS;
      }
    }

    else if (press_state == CALCULATE_FIRST_PRESS) { // calculating first press time
      uint64_t time_in_state = millis() - state_changed_at;
      if (button_state) {
        if (time_in_state > T_MIN_PRESS_FIRST) {
          press_state = WAIT_RELEASE;
          state_changed_at = millis();
        }
      }
      else {
        press_state = WAIT_PRESS;
      }
    }

    else if (press_state == WAIT_RELEASE) { // wait for release
      uint64_t time_in_state = millis() - state_changed_at;
      if (time_in_state > T_MAX_RELEASE_WAIT) { //-------------------------CMD 0 (long click reset configuration)
        if (time_in_state > T_MIN_EXPRESSION_ON_OFF_WAIT) {
          if(DEBUG){
            Serial.println("----------------EXPRESSION ON/OFF");            
          }
          start_program = millis();
          change_expression_on_off = true;
          state_changed_at = millis();
          press_state = COOLDOWN;
        }
        else if (!button_state) { //-------------------------CMD 3 (long click)
          if (DEBUG) {
            Serial.print("----------------");
            Serial.println(17);
          }
          MidiCC(17, 0);
          MidiCC(17, 127);
          state_changed_at = millis();
          press_state = COOLDOWN;
        }
      }
      else if (!button_state) {
        press_state = WAIT_FIRST_INTERVAL;
        state_changed_at = millis();
      }
    }

    else if (press_state == WAIT_FIRST_INTERVAL) { // wait interval for command 0 or proceed to next command
      uint64_t time_in_state = millis() - state_changed_at;
      //Serial.println(int(time_in_state));
      if (button_state) { //------------------------ CMD 1 (double click)
        if (DEBUG) {
          Serial.print("----------------");
          Serial.println(16);
        }
        MidiCC(16, 0);
        MidiCC(16, 127);
        press_state = COOLDOWN;
        state_changed_at = millis();
      }
      else if (time_in_state > T_MAX_WAIT_PRESS_SECOND) {  //--------------------- CMD 2 (short click)
        if (DEBUG) {
          Serial.print("----------------");
          Serial.println(18);
        }
        MidiCC(18, 0);
        MidiCC(18, 127);

        state_changed_at = millis();
        press_state = COOLDOWN;

      }
    }
    else if (press_state == COOLDOWN) { // cooldown wait to accept new cmd
      uint64_t time_in_state = millis() - state_changed_at;

      if (time_in_state > T_COOLDOWN)
        press_state = WAIT_PRESS;

    }


    // analog pedal command   
    if(change_expression_on_off){             
      change_expression_on_off = false;
      expression_on_off = !expression_on_off;      
    }

    if(expression_on_off){
      if (abs(int_ExpMidi - int_ExpMidiPrevious) > int_ExpMinChange) {      
        int_ExpMidiPrevious = int_ExpMidi;
        MidiCC(11, int_ExpMidi);
      }
    }

  }




}

int ConvertRawToMidi(int int_RawVal) {
  // Reference Variables
  int int_RawMin = 150;
  int int_RawMax = 720;
  int int_MidiMin = 0;
  int int_MidiMax = 127;

  int int_NewVal = constrain(map(int_RawVal, int_RawMin, int_RawMax, int_MidiMin, int_MidiMax), 0, 127);

  return int_NewVal;
}

void MidiCC(int c_num, int c_val) {
  // Function for sending a midi Control Change. Simply input the command number and
  // the data value that you want.
  // Control Change message indicated by anything from 176 to 191, use 176.
  if (DEBUG) {
    Serial.print(176);      // tells midi this is a Control Change
    Serial.print(c_num);    // tells midi what command number to change (data byte 1)
    Serial.println(c_val);  // tells midi what value to set for command (data byte 2)
  } else {
    Serial.write(176);      // tells midi this is a Control Change
    Serial.write(c_num);    // tells midi what command number to change (data byte 1)
    Serial.write(c_val);    // tells midi what value to set for command (data byte 2)
  }
}

void MidiPC(int pr_num) {
  // Function to send a midi Program Change (or preset change for Line6). Input the preset number
  // you want to switch to.
  // Program Change message indicated by anything from 192 to 207, use 192.
  if (DEBUG) {
    Serial.print(192);    // tells midi this is a Program Change
    Serial.print(pr_num); // tells midi what program to switch to (data byte 1)
  } else {
    Serial.write(192);    // tells midi this is a Program Change
    Serial.write(pr_num); // tells midi what program to switch to (data byte 1)
  }
}
