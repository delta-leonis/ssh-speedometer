/**
 * Snelheidsmeter
 * Een lichtsluis voor het meten van de snelheid van de bal.
 * Voor documentatie zie Snelheidsmeter_v1.0.pdf van de electro documentatie
 * 
 * @auth Jeroen de Jong
 * @date 7 sept 2015
 * @version 1.0
 */
#include <LiquidCrystal.h> //Include LCDs LibraryLiquid

//defining constants
const int REFRESH_RATE = 300;
const int DEBOUNCE_TIME = 50;             //millis
const int DISPLAY_WAIT_TIME = 1500;       //millis
const long MAX_MEASURE_TIME = 1500000; //micros
//fine-tunable with a potmeter, so check the settings for precise distance
//may differ 1 cm to either side
const int SENSOR_DISTANCE_BASE = 350;     //millis
const int SENSOR_CALIBRATE_LENGTH = 10;   //millis
const int MAX_HISTORY_INDEX = 9;

//defining pinout
const int recordButtonPin = 9;
const int functionButtonPin = 8;
const int ldrFrontPin = A0;
const int ldrBackPin = A1;
const int potmeterDistancePin = A4;
const int potmeterThresholdPin = A5;

//states
const int STATE_DEFAULT = 0x00;
const int STATE_RECORDING = 0x01;
const int STATE_STOPPED_RECORDING = 0x02;
const int STATE_SHOW_SETTINGS = 0x03;

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
int state = STATE_DEFAULT;

//initializing variables
unsigned long ldrFrontTime = 0;
unsigned long ldrBackTime = 0;
unsigned long measurekTime = 0;
unsigned int ldrValue = 0;
unsigned long screenTime = 0;
long recordButtonDebounceTime = 0;
long functionButtonDebounceTime = 0;
long settingsScreenTime = 0;
int recordButtonLastState = LOW;
int functionButtonLastState = LOW;
float history[MAX_HISTORY_INDEX +1];
int index = 0;

void setup() {
  //setting up display
  lcd.begin(16, 2);
  
  //FOR THE CUSTOM ICONS
  byte icons[4][8] = {{B0,B1110,B10101,B10111,B10001,B1110,B0,B0}, {B0,B1000,B1100,B1110,B1100,B1000,B0,B0}, {B0,B11111,B11111,B11111,B11111,B11111,B0,B0}, {B0,B1110,B11111,B11011,B11111,B1110,B0,B0}};
  for(int i=0; i < 4; i++)
    lcd.createChar(i, icons[i]);
    
  emptyHistory();
  lcd.clear();
  Serial.begin(9600);
}

void loop() {
  checkButtons();

  if(state != STATE_STOPPED_RECORDING){
    float speed = measureSpeed();
    if (speed > 0.0f && speed < 30.0f){
        history[index] = speed;
      if(state == STATE_RECORDING){
        index++;
        //exit recording when history is full
        if(index > MAX_HISTORY_INDEX){
          setNextState();
          index = 0;
        }
      }
    }
  }

  if(millis() - screenTime > REFRESH_RATE)
    drawScreen();
}

/**
 * Check if a button has been pressed,
 * and what its action should be when activated
 */
void checkButtons() {
  // Debounce buttons
  int recordButtonRead = digitalRead(recordButtonPin);
  if (recordButtonRead != recordButtonLastState)
    recordButtonDebounceTime = millis();

  int functionButtonRead = digitalRead(functionButtonPin);
  if (functionButtonRead != functionButtonLastState)
    functionButtonDebounceTime = millis();

  if ((millis() - recordButtonDebounceTime) > DEBOUNCE_TIME) {
    recordButtonLastState = !recordButtonLastState;

    
    //show settings when both buttons are pressed
    if (functionButtonRead == HIGH && recordButtonRead == HIGH) {
      state = STATE_SHOW_SETTINGS;
      settingsScreenTime = millis();
    }else if( recordButtonRead == HIGH)
      setNextState();
  }

  if ((millis() - functionButtonDebounceTime) > DEBOUNCE_TIME) {
    functionButtonLastState = !functionButtonLastState;

    //show settings when both buttons are pressed
    if (functionButtonRead == HIGH && recordButtonRead == HIGH) {
      state = STATE_SHOW_SETTINGS;
      settingsScreenTime = millis();
    }
    else if(functionButtonRead == HIGH){
      if(state == STATE_RECORDING || state == STATE_STOPPED_RECORDING)
        (index > 0) ? index-- : index=MAX_HISTORY_INDEX;
    }
  }

  //reset state to default when settings are no longer tweaked
  if(state == STATE_SHOW_SETTINGS)
    if (functionButtonRead == LOW || recordButtonRead == LOW)
      if (settingsScreenTime + DISPLAY_WAIT_TIME < millis())
        state = STATE_DEFAULT;
}

