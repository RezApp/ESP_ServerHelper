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


#define E_SSID_SIZE       32
#define E_PASS_SIZE       32
#define E_AUSER_SIZE      32
#define E_APASS_SIZE      32
#define E_IP_SIZE         16
#define E_NAME_SIZE       32

#define E_START_ADDR      0
#define E_START_SIZE      32

#define E_NAME_ADDR       E_START_ADDR   +   E_START_SIZE
#define E_SSID_ADDR       E_NAME_ADDR    +   E_NAME_SIZE
#define E_PASS_ADDR       E_SSID_ADDR    +   E_SSID_SIZE
#define E_AUSER_ADDR      E_PASS_ADDR    +   E_PASS_SIZE
#define E_APASS_ADDR      E_AUSER_ADDR   +   E_AUSER_SIZE
#define E_IP_ADDR         E_APASS_ADDR   +   E_PASS_SIZE
#define E_GATEWAY_ADDR    E_IP_ADDR      +   E_IP_SIZE
#define E_SUBNET_ADDR     E_GATEWAY_ADDR +   E_IP_SIZE
#define E_DNS_ADDR        E_SUBNET_ADDR  +   E_IP_SIZE

#define E_END_ADDR        E_DNS_ADDR     +   E_IP_SIZE 

#define E_IPV4_ADDR       E_IP_ADDR
#define E_IPV4_SIZE       (E_IP_SIZE * 4)

#define E_AUTH_ADDR       E_AUSER_ADDR
#define E_AUTH_SIZE       (E_AUSER_SIZE + E_APASS_SIZE)

#define E_AP_ADDR         E_SSID_ADDR
#define E_AP_SIZE         (E_SSID_SIZE + E_PASS_SIZE)

class ServerHelper
{
  public:
    String st;
    String content;
    int statusCode;
    bool authMode;

    // Create an instance of the server
    // specify the port to listen on as an argument
    ESP8266WebServer server;

    WiFiServer TelnetServer;
    WiFiClient Telnet;

    Stream *dbg_out;

    String www_username;
    String www_password;

    String apSSID;
    String apPASS;

    String deviceName;

    //holds the current upload
    File fsUploadFile;

    void (*stHandler)(void);
    void (*apHandler)(void);
    void (*onStartUpdateHandler)(void);

    ServerHelper() : server(80), TelnetServer(23)
    {
        dbg_out = &Telnet;
    }
    ServerHelper(Stream *s) : server(80), TelnetServer(23)
    {
        dbg_out = s;
    }

    void setup(void (*handler)(void) = NULL);
    void loop();
    void handleTelnet();
    void OTA_setup();
    void setupAP();

    bool read_ssid_and_pass(String *essid, String *epass);
    void write_ssid_and_pass(String essid, String epass);
    
    bool read_user_and_pass();
    void write_user_and_pass(String user, String pass);

    void read_device_name();
    void write_device_name(String name);
    
    void set_ap_ssid_and_pass(String ssid, String pass);

    bool read_ipv4(IPAddress *ip, IPAddress *gateway, IPAddress *subnet, IPAddress *dns);
    void write_ipv4(String ip, String gateway, String subnet, String dns);
   
    bool testWifi(void);
    void launchWeb(int webtype);

    void listNetworks(void);

    void active_auth_mode();
    void deactive_auth_mode();

    String getContentType(String filename);
    bool handleFileRead(String path);
    void handleFileUpload();
    void handleFileDelete();

    void printMyTime();

    void createWebServer(int webtype);
    void setHandlers(void (*st_h)(void), void (*ap_h)(void));

    bool checkAuthentication();

    void clearEEPROM(int addr = 0, int len = 512);
    
    int writeEEPROM(int addr, String str, int len = 0);
    int readEEPROM(int addr, String *str, int len);

    bool read_and_config();

    void on(const String &uri, HTTPMethod method, ESP8266WebServer::THandlerFunction fn, ESP8266WebServer::THandlerFunction ufn);
    void on(const String &uri, HTTPMethod method, ESP8266WebServer::THandlerFunction fn);
    void on(const String &uri, ESP8266WebServer::THandlerFunction fn);
};

class MyRequestHandler : public RequestHandler
{
  public:
    //typedef bool (*MyHandlerFunction)(void);
    typedef std::function<bool(void)> AuthHandlerFunction;

    MyRequestHandler(AuthHandlerFunction auth, ESP8266WebServer::THandlerFunction fn, ESP8266WebServer::THandlerFunction ufn, const String &uri, HTTPMethod method)
        : _auth(auth), _fn(fn), _ufn(ufn), _uri(uri), _method(method)
    {
    }

    bool canHandle(HTTPMethod requestMethod, String requestUri) override
    {
        if (_method != HTTP_ANY && _method != requestMethod)
            return false;

        if (requestUri != _uri)
            return false;

        return true;
    }

    bool canUpload(String requestUri) override
    {
        if (!_ufn || !canHandle(HTTP_POST, requestUri))
            return false;

        return true;
    }

    bool handle(ESP8266WebServer &server, HTTPMethod requestMethod, String requestUri) override
    {
        (void)server;
        if (!canHandle(requestMethod, requestUri))
            return false;
        if (_auth())
            _fn();
        return true;
    }

    void upload(ESP8266WebServer &server, String requestUri, HTTPUpload &upload) override
    {
        (void)server;
        (void)upload;
        if (canUpload(requestUri))
        {
            if (_auth())
                _ufn();
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
