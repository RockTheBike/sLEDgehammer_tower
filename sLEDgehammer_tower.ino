#define BAUD_RATE 57600
char versionStr[] = "2 Bike sLEDgehammer Panels ver. 2.7 branch:dualcaps";

#define RELAYPIN 2 // relay cutoff output pin // NEVER USE 13 FOR A RELAY
#define NUM_TEAMS 2
#define NUM_LEDS 6 // Number of LED outputs.
const int ledPins[NUM_TEAMS][NUM_LEDS] = {
  { 3, 4, 5, 6, 7,  8 },
  { 9,10,11,12,A5, 13 } };
const int VOLTPIN[NUM_TEAMS] = { A0, A1 };  // Voltage Sensor Pins
// const int AMPSPIN[NUM_TEAMS] = { A3, A2 };  // Current Sensor Pins

// levels at which each LED turns on (not including special states)
const float ledLevels[NUM_LEDS+1] = {
  20, 22, 23, 24, 25, 25.4, 0 }; // last value unused in sledge

#define KNOBPIN A4
int knobAdc = 0;
void doKnob(){
  knobAdc = 1013 - analogRead(KNOBPIN); // 50K knob wired normal on 3-conductor cable (ccw=easy - cw=hard +)
  if (knobAdc < 0) knobAdc = 0; // values 0-10 count as zero
  if (knobAdc >= 0 && knobAdc < 250) {
    knobAdc = 0; }
  else if (knobAdc >= 250 && knobAdc < 500) {
    knobAdc = 250;}
  else if (knobAdc >= 500 && knobAdc < 750) {
    knobAdc = 500; }
  else if (knobAdc >= 750 && knobAdc < 1000) {
    knobAdc = 750; }
  else if (knobAdc >= 1000 && knobAdc < 1013) {
    knobAdc = 1013; }
}

#define AVG_CYCLES 50 // average measured values over this many samples
#define DISPLAY_INTERVAL 500 // when auto-display is on, display every this many milli-seconds
#define LED_UPDATE_INTERVAL 1000
#define BLINK_PERIOD 600
#define FAST_BLINK_PERIOD 150

#define STATE_OFF 0
#define STATE_BLINK 1
#define STATE_BLINKFAST 3
#define STATE_ON 2
#define STARTVOLTAGE 19
#define FAILVOLTAGE 20.5
// on/off/blink/fastblink state of each led
int ledState[NUM_TEAMS][NUM_LEDS] = {
  STATE_OFF};

#define MAX_VOLTS 28.5  // TUNE SAFETY
#define RECOVERY_VOLTS 27.0
int relayState = STATE_OFF;

#define DANGER_VOLTS 34.0
int dangerState = STATE_OFF;

int blinkState = 0;
int fastBlinkState = 0;

#define VOLTCOEFF 13.179  // larger number interprets as lower voltage

int voltsAdc[NUM_TEAMS] = { 0, 0 };
float voltsAdcAvg[NUM_TEAMS] = { 0, 0 };
float realVolts[NUM_TEAMS] = { 0, 0 };
float volts[NUM_TEAMS] = { 0, 0 };
float easyadder[NUM_TEAMS] = { 0, 0 };
float voltshelperfactor[NUM_TEAMS] = { 0, 0 };

#define IDLING 0 // haven't been pedaled yet, or after draining is over
#define CHARGING 1 // someone is pedalling, at least not letting voltage fall
#define FAILING 2 // voltage has fallen in the past 30 seconds, so we drain
#define VICTORY 3 // the winning display is activated until we're drained
#define PLAYING 4 // the winning display is activated until we're drained
#define JUSTBEGAN 5


int situation[NUM_TEAMS] = { IDLING, IDLING }; // what is the system doing?

#define WINTIME 3000 // how many milliseconds you need to be at top level before you win
#define LOSESECONDS 30 // how many seconds ago your voltage is compared to see if you gave up
#define VRSIZE 40 // must be greater than LOSESECONDS but not big enough to use up too much RAM

float voltRecord[NUM_TEAMS][VRSIZE] = { 0 }; // we store voltage here once per second
int vRIndex[NUM_TEAMS] = { 0, 0 }; // keep track of where we store voltage next
unsigned long vRTime = 0; // last time we stored a voltRecord

