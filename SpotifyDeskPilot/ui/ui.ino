// custom libraries
#include "settings.h"
#include <ConnectionManager.h>
#include <SpotifyClient.h>

// installed libraries
#include <ArduinoJson.h>
#include <SimpleFOC.h>
#include <TFT_eSPI.h>
#include <ReactESP.h>

/* Don't forget to set Sketchbook location in File/Preferences to the path of your UI project (the parent folder of this INO file) */

MagneticSensorPWM sensor = MagneticSensorPWM(32, 2, 935);

BLDCMotor motor = BLDCMotor(11);
BLDCDriver6PWM driver = BLDCDriver6PWM(13, 22, 14, 12, 25, 27);


// Filter parameters
const int filterWindowSize = 10;  // Moving average window size
float angleBuffer[filterWindowSize] = { 0 };
float velocityBuffer[filterWindowSize] = { 0 };
int angle_bufferIndex = 0;
bool isMoving = true;
unsigned long previousMillis = 0;
bool movingToPos = true;

float prevAngleDeg = 0;
void doPWM() {
  sensor.handlePWM();
}

// Timer for detecting stopped movement
unsigned long stopTime = 0;
const unsigned long stopThreshold = 500;  // Time in milliseconds (0.5 second) for the motor to be considered stopped

// Setting pins
#define PLAYPAUSE_PIN 39  // GIOP39 pin connected to button
#define NEXTBUTTON_PIN 36  // GIOP36 pin connected to button
#define PREVBUTTON_PIN 34  // GIOP34 pin connected to button

#define LED_1 26
#define LED_2 21
#define LED_3 19
#define LED_4 17

// Interval for periodic execution in milliseconds
const unsigned long checkSongInterval = 5500;  // 5000 milliseconds = 5 seconds

// Store the last time the function was called
unsigned long seconds = 0;
String prevSong = "";
String prevArtist = "";
String prevVolume = "0";
String prevDevice = "";
unsigned long prevDuration = 0;
bool isPlaying = false;

// Pin and network settings
ConnectionManager connection = ConnectionManager(ssid, password);
SpotifyClient spotify = SpotifyClient(clientId, clientSecret, refreshToken);

// button check
int currentPlayPauseState;
int lastPlayPauseState = HIGH;
int currentNextState;
int lastNextState = HIGH;
int currentPrevState;
int lastPrevState = HIGH;
// serial input
const int MAX_BUFFER_SIZE = 16;  // Maximum size for the input buffer
char inputBuffer[MAX_BUFFER_SIZE];
int bufferIndex = 0;

/* ----------------------------- UI DECLARATION ---------------------------*/
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite ui_songInfo = TFT_eSprite(&tft);
TFT_eSprite ui_volume = TFT_eSprite(&tft);


/* ------------------------------ Async Event Loop --------------------------*/
using namespace reactesp;

EventLoop event_loop;

/* ----------------------------- UI Functions ----------------------------- */
void ui_setup() {
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
  tft.setRotation(3);

  ui_songInfo.setColorDepth(8);
  ui_songInfo.setTextDatum(MC_DATUM);

  ui_volume.setColorDepth(8);
  ui_volume.setTextSize(2);
  ui_volume.setTextDatum(MC_DATUM);
  ui_volume.createSprite(240, 240);
  ui_volume.drawString("Starting...", ui_volume.width()/2, ui_volume.height()/2);
  ui_volume.pushSprite(0, 0);
}

