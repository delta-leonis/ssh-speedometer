
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
#include <LinkedList.h>

// pin defenitions
const int             LEFT_BUTTON                   = 9;
const int             RIGHT_BUTTON                  = 8;

const int             LEFT_SENSOR                   = A0;
const int             RIGHT_SENSOR                  = A6;

const int             DISTANCE_POTMETER             = A1;
const int             THRESHOLD_POTMETER            = A7;

// defining constants
const int             BAUDRATE                      = 115200;     
const int             REFRESH_TIME                  = 300;
const int             DEBOUNCE_TIME                 = 30;       // milliseconds
const int             DISPLAY_WAIT_TIME             = 1500;     // milliseconds
const int             BUTTON_HOLD_TIME              = 500;      // milliseconds
const int             SENSOR_ERROR_TIME             = 1500;     // milliseconds
const long            MAX_MEASURE_TIME              = 1500000;  // microseconds
const float           MIN_SPEED                     = 0;
const float           MAX_SPEED                     = 30.0;
  
const byte            ARROW_UP                      = 0;
const byte            ARROW_DOWN                    = 1;
const byte            ARROW_LEFT                    = 2;
const byte            ARROW_RIGHT                   = 3;

const byte            LEFT                          = 0;
const byte            RIGHT                         = 1;

const byte            NO_BUTTON_PRESSED             = 0;
const byte            LEFT_BUTTON_PRESSED           = 1;
const byte            RIGHT_BUTTON_PRESSED          = 2;
const byte            BOTH_BUTTONS_PRESSED          = 3;

// fine-tunable with a potmeter, so check the settings for precise distance
// may differ 1 cm to either side
const int             MIN_SENSOR_DISTANCE           = 275;      // millimeters
const int             MAX_SENSOR_DISTANCE           = 325;      // millimeters
const int             HISTORY_SIZE                  = 100;      // 

// states
const int             STATE_DEFAULT                 = 0;
const int             STATE_HISTORY                 = 1;
const int             STATE_SETTINGS                = 2;
const int             STATE_DELETE                  = 3;
const int             STATE_LEFT_SENSOR_ERROR       = 4;
const int             STATE_RIGHT_SENSOR_ERROR      = 5;
const int             STATE_BOTH_SENSORS_ERROR      = 6;

// settings_display
const int             SETTINGS                      = 0;
const int             SETTINGS_THRESHOLD            = 1;
const int             SETTINGS_SENSOR_VALUES        = 2;
const int             SETTINGS_DISTANCE             = 3;

//initializing variables
int                   state                         = STATE_DEFAULT;
int                   settingsDisplay               = SETTINGS;
int                   writeIndex                    = -1;
int                   readIndex                     = -1;
byte                  buttonState                   = NO_BUTTON_PRESSED;
int                   buttonLastState[2]            = { LOW, LOW };
long                  buttonDebounceTime[2]         = { 0, 0 };
long                  buttonHoldTime                = 0;
unsigned long         screenTime                    = 0;

LinkedList < float >  history                       = LinkedList < float > ( );

LiquidCrystal         lcd ( 12, 11, 5, 4, 3, 2 );

// functions

void setup ( ) {
  emptyHistory ( );
  setupDisplay ( );  
  setupSerial  ( );
}

void setupDisplay ( ) {
    //setting up display
  lcd.begin ( 16, 2 );
  
  // custom icons
  byte icons[8][8] = {  { 0x00, 0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x00 },     // arrow up
                        { 0x00, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00 },     // arrow down
                        { 0x00, 0x04, 0x08, 0x1F, 0x08, 0x04, 0x00, 0x00 },     // arrow left
                        { 0x00, 0x04, 0x02, 0x1F, 0x02, 0x04, 0x00, 0x00 } };   // arrow right
                        
  for ( int i = 0; i < 4; i++ ) {
    lcd.createChar ( i, icons[i] );
  }
    
  lcd.clear ( );
}

void setupSerial ( ) {
  Serial.begin ( BAUDRATE );
}

void loop ( ) {
  checkButtons();
  pollSensors();
  
  if ( millis ( ) - screenTime > REFRESH_TIME ) {
    drawScreen ( );
  }
}

/**
 * Check if a button has been pressed,
 * and what its action should be when activated
 */
