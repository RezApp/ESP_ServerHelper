#include "ServerHelper.h"

#define DBG_OUTPUT (*dbg_out)

enum ConfigMode
{
  SSID_PASS   = 1 << 0,
  USER_PASS   = 1 << 1,
  IPV4        = 1 << 2,
  NAME        = 1 << 3
};

void ServerHelper::handleTelnet()
{
  if (TelnetServer.hasClient())
  {
    if (!Telnet || Telnet.connected())
    {
      if (Telnet)
        Telnet.stop();
      Telnet = TelnetServer.available();
    }
    else
    {
      TelnetServer.available().stop();
    }
  }
}

void ServerHelper::OTA_setup()
{
  DBG_OUTPUT.println("OTA has started");
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  if (deviceName.c_str()[0])
    ArduinoOTA.setHostname(deviceName.c_str());

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
    if (error == OTA_AUTH_ERROR)
      DBG_OUTPUT.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      DBG_OUTPUT.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      DBG_OUTPUT.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      DBG_OUTPUT.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      DBG_OUTPUT.println("End Failed");
  });
  ArduinoOTA.begin();
  DBG_OUTPUT.println("OTA has begun");
}

void ServerHelper::clearEEPROM(int addr, int len)
{
  for (int i = addr; i < addr + len; ++i)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();

  DBG_OUTPUT.print("EEPROM: cleared from ");
  DBG_OUTPUT.print(addr);
  DBG_OUTPUT.print(" to ");
  DBG_OUTPUT.println(addr + len);
}

int ServerHelper::readEEPROM(int addr, String *str, int len)
{
  for (int i = 0; i < len; ++i)
  {
    char ch = char(EEPROM.read(addr + i));
    *str += ch;
    DBG_OUTPUT.print(ch);
  }
  DBG_OUTPUT.println();
  return addr + len;
}

int ServerHelper::writeEEPROM(int addr, String str, int len)
{
  int outaddr = addr;
  int max_len = str.length();

  if (len > 0)
    max_len = len;

  for (int i = 0; i < max_len; ++i)
  {
    EEPROM.write(addr + i, str[i]);
    DBG_OUTPUT.print(str[i]);
    outaddr++;
  }
  DBG_OUTPUT.println();
  return outaddr;
}

bool ServerHelper::read_and_config()
{
  IPAddress ip, gateway, subnet, dns;
  if (read_ipv4(&ip, &gateway, &subnet, &dns))
    return WiFi.config(ip, gateway, subnet, dns);

  return false;
}

void ServerHelper::setup(void (*handler)(void))
{
  onStartUpdateHandler = handler;

  www_username = "admin";
  www_password = "admin";

  TelnetServer.begin();
  TelnetServer.setNoDelay(true);

  WiFi.mode(WIFI_STA);

  SPIFFS.begin();
  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([&]() {
    if (!checkAuthentication())
      return;
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "File Not Found");
  });

  EEPROM.begin(512);

  if (read_user_and_pass())
  {
    DBG_OUTPUT.println("USER AND PASS DO EXIST");
  }
  else
  {
    DBG_OUTPUT.println("USER AND PASS DON'T EXIST");
  }

  read_device_name();

  String essid, epass;

  if (read_ssid_and_pass(&essid, &epass))
  {
    DBG_OUTPUT.println("SSID AND PASS DO EXIST");

    if (read_and_config())
      DBG_OUTPUT.println("READ AND CONFIG OK");
    else
      DBG_OUTPUT.println("READ AND CONFIG FAILED");

    WiFi.begin(essid.c_str(), epass.c_str());
    if (testWifi())
    {
      OTA_setup();
      launchWeb(0);
      return;
    }
  }
  else
  {
    DBG_OUTPUT.println("SSID AND PASS DON'T EXIST");
  }

  setupAP();
  OTA_setup();
}

void ServerHelper::loop()
{
  ArduinoOTA.handle();
  handleTelnet();
  server.handleClient();
}

void ServerHelper::printMyTime()
{
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

bool ServerHelper::testWifi(void)
{
  int c = 0;
  DBG_OUTPUT.println("Waiting for Wifi to connect...");
  while (c < 20)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
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

void ServerHelper::launchWeb(int webtype)
{
  DBG_OUTPUT.println();
  DBG_OUTPUT.println("WiFi connected");

  if (webtype == 0)
  {
    DBG_OUTPUT.print("Local IP: ");
    DBG_OUTPUT.println(WiFi.localIP());
  }
  else
  {
    DBG_OUTPUT.print("SoftAP IP: ");
    DBG_OUTPUT.println(WiFi.softAPIP());
  }

  createWebServer(webtype);
  // Start the server
  server.begin();
  DBG_OUTPUT.println("Server started");
}

void ServerHelper::setupAP()
{
  DBG_OUTPUT.print("SETUP AP: ");
  WiFi.mode(WIFI_AP);

  if (apSSID.length() == 0 || apPASS.length() == 0)
  {
    set_ap_ssid_and_pass("MyIoT", "a1234567");
  }

  if (WiFi.softAP(apSSID.c_str(), apPASS.c_str()))
  {
    DBG_OUTPUT.println("OK");
  }
  else
  {
    DBG_OUTPUT.println("Failed");
    delay(1000);
    ESP.restart();
  }
  launchWeb(1);
}

void ServerHelper::listNetworks(void)
{
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

    if (i + 1 < n)
      st += ",";
  }
  st += "]";
}

String ServerHelper::getContentType(String filename)
{
  if (server.hasArg("download"))
    return "application/octet-stream";
  else if (filename.endsWith(".htm"))
    return "text/html";
  else if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".xml"))
    return "text/xml";
  else if (filename.endsWith(".pdf"))
    return "application/x-pdf";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

