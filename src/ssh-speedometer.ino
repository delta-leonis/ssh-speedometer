
/**
 * object speed measuring system
 *
 * The system consists of two sensors which can detect if a object is in front
 * of it, or not. These sensors are placed at a fixed distance, so when the
 * time between sensing an object at one sensor and the other is determined,
 * the speed of the object can be calculated.
 *
 * For a more detailed documentation please refer to Snelheidsmeter_v1.0.pdf,
 * which can be found in:
 * "Robocup 2015-2016\Documenten\Elektra\Subsystems\Snelheidsmeter\"
 *
 * @auth Jeroen de Jong, Ryan Meulenkamp
 * @date 16 - 11 - 2015
 * @version 2.0
 */
#include <LiquidCrystal.h> //Include LCDs LibraryLiquid
#include <LinkedList.h>

//    pin defenitions

// Button pin numbers:
const int             LEFT_BUTTON                   = 9;
const int             RIGHT_BUTTON                  = 8;

// Sensor pin numbers
const int             LEFT_SENSOR                   = A0;
const int             RIGHT_SENSOR                  = A6;

//potentiometer pin numbers
const int             DISTANCE_POTMETER             = A1;
const int             THRESHOLD_POTMETER            = A7;

//    defining constants:

// The time after which the display text is refreshed
const int             REFRESH_TIME                  = 300;      // milliseconds

// The time the program waits for a button state to become stable
const int             DEBOUNCE_TIME                 = 30;       // milliseconds

// The time the display shows certain messages like "unclear reading"
const int             DISPLAY_WAIT_TIME             = 1500;     // milliseconds

// The time a button has to be hold in order to trigger it's secondary function
const int             BUTTON_HOLD_TIME              = 500;      // milliseconds

// The maximum amount of time a measurement can take
const long            MAX_MEASURE_TIME              = 1500000;  // microseconds

// The minimum speed. Under this value, the speed is not saved
const float           MIN_SPEED                     = 0;        // m/s

// The maximum speed. above this value, the speed is not saved
const float           MAX_SPEED                     = 50.0;     // m/s

// These are used to identify custom icons for the lcd
const unsigned char   ARROW_UP                      = 0;
const unsigned char   ARROW_DOWN                    = 1;
const unsigned char   ARROW_LEFT                    = 2;
const unsigned char   ARROW_RIGHT                   = 3;

// Identifier used for buttons and sensors
const unsigned char   LEFT                          = 0;
const unsigned char   RIGHT                         = 1;

// Possible button states.
const unsigned char   NO_BUTTON_PRESSED             = 0;
const unsigned char   LEFT_BUTTON_PRESSED           = 1;
const unsigned char   RIGHT_BUTTON_PRESSED          = 2;
const unsigned char   BOTH_BUTTONS_PRESSED          = 3;

// The distance between the sensors is adjustable between these two values
const short           MIN_SENSOR_DISTANCE           = 275;      // millimeters
const short           MAX_SENSOR_DISTANCE           = 325;      // millimeters

// The maximum amount of speed values saved to memory
const unsigned char   HISTORY_SIZE                  = 100;

//    System states

// The default state, in this state, measurements will be done constantly
const unsigned char   STATE_DEFAULT                 = 0;

// In this state, the user can scroll through previous measurements
const unsigned char   STATE_HISTORY                 = 1;

const unsigned char   STATE_HISTORY_MENU            = 2;

// In this state, the user can scroll through settings.
const unsigned char   STATE_SETTINGS                = 3;

// In this state, the element at readIndex will be replaced by
const unsigned char   STATE_REPLACE                 = 4;

// When a measurement is done, a question is raised if you want to replace the
// old value with the new one.
const unsigned char   STATE_REPLACE_QUESTION        = 5;

// In this state, the user is asked if he is sure he wants to delete a
// measurement
const unsigned char   STATE_REMOVE                  = 6;

// If a sensor is blocked, the system goes to an error state
const unsigned char   STATE_LEFT_SENSOR_ERROR       = 7;
const unsigned char   STATE_RIGHT_SENSOR_ERROR      = 8;
const unsigned char   STATE_BOTH_SENSORS_ERROR      = 9;

//    settings_display
//    used to determine which setting should be displayd.

// No particular setting is shown here, but only the text "Settings"
const unsigned char   SETTINGS                      = 0;

