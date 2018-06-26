#ifndef ServerHelper_h
#define ServerHelper_h

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <FS.h>
#include <ArduinoOTA.h>


class ServerHelper
{
public:
    String st;
    String content;
    int statusCode;

    // Create an instance of the server
    // specify the port to listen on as an argument
    ESP8266WebServer server;

    WiFiServer TelnetServer;
    WiFiClient Telnet;

    Stream* dbg_out;

    char* www_username;
    char* www_password;

    String coolerName;

    //holds the current upload
    File fsUploadFile;

    void (*stHandler)(void);
    void (*apHandler)(void);
    void (*onStartUpdateHandler)(void);

    ServerHelper() : server(80), TelnetServer(23) {
        dbg_out = &Telnet;
    }
    ServerHelper(Stream* s) : server(80), TelnetServer(23) {
        dbg_out = s;
    }

    void setup(void (*handler)(void) = NULL);
    void loop();
    void handleTelnet();
    void OTA_setup();
    void setupAP();

    bool read_ssid_and_pass(String* esid, String* epass);

    bool testWifi(void);
    void launchWeb(int webtype);

    void listNetworks(void);

    String getContentType(String filename);
    bool handleFileRead(String path);
    void handleFileUpload();
    void handleFileDelete();

    void printMyTime();

    void createWebServer(int webtype);
    void setHandlers(void (*st_h)(void), void (*ap_h)(void));

    bool checkAuthentication();

    int writeEEPROM(int addr, String str);
    void readEEPROM(int addr, String* str, int len);

    bool read_and_config();

    void on(const String &uri, HTTPMethod method, ESP8266WebServer::THandlerFunction fn, ESP8266WebServer::THandlerFunction ufn);
    void on(const String &uri, HTTPMethod method, ESP8266WebServer::THandlerFunction fn);
    void on(const String &uri, ESP8266WebServer::THandlerFunction fn);
};

class MyRequestHandler : public RequestHandler {
public:
    //typedef bool (*MyHandlerFunction)(void);
    typedef std::function<bool(void)> AuthHandlerFunction;

    MyRequestHandler(AuthHandlerFunction auth, ESP8266WebServer::THandlerFunction fn, ESP8266WebServer::THandlerFunction ufn, const String &uri, HTTPMethod method)
        : _auth(auth)
        , _fn(fn)
        , _ufn(ufn)
        , _uri(uri)
        , _method(method)
    {
    }

    bool canHandle(HTTPMethod requestMethod, String requestUri) override  {
        if (_method != HTTP_ANY && _method != requestMethod)
            return false;

        if (requestUri != _uri)
            return false;

        return true;
    }

    bool canUpload(String requestUri) override  {
        if (!_ufn || !canHandle(HTTP_POST, requestUri))
            return false;

        return true;
    }

    bool handle(ESP8266WebServer& server, HTTPMethod requestMethod, String requestUri) override {
        (void) server;
        if (!canHandle(requestMethod, requestUri))
            return false;
        if (_auth())_fn();
        return true;
    }

    void upload(ESP8266WebServer& server, String requestUri, HTTPUpload& upload) override {
        (void) server;
        (void) upload;
        if (canUpload(requestUri)) {
            if (_auth())_ufn();
        }
    }

protected:
    AuthHandlerFunction _auth;
    ESP8266WebServer::THandlerFunction _fn;
    ESP8266WebServer::THandlerFunction _ufn;
    String _uri;
    HTTPMethod _method;
};

#endif