int songInfoScrollOffset = 0;
int artistInfoScrollOffset = 0;
void setSongInfo() {
    // Clear the sprite to the background color
    ui_songInfo.createSprite(210, ui_songInfo.fontHeight()*7.5);
    ui_songInfo.fillSprite(TFT_BLACK);

    ui_songInfo.setTextColor(TFT_WHITE);
    // Draw the song info text
    ui_songInfo.setTextSize(2);
    int songInfoWidth = tft.textWidth(prevSong) * 2;
    if(songInfoWidth > ui_songInfo.width()) {
      songInfoScrollOffset += 1;
    } else {
      songInfoScrollOffset = 0;
    }

    int songInfoPos = ui_songInfo.width()/2 + songInfoScrollOffset;
    if(songInfoPos > ui_songInfo.width() + songInfoWidth/2) {
      songInfoScrollOffset = -ui_songInfo.width()/2 - songInfoWidth/2;
      songInfoPos = -songInfoWidth/2;
    }

    ui_songInfo.drawString(prevSong, songInfoPos, ui_songInfo.fontHeight()/2); // Title

    ui_songInfo.setTextSize(1);
    int artistInfoWidth = tft.textWidth(prevArtist);
    if(artistInfoWidth > ui_songInfo.width()) {
      artistInfoScrollOffset += 1;
    } else {
      artistInfoScrollOffset = 0;
    }

    int artistInfoPos = ui_songInfo.width()/2 + artistInfoScrollOffset;
    if(artistInfoPos > ui_songInfo.width() + artistInfoWidth/2) {
      artistInfoScrollOffset = -ui_songInfo.width()/2 - artistInfoWidth/2;
      artistInfoPos = -artistInfoWidth/2;
    }

    ui_songInfo.drawString(prevArtist, artistInfoPos, ui_songInfo.fontHeight()*3); // Artist

    if(seconds > prevDuration) seconds = prevDuration;

    // Format time and duration
    String time = convertSecondsToMinutes(seconds);
    String durationTime = convertSecondsToMinutes(prevDuration);
    String value = time + "/" + durationTime;

    ui_songInfo.drawString(value, ui_songInfo.width()/2, ui_songInfo.fontHeight()*7); // Time info

    ui_songInfo.drawRoundRect(15, ui_songInfo.fontHeight()*5 + 2, 180, 5, 2, 0x09e3);
    int width = 0;
    if(prevDuration > 0) {
      width= 180*(int)seconds/(int)prevDuration;
    }
    ui_songInfo.drawRoundRect(15, ui_songInfo.fontHeight()*5 + 2, width, 5, 2, 0x25ca);
    // Push the sprite to the display
    ui_songInfo.pushToSprite(&ui_volume, 15, (ui_volume.height() - ui_songInfo.height())/2);
    ui_songInfo.deleteSprite();
}

void setVolume(int volume) {
  ui_volume.setTextSize(1);
  ui_volume.setTextDatum(MC_DATUM);
  ui_volume.createSprite(240, 240);
  ui_volume.fillSprite(TFT_BLACK);
  ui_volume.setTextColor(TFT_WHITE);


  int endAngle = (volume + 1)*360/100;
  ui_volume.drawSmoothArc(ui_volume.width()/2, ui_volume.height()/2, ui_volume.width()/2, -2+ui_volume.width()/2, 0, endAngle, 0x25ca, TFT_BLACK, true);
  
  if(volume == 99) {
    volume = 100;
  }

  ui_volume.setTextSize(1);
  ui_volume.drawString(String(volume), ui_volume.width()/2, ui_volume.fontHeight()/2 + 15);
  ui_volume.drawString(prevDevice, ui_volume.width()/2, ui_volume.fontHeight()*2 + 15);
  setSongInfo();
  ui_volume.pushSprite(0, 0);
  ui_volume.deleteSprite();
}
/* ----------------------------- Spotify Functions ----------------------------- */
void spotify_setup() {
  delay(1000);
  Serial.println("Connecting...");

  connection.Connect();
  spotify.FetchToken();
  Serial.println("Connected");

  // Initialize the pushbutton pin as a pull-up input
  pinMode(PLAYPAUSE_PIN, INPUT);
  pinMode(NEXTBUTTON_PIN, INPUT);
  pinMode(PREVBUTTON_PIN, INPUT);

}

