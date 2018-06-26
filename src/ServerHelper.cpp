#include "ServerHelper.h"

#define DBG_OUTPUT (*dbg_out)

const char* myssid = "MyIoT";
const char* mypass = "a1234567";


void ServerHelper::handleTelnet() {
  if (TelnetServer.hasClient()) {
    if (!Telnet || Telnet.connected()) {
      if (Telnet) Telnet.stop();
      Telnet = TelnetServer.available();
    } else {
      TelnetServer.available().stop();
    }
  }
}

void ServerHelper::OTA_setup() {
  DBG_OUTPUT.println("OTA has started");
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  if(coolerName.c_str()[0])
    ArduinoOTA.setHostname(coolerName.c_str());

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([&]() {
    onStartUpdateHandler();
    DBG_OUTPUT.println("Start");
  });
  ArduinoOTA.onEnd([&]() {
    DBG_OUTPUT.println("\nEnd");
  });
  ArduinoOTA.onProgress([&](unsigned int progress, unsigned int total) {
    DBG_OUTPUT.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([&](ota_error_t error) {
    DBG_OUTPUT.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) DBG_OUTPUT.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) DBG_OUTPUT.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) DBG_OUTPUT.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) DBG_OUTPUT.println("Receive Failed");
    else if (error == OTA_END_ERROR) DBG_OUTPUT.println("End Failed");
  });
  ArduinoOTA.begin();
  DBG_OUTPUT.println("OTA has begun");
}


void ServerHelper::readEEPROM(int addr, String* str, int len) {
  for (int i = 0; i < len; ++i) {
    char ch = char(EEPROM.read(addr + i));
    *str += ch;
    DBG_OUTPUT.print(ch);
  }
  DBG_OUTPUT.println();
}

bool ServerHelper::read_ssid_and_pass(String* esid, String* epass) {
  // read eeprom for ssid and pass
  DBG_OUTPUT.println("Reading EEPROM ssid: ");
  readEEPROM(0, esid, 32);
  DBG_OUTPUT.println("Reading EEPROM pass: ");
  readEEPROM(32, epass, 32);

  int len = esid->length();
  DBG_OUTPUT.print("len = ");
  DBG_OUTPUT.println(len, DEC);

  readEEPROM(176, &coolerName, 100);
  DBG_OUTPUT.print("Name: ");
  DBG_OUTPUT.println(coolerName.c_str());

  if (esid->c_str()[0] != 0) {
    DBG_OUTPUT.println("esid is exist");
    return true;
  }
  return false;
}


bool ServerHelper::read_and_config() {

  IPAddress ip, gateway, subnet;
  String s_ip, s_gateway, s_subnet;

  DBG_OUTPUT.println("Reading EEPROM ip: ");
  readEEPROM(128, &s_ip, 15);
  DBG_OUTPUT.println("Reading EEPROM gateway: ");
  readEEPROM(144, &s_gateway, 15);
  DBG_OUTPUT.println("Reading EEPROM subnet: ");
  readEEPROM(160, &s_subnet, 15);

  bool isOk = ip.fromString(s_ip) && gateway.fromString(s_gateway) && subnet.fromString(s_subnet);
  
  if (isOk) return WiFi.config(ip, gateway, subnet);

  return false;
}


void ServerHelper::setup(void (*handler)(void)) {
  onStartUpdateHandler = handler;

  //pinMode(_RST, INPUT);
  www_username = "admin";
  www_password = "admin";

  TelnetServer.begin();
  TelnetServer.setNoDelay(true);

  WiFi.mode(WIFI_STA);

  SPIFFS.begin();
  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([&]() {
    if (!checkAuthentication()) return;
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "File Not Found");
  });

  EEPROM.begin(512);

  String esid, epass;
  if (read_ssid_and_pass(&esid, &epass)) {
    if (read_and_config()) DBG_OUTPUT.println("read and config: OK");
    WiFi.begin(esid.c_str(), epass.c_str());
    if (testWifi()) {
      OTA_setup();
      launchWeb(0);
      return;
    }
  }

  setupAP();
  OTA_setup();
}


void ServerHelper::loop() {
  ArduinoOTA.handle();
  handleTelnet();
  server.handleClient();
}


