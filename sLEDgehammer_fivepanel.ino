#define BAUD_RATE 57600
#define DEBUG 1 // set to 1 to enable serial information printing
char versionStr[] = "2 Bike sLEDgehammer Panels ver. 2.7 branch:fivepanel";

#define RELAYPIN 2 // relay cutoff output pin // NEVER USE 13 FOR A RELAY
#define VOLTPIN A0 // Voltage Sensor Pin
#define AMPSPIN A3 // Current Sensor Pin
#define NUM_LEDS 6 // Number of LED outputs.
const int ledPins[NUM_LEDS] = { 3, 4, 5, 6, 7, 8};

// levels at which each LED turns on (not including special states)
const float ledLevels[NUM_LEDS+1] = { 20, 22, 23, 24, 25, 25.4, 0 }; // last value unused in sledge

#define KNOBPIN A4
int knobAdc = 0;
void doKnob(){
  knobAdc = 1013 - analogRead(KNOBPIN); // 50K knob wired normal on 3-conductor cable (ccw=easy - cw=hard +)
  if (knobAdc < 0) knobAdc = 0; // values 0-10 count as zero
  if (knobAdc >= 0 && knobAdc < 101) {
    knobAdc = 0; }
  else if (knobAdc >= 101 && knobAdc < 398) {
    knobAdc = 250;}
  else if (knobAdc >= 398 && knobAdc < 710) {
    knobAdc = 500; }
  else if (knobAdc >= 710 && knobAdc < 952) {
    knobAdc = 750; }
  else if (knobAdc >= 952 && knobAdc < 1013) {
    knobAdc = 1013; }
}

#define AVG_CYCLES 50 // average measured values over this many samples
#define DISPLAY_INTERVAL 500 // when auto-display is on, display every this many milli-seconds
#define BLINK_PERIOD 600
#define FAST_BLINK_PERIOD 150

#define STATE_OFF 0
#define STATE_BLINK 1
#define STATE_BLINKFAST 3
#define STATE_ON 2
#define STARTVOLTAGE 19
#define FAILVOLTAGE 20.5
int ledState[NUM_LEDS] = { STATE_OFF}; // on/off/blink/fastblink state of each led

#define MAX_VOLTS 28.5  // TUNE SAFETY
#define RECOVERY_VOLTS 27.0
int relayState = STATE_OFF;

#define DANGER_VOLTS 34.0
int dangerState = STATE_OFF;

int blinkState = 0;
int fastBlinkState = 0;

#define VOLTCOEFF 13.179  // larger number interprets as lower voltage

int voltsAdc = 0;
float voltsAdcAvg = 0;
float volts,realVolts = 0;
float easyadder = 0;
float voltshelperfactor = 0;

#define IDLING 0 // haven't been pedaled yet, or after draining is over
#define FAILING 2 // voltage has fallen in the past 30 seconds, so we drain
#define VICTORY 3 // the winning display is activated until we're drained
#define PLAYING 4 // the winning display is activated until we're drained
#define JUSTBEGAN 5

int situation = IDLING; // what is the system doing?

#define WINTIME 3000 // how many milliseconds you need to be at top level before you win
#define LOSESECONDS 30 // how many seconds ago your voltage is compared to see if you gave up
#define VRSIZE 40 // must be greater than LOSESECONDS but not big enough to use up too much RAM

float voltRecord[VRSIZE] = { 0 }; // we store voltage here once per second
int vRIndex = 0; // keep track of where we store voltage next
unsigned long vRTime = 0; // last time we stored a voltRecord

int ampsAdc = 0;
float ampsAdcAvg = 0;
float amps = 0;
float volts2SecondsAgo = 0;

