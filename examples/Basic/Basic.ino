#include <ServerHelper.h>

#define DBG_OUTPUT  (*serverHelper.dbg_out)
#define content serverHelper.content

ServerHelper serverHelper(&Serial);
//ServerHelper serverHelper;

// Implementation of Station Mode
void stHandler(void) {
  
  serverHelper.on("/", HTTP_GET, []() {
    serverHelper.handleFileRead("/index.html");
  });
  
  serverHelper.on("/setting", HTTP_GET, []() {
      serverHelper.handleFileRead("/setting.html");
  });
}


// Implementation of Access Point Mode
void apHandler(void) {
    serverHelper.on("/setting", HTTP_GET, []() {
      serverHelper.handleFileRead("/setting.html");
    });
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  serverHelper.setHandlers(stHandler, apHandler);
  serverHelper.setup([]() {
      // Before The OTA Update, this function will be called
  });

}

void loop() {
  serverHelper.loop();
}