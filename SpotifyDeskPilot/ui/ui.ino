// custom libraries
#include "settings.h"
#include <ConnectionManager.h>
#include <SpotifyClient.h>

// installed libraries
#include <ArduinoJson.h>
#include <SimpleFOC.h>
#include <TFT_eSPI.h>


/* Don't forget to set Sketchbook location in File/Preferences to the path of your UI project (the parent folder of this INO file) */

MagneticSensorPWM sensor = MagneticSensorPWM(32, 2, 935);

// Filter parameters
const int filterWindowSize = 10;  // Moving average window size
float angleBuffer[filterWindowSize] = { 0 };
float velocityBuffer[filterWindowSize] = { 0 };
int angle_bufferIndex = 0;
bool isMoving = false;

float prevAngleDeg = 0;
void doPWM() {
  sensor.handlePWM();
}

// Timer for detecting stopped movement
unsigned long stopTime = 0;
const unsigned long stopThreshold = 500;  // Time in milliseconds (0.5 second) for the motor to be considered stopped

// Setting pins
#define BUTTON_PIN 26  // GIOP26 pin connected to button

// Interval for periodic execution in milliseconds
const unsigned long checkSongInterval = 5500;  // 5000 milliseconds = 5 seconds

// Store the last time the function was called
unsigned long previousMillis = 0;
unsigned long previousSecond = 0;
unsigned long seconds = 0;
String prevSong = "";
String prevArtist = "";
String prevVolume = "0";
unsigned long prevDuration = 0;
bool isPlaying = false;

// Pin and network settings
ConnectionManager connection = ConnectionManager(ssid, password);
SpotifyClient spotify = SpotifyClient(clientId, clientSecret, refreshToken);

// button check
int currentState;
int lastState = HIGH;
// serial input
const int MAX_BUFFER_SIZE = 16;  // Maximum size for the input buffer
char inputBuffer[MAX_BUFFER_SIZE];
int bufferIndex = 0;

/* ----------------------------- UI DECLARATION ---------------------------*/
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite ui_songInfo = TFT_eSprite(&tft);
TFT_eSprite ui_volume = TFT_eSprite(&tft);

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

void setSongInfo() {
    // Clear the sprite to the background color
    ui_songInfo.createSprite(210, ui_songInfo.fontHeight()*7.5);
    ui_songInfo.fillSprite(TFT_BLACK);

    ui_songInfo.setTextColor(TFT_WHITE);
    // Draw the song info text
    ui_songInfo.setTextSize(2);
    ui_songInfo.drawString(prevSong, ui_songInfo.width()/2, ui_songInfo.fontHeight()/2); // Title

    ui_songInfo.setTextSize(1);
    ui_songInfo.drawString(prevArtist, ui_songInfo.width()/2, ui_songInfo.fontHeight()*3); // Artist

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
  ui_volume.drawString(String(volume), ui_volume.width()/2, ui_volume.fontHeight()/2 + 30);
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
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void getSong() {
  if (connection.isConnected) {
    String playingInfo = spotify.GetCurrentlyPlaying();
    JsonDocument doc = ParseJson(playingInfo);
    String error = String(doc["error"]);

    // Error handling
    if (error.equals("null")) {
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

      if (!volume.equals(prevVolume) && !isMoving) {
        prevVolume = volume;
        // Serial.print("VOLUME: ");
        // Serial.println(volume);
        setVolume(volume.toInt());

        // set ui volume
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

    connection.GetNetworkInfo(false);
  } else {
    Serial.println("Connect to a wifi network");
    //set ui not connected
  }
}

void checkPlayPause() {
  // Read the state of the button:
  currentState = digitalRead(BUTTON_PIN);

  if (currentState != lastState && currentState == HIGH) {
    int code = spotify.PlayPause(isPlaying);
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
    // clear artist ui
  }

  lastState = currentState;
}

void checkVolumeChange() {
  // Update the sensor data
  sensor.update();

  // Get the raw angle and angular velocity in radians and radians/second
  float angleRad = sensor.getAngle();
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
  float angleDeg = angleRadFiltered * (180.0 / 3.14159);          // Correct conversion
  float velocityDegS = velocityRadSFiltered * (180.0 / 3.14159);  // Correct conversion

  // Introduce rounding errors
  float angleDegRounded = round(angleDeg * 2) / 2.0;     // Quantize to 0.5 degree steps
  float velocityDegSRounded = round(velocityDegS / 10);  // Quantize to 10 deg/s steps

  // convert to percentage of 360
  float percentage = (abs((int)angleDegRounded) % 360) * 100 / 360;
  // set ui volume
  setVolume(round(percentage));

  if (velocityDegSRounded == 0 && abs(prevVolume.toInt() - round(percentage)) > 1) {
    prevVolume = String(round(percentage));

    if (spotify.SetVolume(round(percentage)) == 204) {
      // set ui volume
      Serial.println("SPOTIFY CHANGED: " + prevVolume);
    }
    isMoving = false;
  } else {
    isMoving = true;
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
  // Initialise magnetic sensor hardware
  sensor.init();
  // Comment out to use sensor in blocking (non-interrupt) way
  sensor.enableInterrupt(doPWM);

  Serial.println("Sensor ready");
  _delay(1000);
}
void setup() {
  Serial.begin(115200); /* Prepare for possible serial debug */
  encoder_init();
  ui_setup();
  delay(3000);

  spotify_setup();
  Serial.println("Setup done");
}

void loop() {
  checkPlayPause();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= checkSongInterval) {
    previousMillis = currentMillis;
    getSong();
  }

  if (currentMillis - previousSecond >= 1000 && isPlaying) {
    previousSecond = currentMillis;
    seconds = seconds + 1;
    String time = convertSecondsToMinutes(seconds);
    // Serial.println("AUTO TIME: " + time);
    // set ui time
    int32_t progressPercent = (int)100 * ((float)seconds / prevDuration);
    // Serial.println("AUTO PROGRESS: " + String(progressPercent));
    // set ui progress
  }


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

  checkVolumeChange();
  delay(5);
}
