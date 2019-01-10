/*
 * airSynthMidController.ino
 * 
 * v0.1
 * Created: 2018.11.21
 * Author: Matthew Dunlap
 * 
 * The Air Drum is a device that contains three IR distance sensors aiming upwards. This device is driven
 * by an Arduino micro that appears as a USB MIDI device when plugged into a computer or iPad. The device will
 * send a MIDI note and velocity when a user taps or waves their hand in the air above the sensor.
 * 
 */
#include <MCP4922.h>
#include <SPI.h>
#include <Bounce2.h>

const int irDistancePin[6] = {A4, A8, A3, A7, A6, A5}; // pins for IR sensors
const int button1Pin = 6; // buttons on side of device
const int button2Pin = 7;
const int ledPin = 2;
int irDistanceVal[6];  // analog value based on voltage from IR sensor
int noteVal[6] = {60,61,62,63,64,65};      // midi note value 0 - 127
int velocityVal = 64;  // any value between 1-127
const int channel = 1; // midi channel in which we are communicating
bool soundOn = false;  // flag that allows loop to know if the note is still being played
bool noteActuated[3] = {false, false, false}; // track if each sensor has been actuated
bool noteSustained[3] = {false, false, false}; // track if each note is currently sustaining
bool noteReleased[3] = {true, true, true}; // track if note has been released; if true, no note should play
int repeatDelay = 1000;  // delay in milliseconds between each note
int releasePoint = 260;
int actuationPoint = 300;
int sustainPoint = 450;
int modeSelected = 3;  // mode that determines how the velocity and pitch are sent
int delayTime = 50;
bool buttonNoteOn1 = false; // track if the midi note on has been sent for button press
bool buttonNoteOn2 = false;

// setup debounce
Bounce debouncer1 = Bounce();
Bounce debouncer2 = Bounce();

MCP4922 dac1(11,13,10,3);
MCP4922 dac2(11,13,9,4);
MCP4922 dac3(11,13,15,5);

void setup() {
  // setup pins for IR sensors
  for( int i = 0; i < 6; i++ ) {
    pinMode(irDistancePin[i], INPUT);
  } 
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  // attach and setup debouncer interval in ms
  debouncer1.attach(button1Pin);
  debouncer2.attach(button2Pin);
  debouncer1.interval(5);
  debouncer2.interval(5);
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);
  SPI.begin();
  digitalWrite(ledPin, HIGH);
}

void loop() {
    polyPressureMode();
    ircvMode();  
    buttonsMidi();
}

void pollSerial() {
  byte modeSelection = Serial.read();
  
  if (Serial.available() > 0) {
    Serial.println(modeSelection, DEC);
    switch (modeSelection) {
    case 49:
      modeSelected = 1;
      Serial.println("Drummer Mode Selected");
      selectPitchesSerial();
      break;
    case 50:
      modeSelected = 2;
      usbMIDI.sendControlChange(101, 0, channel);
      usbMIDI.sendControlChange(100, 0, channel);
      usbMIDI.sendControlChange(6, 24, channel);// change pitch bend range
      usbMIDI.sendControlChange(101, 127, channel);
      usbMIDI.sendControlChange(100, 127, channel);
      Serial.println("Pitch Bend / Aftertouch Mode Selected");
      break;
    case 51:
      modeSelected = 3;
      Serial.println("IR to CV Mode Selected");
      break;
    case 52:
      modeSelected = 4;
      Serial.println("Serial Selected");
      break;
    default:
      break;
    }
  }  
}

void selectPitchesSerial(){
  String serialRead;
  int notesToSet = 2;
  while (notesToSet > 0){
    int noteDigits = 3;
    serialRead = Serial.read();
    String noteString = "";
    while (noteDigits > 0){
      if (Serial.available() > 0) {
        noteString += serialRead;
        noteDigits -= 1;
      }
    }
    noteVal[2 - notesToSet] = noteString.toInt();
    Serial.println((String)"Note Value " + (2 - notesToSet) + " changed to " + noteString.toInt());
    notesToSet -= 1;
  }
}

// drummerMode is a mode that takes two pitches and emulates a drum
void drummerMode(int noteOne, int noteTwo){
  // for loop to read irDistanceVals and play note
  for( int i = 0; i < 2; i++ ) {
    irDistanceVal[i] = analogRead(irDistancePin[i]);

    // actuate the note for IR sensor 0
    if ((irDistanceVal[i] > actuationPoint) && (noteActuated[i] == false)) {
      // play a note
      usbMIDI.sendNoteOn(noteVal[i], 99, channel); // note, vel, channel
      
      // set flags
      noteActuated[i] = true;
      noteReleased[i] = false;
    }

    // release the note
    if ((irDistanceVal[i] < releasePoint) && (noteReleased[i] == false)){
      // stop sustaining the note
      // ...
      
      // stop the note
      usbMIDI.sendNoteOff(noteVal[i], 99, channel);

      // set flags
      noteActuated[i] = false;
      noteReleased[i] = true;
      noteSustained[i] = false;
    }

    // sustain the note
    if ((irDistanceVal[i] > sustainPoint) && (noteSustained[i] == false)){
      // sustain the note

      // set flag
      noteSustained[i] = true;
    }
    
  }
}

