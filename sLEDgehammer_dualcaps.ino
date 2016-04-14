#define BAUD_RATE 57600
char versionStr[] = "2 Bike sLEDgehammer Panels ver. 2.7 branch:dualcaps";

#define RELAYPIN 2 // relay cutoff output pin // NEVER USE 13 FOR A RELAY
#define VOLTPIN A0 // Voltage Sensor Pin
#define VOLTPIN2 A1 // Voltage Sensor Pin
#define AMPSPIN A3 // Current Sensor Pin
#define NUM_LEDS 6 // Number of LED outputs.

const int ledPins[NUM_LEDS] = {
  3, 4, 5, 6, 7, 8};

const int ledPins2[NUM_LEDS] = {
     9,10,11,12,A5, 13 } ;

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
#define BLINK_PERIOD 1200

#define STATE_OFF 0
#define STATE_BLINK 1
#define STATE_BLINKFAST 3
#define STATE_ON 2
#define STARTVOLTAGE 19
#define READYVOLTAGE 16.5
#define SETVOLTAGE 18
#define FAILVOLTAGE 20.5
// on/off/blink/fastblink state of each led
int ledState[NUM_LEDS] = {
  STATE_OFF};

int ledState2[NUM_LEDS] = {
  STATE_OFF};


#define MAX_VOLTS 28.5  // TUNE SAFETY
#define RECOVERY_VOLTS 27.0
int relayState = STATE_OFF;

#define DANGER_VOLTS 34.0
int dangerState = STATE_OFF;

float blinkDuty1 = 0.25; // duty cycle for blinking
float blinkDuty2 = 0.75;

#define VOLTCOEFF 13.179  // larger number interprets as lower voltage

int voltsAdc, voltsAdc2 = 0;
float voltsAdcAvg, voltsAdcAvg2 = 0;

float volts,realVolts = 0;

float volts2, realVolts2 = 0;

float easyadder = 0;
float voltshelperfactor,  voltshelperfactor2 = 0;

#define IDLING 0 // haven't been pedaled yet, or after draining is over
#define READYSET 1 // someone is pedalling, at least not letting voltage fall
#define FAILING 2 // voltage has fallen in the past 30 seconds, so we drain
#define VICTORY 3 // the winning display is activated until we're drained
#define PLAYING 4 // the winning display is activated until we're drained
#define JUSTBEGAN 5


int situation, situation2 = IDLING; // what is the system doing?

#define WINTIME 3000 // how many milliseconds you need to be at top level before you win
#define LOSESECONDS 30 // how many seconds ago your voltage is compared to see if you gave up
#define VRSIZE 40 // must be greater than LOSESECONDS but not big enough to use up too much RAM

float voltRecord[VRSIZE] = { 0 }; // we store voltage here once per second
float voltRecord2[VRSIZE] = { 0 }; // we store voltage here once per second
int vRIndex, vRIndex2 = 0; // keep track of where we store voltage next
unsigned long vRTime, vRTime2 = 0; // last time we stored a voltRecord

int voltsBuckAdc = 0; // for measuring A1 voltage
float voltsBuckAvg = 0; // for measuring A1 voltage
float voltsBuck = 0; // averaged A1 voltage

float volts2SecondsAgo = 0;
float volts2SecondsAgo2 = 0;


float voltsBefore, voltsBefore2= 0;

// timing variables for various processes: led updates, print, blink, etc
unsigned long time, lastBlinkTime = 0;
unsigned long timeFastBlink,timeFastBlink2 = 0;
unsigned long timeBlink,timeBlink2 = 0;
unsigned long timeDisplay = 0;
unsigned long wattHourTimer = 0;
unsigned long victoryTime,victoryTime2 = 0; // how long it's been since we declared victory
unsigned long topLevelTime,topLevelTime2 = 0; // how long we've been at top voltage level
unsigned long timefailurestarted, timefailurestarted2 = 0;
unsigned long timeArbduinoTurnedOn = 0;
unsigned long clearlyLosingTime, clearlyLosingTime2 = 0; // time when we last were NOT clearly losing
unsigned long serialTime = 0; // time when last serial data was seen
unsigned long drainedTime = 0; // time when volts was last OVER 13.5v
#define EMPTYTIME 1000 // how long caps must be below 13.5v to be considered empty
#define SERIALTIMEOUT 500 // if serial data is older than this, ignore it
#define SERIALINTERVAL 300 // how much time between sending a serial packet
unsigned long serialSent = 0; // last time serial packet was sent
byte otherLevel = 0; // byte we read from the other utility box
int presentLevel, presentLevel2 = 0;  // what "level" of transistors are we lit up to right now?