float watts = 0;
float wattHours = 0;
float voltsBefore = 0;
unsigned long time = 0;
unsigned long timeFastBlink = 0;
unsigned long timeBlink = 0;
unsigned long timeDisplay = 0;
unsigned long wattHourTimer = 0;
unsigned long victoryTime = 0; // how long it's been since we declared victory
unsigned long topLevelTime = 0; // how long we've been at top voltage level
unsigned long timefailurestarted = 0;
unsigned long timeArbduinoTurnedOn = 0;
unsigned long clearlyLosingTime = 0; // time when we last were NOT clearly losing
unsigned long serialTime = 0; // time when last serial data was seen
unsigned long drainedTime = 0; // time when volts was last OVER 13.5v
#define EMPTYTIME 1000 // how long caps must be below 13.5v to be considered empty
#define SERIALTIMEOUT 500 // if serial data is older than this, ignore it
#define SERIALINTERVAL 300 // how much time between sending a serial packet
unsigned long serialSent = 0; // last time serial packet was sent
byte otherLevel = 0; // byte we read from the other utility box
byte presentLevel = 0;  // what "level" of transistors are we lit up to right now?

float voltishFactor = 1.0; // multiplier of voltage for competitive purposes
float voltish = 0; // this is where we store the adjusted voltage

int timeSinceVoltageBeganFalling = 0;
int i = 0;

void setup() {
  Serial.begin(BAUD_RATE);

  if (DEBUG) Serial.println(versionStr);

  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN,LOW);

  for(i = 0; i < NUM_LEDS; i++) {
    pinMode(ledPins[i],OUTPUT);
    digitalWrite(ledPins[i],LOW);
  }
  situation = JUSTBEGAN;
  timeDisplay = millis();
  timeArbduinoTurnedOn = timeDisplay;
  vRTime = timeDisplay; // initialize vRTime since it's a once-per-second thing
}

void loop() {
  time = millis();
  getVolts();
  doSafety();
  realVolts = volts; // save realVolts for printDisplay function
  fakeVoltage(); // adjust 'volts' according to knob
  clearlyWinning(); // check to see if we're clearly losing and update 'voltish'
  if (time - serialSent > SERIALINTERVAL) {
    sendSerial();  // tell other box our presentLevel
    serialSent = time; // reset the timer
  }
  readSerial();  // see if there's a byte waiting on the serial port from other sledgehammer

  if (otherLevel == 10) { // other box has won!  we lose.
    if (situation != FAILING) turnThemOffOneAtATime();
    situation = FAILING;
  }

  if (time - vRTime > 1000) { // we do this once per second exactly
    if(situation == JUSTBEGAN) {
      if (time-timeArbduinoTurnedOn > 2200) situation = IDLING;
    }
    if ( voltish < volts2SecondsAgo + 0.1) { // stuck or slow drift TUNE
      timeSinceVoltageBeganFalling++;
    } else {
      timeSinceVoltageBeganFalling = 0;
    }

    vRTime += 1000; // add a second to the timer index
    voltRecord[vRIndex] = voltish; // store the value. JAKE doing vRIndex++ didn't work. needed to be on two separate lines.
    vRIndex++;
    if (vRIndex >= VRSIZE) vRIndex = 0; // wrap the counter if necessary
  }

  if (volts < STARTVOLTAGE && situation != PLAYING && situation != JUSTBEGAN) {
    situation = IDLING;
  }

  volts2SecondsAgo =  voltRecord[(vRIndex + VRSIZE - 2) % VRSIZE]; // voltage LOSESECONDS ago

  if (situation==IDLING){
    if (voltish - volts2SecondsAgo > 0.4) { // need to get past startup sequences/ TUNE
      situation = PLAYING;
      timeSinceVoltageBeganFalling = 0;
      voltsBefore = voltish;
      resetVoltRecord();
      if (DEBUG) Serial.println("got to PLAYING 1");// pedaling has begun in earnest
    }
  }

   if (timeSinceVoltageBeganFalling > 15 && volts > FAILVOLTAGE && situation != FAILING) {
     Serial.println("Got to Failing. Voltage has been falling for 15 seconds. ");
     situation=FAILING;
   }

  if (situation != VICTORY && situation == PLAYING) { // if we're not in VICTORY mode...
    voltsBefore =  voltRecord[(vRIndex + VRSIZE - LOSESECONDS) % VRSIZE]; // voltage LOSESECONDS ago
    if (timeSinceVoltageBeganFalling > 15) {  // Double test? See line 6 up.
      if (DEBUG) Serial.println("Got to Failing. Voltage has been falling for 15 seconds. ");
      situation=FAILING;
    } else if ((voltsBefore - voltish) > 3) { // if voltage has fallen but they haven't given up TUNE seems harsh. 3V?
      if (DEBUG) Serial.print("voltsBefore: ");
      if (DEBUG) Serial.println(voltsBefore);
      situation = FAILING; // forget it, you lose
      if (DEBUG) Serial.println("got to FAILING 2");
      timefailurestarted = time;
    }
  }

  if (presentLevel < 5) { // voltish < ledLevels[NUM_LEDS-1]){
    topLevelTime = time; // reset timer unless you're at top level
  }

  if ((situation == PLAYING) && (time - topLevelTime > WINTIME) && (presentLevel == 5)) { // it's been WINTIME milliseconds of solid top-level action!
    if (situation != VICTORY) {
      victoryTime = time; // record the start time of victory
      Serial.print("s:s:s:s:s:s:s:s:s:s:s:s:s:s:s:s:s:s:s:s:"); // tell the other box we won!
    }
    situation = VICTORY;
    if (DEBUG) Serial.print("got to VICTORY 1");
  }

  doBlink();  // blink the LEDs
  doLeds();

  if(time - timeDisplay > DISPLAY_INTERVAL){
    printDisplay();
    timeDisplay = time;
  }
}