// pitchAfterMode: IR0 controls pitch; IR1 controls velocity

void pitchAfterMode(){
  int pitchBend = (128/2) - 1;
  int afterTouch = 0;
  for( int i = 0; i < 2; i++ ) {
    irDistanceVal[i] = analogRead(irDistancePin[i]);
    //int note = irDistanceVal[0] - 90;
    //int velocity = irDistanceVal[1] - 90;
    if ((irDistanceVal[0] > actuationPoint) && (noteActuated[0] == false)) {
      // play a note
      usbMIDI.sendNoteOn(60, 127, channel); // note, vel, channel
      
      // set flags
      noteActuated[0] = true;
      noteReleased[0] = false;
    }

    // release the note
    if ((irDistanceVal[0] < releasePoint) && (noteReleased[0] == false)){
      // stop sustaining the note
      // ...
      
      // stop the note
      usbMIDI.sendNoteOff(60, 99, channel);

      // set flags
      noteActuated[i] = false;
      noteReleased[i] = true;
      noteSustained[i] = false;
    }

    // adjust the aftertouch
    afterTouch = (irDistanceVal[0] - releasePoint) / 6;
    if ((afterTouch > 0) && (afterTouch < 128)) {
      usbMIDI.sendAfterTouch(afterTouch, channel);
      Serial.println(afterTouch);
    }
    
    // bend the pitch with second IR distance sensor
    pitchBend = (irDistanceVal[1] - 450) * 18;
    if ((pitchBend > -8193) && (pitchBend < 8193) ) {
      usbMIDI.sendPitchBend(pitchBend, channel);
      delay(10);
    }
  }
}

// send CV based on IR distance
void ircvMode(){
  int dacVal[6];
  for( int i = 0; i < 6; i++ ) {
    irDistanceVal[i] = analogRead(irDistancePin[i]);
    dacVal[i] = irDistanceVal[i] * 4;
  } 
  dac1.Set(dacVal[0],dacVal[1]);
  dac2.Set(dacVal[2],dacVal[3]);
  dac3.Set(dacVal[4],dacVal[5]);
}

// send raw values through serial
void serialMode(){
  for( int i = 0; i < 6; i++ ) {
    irDistanceVal[i] = analogRead(irDistancePin[i]);
    Serial.write(i);
    Serial.write(irDistanceVal[i]);
    
  }
}

void polyPressureMode () {
  for( int i = 0; i < 6; i++ ) {
    irDistanceVal[i] = analogRead(irDistancePin[i]);

    if (irDistanceVal[i] > 200) {
      // send note on
      if (noteActuated[i] == false){
        usbMIDI.sendNoteOn(noteVal[i], 127, channel); // note, vel, channel
        noteActuated[i] = true;
        delay(delayTime);
      }
      // send polyphonic pressure based on value
      int polyPressure = (200 - irDistanceVal[i]) / 6;
      usbMIDI.sendAfterTouchPoly(noteVal[i], polyPressure, channel);
      delay(delayTime);
    }

    // release the note
    if ((irDistanceVal[i] < releasePoint) && (noteActuated[i] == true)){
      // stop the note
      usbMIDI.sendNoteOff(noteVal[i], 127, channel);
      noteActuated[i] = false;
      delay(delayTime);
    }
  }
}

// send a midi note on / off for a button press
void buttonsMidi(){
  // update the state of the buttons
  debouncer1.update();
  debouncer2.update();

  // read the buttons
  int button1 = debouncer1.read();
  int button2 = debouncer2.read();

  // send note on/off if button1 is pressed
  if ((button1 == HIGH) && (buttonNoteOn1 == false)) {
    usbMIDI.sendNoteOn(66, 127, channel);
    delay(delayTime);
    buttonNoteOn1 = true;
    Serial.println("Button 1");
    
  }
  if ((button1 == LOW) && (buttonNoteOn1 == true)) {
    usbMIDI.sendNoteOff(66, 127, channel);
    delay(delayTime);
    buttonNoteOn1 = false;
    
  }
  
  // send note on/off if button2 is pressed
  if ((button2 == HIGH) && (buttonNoteOn2 == false)) {
    usbMIDI.sendNoteOn(67, 127, channel);
    delay(delayTime);
    buttonNoteOn2 = true;
    Serial.println("Button 2");
    
  }
  if ((button2 == LOW) && (buttonNoteOn2 == true)) {
    usbMIDI.sendNoteOff(67, 127, channel);
    delay(delayTime);
    buttonNoteOn2 = false;
    
  }
}