int timeSinceVoltageBeganFalling, timeSinceVoltageBeganFalling2 = 0;
// var for looping through arrays
int i = 0;

void setup() {
  Serial.begin(BAUD_RATE);

  Serial.println(versionStr);

  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN,LOW);

  // init LED pins
  for(i = 0; i < NUM_LEDS; i++) {
    pinMode(ledPins[i],OUTPUT);
      digitalWrite(ledPins[i],LOW);

    pinMode(ledPins2[i],OUTPUT);
      digitalWrite(ledPins2[i],LOW);

  }
  situation = JUSTBEGAN;
    situation2 = JUSTBEGAN;
  timeDisplay = millis();
  timeArbduinoTurnedOn = timeDisplay;
  vRTime = timeDisplay; // initialize vRTime since it's a once-per-second thing
}

void loop() {
  time = millis();

  getVolts();
  getVolts2();

  doSafety();

  realVolts = volts; // save realVolts for printDisplay function
  realVolts2 = volts2; // save realVolts for printDisplay function

  fakeVoltage(); // adjust 'volts' according to knob
  fakeVoltage2(); // adjust 'volts' according to knob


  if (time - vRTime > 1000) { // we do this once per second exactly
  if(situation == JUSTBEGAN){
     if (time-timeArbduinoTurnedOn > 2200) situation = IDLING;
   }

  //Player2 // condense
  if(situation2 == JUSTBEGAN){
     if (time-timeArbduinoTurnedOn > 2200) situation2 = IDLING;
   }

// Maybe this next part has to be duplicated -- has to do with historical storage of voltages.

    vRTime += 1000; // add a second to the timer index
    vRTime2 += 1000; // add a second to the timer index
    voltRecord[vRIndex] = volts;
       voltRecord2[vRIndex2] = volts2;
    // store the value. JAKE doing vRIndex++ didn't work. needed to be on two separate lines.
  /*  Serial.print("voltRecord current entry: ");
    Serial.print(voltRecord[vRIndex]);
    Serial.print(", vRIndex: ");
    Serial.println(vRIndex); */
    vRIndex++;
    vRIndex2++;
    if (vRIndex >= VRSIZE) vRIndex = 0; // wrap the counter if necessary
    if (vRIndex2 >= VRSIZE) vRIndex2 = 0; // wrap the counter if necessary

  }
// Am I idling or PLAYING?

//Player 1
if (volts < STARTVOLTAGE && situation != PLAYING && situation != JUSTBEGAN) {
  situation = IDLING;
}

//Player 2
if (volts2 < STARTVOLTAGE && situation2 != PLAYING && situation2 != JUSTBEGAN) {
  situation2 = IDLING;
}


volts2SecondsAgo =  voltRecord[(vRIndex + VRSIZE - 2) % VRSIZE]; // voltage LOSESECONDS ago

//Yes, have to duplicate voltRecord
volts2SecondsAgo2 =  voltRecord2[(vRIndex2 + VRSIZE - 2) % VRSIZE]; // voltage LOSESECONDS ago


 // Serial.print("Volts 2 seconds ago.");
//Serial.println (volts2SecondsAgo);
// Check for PLAYING . PLAYING is when you were IDLING but now you're PLAYING.


//Player 1
if (situation==IDLING){
//   Serial.print("IDLING, check for PLAYING.");
  //  Serial.println (volts - voltRecord[(vRIndex-2)]);

  if (volts - volts2SecondsAgo > 0.4 && volts >= ledLevels[0] ){ // need to get past startup sequences/ TUNE

//   Serial.println ("hey");
    situation = PLAYING;
    timeSinceVoltageBeganFalling = 0;
    voltsBefore = volts;
    resetVoltRecord();
    Serial.println("Player 1 got to PLAYING");// pedaling has begun in earnest

  }

}

//Player 2
if (situation2==IDLING){
//   Serial.print("IDLING, check for PLAYING.");
//  Serial.println (volts - voltRecord[(vRIndex-2)]);

  if (volts2 - volts2SecondsAgo2 > 0.4 && volts2 >= ledLevels[0] ){ // need to get past startup sequences/ TUNE

//   Serial.println ("hey");
    situation2 = PLAYING;

    timeSinceVoltageBeganFalling2 = 0;
    voltsBefore2 = volts2;
 resetVoltRecord2(); //duplicate?
    Serial.println("Player 2 got to PLAYING");// pedaling has begun in earnest

  }
}
//Check for VICTORY Player 1

  if (presentLevel < 5) { // voltish < ledLevels[NUM_LEDS-1])
      topLevelTime = time; // reset timer unless you're at top level
}

//Player 2

  if (presentLevel2 < 5) { // voltish < ledLevels[NUM_LEDS-1]){
      topLevelTime2 = time; // reset timer unless you're at top level
}


//Player 1
if ((situation == PLAYING) && (time - topLevelTime > WINTIME) && (presentLevel == 5)) { // it's been WINTIME milliseconds of solid top-level action!

    if (situation != VICTORY) {
      victoryTime = time; // record the start time of victory
    }
    situation = VICTORY;
   Serial.print("1 got to VICTORY 1");
 //   Serial.println(volts);

  }


 //Player 2
if ((situation2 == PLAYING) && (time - topLevelTime2 > WINTIME) && (presentLevel2 == 5)) { // it's been WINTIME milliseconds of solid top-level action!

    if (situation2 != VICTORY) {
      victoryTime2 = time; // record the start time of victory

 }
    situation2 = VICTORY;
   Serial.print("2 got to VICTORY 1");

  }


  checkLevel();
  checkLevel2();

  if ( presentLevel==0 && volts2SecondsAgo > volts  ) {
    situation=IDLING;
  }

    if ( presentLevel2==0 && volts2SecondsAgo2 > volts2  ) {
    situation2=IDLING;
  }

  doLeds();
  doLeds2();

if(time - timeDisplay > DISPLAY_INTERVAL){
    printDisplay();
    timeDisplay = time;
  }

}