bool ServerHelper::handleFileRead(String path)
{
  DBG_OUTPUT.println("handleFileRead: " + path);
  if (path.endsWith("/"))
    path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(path))
  {
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void ServerHelper::handleFileUpload()
{
  if (server.uri() != "/upload")
    return;
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    String filename = upload.filename;
    if (!filename.startsWith("/"))
      filename = "/" + filename;
    DBG_OUTPUT.print("handleFileUpload Name: ");
    DBG_OUTPUT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    //DBG_OUTPUT.print("handleFileUpload Data: "); DBG_OUTPUT.println(upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (fsUploadFile)
      fsUploadFile.close();
    DBG_OUTPUT.print("handleFileUpload Size: ");
    DBG_OUTPUT.println(upload.totalSize);
  }
}

void ServerHelper::handleFileDelete()
{
  if (server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
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
  on("/upload", HTTP_POST, [&]() { server.send(200, "text/plain", "Uploaded\r\n"); }, [&]() { handleFileUpload(); });

  on("/cleareeprom", [&]() {
    clearEEPROM();
    server.send(200, "text/plain", "EEPROM is cleared\r\n");
  });

  on("/config", HTTP_POST, [&]() {
    int result = 0;

    String v_ssid = server.arg("ssid");
    String v_pass = server.arg("pass");

    String v_username = server.arg("username");
    String v_userpass = server.arg("userpass");

    String v_ip = server.arg("ip");
    String v_gateway = server.arg("gateway");
    String v_subnet = server.arg("subnet");
    String v_dns = server.arg("dns");

    String v_name = server.arg("name");

    String v_todo = server.arg("todo");

    if (v_ssid.length() > 0 && v_pass.length() > 0)
    {
      write_ssid_and_pass(v_ssid, v_pass);
      result |= ConfigMode::SSID_PASS;
    }

    if (v_username.length() > 0 && v_userpass.length() > 0)
    {
      write_user_and_pass(v_username, v_userpass);
      result |= ConfigMode::USER_PASS;
    }

    if (v_ip.length() > 0)
    {
      if (v_gateway.length() == 0)
        v_gateway = "192.168.1.1";
      if (v_subnet.length() == 0)
        v_subnet = "255.255.255.0";
      if (v_dns.length() == 0)
        v_dns = "8.8.8.8";

      write_ipv4(v_ip, v_gateway, v_subnet, v_dns);
      result |= ConfigMode::IPV4;
    }

    if (v_name.length() > 0)
    {
      write_device_name(v_name);
      result |= ConfigMode::NAME;
    }

    content = "{\"result\":";
    content += result;
    content += "}\r\n";
    server.send(200, "application/json", content);

    if (v_todo.length() > 0)
    {
      if (v_todo.equals("reboot"))
      {
        delay(3000);
        ESP.restart();
      }
    }
  });

  if (webtype == 1)
  {
    apHandler();
  }
  else if (webtype == 0)
  {
    stHandler();
  }
}

void ServerHelper::setHandlers(void (*st_h)(void), void (*ap_h)(void))
{
  stHandler = st_h;
  apHandler = ap_h;
}

bool ServerHelper::checkAuthentication()
{
  if (!authMode)
    return true;

  if (!server.authenticate(www_username.c_str(), www_password.c_str()))
  {
    server.requestAuthentication();
    return false;
  }

  return true;
}

void ServerHelper::on(const String &uri, HTTPMethod method, ESP8266WebServer::THandlerFunction fn, ESP8266WebServer::THandlerFunction ufn)
{
  server.addHandler(new MyRequestHandler([&]() { return checkAuthentication(); }, fn, ufn, uri, method));
}

void ServerHelper::on(const String &uri, HTTPMethod method, ESP8266WebServer::THandlerFunction fn)
{
  on(uri, method, fn, [&]() {});
}

void ServerHelper::on(const String &uri, ESP8266WebServer::THandlerFunction handler)
{
  on(uri, HTTP_ANY, handler);
}

bool ServerHelper::read_ssid_and_pass(String *essid, String *epass)
{
  // read eeprom for ssid and pass
  DBG_OUTPUT.println();
  DBG_OUTPUT.println("> EEPROM: READ");
  DBG_OUTPUT.print("SSID:\t");
  readEEPROM(E_SSID_ADDR, essid, E_SSID_SIZE);
  DBG_OUTPUT.print("PASS:\t");
  readEEPROM(E_PASS_ADDR, epass, E_PASS_SIZE);
  DBG_OUTPUT.println();

  if (essid->c_str()[0] != 0 && epass->c_str()[0] != 0)
  {
    return true;
  }
  return false;
}

void ServerHelper::write_ssid_and_pass(String ssid, String pass)
{
  clearEEPROM(E_AP_ADDR, E_AP_SIZE);

  DBG_OUTPUT.println();
  DBG_OUTPUT.println("> EEPROM: WRITE");
  DBG_OUTPUT.print("SSID:\t");
  writeEEPROM(E_SSID_ADDR, ssid, E_SSID_SIZE);
  DBG_OUTPUT.print("PASS:\t");
  writeEEPROM(E_PASS_ADDR, pass, E_PASS_SIZE);
  DBG_OUTPUT.println();

  EEPROM.commit();
}

bool ServerHelper::read_ipv4(IPAddress *ip, IPAddress *gateway, IPAddress *subnet, IPAddress *dns)
{
  String s_ip, s_gateway, s_subnet, s_dns;

  DBG_OUTPUT.println();
  DBG_OUTPUT.println("> EEPROM: READ");
  DBG_OUTPUT.print("IP:\t\t");
  readEEPROM(E_IP_ADDR, &s_ip, E_IP_SIZE);
  DBG_OUTPUT.print("GATEWAY:\t\t");
  readEEPROM(E_GATEWAY_ADDR, &s_gateway, E_IP_SIZE);
  DBG_OUTPUT.print("SUBNET:\t\t");
  readEEPROM(E_SUBNET_ADDR, &s_subnet, E_IP_SIZE);
  DBG_OUTPUT.print("DNS:\t\t");
  readEEPROM(E_DNS_ADDR, &s_dns, E_IP_SIZE);
  DBG_OUTPUT.println();

  bool isOk = ip->fromString(s_ip) && gateway->fromString(s_gateway) && subnet->fromString(s_subnet) && dns->fromString(s_dns);
  return isOk;
}

void ServerHelper::write_ipv4(String ip, String gateway, String subnet, String dns)
{
  clearEEPROM(E_IPV4_ADDR, E_IPV4_SIZE);

  DBG_OUTPUT.println();
  DBG_OUTPUT.println("> EEPROM: WRITE");
  DBG_OUTPUT.print("IP:\t\t");
  writeEEPROM(E_IP_ADDR, ip, E_IP_SIZE);
  DBG_OUTPUT.print("GATEWAY:\t");
  writeEEPROM(E_GATEWAY_ADDR, gateway, E_IP_SIZE);
  DBG_OUTPUT.print("SUBNET:\t\t");
  writeEEPROM(E_SUBNET_ADDR, subnet, E_IP_SIZE);
  DBG_OUTPUT.print("DNS:\t\t");
  writeEEPROM(E_DNS_ADDR, dns, E_IP_SIZE);
  DBG_OUTPUT.println();

  EEPROM.commit();
}

bool ServerHelper::read_user_and_pass()
{
  String user, pass;

  DBG_OUTPUT.println();
  DBG_OUTPUT.println("> EEPROM: READ");
  DBG_OUTPUT.print("USER:\t");
  readEEPROM(E_AUSER_ADDR, &user, E_AUSER_SIZE);
  DBG_OUTPUT.print("PASS:\t");
  readEEPROM(E_APASS_ADDR, &pass, E_APASS_SIZE);
  DBG_OUTPUT.println();

  bool isOk = (user.c_str()[0] && pass.c_str()[0]);
  if (isOk)
  {
    www_username = user;
    www_password = pass;
  }
  return isOk;
}

void ServerHelper::write_user_and_pass(String user, String pass)
{
  clearEEPROM(E_AUTH_ADDR, E_AUTH_SIZE);

  DBG_OUTPUT.println();
  DBG_OUTPUT.println("> EEPROM: WRITE");
  DBG_OUTPUT.print("USER:\t");
  writeEEPROM(E_AUSER_ADDR, user, E_AUSER_SIZE);
  DBG_OUTPUT.print("PASS:\t");
  writeEEPROM(E_APASS_ADDR, pass, E_APASS_SIZE);
  DBG_OUTPUT.println();

  EEPROM.commit();
}

void ServerHelper::set_ap_ssid_and_pass(String ssid, String pass)
{
  apSSID = ssid;
  apPASS = pass;
}

void ServerHelper::read_device_name()
{
  readEEPROM(E_NAME_ADDR, &deviceName, E_NAME_SIZE);
}

void ServerHelper::write_device_name(String name)
{
  deviceName = name;
  clearEEPROM(E_NAME_ADDR, E_NAME_SIZE);
  if (name.length() > 0 && name.length() <= E_NAME_SIZE)
    writeEEPROM(E_NAME_ADDR, name, E_NAME_SIZE);

  EEPROM.commit();
}

void ServerHelper::active_auth_mode()
{
  authMode = true;
}

void ServerHelper::deactive_auth_mode()
{
  authMode = false;
}