void checkButtons ( ) {
  // Debounce buttons
  
  int buttonRead[2] = { digitalRead ( LEFT_BUTTON ), digitalRead ( RIGHT_BUTTON ) };
  
  for ( int index = 0; index < 2; index++ ) {
    buttonRead[index] != buttonLastState[index] ? buttonDebounceTime[index] = millis ( ) : 1;
    if ( millis ( ) - buttonHoldTime > BUTTON_HOLD_TIME ) {
      if ( buttonState == BOTH_BUTTONS_PRESSED && state == STATE_HISTORY ) {
        state       = STATE_DELETE;
        buttonState = NO_BUTTON_PRESSED;
      }
    } 
    if ( millis ( ) - buttonDebounceTime[index] > DEBOUNCE_TIME ) {
      buttonLastState[index] = !buttonLastState[index];

      //show settings when both buttons are pressed
      if ( buttonRead[RIGHT] == HIGH && buttonRead[LEFT] == HIGH ) {
        buttonHoldTime  = millis ( );
        buttonState     = BOTH_BUTTONS_PRESSED;
      } else if ( buttonState == BOTH_BUTTONS_PRESSED ){
        state       = STATE_DEFAULT;
        buttonState = NO_BUTTON_PRESSED;
      } else if( index == LEFT && buttonRead[index] == HIGH ) {
        buttonState     = LEFT_BUTTON_PRESSED;
      } else if ( index == LEFT && buttonRead[LEFT] == LOW && buttonState == LEFT_BUTTON_PRESSED ) {
        buttonState = NO_BUTTON_PRESSED;
        if ( state == STATE_SETTINGS ) {
          settingsDisplay > 0 ? settingsDisplay-- : 1;
        } else if ( state == STATE_HISTORY ) {
          readIndex > -1 ? readIndex-- : readIndex = history.size() - 1;
        } else if ( state == STATE_DELETE ) {
          if ( readIndex >= 0 ) {
            history.remove ( readIndex );
            if ( readIndex == history.size ( ) ) {
              readIndex--;
            }
          } else {
            emptyHistory();
          }
          state = STATE_HISTORY;
        } else {
          state = STATE_HISTORY;
        }
      } else if ( index == RIGHT && buttonRead[index] == HIGH ) {
        buttonState = RIGHT_BUTTON_PRESSED;        
      } else if ( index == RIGHT && buttonRead[RIGHT] == LOW && buttonState == RIGHT_BUTTON_PRESSED ) {
        buttonState = NO_BUTTON_PRESSED;
        if ( state == STATE_SETTINGS ) {
          settingsDisplay < 3 ? settingsDisplay++ : 1;
        } else if ( state == STATE_HISTORY ) {
          readIndex < history.size ( ) - 1 ? readIndex++ : readIndex = -1;
        } else if ( state == STATE_DELETE ){
          state = STATE_HISTORY;
        } else {
          state = STATE_SETTINGS;
        }
      }
    }
  }
}

void pollSensors ( ) {
  int threshold = analogRead( THRESHOLD_POTMETER );

  if ( state == STATE_DEFAULT || state == STATE_LEFT_SENSOR_ERROR || state == STATE_RIGHT_SENSOR_ERROR || state == STATE_BOTH_SENSORS_ERROR ) {
    float speed = measureSpeed();
    if ( speed > MIN_SPEED && speed < MAX_SPEED ) {
      if ( history.get ( writeIndex ) ) {
        history.set ( writeIndex, speed );
      } else {
        history.add ( speed );
      }
      writeIndex < HISTORY_SIZE - 1 ? writeIndex++ : writeIndex = 0;
    }
  }
}

/**
 * determine what shoudl be drawn on screen based on the state variable
 */
void drawScreen ( ) {
  screenTime = millis ( );
  lcd.clear ( );
  
  switch ( state ) {
    
    case STATE_SETTINGS :
      drawSettings ( );
      
      break;

    case STATE_DEFAULT :
      lcd.print ( "Live measurement" );
      lcd.setCursor ( 0, 1 );
      lcd.print ( history.get( writeIndex == 0 ? history.size ( ) - 1 : writeIndex - 1 ) );
      lcd.print ( "m/s" );
      
      break;

    case STATE_HISTORY :
      if ( readIndex > -1 ) {
        lcd.print ( "no." );
        lcd.print ( readIndex + 1);
        lcd.print ( ": " );
        if ( history.get ( readIndex ) > 0 ) {
          lcd.print ( history.get ( readIndex ) );
          lcd.print ( "m/s" );
        }
      } else {
        lcd.print ( "All measurements" );
      }
      lcd.setCursor ( 0, 1 );
      lcd.print ( "avg: " );
      lcd.print ( getAverage ( ) );
      lcd.print ( "m/s" );
      
      break;

    case STATE_DELETE :
      lcd.print ( "    Delete?     " );
      lcd.setCursor ( 0, 1 );
      lcd.write ( ARROW_LEFT );
      lcd.print ( " Yes   |   No " );
      lcd.write ( ARROW_RIGHT );
      
      break;

    case STATE_LEFT_SENSOR_ERROR :
      lcd.print ( "  Error! Left   " );
      lcd.setCursor ( 0, 1 );
      lcd.print ( "sensor blocked! " );
      
      break;
      
    case STATE_RIGHT_SENSOR_ERROR :
      lcd.print ( "  Error! Right  " );
      lcd.setCursor ( 0, 1 );
      lcd.print ( "sensor blocked! " );
      
      break; 
         
    case STATE_BOTH_SENSORS_ERROR :
      lcd.print ( "   Error! Both  " );
      lcd.setCursor ( 0, 1 );
      lcd.print ( "sensors blocked!" );
      
      break;    
      
  }
}