// Here the threshold is shown.
const unsigned char   SETTINGS_THRESHOLD            = 1;

// Here the sensor readings are shown
const unsigned char   SETTINGS_SENSOR_VALUES        = 2;

// Here the distance between sensors is shown.
const unsigned char   SETTINGS_DISTANCE             = 3;

//    history menu display
//    used to determine which setting should be displayed.

// No particular posibility is shown here but the text "History".
const unsigned char   HISTORY_BACK                  = 0;

// This gives the posibility to replace the current element.
const unsigned char   HISTORY_REPLACE               = 1;

// This gives the posibility to remove the current element.
const unsigned char   HISTORY_REMOVE                = 2;

// This gives the posibility to remove all elements.
const unsigned char   HISTORY_REMOVE_ALL            = 3;

// this gives the posibility to send the current element to another device
// over USB
const unsigned char   HISTORY_USB_SEND              = 4;

// this gives the posibility to send all elements to another device over USB
const unsigned char   HISTORY_USB_SEND_ALL          = 5;

//    Initializing variables

// The current state of the system
unsigned char         state                         = STATE_DEFAULT;

// The current setting on display
 unsigned char        subState                      = SETTINGS;

// The index of the element in the list which should be written when a
// measurement is done. It is not always the last element because this gives
// the possibility to overwrite elements, and start at the beginning without
// clearing the entire list. Starts at -1 because the index is incremented
// before writing the element. This is done because it minimalizes code at
// other places
short                 writeIndex                    = -1;

// The index of the element in the list that is being read. Used in the history
// state. Initialized at -1 because this resembles "All readings" in history
// menu.
short                 readIndex                     = -1;

// The previous state of each button (so the state while debouncing)
unsigned char         buttonLastState[2]            = { LOW, LOW };

// The button's state after debouncing
unsigned char         buttonState                   = NO_BUTTON_PRESSED;

// The time used to debounce
long                  buttonDebounceTime[2]         = { 0, 0 };

// The time a buttonstate has been active
long                  buttonHoldTime                = 0;

// Used to remember how long it has been since the lcd was refreshed
unsigned long         screenTime                    = 0;

// The list of previously measured values.
LinkedList < float >  history                       = LinkedList < float > ( );

// Defining the lcd display
LiquidCrystal         lcd ( 12, 11, 5, 4, 3, 2 );

// functions

/**
 * Setup function as needed by the arduino ide
 */
void setup ( ) {
  // The display is set up
  setupDisplay ( );
}

/**
 * The display is set up here. The size is given and 4 custom icons are created
 */
void setupDisplay ( ) {
  // Starting communication with the display of 16 x 2 characters
  lcd.begin ( 16, 2 );

  // custom icons
  byte icons[4][8] = {
    { 0x00, 0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x00 },     // arrow up
    { 0x00, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00 },     // arrow down
    { 0x00, 0x04, 0x08, 0x1F, 0x08, 0x04, 0x00, 0x00 },     // arrow left
    { 0x00, 0x04, 0x02, 0x1F, 0x02, 0x04, 0x00, 0x00 } };   // arrow right

  // creating the custom characters
  for ( unsigned char index = 0; index < 4; index++ ) {
    lcd.createChar ( index, icons[index] );
  }

  // clear the display
  lcd.clear ( );
}

/**
 * The loop function as needed by the arduino IDE
 */
void loop ( ) {
  // The buttons are polled. This means the software checks if the buttons have
  // changed state
  pollButtons();

  // Next the sensors are polled.
  pollSensors();

  // Then the screen is redrawn after a certain amount of time.
  if ( millis ( ) - screenTime > REFRESH_TIME ) {
    screenTime = millis ( );
    refreshDisplay ( );
  }
}

/**
 * Check if a button has been pressed, and what its action should be when
 * activated. Debouncing is also handled here.
 */
