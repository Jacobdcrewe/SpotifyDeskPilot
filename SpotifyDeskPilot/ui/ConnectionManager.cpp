#include "Arduino.h" 
#include "Wifi.h"
#include "ConnectionManager.h"


const int timeout = 5000;

ConnectionManager::ConnectionManager(const char* ssid, const char* password) {
  _ssid = ssid;
  _password = password;
  isConnected = false;
}


void ConnectionManager::Connect() {
  WiFi.begin(_ssid, _password);
  Serial.print("\nConnecting");
  int startTime = millis();
  int elapsedTime = millis() - startTime;
  while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(100);
      if(elapsedTime > timeout) {
        Serial.println("\nFailed to connect to WiFi network");
        return;
      }

      elapsedTime = millis() - startTime;
  }

  isConnected = true;
  Serial.println("\nConnected to the WiFi network");
  GetNetworkInfo(true);
}

void ConnectionManager::GetNetworkInfo(bool printInfo) {
    if(WiFi.status() == WL_CONNECTED) {
      int rssi = WiFi.RSSI();
      RSSI = rssi;
      if(printInfo) {
        Serial.print("[*] Network information for ");
        Serial.println(_ssid);

        Serial.println("[+] BSSID : " + WiFi.BSSIDstr());
        Serial.print("[+] Gateway IP : ");
        Serial.println(WiFi.gatewayIP());
        Serial.print("[+] Subnet Mask : ");
        Serial.println(WiFi.subnetMask());
        Serial.println((String)"[+] RSSI : " + rssi + " dB");
        Serial.print("[+] ESP32 IP : ");
        Serial.println(WiFi.localIP());
      }  
    }
}
