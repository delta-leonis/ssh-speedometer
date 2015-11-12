/**
 * object speed measuring system
 * 
 * The system consists of two sensors which can detect if a object is in front of it, or not.
 * These sensors are placed at a fixed distance, so when the time between sensing an object 
 * at one sensor and the other is determined, the speed of the object can be calculated.
 * 
 * For a more detailed documentation please refer to Snelheidsmeter_v1.0.pdf, which can be 
 * found in Robocup 2015-2016\Documenten\Elektra\Subsystems\Snelheidsmeter\
 * 
 * @auth Jeroen de Jong
 * @date 7 sept 2015
 * @version 1.0
 */
#include <LiquidCrystal.h> //Include LCDs LibraryLiquid

// pin defenitions
const int     LEFT_BUTTON                   = 9;
const int     RIGHT_BUTTON                  = 8;

const int     LEFT_SENSOR                   = A0;
const int     RIGHT_SENSOR                  = A6;

const int     DISTANCE_POTMETER             = A1;
const int     THRESHOLD_POTMETER            = A7;

// defining constants
const int     BAUDRATE                      = 115200;     
const int     REFRESH_RATE                  = 300;
const int     DEBOUNCE_TIME                 = 30;      //milliseconds
const int     DISPLAY_WAIT_TIME             = 1500;
const int     BUTTON_HOLD_TIME              = 1500;
const long    MAX_MEASURE_TIME              = 1500000;  //microseconds
const float   MIN_SPEED                     = 0;
const float   MAX_SPEED                     = 30.0;

const byte    ARROW_UP                      = 0;
const byte    ARROW_DOWN                    = 1;
const byte    ARROW_LEFT                    = 2;
const byte    ARROW_RIGHT                   = 3;

// fine-tunable with a potmeter, so check the settings for precise distance
// may differ 1 cm to either side
const int     MIN_SENSOR_DISTANCE           = 275;      //millimeters
const int     MAX_SENSOR_DISTANCE           = 325;      //milliseconds
const int     HISTORY_SIZE                  = 100;

// states
const int     STATE_DEFAULT                 = 0;
const int     STATE_HISTORY                 = 1;
const int     STATE_SETTINGS                = 2;

// settings_display
const int     SETTINGS                      = 0;
const int     SETTINGS_THRESHOLD            = 1;
const int     SETTINGS_SENSOR_VALUES        = 2;
const int     SETTINGS_DISTANCE             = 3;

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
int           state                         = STATE_DEFAULT;
int           settingsDisplay               = SETTINGS;

//initializing variables
unsigned long leftSensorTime                = 0;
unsigned long rightSensorTime               = 0;
unsigned long screenTime                    = 0;
int           leftButtonLastState           = LOW;
int           rightButtonLastState          = LOW;
int           writeIndex                    = -1;
int           readIndex                     = 0;
long          leftButtonDebounceTime        = 0;
long          rightButtonDebounceTime       = 0;
long          settingsScreenTime            = 0;
float         history[HISTORY_SIZE +1];

// functions

void setup() {
  emptyHistory();
  setupDisplay();  
  setupSerial();
}

void setupDisplay(){
    //setting up display
  lcd.begin(16, 2);
  
  // custom icons
  byte icons[8][8] = {  { 0x00, 0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x00 },     // arrow up
                        { 0x00, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00 },     // arrow down
                        { 0x00, 0x04, 0x08, 0x1F, 0x08, 0x04, 0x00, 0x00 },     // arrow left
                        { 0x00, 0x04, 0x02, 0x1F, 0x02, 0x04, 0x00, 0x00 } };   // arrow right
                        
  for( int i = 0; i < 4; i++ ){
    lcd.createChar( i, icons[i] );
  }
    
  lcd.clear();
}

void setupSerial(){
  Serial.begin( BAUDRATE );
}

void loop() {
  checkButtons();
  pollSensors();
  
  if( millis() - screenTime > REFRESH_RATE ) {
    drawScreen();
  }
}

/**
 * Check if a button has been pressed,
 * and what its action should be when activated
 */
void checkButtons() {
  // Debounce buttons
  int leftButtonRead   = digitalRead( LEFT_BUTTON  );
  int rightButtonRead  = digitalRead( RIGHT_BUTTON );
  
  if ( leftButtonRead != leftButtonLastState ) {
    leftButtonDebounceTime = millis();
  }

  if ( rightButtonRead != rightButtonLastState ) {
    rightButtonDebounceTime = millis();
  }

  if ( ( millis() - leftButtonDebounceTime ) > DEBOUNCE_TIME ) {
    leftButtonLastState = !leftButtonLastState;

    //show settings when both buttons are pressed
    if ( rightButtonRead == HIGH && leftButtonRead == HIGH ) {
      state = STATE_DEFAULT;
    } else if( leftButtonRead == HIGH ) {
      if ( state == STATE_SETTINGS ) {
         settingsDisplay < 3 ? settingsDisplay++ : 1;
      } else if (state == STATE_HISTORY) {
        readIndex > 0 ? readIndex-- : readIndex = HISTORY_SIZE;
      } else {
        state = STATE_HISTORY;
      }
    }
  }

  if (  ( millis() - rightButtonDebounceTime ) > DEBOUNCE_TIME ) {
    rightButtonLastState = !rightButtonLastState;

    //show settings when both buttons are pressed
    if ( rightButtonRead == HIGH && leftButtonRead == HIGH ) {
      state = STATE_DEFAULT;
    } else if ( rightButtonRead == HIGH ) {
      if ( state == STATE_SETTINGS ) {
        settingsDisplay > 0 ? settingsDisplay-- : 1;
      } else if ( state == STATE_HISTORY ) {
        readIndex < HISTORY_SIZE ? readIndex++ : readIndex = 0;
      } else {
        state = STATE_SETTINGS;
      }
    }
  }
}