void clearlyWinning() { // adjusts voltishFactor according to whether we're clearly losing
  if ((otherLevel != 's') && (otherLevel < (presentLevel + 2))) clearlyLosingTime = time; // reset the timer if we're not losing
  if (time - serialTime > SERIALTIMEOUT) clearlyLosingTime = time; // reset the timer if no recent serial data
  if (time - clearlyLosingTime > 2000) { // we ARE clearly losing, so let's adjust voltishFactor
    if (voltishFactor  < 1.5) voltishFactor  += 0.1; // increase our fakery
    clearlyLosingTime = time; // reset the timer since we made the adjustment
  }
  if (situation == FAILING) voltishFactor  = 1.0; // reset voltishFactor  since we've failed
  voltish = (volts * voltishFactor); // calculate the adjusted voltage
}

void sendSerial() {
  if (DEBUG == 0) {
    Serial.print("s"); // send an "s" to say we're a sledge here!
    if (presentLevel >= 0 && presentLevel <= 10) Serial.print(char(presentLevel+48)); // send a : if presentLevel is 10(victory)
    // DON'T DO A PRINTLN BECAUSE THE NEWLINE IS AN ASCII 10 AND WILL BE DETECTED AS VICTORY GODDAMMIT
    delay(50); // let's not crash the computer with too much serial data
  }
}

void readSerial() {
  if (Serial.available()) {
    byte previousByte = otherLevel; // should be an 's' if this is a data
    otherLevel = Serial.read();
    if ((otherLevel >= 48) && (otherLevel <= 58) && (previousByte == 's')) {
      serialTime = time; // if we got here, it must be another sLEDgehammer
      otherLevel -= 48; // make it an actual number like 'presentLevel'
    }
  }
  if ((time - serialTime > SERIALTIMEOUT) && (otherLevel != 's')) otherLevel = 0; // if the data is expired, assume zero
}

#define FAKEDIVISOR 2900 // 2026 allows doubling of voltage, 3039 allows 50% increase, etc..
float fakeVoltage() {
  doKnob(); // read knob value into knobAdc
  easyadder = (float) knobAdc / 185; //TUNE 1013 / 200 = 5V
  voltshelperfactor = (float) ((realVolts - STARTVOLTAGE) / 4);
  volts = volts + (voltshelperfactor * easyadder);
} // if knob is all the way down, voltage is returned unchanged

void  resetVoltRecord() {

   for(i = 0; i < VRSIZE; i++) {
    voltRecord[i] = volts;
    }

}