//got here, to functions


#define FAKEDIVISOR 2900 // 2026 allows doubling of voltage, 3039 allows 50% increase, etc..

float fakeVoltage() {
  doKnob(); // read knob value into knobAdc

  easyadder = (float) knobAdc / 185; //TUNE 1013 / 200 = 5V

  voltshelperfactor = (float) ((realVolts - STARTVOLTAGE) / 4);

  volts = volts + (voltshelperfactor * easyadder);

} // if knob is all the way down, voltage is returned unchanged

float fakeVoltage2() {
  doKnob(); // read knob value into knobAdc

   easyadder = (float) knobAdc / 185; //TUNE 1013 / 200 = 5V

  voltshelperfactor2 = (float) ((realVolts2 - STARTVOLTAGE) / 4);

  volts2 = volts2 + (voltshelperfactor2 * easyadder);

}

//duplicate?
void  resetVoltRecord() {

   for(i = 0; i < VRSIZE; i++) {
    voltRecord[i] = volts;
    }

}

void  resetVoltRecord2() {

   for(i = 0; i < VRSIZE; i++) {

     voltRecord2[i] = volts2;
    }

}

void checkLevel(){
 for(i = 0; i < NUM_LEDS; i++) {
     if (volts>ledLevels[i]){
       presentLevel=i;
     }

  }

}

void checkLevel2(){
 for(i = 0; i < NUM_LEDS; i++) {
     if (volts2>ledLevels[i]){
       presentLevel2=i;
     }

  }

}