void drawSettings (){
  switch ( settingsDisplay ) {
        
    case SETTINGS :
      lcd.print ( "    Settings    " );
      lcd.setCursor ( 11, 1 );
      lcd.write ( ARROW_UP );
      lcd.setCursor ( 15, 1 );
      lcd.write ( ARROW_DOWN );
          
      break;
          
    case SETTINGS_THRESHOLD :
      lcd.print ( "Threshold: " );
      lcd.print ( analogRead ( THRESHOLD_POTMETER ) );
      lcd.setCursor ( 15, 0 );
      lcd.write ( ARROW_UP );

      lcd.setCursor ( 4, 1 );
      lcd.write ( ARROW_DOWN );
      lcd.setCursor ( 15, 1 );
      lcd.write ( ARROW_DOWN );

      break;
          
    case SETTINGS_SENSOR_VALUES :
      lcd.write ( ARROW_LEFT );
      lcd.print ( "Sensor: " );
      lcd.print ( analogRead ( LEFT_SENSOR ) );
      lcd.setCursor ( 15, 0 );
      lcd.write ( ARROW_UP );
          
      lcd.setCursor ( 0, 1 );
      lcd.write ( ARROW_RIGHT );
      lcd.print ( "Sensor: " );
      lcd.print ( analogRead ( RIGHT_SENSOR ) );
      lcd.setCursor ( 15, 1 );
      lcd.write ( ARROW_DOWN );

      break;

    case SETTINGS_DISTANCE :
      lcd.print ( "Distance: " );
      lcd.print ( getSensorDistance() );
      lcd.setCursor ( 15, 0 );
      lcd.write ( ARROW_UP );

      lcd.setCursor ( 7, 1 );
      lcd.write ( ARROW_DOWN );

      break;
      
  }
}
/**
 * Calculates average speed from history array taking all values > 0 in to account
 * @return average of history array
 */
float getAverage() {
  int   historySize = history.size();
  int   count       = 0;
  float total       = 0.0f;
  
  for ( int index = 0; index < historySize; index++ ) {
    total += history.get(index);
    count++;
  }

  return total / count;
}

/**
 * Clears every entry of the table
 */
void emptyHistory() {
  history.clear();
  writeIndex = 0;
}

/**
 * measure speed based on trigger time of sensors. Returns -1.0f if
 * the travel time between both sensors is greather than MAX_MEASURE_TIME. 
 * -1.0f is also returned when nothing is measured on the first sensor
 * 
 * @return measured speed
 */
float measureSpeed() {
  int   threshold       = analogRead( THRESHOLD_POTMETER );
  int   sensorDistance  = getSensorDistance();
  long  sensorTime      = -1;
  
  //start reading
  for ( int index = 0; index < 2; index++ ) {
    if ( analogRead( (int[2]){LEFT_SENSOR, RIGHT_SENSOR}[index] ) < threshold ) {
      sensorTime = micros();
    
      //finish reading within MAX_MEASURE_TIME
      while ( micros() - sensorTime < MAX_MEASURE_TIME ) {
        if ( analogRead( (int[2]){RIGHT_SENSOR, LEFT_SENSOR}[index] ) < threshold ) {
          return ( sensorDistance * ( 1000000.0f / ( micros ( ) - sensorTime ) ) ) / 1000.0f;
        }
      }

      if ( analogRead( (int[2]) {LEFT_SENSOR, RIGHT_SENSOR}[index] ) < threshold ) {
        if ( state == (int[2]) {RIGHT_SENSOR, LEFT_SENSOR}[index] ) {
          state = STATE_BOTH_SENSORS_ERROR;
        } else {
          state = (int[2]){STATE_LEFT_SENSOR_ERROR, STATE_RIGHT_SENSOR_ERROR}[index];
        }
        sensorTime  = -1;
      }
    } else if ( state == (int[2]){STATE_LEFT_SENSOR_ERROR, STATE_RIGHT_SENSOR_ERROR}[index] ) {
      state = STATE_DEFAULT;
    }
  }

  if ( sensorTime > 0 ) {
    lcd.clear ( );
    lcd.print ( "unclear reading." );
    delay ( DISPLAY_WAIT_TIME );
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