void doBlink(){

  if (((time - timeBlink) > BLINK_PERIOD) && blinkState == 1){
    blinkState = 0;
    timeBlink = time;
  } 
  else if (((time - timeBlink) > BLINK_PERIOD) && blinkState == 0){
    blinkState = 1;
    timeBlink = time;
  }


  if (((time - timeFastBlink) > FAST_BLINK_PERIOD) && fastBlinkState == 1){
    fastBlinkState = 0;
    timeFastBlink = time;
  } 
  else if (((time - timeFastBlink) > FAST_BLINK_PERIOD) && fastBlinkState == 0){
    fastBlinkState = 1;
    timeFastBlink = time;
  }

}

void doLeds() {
  presentLevel = 0; // we will now load presentLevel with highest level achieved
  for(i = 0; i < NUM_LEDS; i++) {
    if(voltish >= ledLevels[i]) {
      ledState[i]=STATE_ON;
      if (easyadder > 4 && i == (NUM_LEDS-1)) {
        ledState[i]=STATE_OFF;
      }
      presentLevel = i; // presentLevel should equal the highest LED level
    } else ledState[i]=STATE_OFF;
  }

  if (situation == VICTORY) presentLevel = 10; // tell the other box we won!
  if (situation == FAILING) presentLevel = 0; // tell the other box the sad truth

  if (dangerState) {
    for(i = 0; i < NUM_LEDS; i++) {
      ledState[i] = STATE_ON; // try to keep the voltage down
    }
  }

  if (situation == VICTORY) { // assuming victory is not over
    if (time - victoryTime <= 3000) {
      for (i = 0; i < NUM_LEDS - 1; i++) {
        ledState[i]=STATE_OFF; // turn them all off but the top one, which helps keep it from suddenly feeling easy.
      }
      ledState[((time - victoryTime) % 1000) / 200]=STATE_ON; // turn on one at a time, bottom to top, 0.1 seconds each
    } else { // 1st victory sequence is over
      turnThemOffOneAtATime();
      situation=FAILING;
      if (DEBUG) Serial.println("I switched to FAILING 1");
      timefailurestarted = time;
    }
  }

  if (situation == FAILING) {
    for (i = 0; i < NUM_LEDS; i++) {  // ALL LEVELS ARE ON DURING FAILING / DRAINING
      ledState[i]=STATE_ON;
    }
  }

  if (situation == IDLING) {
    for (i = 0; i < NUM_LEDS; i++) { // WHICH LEVELS ARE ON DURING FAILING / DRAINING
      ledState[i]=STATE_OFF;
    }
  }

  for(i = 0; i < NUM_LEDS; i++) {
    if(ledState[i]==STATE_ON){
      digitalWrite(ledPins[i], HIGH);
    }
    else if (ledState[i]==STATE_OFF){
      digitalWrite(ledPins[i], LOW);
    }
    else if (ledState[i]==STATE_BLINK && blinkState==1){
      digitalWrite(ledPins[i], HIGH);
    }
    else if (ledState[i]==STATE_BLINK && blinkState==0){
      digitalWrite(ledPins[i], LOW);
    }
    else if (ledState[i]==STATE_BLINKFAST && fastBlinkState==1){
      digitalWrite(ledPins[i], HIGH);
    }
    else if (ledState[i]==STATE_BLINKFAST && fastBlinkState==0){
      digitalWrite(ledPins[i], LOW);
    }
  }
} // END doLeds()

void turnThemOffOneAtATime() { //Go into party mode
  for (i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], HIGH); // turn on all levels
  delay(500);
  for (i = NUM_LEDS - 2; i >= 0; i--) { // leave the top halogen level ON
    delay(300);
    digitalWrite(ledPins[i], LOW); // turn them off one at a time
    if (DEBUG) Serial.print(i);
    if (DEBUG) Serial.println(" OFF");
    delay(50);
  }
}

