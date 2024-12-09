#ifndef ConnectionManager_h
#define ConnectionManager_h
#include "Arduino.h" 
#include "WiFi.h"

class ConnectionManager {
  public:
    ConnectionManager(const char* ssid, const char* password);
    void Connect();
    void GetNetworkInfo(bool printInfo);
    bool isConnected;
  private:
    const char* _ssid;
    const char* _password;
};
#endif