void getSong() {
  if (connection.isConnected) {
    event_loop.onDelay(0, []() {
      String playingInfo = spotify.GetCurrentlyPlaying();
      JsonDocument doc = ParseJson(playingInfo);
      String error = String(doc["error"]);

      // Error handling
      if (error.equals("null")) {
        String device = String(doc["device"]["name"]);
        String song = String(doc["item"]["name"]);
        JsonArray artists = doc["item"]["artists"].as<JsonArray>();
        String artist = "";

        for (JsonObject v : artists) {
          if (!artist.isEmpty()) {
            artist += ", "; // Add a separator between artist names
          }
          artist += v["name"].as<String>();
        }
        String progressString = String(doc["progress_ms"]);
        String durationString = String(doc["item"]["duration_ms"]);
        String volume = String(doc["device"]["volume_percent"]);

        isPlaying = doc["is_playing"];


        if (!song.equals(prevSong)) {
          prevSong = song;
          // Serial.print("SONG: ");
          // Serial.println(song);
          // set ui song
        }

        if (!artist.equals(prevArtist)) {
          prevArtist = artist;
          // Serial.print("ARTIST: ");
          // Serial.println(artist);
          // set ui artist
        }
        if (!volume.equals(prevVolume) && !isMoving || (motor.shaftAngle() > 0 || motor.shaftAngle() < -2*3.141592654)) {
          prevVolume = volume;
          // Serial.print("VOLUME: ");
          // Serial.println(volume);
          Serial.println("Volume: " + volume);
          movingToPos = true;
          motor.enable();

          // set ui volume
        }

        if(!device.equals(prevDevice)) {
          prevDevice = device;
        }

        unsigned long duration_ms = durationString.toInt();
        if (!(duration_ms == 0 && progressString != "0")) {
          unsigned long duration = duration_ms / 1000;
          if (duration != prevDuration) {
            prevDuration = duration;
          }
        }

        unsigned long progress_ms = progressString.toInt();
        if (progress_ms == 0 && progressString != "0") {
          Serial.println("Loading time...");
        } else {
          seconds = progress_ms / 1000;
        }

      } else {
        prevSong = "Not Playing";
        prevArtist = "";
        prevDuration = 0;
        Serial.print(prevSong);
        // set ui not playing
      }
    });
  } else {
    Serial.println("Connect to a wifi network");
    //set ui not connected
  }
}

void checkPlayPause() {
  // Read the state of the button:
  currentPlayPauseState = digitalRead(PLAYPAUSE_PIN);

  if (currentPlayPauseState != lastPlayPauseState && currentPlayPauseState == HIGH) {
    event_loop.onDelay(0, [] () {
      SetLed(LOW,LOW,LOW,LOW);
      spotify.PlayPause(isPlaying);
      GetNetworkInfo();
      isPlaying = !isPlaying;

      if (!isPlaying) {
        Serial.println("Paused");
            prevSong = "Paused";

        // set ui paused
      } else {
        Serial.println("Playing");
        prevSong = "Playing";
        // set ui playing
      }
      prevArtist = "";
    });
    
    // clear artist ui
  }

  lastPlayPauseState = currentPlayPauseState;
}

void checkNext() {
  // Read the state of the button:
  currentNextState = digitalRead(NEXTBUTTON_PIN);

  if (currentNextState != lastNextState && currentNextState == HIGH) {
    event_loop.onDelay(0, [] () {
      SetLed(LOW,LOW,LOW,LOW);
      spotify.NextSong();
      GetNetworkInfo();
    });
    // clear artist ui
  }

  lastNextState = currentNextState;
}

void checkPrev() {
 // Read the state of the button:
  currentPrevState = digitalRead(PREVBUTTON_PIN);

  if (currentPrevState != lastPrevState && currentPrevState == HIGH) {
    event_loop.onDelay(0, [] () {
      SetLed(LOW,LOW,LOW,LOW);
      spotify.PrevSong();
      GetNetworkInfo();
    });
    // clear artist ui
  }

  lastPrevState = currentPrevState;
}