void doLeds(){

      for(i = 0; i < NUM_LEDS; i++) {
      ledState[i] = STATE_OFF; // Start by turning them all OFF. Later if they deserve to be ON, so be it.
      }

 // presentLevel2 = 0; // we will now load presentLevel with highest level achieved
/*  for(i = 0; i < NUM_LEDS; i++) {

    if(volts2 >= ledLevels[i]){
        ledState2[i]=STATE_ON;
       if (easyadder > 4 && i == (NUM_LEDS-1)){ // Only turn on Halogens if on an extra level. PRESERVE.
       ledState2[i]=STATE_OFF;
       }
      presentLevel2 = i; // presentLevel should equal the highest LED level

    }
    else
      ledState2[i]=STATE_OFF;
  } */

// if voltage is below the lowest level, blink the lowest level
  if (volts < ledLevels[1] && realVolts > READYVOLTAGE){ // should it be ledLevels[0]?
    ledState[0]=STATE_BLINK;
  }

  if (dangerState){
    for(i = 0; i < NUM_LEDS; i++) {
      ledState[i] = STATE_ON; // try to keep the voltage down
    }
  }
//BIG TEST
if (situation == PLAYING){

  for(i = 0; i < NUM_LEDS; i++) {
    if (i <= presentLevel){
      ledState[i]=STATE_ON;
     // Serial.println("Got here.");

     if (easyadder > 2 && i == (NUM_LEDS-1)){ // Only turn on Halogens if on an extra level. PRESERVE.
       ledState[i]=STATE_OFF;
       }
    }
  }
} else if (situation == VICTORY) { // assuming victory is not over

    //  Serial.print("VICTORY, volts=");
     // Serial.println(volts);

  if (time - victoryTime <= 3000){
    for (i = 0; i < NUM_LEDS - 1; i++) {
      ledState[i]=STATE_OFF; // turn them all off but the top one, which helps keep it from suddenly feeling easy.
    }
    ledState[((time - victoryTime) % 1000) / 200]=STATE_ON; // turn on one at a time, bottom to top, 0.1 seconds each
    } else { // 1st victory sequence is over


    turnThemOffOneAtATime();
    //delay(3000);
 //  ledState[NUM_LEDS - ((time - victoryTime - 3000) % 1000) / 100] = STATE_OFF; // turn OFF one at a time, top to bottom, 0.2 seconds each



    situation=FAILING;
    Serial.println("1 switched to FAILING after victory sequence");
    timefailurestarted = time;
    }
} else if (situation == FAILING){

        for (i = 0; i < NUM_LEDS; i++) {  // ALL LEVELS ARE ON DURING FAILING / DRAINING
          ledState[i]=STATE_ON;
        }
    //      Serial.print("VICTORY OVER, FAILING, volts = ");
    //  Serial.println(volts);
     if (realVolts < STARTVOLTAGE) situation=IDLING; // ready to play again. TUNE!
} else if (situation == IDLING){

        for (i = 0; i < NUM_LEDS; i++) {
             // WHICH LEVELS ARE ON DURING FAILING / DRAINING
            ledState[i]=STATE_OFF;

        }

        if (realVolts >= READYVOLTAGE){
          ledState[0] = STATE_BLINK;  //      Serial.print("VICTORY OVER, FAILING, volts = ");
        }

    //  Serial.println(volts);
}
  // loop through each led and turn on/off or adjust PWM

for (i=0;i<NUM_LEDS; i++){

if(ledState[i]==STATE_ON){
      digitalWrite(ledPins[i], HIGH);
      //      if (analogState[i] != brightness) analogWrite(ledPins[i], brightness); // don't analogWrite unnecessarily!
 //     analogState2[i] = brightness; //duplicate?
    }
    else if (ledState[i]==STATE_OFF){
      digitalWrite(ledPins[i], LOW);
  //    analogState2[i] = 0;
    }


//Blinking Stuff.

  else if (ledState[i]==STATE_BLINK){
      digitalWrite(ledPins[i], (time % BLINK_PERIOD) > (BLINK_PERIOD * blinkDuty1));
    }


} // END doLeds()
}