void pollSensors() {
  float speed = measureSpeed();
  if ( speed > MIN_SPEED && speed < MAX_SPEED ) {
    writeIndex < HISTORY_SIZE ? writeIndex++ : writeIndex = 0;
    history[writeIndex] = speed;
  }
}

/**
 * determine what shoudl be drawn on screen based on the state variable
 */
void drawScreen() {
  screenTime = millis();
  lcd.clear();
  
  switch ( state ) {
    
    case STATE_SETTINGS :
      drawSettings();
      break;

    case STATE_DEFAULT :
      lcd.print( "Live measurement" );
      lcd.setCursor( 0, 1 );
      lcd.print( history[writeIndex] );
      lcd.print( "m/s" );
      break;

    case STATE_HISTORY :
      lcd.print( "no." );
      lcd.print( readIndex );
      lcd.print( ": " );
      if ( history[readIndex] > 0 ) {
        lcd.print( history[readIndex] );
        lcd.print( "m/s" );
      }
      lcd.setCursor( 0,1 );
      lcd.print( "avg: " );
      lcd.print( getAverage() );
      lcd.print( "m/s" );
      break;
  }
}

void drawSettings (){
  switch ( settingsDisplay ) {
        
    case SETTINGS :
      lcd.print( "Settings" );
      lcd.setCursor( 15, 1 );
      lcd.write(ARROW_DOWN);
          
      break;
          
    case SETTINGS_THRESHOLD :
      lcd.print( "Threshold: " );
      lcd.print( analogRead ( THRESHOLD_POTMETER ) );
      lcd.setCursor( 15, 0 );
      lcd.write(ARROW_UP);

      lcd.setCursor( 4, 1 );
      lcd.write(ARROW_DOWN);
      lcd.setCursor( 15, 1 );
      lcd.write(ARROW_DOWN);

      break;
          
    case SETTINGS_SENSOR_VALUES :
      lcd.write( ARROW_LEFT );
      lcd.print( "Sensor: " );
      lcd.print( analogRead ( LEFT_SENSOR ) );
      lcd.setCursor( 15, 0 );
      lcd.write( ARROW_UP );
          
      lcd.setCursor( 0, 1 );
      lcd.write( ARROW_RIGHT );
      lcd.print( "Sensor: " );
      lcd.print( analogRead ( RIGHT_SENSOR ) );
      lcd.setCursor( 15, 1 );
      lcd.write( ARROW_DOWN );

      break;

    case SETTINGS_DISTANCE :
      lcd.print( "Distance: " );
      lcd.print( getSensorDistance() );
      lcd.setCursor( 15, 0 );
      lcd.write( ARROW_UP );

      lcd.setCursor( 7, 1 );
      lcd.write( ARROW_DOWN );

      break;
  }
}
/**
 * Calculates average speed from history array taking all values > 0 in to account
 * @return average of history array
 */
float getAverage() {
  int   count = 0;
  float total = 0.0f;
  
  for ( int i = 0; i < HISTORY_SIZE; i++ ) {
    if ( history[i] > 0 ){
      total += history[i];
      count++;
    }
  }

  return total / count;
}

/**
 * Clears every entry of the table
 */
void emptyHistory() {
  for ( int i = 0; i < HISTORY_SIZE; i++ ){
    history[i] = 0.0f;
  }
}

/**
 * measure speed based on trigger time of sensors. Returns -1.0f if
 * the travel time between both sensors is greather than MAX_MEASURE_TIME. 
 * -1.0f is also returned when nothing is measured on the first sensor
 * 
 * @return measured speed
 */
float measureSpeed() {
  int lightThreshold  = analogRead( THRESHOLD_POTMETER );
  rightSensorTime      = 0;
  
  //start reading
  if ( analogRead( LEFT_SENSOR ) < lightThreshold ) {
    leftSensorTime = micros();

    //finish reading within MAX_MEASURE_TIME
    while ( ( micros() - leftSensorTime ) < MAX_MEASURE_TIME ) {
      if ( analogRead( RIGHT_SENSOR ) < lightThreshold ) {
        return ( getSensorDistance() * ( 1000000.0f / ( micros() - leftSensorTime ) ) ) / 1000;
      }
    }

    if ( state == STATE_DEFAULT ) {
      lcd.clear();
      lcd.print( "unclear reading." );
      delay( DISPLAY_WAIT_TIME );
    }
  }
  return -1.0f;
}

/**
 * calculate the calibrated distance between two sensors, in the dimension defined in SENSOR_DISTANCE_BASE and SENSOR_CALIBRATE_LENTH
 * @return distance between sensors
 */
int getSensorDistance() {
  return map( analogRead( DISTANCE_POTMETER ), 0, 1024, MIN_SENSOR_DISTANCE, MAX_SENSOR_DISTANCE );
}