void doSafety() {
  if (volts > MAX_VOLTS){
    digitalWrite(RELAYPIN, HIGH);
    relayState = STATE_ON;
    if (DEBUG) Serial.println("RELAY OPEN");
  }

  if (relayState == STATE_ON && situation != FAILING && volts < RECOVERY_VOLTS){
    digitalWrite(RELAYPIN, LOW);
    relayState = STATE_OFF;
    if (DEBUG) Serial.println("RELAY CLOSED");
  }

  if (volts > DANGER_VOLTS){
    dangerState = STATE_ON;
  } else {
    dangerState = STATE_OFF;
  }

  if (situation == FAILING && relayState!=STATE_ON && (time - timefailurestarted) > 10000 ) { //       Open the Relay so volts can drop;
    digitalWrite(RELAYPIN, HIGH);
    relayState = STATE_ON;
    if (DEBUG) Serial.println("FAILING 10seconds: RELAY OPEN");
  }

  if (volts > FAILVOLTAGE) { //TUNE
    drainedTime = time;
  }
  if ((time - drainedTime > EMPTYTIME) && situation == FAILING ){
    situation = IDLING; //FAILING worked! we brought the voltage back to under 14.
    delay(2000);
    timeSinceVoltageBeganFalling = 0;
    digitalWrite(RELAYPIN, LOW);
    relayState = STATE_OFF;
    if (DEBUG) Serial.println("EMPTYTIME, got to IDLING 1: RELAY CLOSED");
  }
}

void getAmps(){
  ampsAdc = analogRead(AMPSPIN);
  ampsAdcAvg = average(ampsAdc, ampsAdcAvg);
  amps = adc2amps(ampsAdcAvg);
}

void getVolts(){
  voltsAdc = analogRead(VOLTPIN);
  voltsAdcAvg = average(voltsAdc, voltsAdcAvg);
  volts = adc2volts(voltsAdcAvg);
}

float average(float val, float avg){
  if(avg == 0) avg = val;
  return (val + (avg * (AVG_CYCLES - 1))) / AVG_CYCLES;
}

float adc2volts(float adc){
  return adc * (1 / VOLTCOEFF);
}

float adc2amps(float adc){
  return (adc - 512) * 0.1220703125;
}

void calcWatts(){
  watts = volts * amps;
}

void calcWattHours(){
  wattHours += (watts * ((time - wattHourTimer) / 1000.0) / 3600.0); // measure actual watt-hours
  //wattHours +=  watts *     actual timeslice / in seconds / seconds per hour
  // In the main loop, calcWattHours is being told to run every second.
}

void printDisplay(){
  if (DEBUG) Serial.print(realVolts);
  if (DEBUG) Serial.print("v ");
  if (DEBUG) Serial.print(volts);
  if (DEBUG) Serial.print("fv ");
  if (DEBUG) Serial.print(knobAdc);
  if (DEBUG) Serial.print("knobAdc (");
  if (DEBUG) Serial.print(1013 - analogRead(KNOBPIN));
  if (DEBUG) Serial.print(") ");
  if (DEBUG) Serial.print(presentLevel);
  if (DEBUG) Serial.print("presentLevel ");
  if (DEBUG) Serial.print(easyadder);
  if (DEBUG) Serial.print("easyadder ");
  if (DEBUG) Serial.print(voltshelperfactor);
  if (DEBUG) Serial.print("voltshelperfactor ");


  if (DEBUG && voltishFactor > 1.0) Serial.print(voltish);
  if (DEBUG && voltishFactor > 1.0) Serial.print("voltish ");
  // if (DEBUG) Serial.print(analogRead(VOLTPIN));
  if (DEBUG) Serial.print("   Situation: ");
  if (DEBUG) Serial.print(situation);
  if (DEBUG) Serial.print("  time - topLevelTime: ");
  if (DEBUG) Serial.print(time - topLevelTime);
  if (DEBUG) Serial.print("  Voltage has been flat or falling for ");
  if (DEBUG) Serial.print(timeSinceVoltageBeganFalling);
  if (DEBUG) Serial.print(" S. & v2Secsago = ");
  if (DEBUG) Serial.println(volts2SecondsAgo);
}