void pollButtons ( ) {
  // Read current state of both buttons.
  int buttonRead[2] = { digitalRead ( LEFT_BUTTON  ),
                        digitalRead ( RIGHT_BUTTON ) };

  // check both buttons
  for ( int index = 0; index < 2; index++ ) {
    // If the state changed, save the current time
    if ( buttonRead[index] != buttonLastState[index] ) {
      buttonDebounceTime[index] = millis ( );
    }

    // When a button is hold for more than a certain amount of time
    if ( millis ( ) - buttonHoldTime > BUTTON_HOLD_TIME ) {
      // When both buttons are hold in the history state, an pop up menu is
      // shown which gives you the choice wat to do with the current element.
      if (  buttonState == BOTH_BUTTONS_PRESSED &&
            state       == STATE_HISTORY        &&
            readIndex   >= 0                        ) {
        // when both buttons are hold and the history state is active, a option
        // menu will be opened.
        state = STATE_HISTORY_MENU;
      } else if ( buttonState == RIGHT_BUTTON_PRESSED &&
                  state       == STATE_HISTORY_MENU ) {
        // when a button is hold for a certain amount of time, this function
        // is called
        rightButtonHold();
      }
      buttonState = NO_BUTTON_PRESSED;
    }

    // When a button is pressed and released within a certain amount of time
    if ( millis ( ) - buttonDebounceTime[index] > DEBOUNCE_TIME ) {
      buttonLastState[index] = !buttonLastState[index];

      //    State logic
      if ( buttonRead[RIGHT] == HIGH && buttonRead[LEFT] == HIGH ) {

        // current system time is set. This is needed to check how long a button
        // was hold.
        buttonHoldTime  = millis ( );
        buttonState     = BOTH_BUTTONS_PRESSED;
      } else if ( buttonState       == BOTH_BUTTONS_PRESSED ){

        // run function on both button release
        bothButtonsPressed ( );
      } else if ( index             == LEFT   &&
                  buttonRead[index] == HIGH ) {

        // current system time is set. This is needed to check how long a button
        // was hold.
        buttonHoldTime  = millis ( );
        buttonState     = LEFT_BUTTON_PRESSED;
      } else if ( index             == LEFT   &&
                  buttonRead[LEFT]  == LOW    &&
                  buttonState       == LEFT_BUTTON_PRESSED ) {

        // run function on button release
        leftButtonPressed ( );
      } else if ( index             == RIGHT  &&
                  buttonRead[index] == HIGH      ) {

        buttonHoldTime  = millis ( );
        buttonState     = RIGHT_BUTTON_PRESSED;
      } else if ( index             == RIGHT  &&
                  buttonRead[RIGHT] == LOW    &&
                  buttonState       == RIGHT_BUTTON_PRESSED ) {

        // run function on button release
        rightButtonPressed ( );
      }
    }
  }
}

/**
 * When the right button is pressed and released within a certain amount of time
 */
void rightButtonPressed ( ) {
  // When the right button is released
  buttonState     = NO_BUTTON_PRESSED;
  if ( state == STATE_SETTINGS ) {
    // The subState will scroll down if it is not already at the
    // bottom
    subState < 3 ? subState++ : 1;
  } else if ( state == STATE_HISTORY ) {
    // The history display will scroll down if it is not already at the
    // bottom. Else it will become -1 (All measurements)
    if ( history.size ( ) > 0 ) {
      readIndex < history.size ( ) - 1 ? readIndex++ : readIndex = 0;
    }

  } else if ( state == STATE_HISTORY_MENU ) {
    // scroll through the state history menu
    subState < 5 ? subState++ : 1;
  } else if ( state == STATE_REPLACE ) {
  } else if ( state == STATE_REPLACE_QUESTION ) {
    history.remove ( readIndex );
    state = STATE_HISTORY;
  } else if ( state == STATE_REMOVE ) {
    state = STATE_HISTORY;
  } else {
    state = STATE_SETTINGS;
  }
}

/**
 * When the left button is pressed and released within a certain amount of time
 */
void leftButtonPressed ( ) {
  // When the left button is released
  buttonState     = NO_BUTTON_PRESSED;
  if ( state == STATE_SETTINGS || state == STATE_HISTORY_MENU ) {
    // when the left button is pressed in the settings state, the settings
    // on display will scroll up, unless it is already at the top.
    subState > 0 ? subState-- : 1;
  } else if ( state == STATE_HISTORY ) {
    // when the left butten is pressed in the history state, the display
    // will scroll up, unless it is at the top. Then it will start at -1.
    // -1 because this is seen as the index for "All measurements".
    readIndex > 0 ? readIndex-- : readIndex = history.size() - 1;
  } else if ( state == STATE_REPLACE ) {
  } else if ( state == STATE_REPLACE_QUESTION ) {
    history.remove ( readIndex + 1 );
    state = STATE_HISTORY;
  } else if ( state == STATE_REMOVE ) {
    // if the index == -1 then all the history will be cleared. else the
    // element at readIndex will be removed.
    if ( readIndex >= 0 ) {
      history.remove ( readIndex );
      readIndex == history.size ( ) ? readIndex-- : 1;
    } else {
      clearHistory();
    }
    state = STATE_HISTORY;
  } else {
    state = STATE_HISTORY;
  }
}