void checkVolumeChange() {
  float pi = 3.141592654;
  if(movingToPos) {
    isMoving = true;
    float angleRads = -prevVolume.toInt() * 2 * pi/100;
    motor.move(angleRads);
    if(abs(motor.shaftAngle() - angleRads) < 0.05) {
        isMoving = false;
        movingToPos = false;
        motor.disable();
    }
  } else {
    // Get the raw angle and angular velocity in radians and radians/second
    float angleRad = -sensor.getAngle();
    float velocityRadS = sensor.getVelocity();
    // Add raw data to buffers
    angleBuffer[angle_bufferIndex] = angleRad;
    velocityBuffer[angle_bufferIndex] = velocityRadS;

    // Compute the moving average for noise reduction
    float angleRadFiltered = computeMovingAverage(angleBuffer, filterWindowSize);
    float velocityRadSFiltered = computeMovingAverage(velocityBuffer, filterWindowSize);

    // Update the buffer index
    angle_bufferIndex = (angle_bufferIndex + 1) % filterWindowSize;

    // Convert filtered values to degrees and degrees/second
    float angleDeg = angleRadFiltered * (180.0 / pi);          // Correct conversion
    float velocityDegS = velocityRadSFiltered * (180.0 / pi);  // Correct conversion

    // Introduce rounding errors
    float angleDegRounded = round(angleDeg * 2) / 2.0;     // Quantize to 0.5 degree steps
    float velocityDegSRounded = round(velocityDegS / 10);  // Quantize to 10 deg/s steps

    // convert to percentage of 360
    int percentage = angleDegRounded * 100 / 360;
    // set ui volume
    if(percentage < 0) {
      percentage = 0;
    }

    if (percentage > 100) {
      percentage = 100;
    } 

    if(!prevSong.isEmpty()) setVolume(percentage);

    if (velocityDegSRounded == 0) {
      isMoving = false;
      if(abs(prevVolume.toInt() - percentage) > 1.5) {
          isMoving = true;
          if(millis() - previousMillis > 1000) {
            prevVolume = String(percentage);

            event_loop.onDelay(0, [percentage]() {
              if (spotify.SetVolume(round(percentage)) != 204) {
                // set ui volume
                Serial.println("ERR-VOLUME NOT SET");
              }
              isMoving = false;
            });
            previousMillis = millis();
          }
      }
    } else {
      isMoving = true;
    }
  }
  
}


/* ----------------------------- Utility Functions ----------------------------- */

JsonDocument ParseJson(String json) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json.c_str());

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    doc.clear();
    doc["error"] = error.f_str();
    prevSong = "ERROR-JSON";
    prevArtist = "";
    // set error ui;
  }

  return doc;
}

// Function to compute the moving average of a buffer
float computeMovingAverage(float buffer[], int size) {
  float sum = 0;
  for (int i = 0; i < size; i++) {
    sum += buffer[i];
  }
  return sum / size;
}

String convertSecondsToMinutes(unsigned long seconds) {
  int minutes = seconds / 60;
  int secs = seconds % 60;
  String secondsString = String(secs);
  if (secondsString.length() < 2) {
    secondsString = "0" + secondsString;
  }
  return String(minutes) + ":" + secondsString;
}

bool isInteger(const char* str) {
  for (int i = 0; str[i] != '\0'; i++) {
    if (!isdigit(str[i])) {
      return false;
    }
  }
  return true;
}

/* ----------------------------- Setup and Loop ----------------------------- */
void encoder_init() {
  // enable more verbose output for debugging
  // comment out if not needed
  SimpleFOCDebug::enable(&Serial);

  // initialise magnetic sensor hardware
  sensor.init();
  sensor.enableInterrupt(doPWM);
  // link the motor to the sensor
  motor.linkSensor(&sensor);

  // driver config
  // power supply voltage [V]
  driver.voltage_power_supply = 5;
  driver.init();
  // link the motor and the driver
  motor.linkDriver(&driver);

  // choose FOC modulation (optional)
  motor.foc_modulation = FOCModulationType::SpaceVectorPWM;

  // set motion control loop to be used
  motor.controller = MotionControlType::angle;

  // contoller configuration
  // default parameters in defaults.h

  // velocity PI controller parameters
  motor.PID_velocity.P = 0.2f;
  motor.PID_velocity.I = 20;
  motor.PID_velocity.D = 0;
  // maximal voltage to be set to the motor
  motor.voltage_limit = 12;

  // velocity low pass filtering time constant
  // the lower the less filtered
  motor.LPF_velocity.Tf = 0.01f;

  // angle P controller
  motor.P_angle.P = 20;
  // maximal velocity of the position control
  motor.velocity_limit = 5;
  
  // comment out if not needed
  motor.useMonitoring(Serial);


  // initialize motor
  motor.init();
  // align sensor and start FOC
  motor.initFOC();

  Serial.println(F("Motor ready."));
  _delay(1000);
}