//Current related variables
/*int ampsAdc = 0;
float ampsAdcAvg[NUM_TEAMS] = { 0, 0 };
const float ampsBase[NUM_TEAMS] = { 508, 510 };  // measurement with zero current
const float rawAmpsReadingAt3A[NUM_TEAMS] = { 481, 483 };
const float ampsScale[NUM_TEAMS] = {
  3 / ( rawAmpsReadingAt3A[0] - ampsBase[0] ),
  3 / ( rawAmpsReadingAt3A[1] - ampsBase[1] ) };
float amps = 0; */
float volts2SecondsAgo[NUM_TEAMS] = { 0, 0 };

// float watts = 0;
// float wattHours = 0;
float voltsBefore[NUM_TEAMS] = { 0, 0 };
// timing variables for various processes: led updates, print, blink, etc
unsigned long time = 0;
unsigned long timeFastBlink = 0;
unsigned long timeBlink = 0;
unsigned long timeDisplay = 0;
unsigned long wattHourTimer = 0;
unsigned long victoryTime[NUM_TEAMS] = { 0, 0 }; // how long it's been since we declared victory
unsigned long topLevelTime[NUM_TEAMS] = { 0, 0 }; // how long we've been at top voltage level
unsigned long timefailurestarted[NUM_TEAMS] = { 0, 0 };
unsigned long timeArbduinoTurnedOn = 0;
unsigned long clearlyLosingTime = 0; // time when we last were NOT clearly losing
unsigned long serialTime = 0; // time when last serial data was seen
unsigned long drainedTime[NUM_TEAMS] = { 0, 0 }; // time when volts was last OVER 13.5v
#define EMPTYTIME 1000 // how long caps must be below 13.5v to be considered empty
#define SERIALTIMEOUT 500 // if serial data is older than this, ignore it
#define SERIALINTERVAL 300 // how much time between sending a serial packet
unsigned long serialSent = 0; // last time serial packet was sent
byte otherLevel = 0; // byte we read from the other utility box
byte presentLevel[NUM_TEAMS] = { 0, 0 };  // what "level" of transistors are we lit up to right now?

float voltishFactor = 1.0; // multiplier of voltage for competitive purposes
float voltish[NUM_TEAMS] = { 0, 0 }; // this is where we store the adjusted voltage

int timeSinceVoltageBeganFalling[NUM_TEAMS] = { 0, 0 };
int i = 0;
int team;

void setup() {
  Serial.begin(BAUD_RATE);

  Serial.println(versionStr);

  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN,LOW);

  // init LED pins
  for(team = 0; team < NUM_TEAMS; team++)
    for(i = 0; i < NUM_LEDS; i++) {
      pinMode(ledPins[team][i],OUTPUT);
      situation[team] = JUSTBEGAN;
    }
  timeDisplay = millis();
  timeArbduinoTurnedOn = timeDisplay;
  vRTime = timeDisplay; // initialize vRTime since it's a once-per-second thing
}

