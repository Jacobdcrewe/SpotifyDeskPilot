#include <HTTPClient.h>
#include "SpotifyClient.h"
#include <base64.h>
#include "ArduinoJson.h"

SpotifyClient::SpotifyClient(String clientId, String clientSecret, String refreshToken) {
  _clientId = clientId;
  _clientSecret = clientSecret;
  _refreshToken = refreshToken;

  // cet certificate for client to be able to perform SSL connections
  client.setCACert(rootCertificate);
}

void SpotifyClient::FetchToken() {
  HTTPClient http;
  String body = "grant_type=refresh_token&refresh_token=" + _refreshToken;
  String authorizationRaw = _clientId + ":" + _clientSecret;
  String authorization = base64::encode(authorizationRaw);
  http.begin(client, "https://accounts.spotify.com/api/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Authorization", "Basic " + authorization);

  int httpCode = http.POST(body);
  if (httpCode > 0) {
    String returnedPayload = http.getString();
    if (httpCode == 200) {
      Serial.println(returnedPayload);
      // Allocate the JSON document
      JsonDocument doc;
      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, returnedPayload.c_str());

      // Test if parsing succeeds.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }


      _accessToken = String(doc["access_token"]);
      Serial.println("Got new access token");
    } else {
      Serial.println("Failed to get new access token");
      Serial.println(httpCode);
      Serial.println(returnedPayload);
    }
  } else {
    Serial.println("Failed to connect to https://accounts.spotify.com/api/token");
  }
  http.end();
}

String SpotifyClient::GetCurrentlyPlaying() {
  Serial.println("GetCurrentlyPlaying()");
  HttpResult result = CallAPI("GET", "https://api.spotify.com/v1/me/player", "");
  if (result.httpCode == 200) {
    //Serial.println(result.payload);

    return result.payload;

  } else if (result.httpCode == 401) {
    FetchToken();
    return GetCurrentlyPlaying();
  } else {
    return "ERR-" + result.httpCode;
  }
}

int SpotifyClient::PlayPause(bool isPlaying) {
  Serial.println("PlayPause(): " + isPlaying);
  if (isPlaying) {
    HttpResult result = CallAPI("PUT", "https://api.spotify.com/v1/me/player/pause", "");
    return result.httpCode;
  } else {
    HttpResult result = CallAPI("PUT", "https://api.spotify.com/v1/me/player/play", "");
    return result.httpCode;
  }
}

int SpotifyClient::SetVolume(int volumePercent) {
  Serial.println("SetVolume( " + String(volumePercent) + " )");
  HttpResult result = CallAPI("PUT", "https://api.spotify.com/v1/me/player/volume?volume_percent=" + String(volumePercent), "");
  return result.httpCode;
}

int SpotifyClient::NextSong() {
  Serial.println("NextSong()");
  HttpResult result = CallAPI("POST", "https://api.spotify.com/v1/me/player/next", "");
  return result.httpCode;
}

int SpotifyClient::PrevSong() {
  Serial.println("PrevSong()");
  HttpResult result = CallAPI("POST", "https://api.spotify.com/v1/me/player/previous", "");
  return result.httpCode;
}

HttpResult SpotifyClient::CallAPI(String method, String url, String body) {
  HttpResult result;
  result.httpCode = 0;
  Serial.print(url);
  Serial.print(" returned: ");

  HTTPClient http;
  http.begin(client, url);

  String authorization = "Bearer " + _accessToken;

  http.addHeader(F("Content-Type"), "application/json");
  http.addHeader(F("Authorization"), authorization);

  // address bug where Content-Length not added by HTTPClient is content length is zero
  if (body.length() == 0) {
    http.addHeader(F("Content-Length"), String(0));
  }

  if (method == "PUT") {
    result.httpCode = http.PUT(body);
  } else if (method == "POST") {
    result.httpCode = http.POST(body);
  } else if (method == "GET") {
    result.httpCode = http.GET();
  }

  if (result.httpCode > 0) {
    Serial.println(result.httpCode);
    if (http.getSize() > 0) {
      result.payload = http.getString();
    }
  } else {
    Serial.print("Failed to connect to ");
    Serial.println(url);
  }
  http.end();

  return result;
}