// check wifi strength. set leds
void GetNetworkInfo() {
  connection.GetNetworkInfo(false);
  int rssi = connection.RSSI;
  // excellent strength
  if(rssi >= -50) {
    SetLed(HIGH, HIGH, HIGH, HIGH);
  } else if(rssi >= -70) {
    // good strength
    SetLed(HIGH, HIGH, HIGH, LOW);
  } else if(rssi >= -85) {
    // weak
    SetLed(HIGH, HIGH, LOW, LOW);
  } else if(rssi >= -100) {
    // very very weak
    SetLed(HIGH, LOW, LOW, LOW);
  } else {
    // no
    SetLed(LOW, LOW, LOW, LOW);
  }
}

void SetLed(uint8_t led1, uint8_t led2, uint8_t led3, uint8_t led4) {
  digitalWrite(LED_1, led1);
  digitalWrite(LED_2, led2);
  digitalWrite(LED_3, led3);
  digitalWrite(LED_4, led4);

}

void setup() {
  Serial.begin(115200); /* Prepare for possible serial debug */
  // setup leds
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);
  pinMode(LED_3, OUTPUT);
  pinMode(LED_4, OUTPUT);
  SetLed(HIGH, LOW, LOW, LOW);
  delay(500);
  SetLed(HIGH, HIGH, LOW, LOW);
  delay(500);
  SetLed(HIGH, HIGH, HIGH, LOW);
  delay(500);
  SetLed(HIGH, HIGH, HIGH, HIGH);
  delay(500);
  SetLed(LOW, LOW, LOW, LOW);

  ui_setup();
  encoder_init();
  spotify_setup();
  Serial.println("Setup done");
  getSong();
  event_loop.onTick([] () {
    checkPlayPause();
  });

  event_loop.onTick([] () {
    checkNext();
  });

  
  event_loop.onTick([] () {
    checkPrev();
  });

  event_loop.onTick([] () {
    GetNetworkInfo();
  });

  event_loop.onTick([] () {
    motor.loopFOC();
    // to do: add reset to the max and min
    checkVolumeChange();
  });

  event_loop.onRepeat(checkSongInterval, [] () {
    getSong();
  });

  event_loop.onRepeat(1000, [] () {
    if(isPlaying) {
      seconds = seconds + 1;
    }
  });
}

void loop() {
  event_loop.tick();

  while (Serial.available()) {
    char incomingChar = Serial.read();  // Read a character

    // Check for newline to process input
    if (incomingChar == '\n') {
      inputBuffer[bufferIndex] = '\0';  // Null-terminate the string

      if (isInteger(inputBuffer)) {     // Check if it's an integer
        int value = atoi(inputBuffer);  // Convert to integer
        Serial.print("You entered an integer: ");
        Serial.println(value);
        if (spotify.SetVolume(value) == 204) {
          prevVolume = String(value);
        }
        // Do something with the integer
      } else if (strcmp(inputBuffer, "n") == 0) {  // Check if input is "n"
        Serial.println("You entered 'n'");
        spotify.NextSong();
        // Do something for "n"
      } else if (strcmp(inputBuffer, "p") == 0) {  // Check if input is "p"
        Serial.println("You entered 'p'");
        spotify.PrevSong();
        // Do something for "p"
      } else {
        Serial.println("Invalid input.");
      }

      // Reset buffer index for the next input
      bufferIndex = 0;
    } else if (bufferIndex < MAX_BUFFER_SIZE - 1) {
      inputBuffer[bufferIndex++] = incomingChar;  // Add character to buffer
    }
  }
}