void loop() {
  time = millis();
  for(team = 0; team < NUM_TEAMS; team++) {
    getVolts(team);
    doSafety();
    realVolts[team] = volts[team]; // save realVolts for printDisplay function
    fakeVoltage(team); // adjust 'volts' according to knob
    if (time - vRTime > 1000) { // we do this once per second exactly
      if(situation[team] == JUSTBEGAN){
        if (time-timeArbduinoTurnedOn > 2200) situation[team] = IDLING;
      }
      if ( voltish[team] < volts2SecondsAgo[team] + 0.1) { // stuck or slow drift TUNE
        timeSinceVoltageBeganFalling[team]++;
      } else {
        timeSinceVoltageBeganFalling[team] = 0;
      }
      vRTime += 1000; // add a second to the timer index
      voltRecord[team][vRIndex[team]] = voltish[team]; // store the value. JAKE doing vRIndex++ didn't work. needed to be on two separate lines.
      vRIndex[team]++;
      if (vRIndex[team] >= VRSIZE) vRIndex[team] = 0; // wrap the counter if necessary
    }
    if (volts[team] < STARTVOLTAGE && situation[team] != PLAYING && situation[team] != JUSTBEGAN) {
      situation[team] = IDLING;
    }
    volts2SecondsAgo[team] =  voltRecord[team][(vRIndex[team] + VRSIZE - 2) % VRSIZE]; // voltage LOSESECONDS ago
    if (situation[team]==IDLING){
      if (voltish[team] - volts2SecondsAgo[team] > 0.4){ // need to get past startup sequences/ TUNE
        situation[team] = PLAYING;
        timeSinceVoltageBeganFalling[team] = 0;
        voltsBefore[team] = voltish[team];
        resetVoltRecord(team);
        Serial.println("got to PLAYING 1");// pedaling has begun in earnest
      }
    }
    if (timeSinceVoltageBeganFalling[team] > 15 && volts[team] > FAILVOLTAGE && situation[team] != FAILING){
      Serial.println("Got to Failing. Voltage has been falling for 15 seconds. ");
      situation[team]=FAILING;
    }
    if (situation[team] != VICTORY && situation[team] == PLAYING) { // if we're not in VICTORY mode...
      voltsBefore[team] =  voltRecord[team][(vRIndex[team] + VRSIZE - LOSESECONDS) % VRSIZE]; // voltage LOSESECONDS ago
      if (timeSinceVoltageBeganFalling[team] > 15) {  // Double test? See line 6 up.
        Serial.println("Got to Failing. Voltage has been falling for 15 seconds. ");
        situation[team]=FAILING;
      } else if ((voltsBefore[team] - voltish[team]) > 3) { // if voltage has fallen but they haven't given up TUNE seems harsh. 3V?
        Serial.print("voltsBefore: ");
        Serial.println(voltsBefore[team]);
	situation[team] = FAILING; // forget it, you lose
        Serial.println("got to FAILING 2");
        timefailurestarted[team] = time;
      }
    }
    if (presentLevel[team] < 5) { // voltish < ledLevels[NUM_LEDS-1]){
      topLevelTime[team] = time; // reset timer unless you're at top level
    }
    if ((situation[team] == PLAYING) && (time - topLevelTime[team] > WINTIME) && (presentLevel[team] == 5)) { // it's been WINTIME milliseconds of solid top-level action!
      if (situation[team] != VICTORY) {
        victoryTime[team] = time; // record the start time of victory
      }
    situation[team] = VICTORY;
    Serial.print("got to VICTORY 1");
    }

    doBlink();  // blink the LEDs
    doLeds(team);

    if(time - timeDisplay > DISPLAY_INTERVAL){
      printDisplay(team);
      timeDisplay = time;
    }
  }
}

/*void clearlyWinning() { // adjusts voltishFactor according to whether we're clearly losing
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
}*/

#define FAKEDIVISOR 2900 // 2026 allows doubling of voltage, 3039 allows 50% increase, etc..
float fakeVoltage(int team) {
  doKnob(); // read knob value into knobAdc

   easyadder[team] = (float) knobAdc / 185; //TUNE 1013 / 200 = 5V

   voltshelperfactor[team] = (float) ((realVolts[team] - STARTVOLTAGE) / 4);


  volts[team] = volts[team] + (voltshelperfactor[team] * easyadder[team]);
  //  float multiplier = (float)FAKEDIVISOR / (float)(FAKEDIVISOR - knobAdc);
//Serial.println(volt); // just for debugging
/*if (presentLevel == 1){ //TUNE
  volts = volts + (float)(easyadder / 4); // turning knob up returns higher voltage
}
if (presentLevel == 2){ //TUNE
  volts = volts + (float)(easyadder / 4); // turning knob up returns higher voltage
}
if (presentLevel == 3){ //TUNE
  volts = volts + (float)(easyadder / 2); // turning knob up returns higher voltage
}
if (presentLevel == 4){ //TUNE
  volts = volts + (float) (easyadder / 1.5); // turning knob up returns higher voltage
}
if (presentLevel == 5){ //TUNE
  volts = volts + easyadder; // turning knob up returns higher voltage
}*/
  // JAKE -- research how to do 'return'. It wasn't working so I changed to the volts = ... above.

} // if knob is all the way down, voltage is returned unchanged