void doLeds2(){

      for(i = 0; i < NUM_LEDS; i++) {
      ledState2[i] = STATE_OFF; // Start by turning them all OFF. Later if they deserve to be ON, so be it.
      }

 // presentLevel2 = 0; // we will now load presentLevel with highest level achieved
/*  for(i = 0; i < NUM_LEDS; i++) {

    if(volts2 >= ledLevels[i]){
        ledState2[i]=STATE_ON;
       if (easyadder > 4 && i == (NUM_LEDS-1)){ // Only turn on Halogens if on an extra level. PRESERVE.
       ledState2[i]=STATE_OFF;
       }
      presentLevel2 = i; // presentLevel should equal the highest LED level

    }
    else
      ledState2[i]=STATE_OFF;
  } */

// if voltage is below the lowest level, blink the lowest level
  if (volts2 < ledLevels[1]  && realVolts2 > READYVOLTAGE){ // should it be ledLevels[0]?
    ledState2[0]=STATE_BLINK;
  }

  if (dangerState){
    for(i = 0; i < NUM_LEDS; i++) {
      ledState2[i] = STATE_ON; // try to keep the voltage down
    }
  }
//BIG TEST
if (situation2 == PLAYING){

  for(i = 0; i < NUM_LEDS; i++) {
    if (i <= presentLevel2){
      ledState2[i]=STATE_ON;

   if (easyadder > 2 && i == (NUM_LEDS-1)){ // Only turn on Halogens if on an extra level. PRESERVE.
       ledState2[i]=STATE_OFF;
       }
     // Serial.println("Got here.");
    }
  }
} else if (situation2 == VICTORY) { // assuming victory is not over

    //  Serial.print("VICTORY, volts=");
     // Serial.println(volts);

  if (time - victoryTime2 <= 3000){
    for (i = 0; i < NUM_LEDS - 1; i++) {
      ledState2[i]=STATE_OFF; // turn them all off but the top one, which helps keep it from suddenly feeling easy.
    }
    ledState2[((time - victoryTime2) % 1000) / 200]=STATE_ON; // turn on one at a time, bottom to top, 0.1 seconds each
    } else { // 1st victory sequence is over


    turnThemOffOneAtATime2();
    //delay(3000);
 //  ledState[NUM_LEDS - ((time - victoryTime - 3000) % 1000) / 100] = STATE_OFF; // turn OFF one at a time, top to bottom, 0.2 seconds each



    situation2=FAILING;
    Serial.println("2 switched to FAILING after victory sequence");
    timefailurestarted2 = time;
    }
} else if (situation2 == FAILING){

        for (i = 0; i < NUM_LEDS; i++) {  // ALL LEVELS ARE ON DURING FAILING / DRAINING
          ledState2[i]=STATE_ON;
        }
    //      Serial.print("VICTORY OVER, FAILING, volts = ");
    //  Serial.println(volts);
     if (realVolts2 < STARTVOLTAGE) situation2=IDLING; // ready to play again. TUNE!
} else if (situation2 == IDLING){

        for (i = 0; i < NUM_LEDS; i++) {
             // WHICH LEVELS ARE ON DURING FAILING / DRAINING
            ledState2[i]=STATE_OFF;

        }

        if (realVolts2>=READYVOLTAGE){
        ledState2[0] = STATE_BLINK;  //      Serial.print("VICTORY OVER, FAILING, volts = ");
        }
    //  Serial.println(volts);
}
  // loop through each led and turn on/off or adjust PWM

for (i=0;i<NUM_LEDS; i++){

if(ledState2[i]==STATE_ON){
      digitalWrite(ledPins2[i], HIGH);
      //      if (analogState[i] != brightness) analogWrite(ledPins[i], brightness); // don't analogWrite unnecessarily!
 //     analogState2[i] = brightness; //duplicate?
    }
    else if (ledState2[i]==STATE_OFF){
      digitalWrite(ledPins2[i], LOW);
  //    analogState2[i] = 0;
    }


//Blinking Stuff.

  else if (ledState2[i]==STATE_BLINK){
      digitalWrite(ledPins2[i], (time % BLINK_PERIOD) > (BLINK_PERIOD * blinkDuty2));
    }

} // END doLeds()
}


void turnThemOffOneAtATime(){
        //Go into party mode
  for (i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], HIGH); // turn on all levels
  delay(500);
  for (i = NUM_LEDS - 2; i >= 0; i--) { // leave the top halogen level ON
  delay(300);
    digitalWrite(ledPins[i], LOW); // turn them off one at a time
    Serial.print(i);
    Serial.println(" OFF");
    delay(50);
  }
}