void ServerHelper::printMyTime() {
  long t = millis() / 1000;
  word h = t / 3600;
  byte m = (t / 60) % 60;
  byte s = t % 60;

  DBG_OUTPUT.print(h / 10);
  DBG_OUTPUT.print(h % 10);
  DBG_OUTPUT.print(":");
  DBG_OUTPUT.print(m / 10);
  DBG_OUTPUT.print(m % 10);
  DBG_OUTPUT.print(":");
  DBG_OUTPUT.print(s / 10);
  DBG_OUTPUT.print(s % 10);
  DBG_OUTPUT.println();
}


bool ServerHelper::testWifi(void) {
  int c = 0;
  DBG_OUTPUT.println("Waiting for Wifi to connect...");
  while ( c < 20 ) {
    if (WiFi.status() == WL_CONNECTED) {
      DBG_OUTPUT.println();
      return true;
    }
    delay(500);
    DBG_OUTPUT.print(WiFi.status());
    c++;
  }
  DBG_OUTPUT.println();
  DBG_OUTPUT.println("Connect timed out, opening AP");
  return false;
}

void ServerHelper::launchWeb(int webtype) {
  DBG_OUTPUT.println("");
  DBG_OUTPUT.println("WiFi connected");
  DBG_OUTPUT.print("Local IP: ");
  DBG_OUTPUT.println(WiFi.localIP());
  DBG_OUTPUT.print("SoftAP IP: ");
  DBG_OUTPUT.println(WiFi.softAPIP());

  createWebServer(webtype);
  // Start the server
  server.begin();
  DBG_OUTPUT.println("Server started");
}


void ServerHelper::setupAP() {
  DBG_OUTPUT.print("SETUP AP: ");
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP(myssid, mypass)) {
    DBG_OUTPUT.println("OK");
  } else {
    DBG_OUTPUT.println("Failed");
    delay(1000);
    ESP.restart();
  }
  launchWeb(1);
}

void ServerHelper::listNetworks(void) {
  int n = WiFi.scanNetworks();
  DBG_OUTPUT.println("scan done");
  if (n == 0)
    DBG_OUTPUT.println("no networks found");
  else
  {
    DBG_OUTPUT.print(n);
    DBG_OUTPUT.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      DBG_OUTPUT.print(i + 1);
      DBG_OUTPUT.print(": ");
      DBG_OUTPUT.print(WiFi.SSID(i));
      DBG_OUTPUT.print(" (");
      DBG_OUTPUT.print(WiFi.RSSI(i));
      DBG_OUTPUT.print(")");
      DBG_OUTPUT.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
      delay(10);
    }
  }
  DBG_OUTPUT.println("");
  st = "[";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "{\"ssid\":\"";
    st += WiFi.SSID(i);
    st += "\",\"rssi\":\"";
    st += WiFi.RSSI(i);
    st += "\",\"encyrption\":";
    st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "false" : "true";
    st += "}";

    if (i + 1 < n) st += ",";
  }
  st += "]";
}




String ServerHelper::getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool ServerHelper::handleFileRead(String path) {
  DBG_OUTPUT.println("handleFileRead: " + path);
  //if (path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}


void ServerHelper::handleFileUpload() {
  if (server.uri() != "/upload") return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    DBG_OUTPUT.print("handleFileUpload Name: "); DBG_OUTPUT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    //DBG_OUTPUT.print("handleFileUpload Data: "); DBG_OUTPUT.println(upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile)
      fsUploadFile.close();
    DBG_OUTPUT.print("handleFileUpload Size: "); DBG_OUTPUT.println(upload.totalSize);
  }
}