/**
 * when both buttons are pressed at the same time and released within a certain
 * amount of time, this function is called.
 */
void bothButtonsPressed ( ) {
  switch ( state ) {
    case STATE_HISTORY_MENU :
    case STATE_REPLACE      :
      // when the state is replace or history menu, than it should return to
      // history
      state = STATE_HISTORY;
      break;

    case default :
      // in other cases it should return to live measurement
      state = STATE_DEFAULT;
  }
  state       = ( state == STATE_HISTORY_MENU ) ? STATE_HISTORY : STATE_DEFAULT;
  subState    = 0;
  buttonState = NO_BUTTON_PRESSED;
}

/**
 * this function will be called when the right button is hold for a certain
 * amount of time.
 */
void rightButtonHold ( ) {
  switch ( subState ) {
    case HISTORY_REPLACE :
      state = STATE_REPLACE;

      break;
    case HISTORY_REMOVE :
      state = STATE_REMOVE;

      break;
    case HISTORY_REMOVE_ALL :
      readIndex = -1;
      state     = STATE_REMOVE;

      break;
    default :
      state     = STATE_HISTORY;
  }
}

/**
 * Function which does some checking before measuring the speed using
 * the sensors
 */
void pollSensors ( ) {
  // In the settings, and history display, no measurements will be done.
  if (  state == STATE_DEFAULT            ||
        state == STATE_LEFT_SENSOR_ERROR  ||
        state == STATE_RIGHT_SENSOR_ERROR ||
        state == STATE_BOTH_SENSORS_ERROR ||
        state == STATE_REPLACE               ) {

    // A speed measurement is now tried
    float speed = measureSpeed();

    // The speed should be between certain values
    if ( speed > MIN_SPEED && speed < MAX_SPEED ) {
      if ( state == STATE_REPLACE ) {
        // When replacing a value, it is temporarily inserted instead of added
        // or directly replacing the element at the writeIndex. Later the choice
        // is given to delete the inserted value or the element originally on
        // that index
        history.add( readIndex, speed );

        state = STATE_REPLACE_QUESTION;
      } else {
        // if a measurement was done, the write index is incremented
        writeIndex < HISTORY_SIZE - 1 ? writeIndex++ : writeIndex = 0;

        // if there where no measurements already, the readIndex should be set.
        // readIndex = -1 means "history empty"
        readIndex < 0 ? readIndex = 0 : 1;

        // the system checks if the list element already exists
        if ( history.get ( writeIndex ) ) {
          history.set ( writeIndex, speed );
        } else {
          history.add ( speed );
        }
      }
    }
  }
}

/**
 * determine what should be drawn on screen based on the state variable
 */