void turnThemOffOneAtATime2(){
        //Go into party mode
  for (i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins2[i], HIGH); // turn on all levels
  delay(500);
  for (i = NUM_LEDS - 2; i >= 0; i--) { // leave the top halogen level ON
  delay(300);
    digitalWrite(ledPins2[i], LOW); // turn them off one at a time
    Serial.print(i);
    Serial.println(" OFF");
    delay(50);
  }
}

void doSafety() {
  if (volts > MAX_VOLTS || volts2 > MAX_VOLTS ){
    digitalWrite(RELAYPIN, HIGH);
    relayState = STATE_ON;
    Serial.println("RELAY OPEN");
  }

  if (relayState == STATE_ON && situation != FAILING && volts < RECOVERY_VOLTS && volts2 < RECOVERY_VOLTS ){
    digitalWrite(RELAYPIN, LOW);
    relayState = STATE_OFF;
    Serial.println("RELAY CLOSED"); // Tower 1 seems to have better hysteresis than tower 2.
  }

  if (volts > DANGER_VOLTS || volts2 > DANGER_VOLTS ){
    dangerState = STATE_ON;

  }
  else {
    dangerState = STATE_OFF;

  }

  if (situation == FAILING && relayState!=STATE_ON && (time - timefailurestarted) > 10000 ) {
//       Open the Relay so volts can drop;
    digitalWrite(RELAYPIN, HIGH);
    relayState = STATE_ON;
    Serial.println("FAILING 10seconds: RELAY OPEN");
  }


  if (volts > FAILVOLTAGE) { //TUNE
    drainedTime = time;
  } else {
   //Serial.println("volts is less than failvoltage");
  }
  if ((time - drainedTime > EMPTYTIME) && situation == FAILING ){
    situation = IDLING; //FAILING worked! we brought the voltage back to under 14.
    delay(2000);
    timeSinceVoltageBeganFalling = 0;
    digitalWrite(RELAYPIN, LOW);
    relayState = STATE_OFF;
    Serial.println("EMPTYTIME, got to IDLING 1: RELAY CLOSED");
  }
}




void getVolts(){
  voltsAdc = analogRead(VOLTPIN);
  voltsAdcAvg = average(voltsAdc, voltsAdcAvg);
  volts = adc2volts(voltsAdcAvg);
}

void getVolts2(){
  voltsAdc2 = analogRead(VOLTPIN2);
  voltsAdcAvg2 = average(voltsAdc2, voltsAdcAvg2);
  volts2 = adc2volts(voltsAdcAvg2);
}

float average(float val, float avg){
  if(avg == 0)
    avg = val;
  return (val + (avg * (AVG_CYCLES - 1))) / AVG_CYCLES;
}

float adc2volts(float adc){
  return adc * (1 / VOLTCOEFF);
}


void printDisplay(){

  Serial.print(realVolts);
  Serial.print("v ");
  Serial.print(volts);
  Serial.print("fv ");
  Serial.print(presentLevel);
  Serial.print("pl ");
  Serial.print(situation);
  Serial.print("situ ");
  for (i = 0; i < NUM_LEDS; i++) Serial.print(ledState[i]);
  Serial.print("   ");

  Serial.print(realVolts2);
  Serial.print("v2 ");
  Serial.print(volts2);
  Serial.print("fv2 ");
  Serial.print(presentLevel2);
  Serial.print("pl2 ");
  Serial.print(situation2);
  Serial.print("situ2 ");
  for (i = 0; i < NUM_LEDS; i++) Serial.print(ledState2[i]);
  Serial.print("   ");

  Serial.print(easyadder);
  Serial.print("easyadder  ");
  Serial.print(voltshelperfactor);
  Serial.print("voltshelperfactor ");
  Serial.print(knobAdc);
  Serial.print("knobAdc ");

  /* Serial.print("  time - topLevelTime: ");
  Serial.print(time - topLevelTime2);
  Serial.print("  Voltage has been flat or falling for ");
  Serial.print(timeSinceVoltageBeganFalling2);
  Serial.print(" S. & v2Secsago = ");
  Serial.print(volts2SecondsAgo2); */
  Serial.println();
}