void ServerHelper::handleFileDelete() {
  if (server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT.print("path = ");
  DBG_OUTPUT.println(path);
  DBG_OUTPUT.println("handleFileDelete: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}



int ServerHelper::writeEEPROM(int addr, String str) {
  int outaddr = addr;
  for (int i = 0; i < str.length(); ++i)
  {
    EEPROM.write(addr + i, str[i]);
    DBG_OUTPUT.print(str[i]);
    outaddr++;
  }
  DBG_OUTPUT.println();
  return outaddr;
}

void ServerHelper::createWebServer(int webtype)
{

  on("/networks", HTTP_GET, [&]() {
    listNetworks();
    server.send(200, "application/json", st);
  });


  //delete file
  on("/upload", HTTP_DELETE, [&]() {
    handleFileDelete();
    server.send(200, "text/plain", "Deleted\r\n");
  });

  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  on("/upload", HTTP_POST, [&]() {
    server.send(200, "text/plain", "Uploaded\r\n");
  }, [&]() {
    handleFileUpload();
  });


  on("/cleareeprom", [&]() {
    for (int i = 0; i < 512; ++i) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    server.send(200, "text/plain", "EEPROM is cleared\r\n");
  });

  on("/config", HTTP_POST, [&]() {
    String v_ssid = server.arg("ssid");
    String v_pass = server.arg("pass");

    String v_username = server.arg("username");
    String v_userpass = server.arg("userpass");
    String v_ip = server.arg("ip");
    String v_gateway = server.arg("gateway");
    String v_subnet = server.arg("subnet");

    String v_name = server.arg("name");

    if (v_name.length() > 0) {
      int out = writeEEPROM(176, v_name);
      EEPROM.write(out, 0);
    }

    if (v_ssid.length() > 0 && v_pass.length() > 0) {
      DBG_OUTPUT.println("clearing eeprom");
      for (int i = 0; i < 64; ++i) {
        EEPROM.write(i, 0);
      }
      DBG_OUTPUT.println(v_ssid);
      DBG_OUTPUT.println("");
      DBG_OUTPUT.println(v_pass);
      DBG_OUTPUT.println("");

      DBG_OUTPUT.println("writing eeprom ssid:");
      writeEEPROM(0, v_ssid);
      DBG_OUTPUT.println("writing eeprom pass:");
      writeEEPROM(32, v_pass);


      if (v_username.length() > 0 && v_userpass.length() > 0) {
        for (int i = 64; i < 128; ++i)
          EEPROM.write(i, 0);
        DBG_OUTPUT.println("writing eeprom username:");
        writeEEPROM(64, v_username);
        DBG_OUTPUT.println("writing eeprom userpass:");
        writeEEPROM(96, v_userpass);
      }


      if (v_ip.length() != 0) DBG_OUTPUT.println("ip ok");
      if (v_gateway.length() != 0) DBG_OUTPUT.println("gateway ok");
      if (v_subnet.length() != 0) DBG_OUTPUT.println("subnet ok");

      if (v_gateway.length() == 0)  v_gateway = "192.168.1.1";
      if (v_subnet.length() == 0)  v_subnet = "255.255.255.0";

      if (v_ip.length() > 0) {
        for (int i = 128; i < 160; ++i)
          EEPROM.write(i, 0);
        DBG_OUTPUT.println("writing eeprom ip:");
        writeEEPROM(128, v_ip);
        DBG_OUTPUT.println("writing eeprom gateway:");
        writeEEPROM(144, v_gateway);
        DBG_OUTPUT.println("writing eeprom subnet:");
        writeEEPROM(160, v_subnet);
      }


      EEPROM.commit();
      content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
      statusCode = 200;
      server.send(statusCode, "application/json", content);
      delay(3000);
      ESP.restart();
    } else {
      content = "{\"Error\":\"404 not found\"}";
      statusCode = 404;
      DBG_OUTPUT.println("Sending 404");
    }
    server.send(statusCode, "application/json", content);
  });

  if ( webtype == 1 ) {
    apHandler();
  } else if (webtype == 0) {
    stHandler();
  }
}
void ServerHelper::setHandlers(void (*st_h)(void), void (*ap_h)(void)) {
  stHandler = st_h;
  apHandler = ap_h;
}


bool ServerHelper::checkAuthentication() {
  /*
  if (!server.authenticate(www_username, www_password)) {
    server.requestAuthentication();
    return false;
  }
  */
  return true;
}


void ServerHelper::on(const String &uri, HTTPMethod method, ESP8266WebServer::THandlerFunction fn, ESP8266WebServer::THandlerFunction ufn) {
  server.addHandler(new MyRequestHandler([&]() {return checkAuthentication();}, fn, ufn, uri, method));
}

void ServerHelper::on(const String &uri, HTTPMethod method, ESP8266WebServer::THandlerFunction fn) {
  on(uri, method, fn, [&]() {});
}

void ServerHelper::on(const String &uri, ESP8266WebServer::THandlerFunction handler) {
  on(uri, HTTP_ANY, handler);
}