void  resetVoltRecord(int team) {

   for(i = 0; i < VRSIZE; i++) {
    voltRecord[team][i] = volts[team];
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

void doLeds(int team){

  presentLevel[team] = 0; // we will now load presentLevel with highest level achieved
  for(i = 0; i < NUM_LEDS; i++) {

    if(voltish[team] >= ledLevels[i]){
        ledState[team][i]=STATE_ON;
       if (easyadder[team] > 4 && i == (NUM_LEDS-1)){
       ledState[team][i]=STATE_OFF;
       }
      presentLevel[team] = i; // presentLevel should equal the highest LED level

    }
    else
      ledState[team][i]=STATE_OFF;
  }

  if (situation[team] == VICTORY) presentLevel[team] = 10; // tell the other box we won!
  if (situation[team] == FAILING) presentLevel[team] = 0; // tell the other box the sad truth

  // if voltage is below the lowest level, blink the lowest level
  if (voltish[team] < ledLevels[0]){
    // ledState[0]=STATE_BLINK;
  }

  // turn off first x levels if voltage is above 3rd level
  // if(voltish > ledLevels[1]){
    // ledState[0] = STATE_OFF;
//    ledState[1] = STATE_OFF;
  //}

  if (dangerState){
    for(i = 0; i < NUM_LEDS; i++) {
      ledState[team][i] = STATE_ON; // try to keep the voltage down
    }
  }

  if (voltish[team] >= ledLevels[NUM_LEDS]) {// if at the top voltage level, blink last LEDS fast
//     ledState[NUM_LEDS-1] = STATE_BLINKFAST; // last set of LEDs
  }

  if (situation[team] == VICTORY) { // assuming victory is not over

    //  Serial.print("VICTORY, volts=");
     // Serial.println(volts);

  if (time - victoryTime[team] <= 3000){
    for (i = 0; i < NUM_LEDS - 1; i++) {
      ledState[team][i]=STATE_OFF; // turn them all off but the top one, which helps keep it from suddenly feeling easy.
    }
    ledState[team][((time - victoryTime[team]) % 1000) / 200]=STATE_ON; // turn on one at a time, bottom to top, 0.1 seconds each
    } else { // 1st victory sequence is over


    turnThemOffOneAtATime(team);
    //delay(3000);
 //  ledState[NUM_LEDS - ((time - victoryTime - 3000) % 1000) / 100] = STATE_OFF; // turn OFF one at a time, top to bottom, 0.2 seconds each



    situation[team]=FAILING;
    Serial.println("I switched to FAILING 1");
    timefailurestarted[team] = time;
}}

  //set failtime


    if (situation[team] == FAILING){

        for (i = 0; i < NUM_LEDS; i++) {  // ALL LEVELS ARE ON DURING FAILING / DRAINING
          ledState[team][i]=STATE_ON;
        }
    //      Serial.print("VICTORY OVER, FAILING, volts = ");
    //  Serial.println(volts);
      }

          if (situation[team] == IDLING){

        for (i = 0; i < NUM_LEDS; i++) {
             // WHICH LEVELS ARE ON DURING FAILING / DRAINING
            ledState[team][i]=STATE_OFF;

        }
    //      Serial.print("VICTORY OVER, FAILING, volts = ");
    //  Serial.println(volts);
      }



  for(i = 0; i < NUM_LEDS; i++) {
    if(ledState[team][i]==STATE_ON){
      digitalWrite(ledPins[team][i], HIGH);
    }
    else if (ledState[team][i]==STATE_OFF){
      digitalWrite(ledPins[team][i], LOW);
    }
    else if (ledState[team][i]==STATE_BLINK && blinkState==1){
      digitalWrite(ledPins[team][i], HIGH);
    }
    else if (ledState[team][i]==STATE_BLINK && blinkState==0){
      digitalWrite(ledPins[team][i], LOW);
    }
    else if (ledState[team][i]==STATE_BLINKFAST && fastBlinkState==1){
      digitalWrite(ledPins[team][i], HIGH);
    }
    else if (ledState[team][i]==STATE_BLINKFAST && fastBlinkState==0){
      digitalWrite(ledPins[team][i], LOW);
    }
  }

} // END doLeds()

void turnThemOffOneAtATime(int team){
        //Go into party mode
  for (i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[team][i], HIGH); // turn on all levels
  delay(500);
  for (i = NUM_LEDS - 2; i >= 0; i--) { // leave the top halogen level ON
  delay(300);
    digitalWrite(ledPins[team][i], LOW); // turn them off one at a time
    Serial.print(i);
    Serial.println(" OFF");
    delay(50);
  }
}

void doSafety() {
  if ((volts[0] > MAX_VOLTS) || (volts[1] > MAX_VOLTS)) {
    digitalWrite(RELAYPIN, HIGH);
    relayState = STATE_ON;
    Serial.println("RELAY OPEN");
  }

  if (relayState == STATE_ON && situation[0] != FAILING && situation[1] != FAILING)
    if (volts[0] < RECOVERY_VOLTS && volts[1] < RECOVERY_VOLTS ){
      digitalWrite(RELAYPIN, LOW);
      relayState = STATE_OFF;
      Serial.println("RELAY CLOSED");
    }

  if ((volts[0] > DANGER_VOLTS) || (volts[1] > DANGER_VOLTS)) {
    dangerState = STATE_ON;

  }
  else {
    dangerState = STATE_OFF;

  }

  if (((situation[0] == FAILING && (time - timefailurestarted[0]) > 10000) || 
       (situation[1] == FAILING && (time - timefailurestarted[1]) > 10000)) && relayState!=STATE_ON ) {
//       Open the Relay so volts can drop;
    digitalWrite(RELAYPIN, HIGH);
    relayState = STATE_ON;
    Serial.println("FAILING 10seconds: RELAY OPEN");
  }

  if (volts[0] > FAILVOLTAGE) { //TUNE
    drainedTime[0] = time;
  }
  if (volts[1] > FAILVOLTAGE) { //TUNE
    drainedTime[1] = time;
  }
  if ((time - drainedTime[team] > EMPTYTIME) && situation[team] == FAILING ){
    situation[team] = IDLING; //FAILING worked! we brought the voltage back to under 14.
    delay(2000);
    timeSinceVoltageBeganFalling[team] = 0;
    digitalWrite(RELAYPIN, LOW);
    relayState = STATE_OFF;
    Serial.println("EMPTYTIME, got to IDLING 1: RELAY CLOSED");
  }
}

/*void getAmps(){
  ampsAdc = analogRead(AMPSPIN);
  ampsAdcAvg = average(ampsAdc, ampsAdcAvg);
  amps = adc2amps(ampsAdcAvg);
}*/

void getVolts(int team){
  voltsAdc[team] = analogRead(VOLTPIN[team]);
  voltsAdcAvg[team] = average(voltsAdc[team], voltsAdcAvg[team]);
  volts[team] = adc2volts(voltsAdcAvg[team]);
}

float average(float val, float avg){
  if(avg == 0)
    avg = val;
  return (val + (avg * (AVG_CYCLES - 1))) / AVG_CYCLES;
}

float adc2volts(float adc){
  return adc * (1 / VOLTCOEFF);
}

/*float adc2amps(float adc){
  return (adc - 512) * 0.1220703125;
}

void calcWatts(){
  watts = volts * amps;
}

void calcWattHours(){
  wattHours += (watts * ((time - wattHourTimer) / 1000.0) / 3600.0); // measure actual watt-hours
  //wattHours +=  watts *     actual timeslice / in seconds / seconds per hour
  // In the main loop, calcWattHours is being told to run every second.
  }*/

void printDisplay(int team){
  Serial.print(realVolts[team]);
  Serial.print("v ");
  Serial.print(volts[team]);
  Serial.print("fv ");
  Serial.print(knobAdc);
  Serial.print("knobAdc ");
    Serial.print(presentLevel[team]);
  Serial.print("presentLevel ");
  Serial.print(easyadder[team]);
  Serial.print("easyadder ");
    Serial.print(voltshelperfactor[team]);
  Serial.print("voltshelperfactor ");


  if (voltishFactor > 1.0) Serial.print(voltish[team]);
  if (voltishFactor > 1.0) Serial.print("voltish ");
  // Serial.print(analogRead(VOLTPIN));
  Serial.print("   Situation: ");
  Serial.print(situation[team]);
  Serial.print("  time - topLevelTime: ");
  Serial.print(time - topLevelTime[team]);
  Serial.print("  Voltage has been flat or falling for ");
  Serial.print(timeSinceVoltageBeganFalling[team]);
  Serial.print(" S. & v2Secsago = ");
  Serial.println(volts2SecondsAgo[team]);
}