/**
 * Cycle through to the next stage
 */
int setNextState() {
  state++;
  //skip settings screen
  if (state > STATE_STOPPED_RECORDING){
    state = STATE_DEFAULT;
    emptyHistory();
  }
}

/**
 * determine what shoudl be drawn on screen based on the state variable
 */
void drawScreen() {
  screenTime = millis();
  lcd.clear();
  switch (state) {
    case STATE_SHOW_SETTINGS:
      lcd.print("Threshold: ");
      lcd.print(analogRead(potmeterThresholdPin));
      lcd.setCursor(0,1);

      lcd.print("distance: ");
      lcd.print(getSensorDistance());
      break;

    case STATE_DEFAULT:
      lcd.print("1 measurement");
      lcd.setCursor(0,1);
      lcd.print(history[0]);
      lcd.print("m/s");
      break;

    case STATE_STOPPED_RECORDING:
    case STATE_RECORDING:
      lcd.print("no.");
      lcd.print(index);
      lcd.print(" ");
      lcd.print(history[index]);
      lcd.print("m/s");
      lcd.setCursor(0,1);
      lcd.print("avg: ");
      lcd.print(getAverage());
      lcd.print("m/s" );
      break;
  }

  lcd.setCursor(15,0);
  lcd.write(byte(state));
}

/**
 * Calculates average speed from history array taking all values > 0 in to account
 * @return average of history array
 */
float getAverage(){
  int count = 0;
  float total = 0.0f;
  for(int i = 0; i < MAX_HISTORY_INDEX; i++){
    total += history[i];
    if(history[i] > 0) count++;
  }

  return total/count;
}

/**
 * Clears every entry of the table
 */
void emptyHistory(){
  for(int i = 0; i < MAX_HISTORY_INDEX; i++)
    history[i] = 0.0f;
}

/**
 * measure speed based on trigger time of sensors. Returns -1.0f if
 * the travel time between both sensors is greather than MAX_MEASURE_TIME. 
 * -1.0f is also returned when nothing is measured on the first sensor
 * 
 * @return measured speed
 */
float measureSpeed() {
  int lightThreshold = analogRead(potmeterThresholdPin);
  ldrBackTime = 0;
  //start reading
  if (analogRead(ldrFrontPin) < lightThreshold) {
    ldrFrontTime = micros();

    //finish reading within MAX_MEASURE_TIME
    while ((micros() - ldrFrontTime) < MAX_MEASURE_TIME) {
      if (analogRead(ldrBackPin) < lightThreshold) {
        float tmp = 0.0f;
        tmp = 1000000.0f / (micros() - ldrFrontTime); 
        tmp = (getSensorDistance() * tmp) /1000;
        return tmp;
      }
    }

    lcd.clear();
    lcd.print("unclear reading.");
    delay(DISPLAY_WAIT_TIME);
    return -1.0f;
  }
  return -1.0f;
}

/**
 * calculate the calibrated distance between two sensors, in the dimension defined in SENSOR_DISTANCE_BASE and SENSOR_CALIBRATE_LENTH
 * @return distance between sensors
 */
int getSensorDistance() {
  int scale = analogRead(potmeterDistancePin);
  scale = ((scale > SENSOR_DISTANCE_BASE/2) ? scale - SENSOR_DISTANCE_BASE/2 : scale) / (SENSOR_DISTANCE_BASE/2/SENSOR_CALIBRATE_LENGTH);
  return SENSOR_DISTANCE_BASE - scale;
}