void refreshDisplay ( ) {
  // first the display is cleared of previous graphics
  lcd.clear ( );

  // then it is redrawn depending on the current system state
  switch ( state ) {

    // the settings state is a lot more extensive in display than the other
    // states, so it has its own function.
    case STATE_SETTINGS :
      drawSettings ( );

      break;

    // In the default state, live measurements are shown
    case STATE_DEFAULT :
      lcd.print ( "Live measurement" );
      lcd.setCursor ( 0, 1 );
      lcd.print ( history.get( writeIndex ) );
      lcd.print ( "m/s" );

      break;

    // In the history state, all previous measurements are shown, as well as
    // posibility to delete one or all of them.
    case STATE_HISTORY :
      if ( readIndex > -1 ) {
        lcd.print ( "No." );
        lcd.print ( readIndex + 1);
        lcd.print ( ": " );
        lcd.print ( history.get ( readIndex ) );
        lcd.print ( "m/s" );

        lcd.setCursor ( 0, 1 );
        lcd.print ( "Avg: " );
        lcd.print ( getAverage ( ) );
        lcd.print ( "m/s" );
      } else {
        lcd.print ( "No measurements" );
      }

      break;

    // In this state, the history pop-up menu is shown.
    case STATE_HISTORY_MENU :
      drawHistoryMenu();

      break;

      // In this state, the choice is given if you want to overwrite the
      // selected element
    case STATE_REPLACE :

      lcd.print ( "  Measurement   " );
      lcd.setCursor ( 0, 1 );
      lcd.print ( "   pending..    " );

      break;

    case STATE_REPLACE_QUESTION :

      lcd.print ( "Replace by " );
      lcd.print ( history.get ( readIndex ) );
      lcd.setCursor ( 0, 1 );
      lcd.write ( ARROW_LEFT );
      lcd.print ( " Yes   |   No " );
      lcd.write ( ARROW_RIGHT );

      break;

    // In this state, the choice is given if you want to delete the selected
    // element(s)
    case STATE_REMOVE :
      lcd.print ( "    Delete?     " );
      lcd.setCursor ( 0, 1 );
      lcd.write ( ARROW_LEFT );
      lcd.print ( " Yes   |   No " );
      lcd.write ( ARROW_RIGHT );

      break;

    // the following three states are triggered when sensors are blocked for
    // more than a certain amount of time.
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

/*
 * The settings state is larger than other states, so to keep things clear, it
 * has it's own function.
 */
void drawSettings (){
  switch ( subState ) {

    // when the settings menu is opened it starts of with showing it actually
    // is in the settings menu.
    case SETTINGS :
      lcd.print ( "    Settings    " );
      lcd.setCursor ( 11, 1 );
      lcd.write ( ARROW_UP );
      lcd.setCursor ( 15, 1 );
      lcd.write ( ARROW_DOWN );

      break;

    // Here the current threshold is shown. The theshold can be modified by
    // turning the middle potentiometer.
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

    // here the current readings of both sensors is displayed.
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

    // here the distance between sensors is shown. this value can also be
    // modified by turning a potentiometer
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

void drawHistoryMenu ( ) {
  switch ( subState ) {
    // when the settings menu is opened it starts of with showing it actually
    // is in the settings menu.
    case HISTORY_BACK :

      lcd.write ( ARROW_RIGHT );
      lcd.print ( "   ..History   " );

      lcd.setCursor ( 0, 1 );
      lcd.print ( " Replace cur   " );
      lcd.write ( ARROW_DOWN );

      break;

    case HISTORY_REPLACE :

      lcd.print ( "    ..History  " );
      lcd.setCursor ( 15, 0 );
      lcd.write ( ARROW_UP );

      lcd.setCursor ( 0, 1 );
      lcd.write ( ARROW_RIGHT );
      lcd.print ( "Replace cur   " );

      lcd.setCursor ( 15, 1 );
      lcd.write ( ARROW_DOWN );

      break;

    // Here the current threshold is shown. The theshold can be modified by
    // turning the middle potentiometer.
    case HISTORY_REMOVE :

      lcd.write ( ARROW_RIGHT );
      lcd.print ( "Remove current " );
      lcd.setCursor ( 15, 0 );
      lcd.write ( ARROW_UP );

      lcd.setCursor ( 0, 1 );
      lcd.print ( " Remove all    ");
      lcd.setCursor ( 15, 1 );
      lcd.write ( ARROW_DOWN );

      break;

    // here the current readings of both sensors is displayed.
    case HISTORY_REMOVE_ALL :

      lcd.print ( " Remove current " );
      lcd.setCursor ( 15, 0 );
      lcd.write ( ARROW_UP );

      lcd.setCursor ( 0, 1 );
      lcd.write ( ARROW_RIGHT );
      lcd.print ( "Remove all    ");
      lcd.setCursor ( 15, 1 );
      lcd.write ( ARROW_DOWN );

      break;

      // Here the current threshold is shown. The theshold can be modified by
      // turning the middle potentiometer.
      case HISTORY_USB_SEND :

        lcd.write ( ARROW_RIGHT );
        lcd.print ( "Send over USB " );
        lcd.setCursor ( 15, 0 );
        lcd.write ( ARROW_UP );

        lcd.setCursor ( 0, 1 );
        lcd.print ( " Send all");
        lcd.write ( ARROW_RIGHT );
        lcd.print ( " USB");
        lcd.setCursor ( 15, 1 );
        lcd.write ( ARROW_DOWN );

        break;

      // here the current readings of both sensors is displayed.
      case HISTORY_USB_SEND_ALL :

        lcd.print ( " Send over USB " );
        lcd.setCursor ( 15, 0 );
        lcd.write ( ARROW_UP );

        lcd.setCursor ( 0, 1 );
        lcd.write ( ARROW_RIGHT );
        lcd.print ( "Send all");
        lcd.write ( ARROW_RIGHT );
        lcd.print ( " USB");
        lcd.setCursor ( 15, 1 );
        lcd.write ( ARROW_DOWN );

        break;
  }
}

/**
 * Calculates average speed from history list
 *
 * @return average of history list
 */
float getAverage ( ) {
  // the history size is determined once
  short historySize = history.size ( );
  short count       = 0;
  float total       = 0.0f;

  // all elements of the history list are added up and the number of elements
  // is counted.
  for ( short index = 0; index < historySize; index++ ) {
    total += history.get ( index );
    count++;
  }

  // then the average is calculated and returned
  return total / count;
}

/**
 * Deletes every entry of the table and resets the write index. The readIndex
 * is already -1 when clearHistory function is called.
 */
void clearHistory ( ) {
  history.clear ( );
  writeIndex  = -1;
}

/**
 * measure speed based on trigger time of sensors. Returns -1.0f if
 * the travel time between both sensors is greather than MAX_MEASURE_TIME.
 *
 * @return measured speed
 */
float measureSpeed() {
  short threshold       = analogRead( THRESHOLD_POTMETER );
  short sensorDistance  = getSensorDistance();
  long  sensorTime      = -1;

  //start reading
  for ( unsigned char index = 0; index < 2; index++ ) {
    // trigger on reading higher than the threshold.
    if ( analogRead( (int[2]){ LEFT_SENSOR,
                               RIGHT_SENSOR }[index] ) < threshold ) {
      sensorTime = micros();

      //finish reading within MAX_MEASURE_TIME
      while ( micros() - sensorTime < MAX_MEASURE_TIME ) {
        // stop on reading higher than the threshold at the other sensor.
        if ( analogRead( (int[2]){ RIGHT_SENSOR,
                                   LEFT_SENSOR}[index] ) < threshold ) {
          sensorTime = micros ( ) - sensorTime;
          // wait until sensor is unblocked before continuing. Otherwise the
          // system will directly start a new measurement, thus showing the
          // "unclear reading" message.
          while ( analogRead( (int[2]){ RIGHT_SENSOR,
                                        LEFT_SENSOR}[index] ) < threshold );
          return ( sensorDistance * ( 1000000.0f / sensorTime ) ) / 1000.0f;
        }
      }

      // if the reading on the first sensor keeps below the threshold,
      // something must be wrong so state will be switched to an sensor error
      // state.
      if ( analogRead( (int[2]) { LEFT_SENSOR,
                                  RIGHT_SENSOR}[index] ) < threshold ) {
        if ( state == (int[2]) { RIGHT_SENSOR, LEFT_SENSOR }[index] ) {
          state = STATE_BOTH_SENSORS_ERROR;
        } else {
          state = (int[2]){ STATE_LEFT_SENSOR_ERROR,
                            STATE_RIGHT_SENSOR_ERROR}[index];
        }
        sensorTime  = -1;
      }
    }
    // when the reading on a sensor is above the threshold and the system is in
    // a state of error, the state is reset to default.
    else if ( state == (int[2]){  STATE_LEFT_SENSOR_ERROR,
                                    STATE_RIGHT_SENSOR_ERROR}[index] ) {
      state = STATE_DEFAULT;
    }
  }

  // when a reading is done on one sensor but not on the second in time, an
  // error message is shown.
  if ( sensorTime > 0 ) {
    lcd.clear ( );
    lcd.print ( "unclear reading." );
    delay ( DISPLAY_WAIT_TIME );
  }

  return -1.0f;
}

/**
 * calculate the calibrated distance between two sensors, in the dimension
 * defined in SENSOR_DISTANCE_BASE and SENSOR_CALIBRATE_LENTH
 * @return distance between sensors
 */
short getSensorDistance() {
  return map( analogRead( DISTANCE_POTMETER ), 0, 1024,
              MIN_SENSOR_DISTANCE, MAX_SENSOR_DISTANCE );
}
