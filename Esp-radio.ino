//******************************************************************************************
//*  Esp_radio -- Webradio receiver for ESP8266, (color) display and VS1053 MP3 module,    *
//*               by Ed Smallenburg (ed@smallenburg.nl)                                    *
//*  With ESP8266 running at 80 MHz, it is capable of handling up to 256 kb bitrate.       *
//*  With ESP8266 running at 160 MHz, it is capable of handling up to 320 kb bitrate.      *
//******************************************************************************************
// ESP8266 libraries used:
//  - ESP8266WiFi       - Part of ESP8266 Arduino default libraries.
//  - SPI               - Part of Arduino default libraries.
//  - ESPAsyncTCP       - https://github.com/me-no-dev/ESPAsyncTCP
//  - ESPAsyncWebServer - https://github.com/me-no-dev/ESPAsyncWebServer
//  - FS - https://github.com/esp8266/arduino-esp8266fs-plugin/releases/download/0.2.0/ESP8266FS-0.2.0.zip
//  - ArduinoOTA        - Part of ESP8266 Arduino default libraries.
//
// A library for the VS1053 (for ESP8266) is not available (or not easy to find).  Therefore
// a class for this module is derived from the maniacbug library and integrated in this sketch.
//
// Compiling: Set SPIFS to 3 MB.  Set IwIP variant to "V1.4 Higher Bandwidth".
// See http://www.internet-radio.com for suitable stations.  Add the stations of your choice
// to the .ini-file.
//
// Brief description of the program:
// First a suitable WiFi network is found and a connection is made.
// Then a connection will be made to a shoutcast server.  The server starts with some
// info in the header in readable ascii, ending with a double CRLF, like:
//  icy-name:Classic Rock Florida - SHE Radio
//  icy-genre:Classic Rock 60s 70s 80s Oldies Miami South Florida
//  icy-url:http://www.ClassicRockFLorida.com
//  content-type:audio/mpeg
//  icy-pub:1
//  icy-metaint:32768          - Metadata after 32768 bytes of MP3-data
//  icy-br:128                 - in kb/sec (for Ogg this is like "icy-br=Quality 2"
//
// After de double CRLF is received, the server starts sending mp3- or Ogg-data.  For mp3, this
// data may contain metadata (non mp3) after every "metaint" mp3 bytes.
// The metadata is empty in most cases, but if any is available the content will be presented on the TFT.
// Pushing the input button causes the player to select the next preset station present in the .ini file.
//
// The display used is a Chinese 1.8 color TFT module 128 x 160 pixels.  The TFT_ILI9163C.h
// file has been changed to reflect this particular module.  TFT_ILI9163C.cpp has been
// changed to use the full screenwidth if rotated to mode "3".  Now there is room for 26
// characters per line and 16 lines.  Software will work without installing the display.
// If no TFT is used, you may use GPIO2 and GPIO15 as control buttons.  See definition of "USETFT" below.
// Switches are than programmed as:
// GPIO2 : "Goto station 1"
// GPIO0 : "Next station"
// GPIO15: "Previous station".  Note that GPIO15 has to be LOW when starting the ESP8266.
//         The button for GPIO15 must therefore be connected to VCC (3.3V) instead of GND.

//
// For configuration of the WiFi network(s): see the global data section further on.
//
// The SPI interface for VS1053 and TFT uses hardware SPI.
//
// Wiring:
// NodeMCU  GPIO    Pin to program  Wired to LCD        Wired to VS1053      Wired to rest
// -------  ------  --------------  ---------------     -------------------  ---------------------
// D0       GPIO16  16              -                   (10) XDCS            LED on nodeMCU
// D1       -       -               SCL                 -                    -
// D2       -       -               SDA                 -                    -
// D3       GPIO0   0 FLASH         -                   (7) DREQ             -
// D4       GPIO2   2               -                   -                    -
// D5       GPIO14  14 SCLK         -                   (6) SCK              -
// D6       GPIO12  12 MISO         -                   (4) MISO             -
// D7       GPIO13  13 MOSI         -                   (5) MOSI             -
// D8       GPIO15  15              -                   -                    -
// D9       GPIO3   3 RXD0          -                   -                    Reserved serial input
// D10      GPIO1   1 TXD0          -                   -                    Reserved serial output
// CLK      GPIO9   9               -                   (9) X S              -
// SD3      GPIO10  10 INPUT        -                   -                    -
// -------  ------  --------------  ---------------     -------------------  ---------------------
// GND      -       -               GND                 (3) DGND             Power supply
// VCC 3.3  -       -               -                   -                    LDO 3.3 Volt
// VCC 5 V  -       -               VCC                 (1,2) 5V             Power supply
// RST      -       -               -                   (8) XRST             Reset circuit
//
// The reset circuit is a circuit with 2 diodes to GPIO5 and GPIO16 and a resistor to ground
// (wired OR gate) because there was not a free GPIO output available for this function.
// This circuit is included in the documentation.
// Issues:
// Webserver produces error "LmacRxBlk:1" after some time.  After that it will work very slow.
// The program will reset the ESP8266 in such a case.  Now we have switched to async webserver,
// the problem still exists, but the program will not crash anymore.
// Upload to ESP8266 not reliable.
//
// 31-03-2016, ES: First set-up.
// 01-04-2016, ES: Detect missing VS1053 at start-up.
// 05-04-2016, ES: Added commands through http server on port 80.
// 14-04-2016, ES: Added icon and switch preset on stream error.
// 18-04-2016, ES: Added SPIFFS for webserver.
// 19-04-2016, ES: Added ringbuffer.
// 20-04-2016, ES: WiFi Passwords through SPIFFS files, enable OTA.
// 21-04-2016, ES: Switch to Async Webserver.
// 27-04-2016, ES: Save settings, so same volume and preset will be used after restart.
// 03-05-2016, ES: Add bass/treble settings (see also new index.html).
// 04-05-2016, ES: Allow stations like "skonto.ls.lv:8002/mp3".
// 06-05-2016, ES: Allow hiddens WiFi station if this is the only .pw file.
// 07-05-2016, ES: Added preset selection in webserver.
// 12-05-2016, ES: Added support for Ogg-encoder.
// 13-05-2016, ES: Better Ogg detection.
// 17-05-2016, ES: Analog input for commands, extra buttons if no TFT required.
// 26-05-2016, ES: Fixed BUTTON3 bug (no TFT).
// 27-05-2016, ES: Fixed restore station at restart.
// 04-07-2016, ES: WiFi.disconnect clears old connection now (thanks to Juppit).
// 23-09-2016, ES: Added commands via MQTT and Serial input, Wifi set-up in AP mode.
// 04-10-2016, ES: Configuration in .ini file. No more use of EEPROM and .pw files.
// 11-10-2016, ES: Allow stations that have no bitrate in header like icecast.err.ee/raadio2.mp3.
// 14-10-2016, ES: Updated for async-mqtt-client-master 0.5.0
// 22-10-2016, ES: Correction mute/unmute.
// 15-11-2016, ES: Support for .m3u playlists.
// 22-12-2016, ES: Support for localhost (play from SPIFFS).
// 28-12-2016, ES: Implement "Resume" request.
// 31-12-2016, ES: Allow ContentType "text/css".
// 02-01-2017, ES: Webinterface in PROGMEM.
// 16-01-2017, ES: Correction playlists.
// 17-01-2017, ES: Bugfix config page and playlist.
// 23-01-2017, ES: Bugfix playlist.
// 26-01-2017, ES: Check on wrong icy-metaint.
// 30-01-2017, ES: Allow chunked transfer encoding.
// 01-02-2017, ES: Bugfix file upload.
// 26-04-2017, ES: Better output webinterface on preset change.
// 03-05-2017, ES: Prevent to start inputstream if no network.
// 04-05-2017, ES: Integrate iHeartRadio, thanks to NonaSuomy.
// 09-05-2017, ES: Fixed abs problem.
// 11-05-2017, ES: Convert UTF8 characters before display, thanks to everyb313.
// 24-05-2017, ES: Correction. Do not skip first part of .mp3 file.
// 26-05-2017, ES: Correction playing from .m3u playlist and LC/UC problem.
// 31-05-2017, ES: Volume indicator on TFT.
// 02-02-2018, ES: Force 802.11N connection.
// 18-04-2018, ES: Workaround for not working wifi.connected().
// 05-10-2018, ES: Fixed exception if no network was found.
// 23-04-2018, ES: Check BASS setting.
//
// Define the version number, also used for webserver as Last-Modified header:
#define VERSION "Arthur Ferrai v1.0"
#define DEBUG
// TFT.  Define USETFT if required.
//#define USETFT
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <Ticker.h>
#include <stdio.h>
#include <string.h>
#include <FS.h>
#include <ArduinoOTA.h>
#include "VS1053.h"
#include "debug.h"
#include "display.h"
#include "asciitools.h"

extern "C"
{
#include "user_interface.h"
}

// Definitions for 3 control switches on analog input
// You can test the analog input values by holding down the switch and select /?analog=1
// in the web interface. See schematics in the documentation.
// Switches are programmed as "Goto station 1", "Next station" and "Previous station" respectively.
// Set these values to 2000 if not used or tie analog input to ground.
#define NUMANA 3
//#define asw1    252
//#define asw2    334
//#define asw3    499
#define asw1 2000
#define asw2 2000
#define asw3 2000
//
// Digital I/O used
// Pins for VS1053 module
const uint8_t VS1053_CS = 9;
const uint8_t VS1053_DCS = D0;
const uint8_t VS1053_DREQ = D3;
// Ringbuffer for smooth playing. 20000 bytes is 160 Kbits, about 1.5 seconds at 128kb bitrate.
#define RINGBFSIZ 20000
// Name of the ini file
#define INIFILENAME "/radio.ini"
// Access point name if connection to WiFi network fails.  Also the hostname for WiFi and OTA.
// Not that the password of an AP must be at least as long as 8 characters.
// Also used for other naming.
#define NAME "Esp-radio"
//
//******************************************************************************************
// Forward declaration of various functions                                                *
//******************************************************************************************
void showstreamtitle(const char *ml, bool full = false);
void handlebyte(uint8_t b, bool force = false);
void handlebyte_ch(uint8_t b, bool force = false);
void handleFS(AsyncWebServerRequest *request);
void handleFSf(AsyncWebServerRequest *request, const String &filename);
void handleCmd(AsyncWebServerRequest *request);
void handleFileUpload(AsyncWebServerRequest *request, String filename,
                      size_t index, uint8_t *data, size_t len, bool final);
char *analyzeCmd(const char *str);
char *analyzeCmd(const char *par, const char *val);
String chomp(String str);
void connectToHost(String &host);

//
//******************************************************************************************
// Global data section.                                                                    *
//******************************************************************************************
// There is a block ini-data that contains some configuration.  Configuration data is      *
// saved in the SPIFFS file radio.ini by the webinterface.  On restart the new data will   *
// be read from this file.                                                                 *
// Items in ini_block can be changed by commands from webserver/MQTT/Serial.               *
//******************************************************************************************
struct ini_struct {
  uint8_t reqvol;   // Requested volume
  uint8_t rtone[4]; // Requested bass/treble settings
  int8_t newpreset; // Requested preset
  String ssid;      // SSID of WiFi network to connect to
  String passwd;    // Password for WiFi network
};

enum datamode_t {
  INIT = 1,            // Initialize for header receive
  HEADER = 2,          // read mp3 header
  DATA = 4,            // read mp3/ogg data
  METADATA = 8,        // read metadata
  PLAYLISTINIT = 16,   // Initialize for playlist handling
  PLAYLISTHEADER = 32, // Read playlist header
  PLAYLISTDATA = 64,   // Read playlist data
  STOPREQD = 128,      // STOP requested
  STOPPED = 256        // Stopped
};                     // State for datastream

// Global variables
ini_struct ini_block;                           // Holds configurable data
WiFiClient *mp3client = NULL;                   // An instance of the mp3 client
AsyncWebServer cmdserver(80);                   // Instance of embedded webserver on port 80
// char cmd[130];                                  // Command from MQTT or Serial
Ticker tckr;                                    // For timing 100 msec
uint32_t totalcount = 0;                        // Counter mp3 data
datamode_t datamode;                            // State of datastream
int metacount;                                  // Number of bytes in metadata
int datacount;                                  // Counter databytes before metadata
String metaline;                                // Readable line in metadata
String icystreamtitle;                          // Streamtitle from metadata
String icyname;                                 // Icecast station name
int bitrate;                                    // Bitrate in kb/sec
int metaint = 0;                                // Number of databytes between metadata
int8_t currentpreset = -1;                      // Preset station playing
String host;                                    // The URL to connect to or file to play
String playlist;                                // The URL of the specified playlist
bool hostreq = false;                           // Request for new host
bool reqtone = false;                           // New tone setting requested
bool muteflag = false;                          // Mute output
uint8_t ringbuf[RINGBFSIZ];                     // Ringbuffer for VS1053
uint16_t ringBufferWorkingindex = 0;            // Fill pointer in ringbuffer
uint16_t ringBufferEmptyindex = RINGBFSIZ - 1;  // Emptypointer in ringbuffer
uint16_t rcount = 0;                            // Number of bytes in ringbuffer
uint16_t analogSwitch[NUMANA] = {asw1, asw2, asw3}; // 3 levels of analog input
uint16_t analogrest;                            // Rest value of analog input
bool resetRequest = false;                          // Request to reset the ESP8266
bool NetworkFound;                              // True if WiFi network connected
String networks;                                // Found networks
String acceptableNetworks;                               // Aceptable networks (present in .ini file)
String presetlist;                              // List for webserver
uint8_t acceptableNetworksCount;                                 // Number of acceptable networks in .ini file
String testfilename = "";                       // File to test (SPIFFS speed)
int8_t playlist_num = 0;                        // Nonzero for selection from playlist
File mp3file;                                   // File containing mp3 on SPIFFS
bool isLocalFile = false;                         // Play from local mp3-file or not
bool chunked = false;                           // Station provides chunked transfer
int chunkcount = 0;                             // Counter for chunked transfer

String stationServer(""); // Radio stream server
String stationPort("");   // Radio stream port
String stationMount("");  // Radio stream Callsign

//******************************************************************************************
// End of global data section.                                                             *
//******************************************************************************************

//******************************************************************************************
// Pages and CSS for the webinterface.                                                     *
//******************************************************************************************
#include "about_html.h"
#include "config_html.h"
#include "index_html.h"
#include "radio_css.h"
#include "favicon_ico.h"
//

// The object for the MP3 player
VS1053 vs1053player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

//******************************************************************************************
// Ringbuffer (fifo) routines.                                                             *
//******************************************************************************************
//******************************************************************************************
//                              R I N G S P A C E                                          *
//******************************************************************************************
inline bool ringspace() {
  return (rcount < RINGBFSIZ); // True if at least one byte of free space is available
}

//******************************************************************************************
//                              R I N G A V A I L                                          *
//******************************************************************************************
inline uint16_t ringavail() {
  return rcount; // Return number of bytes available
}

//******************************************************************************************
//                                P U T R I N G                                            *
//******************************************************************************************
void putring(uint8_t b) { // Put one byte in the ringbuffer
  // No check on available space.  See ringspace()
  *(ringbuf + ringBufferWorkingindex) = b;   // Put byte in ringbuffer
  if (++ringBufferWorkingindex == RINGBFSIZ) // Increment pointer and
    ringBufferWorkingindex = 0; // wrap at end
  rcount++; // Count number of bytes in the
}

//******************************************************************************************
//                                G E T R I N G                                            *
//******************************************************************************************
uint8_t getring() {
  // Assume there is always something in the bufferpace.  See ringavail()
  if (++ringBufferEmptyindex == RINGBFSIZ) // Increment pointer and
  {
    ringBufferEmptyindex = 0; // wrap at end
  }
  rcount--;                     // Count is now one less
  return *(ringbuf + ringBufferEmptyindex); // return the oldest byte
}

//******************************************************************************************
//                               E M P T Y R I N G                                         *
//******************************************************************************************
void emptyring() {
  ringBufferWorkingindex = 0; // Reset ringbuffer administration
  ringBufferEmptyindex = RINGBFSIZ - 1;
  rcount = 0;
}

//******************************************************************************************
//                             G E T E N C R Y P T I O N T Y P E                           *
//******************************************************************************************
// Read the encryption type of the network and return as a 4 byte name                     *
//*********************4********************************************************************
const char *getEncryptionType(int thisType) {
  switch (thisType) {
  case ENC_TYPE_WEP:
    return "WEP ";
  case ENC_TYPE_TKIP:
    return "WPA ";
  case ENC_TYPE_CCMP:
    return "WPA2";
  case ENC_TYPE_NONE:
    return "None";
  case ENC_TYPE_AUTO:
    return "Auto";
  }
  return "????";
}

//******************************************************************************************
//                                L I S T N E T W O R K S                                  *
//******************************************************************************************
// List the available networks and select the strongest.                                   *
// Acceptable networks are those who have a "SSID.pw" file in the SPIFFS.                  *
// SSIDs of available networks will be saved for use in webinterface.                      *
//******************************************************************************************
void listNetworksAndSelectBest() {
  int strongestSignal = -1000; // Used for searching strongest WiFi signal

  ini_block.ssid = "none"; // No selceted network yet
  // scan for nearby networks:
  dbgprint("* Scan Networks *");
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1) {
    dbgprint("Couldn't get a wifi connection");
    return;
  }
  // print the list of networks seen:
  dbgprint("Number of available networks: %d", numSsid);
  // Print the network number and name for each network found and
  // find the strongest acceptable network
  for (int i = 0; i < numSsid; i++) {
    const char *acceptable = "";                     // Assume not acceptable
    int currentStrength = WiFi.RSSI(i);          // Get the signal strenght
    if (acceptableNetworks.indexOf(WiFi.SSID(i) + "|") >= 0) { // Is this SSID acceptable?
      acceptable = "Acceptable";
      if (currentStrength > strongestSignal) {// This is a better Wifi
        strongestSignal = currentStrength;
        ini_block.ssid = WiFi.SSID(i); // Remember SSID name
      }
    }
    dbgprint("%2d - %-25s Signal: %3d dBm Encryption %4s  %s",
             i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
             getEncryptionType(WiFi.encryptionType(i)),
             acceptable);
    // Remember this network for later use
    networks += WiFi.SSID(i) + "|";
  }
  dbgprint("--------------------------------------");
}

bool inPlaylistMode() {
  return datamode & (PLAYLISTDATA | PLAYLISTINIT | PLAYLISTHEADER);
}
//******************************************************************************************
//                                  T I M E R 1 0 S E C                                    *
//******************************************************************************************
// Extra watchdog.  Called every 10 seconds.                                               *
// If totalcount has not been changed, there is a problem and playing will stop.           *
// Note that a "yield()" within this routine or in called functions will cause a crash!    *
//******************************************************************************************
void callback10seconds() {
  static uint32_t oldtotalcount = 7321; // Needed foor change detection
  static uint8_t failCounter = 0;      // Counter for succesive fails

  if (is_playing()) { // Still playing?
    if (totalcount == oldtotalcount) {
      dbgprint("No data input"); // No data detected!
      if (failCounter > 10) {   // Happened too many times?
        ESP.restart(); // Reset the CPU, probably no return
      }
      if (inPlaylistMode()) {
        playlist_num = 0; // Yes, end of playlist
      }
      if ((failCounter > 0) || (playlist_num > 0)) { // Happened more than once or playlist active?
        datamode = STOPREQD;   // Stop player
        ini_block.newpreset++; // Yes, try next channel
        dbgprint("Trying other station/file...");
      }
      failCounter++; // Count the fails
    } else {
      if (failCounter) { // Recovered from data loss?
        dbgprint("Recovered from dataloss");
        failCounter = 0; // Data see, reset failcounter
      }
      oldtotalcount = totalcount; // Save for comparison in next cycle
    }
  }
}

//******************************************************************************************
//                                  A N A G E T S W                                        *
//******************************************************************************************
// Translate analog input to switch number.  0 is inactive.                                *
// Note that it is adviced to avoid expressions as the argument for the abs function.      *
//******************************************************************************************
uint8_t getAnalogSwitch(uint16_t currentValue) {
  int smallestAnalogValue = 1000; // Detection least difference
  uint8_t sw = 0;        // Number of switch detected (0 or 1..3)

  if (currentValue > analogrest) { // Inactive level?
    for (int i = 0; i < NUMANA; i++) {
      int newValue = abs(analogSwitch[i] - currentValue); // Compute difference
      if (newValue < smallestAnalogValue) { // New least difference?
        smallestAnalogValue = newValue; // Yes, remember
        sw = i + 1;           // Remember switch
      }
    }
  }
  return sw; // Return active switch
}

//******************************************************************************************
//                               T E S T F I L E                                           *
//******************************************************************************************
// Test the performance of SPIFFS read.                                                    *
//******************************************************************************************
void testSPIFFSPerformance(String fileToTest) {
  File testFile;            // File containing mp3
  uint32_t len, savlen;  // File length
  uint32_t testStartTime, fileReadStartTime, lastTimeStamp; // For time test
  uint32_t slowReadsCount = 0;  // Number of slow reads

  dbgprint("Start test of file %s", fileToTest.c_str());
  testStartTime = millis();                  // Timestamp at start
  lastTimeStamp = testStartTime;                      // For report
  testFile = SPIFFS.open("/" + fileToTest, "r"); // Open the file
  if (testFile) {
    len = testFile.available(); // Get file length
    savlen = len;            // Save for result print
    while (len--) {          // Any data left?
      fileReadStartTime = millis();           // To meassure read time
      testFile.read();            // Read one byte
      if ((millis() - fileReadStartTime) > 5) { // Read took more than 5 msec?
        slowReadsCount++; // Yes, count slow reads
      }
      if ((len % 100) == 0) { // Yield reguarly
        yield();
      }
      if (((fileReadStartTime - lastTimeStamp) / 1000) > 0 || len == 0) {
        // Show results for debug
        dbgprint("Read %s, length %d/%d took %d seconds, %d slow reads",
                 fileToTest.c_str(), savlen - len, savlen, (fileReadStartTime - testStartTime) / 1000, slowReadsCount);
        lastTimeStamp = fileReadStartTime;
      }
      if ((fileReadStartTime - testStartTime) > 100000) { // Give up after 100 seconds
        dbgprint("Give up...");
        break;
      }
    }
    testFile.close();
    dbgprint("EOF"); // End of file
  }
}

void handleAnalogInput() {
  static uint8_t lastActiveAnalogSwitch = 0;

  uint8_t currentAnalogSwitch = getAnalogSwitch(analogRead(A0));  // Check analog value for program switches
    if (currentAnalogSwitch != lastActiveAnalogSwitch) { // Change?
      lastActiveAnalogSwitch = currentAnalogSwitch; // Remember value for change detection
      switch (currentAnalogSwitch) {
      case 1:
        ini_block.newpreset = 0; // Yes, goto first preset
        break;
      case 2:
        ini_block.newpreset = currentpreset + 1; // Yes, goto next preset
        break;
      case 3:
        ini_block.newpreset = currentpreset - 1; // Yes, goto previous preset
        break;
      default:
        break;
      }
    }
}
//******************************************************************************************
//                                  T I M E R 1 0 0                                        *
//******************************************************************************************
// Examine button every 100 msec.                                                          *
//******************************************************************************************
void callback100miliseconds() {
  static int iterationCounter = 0; // Counter for activatie 10 seconds process

  if (++iterationCounter == 100) { // 10 seconds passed?
    callback10seconds();   // Yes, do 10 second procedure
    iterationCounter = 0; // Reset count
  } else {
    handleAnalogInput();
  }
}

//******************************************************************************************
//                              D I S P L A Y V O L U M E                                  *
//******************************************************************************************
// Show the current volume as an indicator on the screen.                                  *
//******************************************************************************************
void displayvolume() {

}

//******************************************************************************************
//                        S H O W S T R E A M T I T L E                                    *
//******************************************************************************************
// Show artist and songtitle if present in metadata.                                       *
// Show always if full=true.                                                               *
//******************************************************************************************
void showstreamtitle(const char *ml, bool full)
{
  char *p1;
  char *p2;
  char streamtitle[150]; // Streamtitle from metadata

  if (strstr(ml, "StreamTitle=")) {
    dbgprint("Streamtitle found, %d bytes", strlen(ml));
    dbgprint(ml);
    p1 = (char *)ml + 12;       // Begin of artist and title
    if ((p2 = (char*)strstr(ml, ";"))) // Search for end of title
    {
      if (*p1 == '\'') // Surrounded by quotes?
      {
        p1++;
        p2--;
      }
      *p2 = '\0'; // Strip the rest of the line
    }
    // Save last part of string as streamtitle.  Protect against buffer overflow
    strncpy(streamtitle, p1, sizeof(streamtitle));
    streamtitle[sizeof(streamtitle) - 1] = '\0';
  }
  else if (full) {
    // Info probably from playlist
    strncpy(streamtitle, ml, sizeof(streamtitle));
    streamtitle[sizeof(streamtitle) - 1] = '\0';
  } else {
    icystreamtitle = ""; // Unknown type
    return;              // Do not show
  }
  // Save for status request from browser ;
  icystreamtitle = streamtitle;
  if ((p1 = strstr(streamtitle, " - "))) // look for artist/title separator
  {
    char *artist = streamtitle;
    *p1 = '\0';
    char *song = p1 + 3;
    showNowPlayingInfo(artist, song);
  }
  else
  {
    showNowPlayingInfo(streamtitle, "");
  }
}

//******************************************************************************************
//                            S T O P _ M P 3 C L I E N T                                  *
//******************************************************************************************
// Disconnect from the server.                                                             *
//******************************************************************************************
void stopMp3Client()
{
  if (mp3client)
  {
    if (mp3client->connected()) // Need to stop client?
    {
      dbgprint("Stopping client"); // Stop connection to host
      mp3client->flush();
      mp3client->stop();
      delay(500);
    }
    delete (mp3client);
    mp3client = NULL;
  }
}

void removeProtocolFromHost(String &host) {
  if (host.startsWith("http://"))
    host = host.substring(7);
  else if (host.startsWith("https://"))
    host = host.substring(8);
}

int removePortFromHost(String &host) {
  int port = 80;
  int inx = host.indexOf(":");
  if (inx >= 0) {
    int slash = host.indexOf('/');
    if (slash == -1) {
      port = host.substring(inx + 1).toInt(); // Get portnumber as integer
      host = host.substring(0, inx);
    } else {
      port = host.substring(inx + 1, slash).toInt();
      host = host.substring(0, inx) + host.substring(slash);
    }
  }
  return port;
}

String removeExtensionFromHost(String &host) {
  String extension = "/";
  int inx = host.indexOf("/"); // Search for begin of extension
  if (inx > 0) {           // Is there an extension?
    extension = host.substring(inx);    // Yes, change the default
    host = host.substring(0, inx); // Host without extension
  }
  return extension;
}

bool startMp3Client(String &host, int port, String &extension) {
  mp3client = new WiFiClient();
  if (mp3client->connect(host.c_str(), port)) {
    // This will send the request to the server. Request metadata.
    mp3client->print(String("GET ") +
                     extension +
                     String(" HTTP/1.1\r\n") +
                     String("Host: ") +
                     host +
                     String("\r\n") +
                     String("Icy-MetaData:1\r\n") +
                     String("Connection: close\r\n\r\n"));
    return true;
  }
  return false;
}

//******************************************************************************************
//                            C O N N E C T T O H O S T                                    *

//******************************************************************************************
// Connect to the Internet radio server specified by newpreset.                            *
//******************************************************************************************
void connectToHost(String &host) {
  int inx;                // Position of ":" in hostname
  int port = 80;          // Port number for host
  String extension = "/"; // May be like "/mp3" in "skonto.ls.lv:8002/mp3"

  stopMp3Client(); // Disconnect if still connected
  dbgprint("Connect to new host %s", host.c_str());
  displayDebug("** Internet radio **");

  datamode = INIT;           // Start default in metamode
  chunked = false;           // Assume not chunked

  if (host.endsWith(".m3u")) { // Is it an m3u playlist?
    playlist = host;         // Save copy of playlist URL
    datamode = PLAYLISTINIT; // Yes, start in PLAYLIST mode
    if (playlist_num == 0) { // First entry to play?
      playlist_num = 1; // Yes, set index
    }
    dbgprint("Playlist request, entry %d", playlist_num);
  }

  removeProtocolFromHost(host);
  extension = removeExtensionFromHost(host);
  port = removePortFromHost(host);

  displayDebug(dbgprint("Connect to %s on port %d, extension %s",
               host.c_str(), port, extension.c_str()));
  if (startMp3Client(host, port, extension)) {
    dbgprint("Connected to server");
  } else {
    dbgprint("Request failed!");
  }
}

//******************************************************************************************
//                               C O N N E C T T O F I L E                                 *
//******************************************************************************************
// Open the local mp3-file.                                                                *
//******************************************************************************************
void connectToFile(String &host) {
  String path; // Full file spec

  displayDebug("**** MP3 Player ****");
  path = host.substring(9);         // Path, skip the "localhost" part
  mp3file = SPIFFS.open(path, "r"); // Open the file
  if (!mp3file) {
    dbgprint("Error opening file %s", path.c_str()); // No luck
    return;
  }
  showstreamtitle(path.c_str() + 1, true); // Show the filename as title
  displayDebug("Playing from local file"); // Show Source at position 60
  icyname = "";                            // No icy name yet
  chunked = false;                         // File not chunked
  datamode = DATA;
}

//******************************************************************************************
//                               C O N N E C T W I F I                                     *
//******************************************************************************************
// Connect to WiFi using passwords available in the SPIFFS.                                *
// If connection fails, an AP is created and the function returns false.                   *
//******************************************************************************************
bool connectToWiFi(const char *ssid, const char *password) {
  WiFi.disconnect();           // After restart the router could
  WiFi.softAPdisconnect(true); // still keep the old connection
  WiFi.begin(ssid, password);  // Connect to selected SSID
  displayDebug(dbgprint("Try WiFi %s", ini_block.ssid.c_str())); // Message to show during WiFi connect

  if (WiFi.waitForConnectResult() != WL_CONNECTED) { // Try to connect
    dbgprint("WiFi Failed!");
    WiFi.softAP(NAME, NAME); // This ESP will be an AP
    delay(2000);
    displayDebug(dbgprint("AP: %s, pass: %s", NAME, NAME));
    delay(3000);
    displayDebug(dbgprint("IP = 192.168.4.1")); // Address if AP
    return false;
  }
  displayDebug(dbgprint("IP = %s", WiFi.localIP().toString().c_str()));
  return true;
}

//******************************************************************************************
//                                   O T A S T A R T                                       *
//******************************************************************************************
// Update via WiFi has been started by Arduino IDE.                                        *
//******************************************************************************************
void otastart() {
  dbgprint("OTA Started");
}

//******************************************************************************************
//                          R E A D H O S T F R O M I N I F I L E                          *
//******************************************************************************************
// Read the mp3 host from the ini-file specified by the parameter.                         *
// The host will be returned.                                                              *
//******************************************************************************************
String getPresetFromIniFile(int8_t preset) {
  File inifile;    // File containing URL with mp3
  char tkey[10];   // Key as an array of chars
  String res = ""; // Assume not found

  inifile = SPIFFS.open(INIFILENAME, "r"); // Open the file
  if (inifile) {
    sprintf(tkey, "preset_%02d", preset); // Form the search key
    while (inifile.available()) {
      String currentLine = inifile.readStringUntil('\n'); // Read next line
      String lowerCaseLine = currentLine;                        // Copy for lowercase
      lowerCaseLine.toLowerCase();                 // Set to lowercase
      if (lowerCaseLine.startsWith(tkey)) {        // Found the key?
        int inx = currentLine.indexOf("="); // Get position of "="
        if (inx > 0) {           // Equal sign present?
          currentLine.remove(0, inx + 1); // Yes, remove key
          res = chomp(currentLine);       // Remove garbage
          break;                   // End the while loop
        }
      }
    }
    inifile.close(); // Close the file
  } else {
    dbgprint("File %s not found, please create one!", INIFILENAME);
  }
  return res;
}

//******************************************************************************************
//                               R E A D I N I F I L E                                     *
//******************************************************************************************
// Read the .ini file and interpret the commands.                                          *
//******************************************************************************************
void parseIniFile() {
  File inifile; // File containing URL with mp3

  inifile = SPIFFS.open(INIFILENAME, "r"); // Open the file
  if (inifile) {
    while (inifile.available()) {
      analyzeCmd(inifile.readStringUntil('\n').c_str());
    }
    inifile.close(); // Close the file
  } else {
    dbgprint("File %s not found, use save command to create one!", INIFILENAME);
  }
}

//******************************************************************************************
//                             S C A N S E R I A L                                         *
//******************************************************************************************
// Listen to commands on the Serial inputline.                                             *
//******************************************************************************************
void parseSerialCommands() {
  static String serialcmd; // Command from Serial input

  while (Serial.available()) { // Any input seen?
    char c = (char)Serial.read(); // Yes, read the next input character
    if ((c == '\n') || (c == '\r')) {
      if (!serialcmd.isEmpty()) {
        dbgprint(analyzeCmd(serialcmd.c_str()));
        serialcmd = "";
      }
    }
    if (c >= ' ') { // Only accept useful characters
      serialcmd += c; // Add to the command
    }
    if (serialcmd.length() >= 165) { // Check for excessive length
      serialcmd = ""; // Too long, reset
    }
  }
}

//******************************************************************************************
//                                   M K _ L S A N                                         *
//******************************************************************************************
// Make a list of acceptable networks in .ini file.                                        *
// The result will be stored in acceptableNetworks like "|SSID1|SSID2|......|SSIDN|".      *
// The number of acceptable networks will be stored in acceptableNetworksCount.            *
//******************************************************************************************
void populateAcceptableNetworks() {
  File inifile; // File containing URL with mp3
  String line;  // Input line from .ini file
  String ssid;  // SSID in line
  int inx;      // Place of "/"

  acceptableNetworksCount = 0;             // Count acceptable networks
  acceptableNetworks = "|";                // Initial value
  inifile = SPIFFS.open(INIFILENAME, "r"); // Open the file
  if (inifile) {
    while (inifile.available()) {
      line = inifile.readStringUntil('\n'); // Read next line
      ssid = line;                          // Copy holds original upper/lower case
      line.toLowerCase();                   // Case insensitive
      if (line.startsWith("wifi")) {        // Line with WiFi spec?
        inx = line.indexOf("/"); // Find separator between ssid and password
        if (inx > 0) {           // Separator found?
          ssid = ssid.substring(5, inx); // Line holds SSID now
          dbgprint("Added SSID %s to acceptable networks",
                   ssid.c_str());
          acceptableNetworks += ssid; // Add to list
          acceptableNetworks += "|";  // Separator
          acceptableNetworksCount++;          // Count number oif acceptable networks
        }
      }
    }
    inifile.close(); // Close the file
  }
  else
  {
    dbgprint("File %s not found!", INIFILENAME); // No .ini file
  }
}

//******************************************************************************************
//                             G E T P R E S E T S                                         *
//******************************************************************************************
// Make a list of all preset stations.                                                     *
// The result will be stored in the String presetlist (global data).                       *
//******************************************************************************************
void getpresets()
{
  String path;  // Full file spec as string
  File inifile; // File containing URL with mp3
  String line;  // Input line from .ini file
  int inx;      // Position of search char in line
  int i;        // Loop control
  char vnr[3];  // 2 digit presetnumber as string

  presetlist = String("");          // No result yet
  path = String(INIFILENAME);       // Form full path
  inifile = SPIFFS.open(path, "r"); // Open the file
  if (inifile)
  {
    while (inifile.available())
    {
      line = inifile.readStringUntil('\n'); // Read next line
      if (line.startsWith("preset_"))       // Found the key?
      {
        i = line.substring(7, 9).toInt(); // Get index 00..99
        // Show just comment if available.  Otherwise the preset itself.
        inx = line.indexOf("#"); // Get position of "#"
        if (inx > 0)             // Hash sign present?
        {
          line.remove(0, inx + 1); // Yes, remove non-comment part
        }
        else
        {
          inx = line.indexOf("="); // Get position of "="
          if (inx > 0)             // Equal sign present?
          {
            line.remove(0, inx + 1); // Yes, remove first part of line
          }
        }
        line = chomp(line);                 // Remove garbage from description
        sprintf(vnr, "%02d", i);            // Preset number
        presetlist += (String(vnr) + line + // 2 digits plus description
                       String("|"));
      }
    }
    inifile.close(); // Close the file
  }
}

void setup_SPIFFS()
{
  FSInfo fs_info;  // Info about SPIFFS
  Dir dir;         // Directory struct for SPIFFS
  File f;          // Filehandle
  String filename; // Name of file found in SPIFFS

  SPIFFS.begin(); // Enable file system
  // Show some info about the SPIFFS
  SPIFFS.info(fs_info);
  dbgprint("FS Total %d, used %d", fs_info.totalBytes, fs_info.usedBytes);
  if (fs_info.totalBytes == 0)
  {
    dbgprint("No SPIFFS found!  See documentation.");
  }
  dir = SPIFFS.openDir("/"); // Show files in FS
  while (dir.next())         // All files
  {
    f = dir.openFile("r");
    filename = dir.fileName();
    dbgprint("%-32s - %7d", // Show name and size
             filename.c_str(), f.size());
  }
}

void setup_wifi()
{
  WiFi.setPhyMode(WIFI_PHY_MODE_11N); // Force 802.11N connection
  WiFi.persistent(false);             // Do not save SSID and password
}
//******************************************************************************************
//                                   S E T U P                                             *
//******************************************************************************************
// Setup for the program.                                                                  *
//******************************************************************************************
void setup()
{

  Serial.begin(115200); // For debug
  setupDisplay();

  Serial.println();
  system_update_cpu_freq(160);              // Set to 80/160 MHz
  memset(&ini_block, 0, sizeof(ini_block)); // Init ini_block

  setup_SPIFFS();

  populateAcceptableNetworks();      // Make a list of acceptable networks in ini file.
  listNetworksAndSelectBest(); // Search for WiFi networks
  parseIniFile();  // Read .ini file
  getpresets();   // Get the presets from .ini-file
  setup_wifi();
  SPI.begin(); // Init SPI bus
  // Print some memory and sketch info
  dbgprint("Starting ESP Version %s...  Free memory %d",
           VERSION,
           system_get_free_heap_size());
  dbgprint("Sketch size %d, free size %d",
           ESP.getSketchSize(),
           ESP.getFreeSketchSpace());
  vs1053player.begin(); // Initialize VS1053 player
  delay(10);
  tckr.attach(0.100, callback100miliseconds); // Every 100 msec
  dbgprint("Selected network: %-25s", ini_block.ssid.c_str());
  NetworkFound = connectToWiFi(ini_block.ssid.c_str(), ini_block.passwd.c_str()); // Connect to WiFi network
  dbgprint("Start server for commands");
  cmdserver.on("/", handleCmd);             // Handle startpage
  cmdserver.onNotFound(handleFS);           // Handle file from FS
  cmdserver.onFileUpload(handleFileUpload); // Handle file uploads
  cmdserver.begin();
  if (NetworkFound) // OTA and MQTT only if Wifi network found
  {
    ArduinoOTA.setHostname(NAME); // Set the hostname
    ArduinoOTA.onStart(otastart);
    ArduinoOTA.begin(); // Allow update over the air
  }
  else
  {
    currentpreset = ini_block.newpreset; // No network: do not start radio
  }
  delay(1000);                              // Show IP for a while
  analogrest = (analogRead(A0) + asw1) / 2; // Assumed inactive analog input
}

bool is_playing()
{
  return datamode & ~(STOPPED | STOPREQD);
}

void stop_playback()
{
  dbgprint("STOP requested");
  if (isLocalFile)
  {
    mp3file.close();
  }
  else
  {
    stopMp3Client(); // Disconnect if still connected
  }
  handlebyte_ch(0, true);    // Force flush of buffer
  vs1053player.setVolume(0); // Mute
  vs1053player.stopSong();   // Stop playing
  emptyring();               // Empty the ringbuffer
  datamode = STOPPED;        // Yes, state becomes STOPPED
  yield();
  delay(500);
  yield();
}

void feed_ring_buffer()
{
  uint32_t maxfilechunk; // bytes to read from stream (clamped to 1024)

  if (isLocalFile)
  {
    maxfilechunk = mp3file.available(); // Bytes left in file
    if (maxfilechunk > 1024)
      maxfilechunk = 1024;

    while (ringspace() && maxfilechunk--)
    {
      putring(mp3file.read()); // Yes, store one byte in ringbuffer
      yield();
    }
  }
  else
  {
    maxfilechunk = mp3client->available(); // Bytes available from mp3 server
    if (maxfilechunk > 1024)
      maxfilechunk = 1024;

    while (ringspace() && maxfilechunk--)
    {
      putring(mp3client->read()); // Yes, store one byte in ringbuffer
      yield();
    }
  }
}

bool local_playback_ended()
{
  return (isLocalFile) && (is_playing()) && ((mp3file.available() == 0) && (ringavail() == 0));
}
//******************************************************************************************
//                                   L O O P                                               *
//******************************************************************************************
// Main loop of the program.  Minimal time is 20 usec.  Will take about 4 msec if VS1053   *
// needs data.                                                                             *
// Sometimes the loop is called after an interval of more than 100 msec.                   *
// In that case we will not be able to fill the internal VS1053-fifo in time (especially   *
// at high bitrate).                                                                       *
// A connection to an MP3 server is active and we are ready to receive data.               *
// Normally there is about 2 to 4 kB available in the data stream.  This depends on the    *
// sender.                                                                                 *
//******************************************************************************************
void loop()
{
  // stream or file

  // Try to keep the ringbuffer filled up by adding as much bytes as possible
  if (is_playing())
  {
    feed_ring_buffer();
    yield();
  }

  while (vs1053player.data_request() && ringavail())
  {                           // Try to keep VS1053 filled
    handlebyte_ch(getring()); // Yes, handle it
  }
  yield();
  if (datamode == STOPREQD)
    stop_playback();

  if (local_playback_ended())
    datamode = STOPREQD;

  if (ini_block.newpreset != currentpreset)
  { // New station or next from playlist requested?
    if (datamode != STOPPED)
    {
      datamode = STOPREQD;
    }
    else
    {
      if (playlist_num)
      { // Playing from playlist?
        // Yes, retrieve URL of playlist
        dbgprint("from playlist");
        playlist_num += ini_block.newpreset -
                        currentpreset;       // Next entry in playlist
        ini_block.newpreset = currentpreset; // Stay at current preset
      }
      else
      {
        if (ini_block.newpreset <= 0)
          ini_block.newpreset = 1;
        host = getPresetFromIniFile(ini_block.newpreset); // Lookup preset in ini-file
      }
      dbgprint("New preset/file requested (%d/%d) from %s",
               currentpreset, playlist_num, host.c_str());
      if (host != "")
      {                 // Preset in ini-file?
        hostreq = true; // Force this station as new preset
      }
      else
      {
        // This preset is not available, return to preset 0, will be handled in next loop()
        ini_block.newpreset = 0; // Wrap to first station
      }
    }
  }
  if (hostreq) { // New preset or station?
    hostreq = false;
    currentpreset = ini_block.newpreset; // Remember current preset

    isLocalFile = host.startsWith("localhost/"); // Find out if this URL is on localhost
    if (isLocalFile) { // Play file from localhost?
      connectToFile(host);
    } else {
      connectToHost(host); // Switch to new host
      yield();
    }
  }
  if (reqtone)
  { // Request to change tone?
    reqtone = false;
    vs1053player.setTone(ini_block.rtone); // Set SCI_BASS to requested value
  }
  if (resetRequest)
  { // Reset requested?
    dbgprint("#####RESET#####");
    delay(1000);   // Yes, wait some time
    ESP.restart(); // Reboot
  }
  if (muteflag)
  {
    vs1053player.setVolume(0); // Mute
  }
  else
  {
    vs1053player.setVolume(ini_block.reqvol); // Unmute
  }
  displayvolume(); // Show volume on display
  if (testfilename.length())
  {                         // File to test?
    testSPIFFSPerformance(testfilename); // Yes, do the test
    testfilename = "";      // Clear test request
  }
  parseSerialCommands();        // Handle serial input
  ArduinoOTA.handle(); // Check for OTA
}

//******************************************************************************************
//                            C H K H D R L I N E                                          *
//******************************************************************************************
// Check if a line in the header is a reasonable headerline.                               *
// Normally it should contain something like "icy-xxxx:abcdef".                            *
//******************************************************************************************
bool is_reasonable_header_line(const char *str)
{
  char b;      // Byte examined
  int len = 0; // Lengte van de string

  while ((b = *str++))
  {        // Search to end of string
    len++; // Update string length
    if (b == ':')
    {                                   // Found a colon?
      return ((len > 5) && (len < 50)); // Yes, okay if length is okay
    }
    if (!isalpha(b) && (b != '-'))
    {
      return false; // Not a legal character
    }
  }
  return false; // End of string without colon
}

bool in_data_mode()
{
  return datamode & (DATA | METADATA | PLAYLISTDATA);
}

uint8_t hex_byte_to_int(uint8_t b)
{
  uint8_t ret = toupper(b) - '0'; // Be sure we have uppercase
  if (ret > 9)
    ret -= 7; // Translate A..F to 10..15
  return ret;
}
//******************************************************************************************
//                           H A N D L E B Y T E _ C H                                     *
//******************************************************************************************
// Handle the next byte of data from server.                                               *
// Chunked transfer encoding aware. Chunk extensions are not supported.                    *
//******************************************************************************************
void handlebyte_ch(uint8_t b, bool force)
{
  static int chunksize = 0; // Chunkcount read from stream

  if (chunked && !force && in_data_mode())
  {
    if (chunkcount == 0)
    {
      if (b == '\r')
      {
        return;
      }
      else if (b == '\n')
      {
        chunkcount = chunksize; // Yes, set new count
        chunksize = 0;          // For next decode
        return;
      }
      // We have received a hexadecimal character.  Decode it and add to the result.
      b = hex_byte_to_int(b);
      chunksize = (chunksize << 4) + b;
    }
    else
    {
      handlebyte(b, force); // Normal data byte
      chunkcount--;         // Update count to next chunksize block
    }
  }
  else
  {
    handlebyte(b, force); // Normal handling of this byte
  }
}

bool should_ignore_header_character(char c)
{
  return (c > 0x7F) || (c == '\r') || (c == '\0');
}
//******************************************************************************************
//                           H A N D L E B Y T E                                           *
//******************************************************************************************
// Handle the next byte of data from server.                                               *
// This byte will be send to the VS1053 most of the time.                                  *
// Note that the buffer the data chunk must start at an address that is a muttiple of 4.   *
// Set force to true if chunkbuffer must be flushed.                                       *
//******************************************************************************************
void handlebyte(uint8_t b, bool force)
{
  static uint16_t playlistcnt;                        // Counter to find right entry in playlist
  static bool firstmetabyte;                          // True if first metabyte (counter)
  static int lineFeedCount;                           // Detection of end of header
  static __attribute__((aligned(4))) uint8_t buf[32]; // Buffer for chunk
  static int bufcnt = 0;                              // Data in chunk
  static bool firstchunk = true;                      // First chunk as input
  String lowerCaseMetaline;                           // Lower case metaline
  String contentType;                                 // Contents type
  static bool foundContentType = false;               // First line of header seen or not
  int inx;                                            // Pointer in metaline
  int i;                                              // Loop control

  switch (datamode)
  {
  case INIT:
  {
    foundContentType = false; // Contents type not seen yet
    metaint = 0;              // No metaint found
    lineFeedCount = 0;        // For detection end of header
    bitrate = 0;              // Bitrate still unknown
    dbgprint("Switch to HEADER");
    datamode = HEADER; // Handle header
    totalcount = 0;    // Reset totalcount
    metaline = "";     // No metadata yet
    firstchunk = true; // First chunk expected
    break;
  }

  case HEADER:
  {
    if (should_ignore_header_character(b))
    {
      break;
    }
    else if (b == '\n')
    {                  // Linefeed ?
      lineFeedCount++; // Count linefeeds
      if (is_reasonable_header_line(metaline.c_str()))
      {
        lowerCaseMetaline = metaline; // Use lower case for compare
        lowerCaseMetaline.toLowerCase();
        dbgprint(metaline.c_str()); // Yes, Show it
        if (lowerCaseMetaline.indexOf("content-type") >= 0)
        {                                       // Line with "Content-Type: xxxx/yyy"
          foundContentType = true;              // Yes, remember seeing this
          contentType = metaline.substring(14); // Set contentstype. Not used yet
          showContentType(contentType.c_str());
          dbgprint("%s seen.", contentType.c_str());
        }
        if (lowerCaseMetaline.startsWith("icy-br:"))
        {
          bitrate = metaline.substring(7).toInt(); // Found bitrate tag, read the bitrate
          if (bitrate == 0)                        // For Ogg br is like "Quality 2"
          {
            bitrate = 87; // Dummy bitrate
          }
        }
        else if (lowerCaseMetaline.startsWith("icy-metaint:"))
        {
          metaint = metaline.substring(12).toInt(); // Found metaint tag, read the value
        }
        else if (lowerCaseMetaline.startsWith("icy-name:"))
        {
          icyname = metaline.substring(9); // Get station name
          icyname.trim();                  // Remove leading and trailing spaces
        }
        else if (lowerCaseMetaline.startsWith("transfer-encoding:"))
        {
          // Station provides chunked transfer
          if (lowerCaseMetaline.endsWith("chunked"))
          {
            chunked = true; // Remember chunked transfer mode
            chunkcount = 0; // Expect chunkcount in DATA
          }
        }
        else if (lowerCaseMetaline.startsWith("icy-description:"))
        {
          showStationName(metaline.substring(16).c_str());
        }
      }
      metaline = ""; // Reset this line
      if ((lineFeedCount == 2) && foundContentType)
      {                                          // Some data seen and a double LF?
        dbgprint("Switch to DATA, bitrate is %d" // Show bitrate
                 ", metaint is %d",              // and metaint
                 bitrate, metaint);
        datamode = DATA;          // Expecting data now
        datacount = metaint;      // Number of bytes before first metadata
        bufcnt = 0;               // Reset buffer count
        vs1053player.startSong(); // Start a new song
      }
    }
    else
    {
      metaline += (char)b; // Normal character, put new char in metaline
      lineFeedCount = 0;   // Reset double CRLF detection
    }
    break;
  }

  case METADATA:
  {
    if (firstmetabyte)
    {                         // First byte of metadata?
      firstmetabyte = false;  // Not the first anymore
      metacount = b * 16 + 1; // New count for metadata including length byte
      if (metacount > 1)
      {
        dbgprint("Metadata block %d bytes",
                 metacount - 1); // Most of the time there are zero bytes of metadata
      }
      metaline = ""; // Set to empty
    }
    else
    {
      metaline += (char)b; // Normal character, put new char in metaline
    }
    if (--metacount == 0)
    {
      if (metaline.length())
      { // Any info present?
        // metaline contains artist and song name.  For example:
        // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
        // Sometimes it is just other info like:
        // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
        // Isolate the StreamTitle, remove leading and trailing quotes if present.
        showstreamtitle(metaline.c_str()); // Show artist and title if present in metadata
      }
      if (metaline.length() > 1500)
      { // Unlikely metaline length?
        dbgprint("Metadata block too long! Skipping all Metadata from now on.");
        metaint = 0;   // Probably no metadata
        metaline = ""; // Do not waste memory on this
      }
      datacount = metaint; // Reset data count
      bufcnt = 0;          // Reset buffer count
      datamode = DATA;     // Expecting data
    }
    break;
  }

  case DATA:
  {
    buf[bufcnt++] = b; // Save byte in chunkbuffer
    if (bufcnt == sizeof(buf) || force)
    { // Buffer full?
      if (firstchunk)
      {
        firstchunk = false;
        dbgprint("First chunk:"); // Header for printout of first chunk
        for (i = 0; i < 32; i += 8)
        { // Print 4 lines
          dbgprint("%02X %02X %02X %02X %02X %02X %02X %02X",
                   buf[i + 0], buf[i + 1], buf[i + 2], buf[i + 3],
                   buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7]);
        }
      }
      vs1053player.playChunk(buf, bufcnt); // Yes, send to player
      bufcnt = 0;                          // Reset count
    }
    totalcount++; // Count number of bytes, ignore overflow
    if (metaint != 0)
    { // No METADATA on Ogg streams or mp3 files
      if (--datacount == 0)
      { // End of datablock?
        if (bufcnt)
        {                                      // Yes, still data in buffer?
          vs1053player.playChunk(buf, bufcnt); // Yes, send to player
          bufcnt = 0;                          // Reset count
        }
        datamode = METADATA;
        firstmetabyte = true; // Expecting first metabyte (counter)
      }
    }
    break;
  }

  case PLAYLISTINIT:
  {
    // We are going to use metadata to read the lines from the .m3u file
    metaline = "";             // Prepare for new line
    lineFeedCount = 0;         // For detection end of header
    datamode = PLAYLISTHEADER; // Handle playlist data
    playlistcnt = 1;           // Reset for compare
    totalcount = 0;            // Reset totalcount
    dbgprint("Read from playlist");
    break;
  }

  case PLAYLISTHEADER:
  {
    if (should_ignore_header_character(b))
    {
      break;
    }
    else if (b == '\n')
    {                                // Linefeed ?
      lineFeedCount++;               // Count linefeeds
      dbgprint("Playlistheader: %s", // Show playlistheader
               metaline.c_str());
      metaline = ""; // Ready for next line
      if (lineFeedCount == 2)
      {
        dbgprint("Switch to PLAYLISTDATA");
        datamode = PLAYLISTDATA; // Expecting data now
        break;
      }
    }
    else
    {
      metaline += (char)b; // Normal character, put new char in metaline
      lineFeedCount = 0;   // Reset double CRLF detection
    }
    break;
  }

  case PLAYLISTDATA:
  {
    if (should_ignore_header_character(b))
    {
      break;
    }
    else if (b == '\n')
    {                              // Linefeed ?
      dbgprint("Playlistdata: %s", // Show playlistheader
               metaline.c_str());
      if (metaline.length() < 5)
      { // Skip short lines
        break;
      }
      if (metaline.indexOf("#EXTINF:") >= 0)
      { // Info?
        if (playlist_num == playlistcnt)
        {                              // Info for this entry?
          inx = metaline.indexOf(","); // Comma in this line?
          if (inx > 0)
          {
            // Show artist and title if present in metadata
            showstreamtitle(metaline.substring(inx + 1).c_str(), true);
          }
        }
      }
      if (metaline.startsWith("#"))
      { // Commentline?
        metaline = "";
        break; // Ignore commentlines
      }
      // Now we have an URL for a .mp3 file or stream.  Is it the rigth one?
      dbgprint("Entry %d in playlist found: %s", playlistcnt, metaline.c_str());
      if (playlist_num == playlistcnt)
      {
        host = metaline; // Yes, set new host
        connectToHost(host); // Connect to it
      }
      metaline = "";
      host = playlist; // Back to the .m3u host
      playlistcnt++;   // Next entry in playlist
    }
    else
    {
      metaline += (char)b; // Normal character, add it to metaline
    }
    break;
  }
  }
}

//******************************************************************************************
//                             G E T C O N T E N T T Y P E                                 *
//******************************************************************************************
// Returns the contenttype of a file to send.                                              *
//******************************************************************************************
String getContentType(String filename)
{
  if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  else if (filename.endsWith(".mp3"))
    return "audio/mpeg";
  else if (filename.endsWith(".pw"))
    return ""; // Passwords are secret
  return "text/plain";
}

//******************************************************************************************
//                         H A N D L E F I L E U P L O A D                                 *
//******************************************************************************************
// Handling of upload request.  Write file to SPIFFS.                                      *
//******************************************************************************************
void handleFileUpload(AsyncWebServerRequest *request, String filename,
                      size_t index, uint8_t *data, size_t len, bool final)
{
  String path;                 // Filename including "/"
  static File f;               // File handle output file
  char *reply;                 // Reply for webserver
  static uint32_t t;           // Timer for progress messages
  uint32_t t1;                 // For compare
  static uint32_t totallength; // Total file length
  static size_t lastindex;     // To test same index

  if (index == 0)
  {
    path = String("/") + filename; // Form SPIFFS filename
    SPIFFS.remove(path);           // Remove old file
    f = SPIFFS.open(path, "w");    // Create new file
    t = millis();                  // Start time
    totallength = 0;               // Total file lengt still zero
    lastindex = 0;                 // Prepare test
  }
  t1 = millis(); // Current timestamp
  // Yes, print progress
  dbgprint("File upload %s, t = %d msec, len %d, index %d",
           filename.c_str(), t1 - t, len, index);
  if (len) // Something to write?
  {
    if ((index != lastindex) || (index == 0)) // New chunk?
    {
      f.write(data, len); // Yes, transfer to SPIFFS
      totallength += len; // Update stored length
      lastindex = index;  // Remenber this part
    }
  }
  if (final) // Was this last chunk?
  {
    f.close(); // Yes, clode the file
    reply = dbgprint("File upload %s, %d bytes finished",
                     filename.c_str(), totallength);
    request->send(200, "", reply);
  }
}

//******************************************************************************************
//                                H A N D L E F S F                                        *
//******************************************************************************************
// Handling of requesting files from the SPIFFS/PROGMEM. Example: /favicon.ico             *
//******************************************************************************************
void handleFSf(AsyncWebServerRequest *request, const String &filename)
{
  static String ct;                 // Content type
  AsyncWebServerResponse *response; // For extra headers

  dbgprint("FileRequest received %s", filename.c_str());
  ct = getContentType(filename);      // Get content type
  if ((ct == "") || (filename == "")) // Empty is illegal
  {
    request->send(404, "text/plain", "File not found");
  }
  else
  {
    if (filename.indexOf("index.html") >= 0) // Index page is in PROGMEM
    {
      response = request->beginResponse_P(200, ct, index_html);
    }
    else if (filename.indexOf("radio.css") >= 0) // CSS file is in PROGMEM
    {
      response = request->beginResponse_P(200, ct, radio_css);
    }
    else if (filename.indexOf("config.html") >= 0) // Config page is in PROGMEM
    {
      response = request->beginResponse_P(200, ct, config_html);
    }
    else if (filename.indexOf("about.html") >= 0) // About page is in PROGMEM
    {
      response = request->beginResponse_P(200, ct, about_html);
    }
    else if (filename.indexOf("favicon.ico") >= 0) // Favicon icon is in PROGMEM
    {
      response = request->beginResponse_P(200, ct, favicon_ico, sizeof(favicon_ico));
    }
    else
    {
      response = request->beginResponse(SPIFFS, filename, ct);
    }
    // Add extra headers
    response->addHeader("Server", NAME);
    response->addHeader("Cache-Control", "max-age=3600");
    response->addHeader("Last-Modified", VERSION);
    request->send(response);
  }
  dbgprint("Response sent");
}

//******************************************************************************************
//                                H A N D L E F S                                          *
//******************************************************************************************
// Handling of requesting files from the SPIFFS. Example: /favicon.ico                     *
//******************************************************************************************
void handleFS(AsyncWebServerRequest *request)
{
  handleFSf(request, request->url()); // Rest of handling
}

//******************************************************************************************
//                             A N A L Y Z E C M D                                         *
//******************************************************************************************
// Handling of the various commands from remote webclient, Serial or MQTT.                 *
// Version for handling string with: <parameter>=<value>                                   *
//******************************************************************************************
char *analyzeCmd(const char *str)
{
  char *value; // Points to value after equalsign in command

  value = (char*)strstr(str, "="); // See if command contains a "="
  if (value)
  {
    *value = '\0'; // Separate command from value
    value++;       // Points to value after "="
  }
  else
  {
    value = (char *)"0"; // No value, assume zero
  }
  return analyzeCmd(str, value); // Analyze command and handle it
}

//******************************************************************************************
//                                 C H O M P                                               *
//******************************************************************************************
// Do some filtering on de inputstring:                                                    *
//  - String comment part (starting with "#").                                             *
//  - Strip trailing CR.                                                                   *
//  - Strip leading spaces.                                                                *
//  - Strip trailing spaces.                                                               *
//******************************************************************************************
String chomp(String str)
{
  int inx; // Index in de input string

  if ((inx = str.indexOf("#")) >= 0) // Comment line or partial comment?
  {
    if (inx > 0 || str[inx - 1] == '\\')
    {
      dbgprint("removing escape from %s", str.c_str());
      str.remove(inx - 1, 1);
    }
    else
    {
      str.remove(inx); // Yes, remove
    }
  }
  str.trim(); // Remove spaces and CR
  return str; // Return the result
}

//******************************************************************************************
//                             A N A L Y Z E C M D                                         *
//******************************************************************************************
// Handling of the various commands from remote webclient, serial or MQTT.                 *
// par holds the parametername and val holds the value.                                    *
// "wifi_00" and "preset_00" may appear more than once, like wifi_01, wifi_02, etc.        *
// Examples with available parameters:                                                     *
//   preset     = 12                        // Select start preset to connect to           *
//   preset_00  = <mp3 stream>              // Specify station for a preset 00-99 *)       *
//   volume     = 95                        // Percentage between 0 and 100                *
//   upvolume   = 2                         // Add percentage to current volume            *
//   downvolume = 2                         // Subtract percentage from current volume     *
//   toneha     = <0..15>                   // Setting treble gain                         *
//   tonehf     = <0..15>                   // Setting treble frequency                    *
//   tonela     = <0..15>                   // Setting bass gain                           *
//   tonelf     = <0..15>                   // Setting treble frequency                    *
//   station    = <mp3 stream>              // Select new station (will not be saved)      *
//   station    = <URL>.mp3                 // Play standalone .mp3 file (not saved)       *
//   station    = <URL>.m3u                 // Select playlist (will not be saved)         *
//   stop                                   // Stop playing                                *
//   resume                                 // Resume playing                              *
//   mute                                   // Mute the music                              *
//   unmute                                 // Unmute the music                            *
//   wifi_00    = mySSID/mypassword         // Set WiFi SSID and password *)               *
//   mqttbroker = mybroker.com              // Set MQTT broker to use *)                   *
//   mqttport   = 1883                      // Set MQTT port to use, default 1883 *)       *
//   mqttuser   = myuser                    // Set MQTT user for authentication *)         *
//   mqttpasswd = mypassword                // Set MQTT password for authentication *)     *
//   mqtttopic  = mytopic                   // Set MQTT topic to subscribe to *)           *
//   mqttpubtopic = mypubtopic              // Set MQTT topic to publish to *)             *
//   status                                 // Show current URL to play                    *
//   testfile   = <file on SPIFFS>          // Test SPIFFS reads for debugging purpose     *
//   test                                   // For test purposes                           *
//   debug      = 0 or 1                    // Switch debugging on or off                  *
//   reset                                  // Restart the ESP8266                         *
//   analog                                 // Show current analog input                   *
// Commands marked with "*)" are sensible in ini-file only                                 *
// Note that it is adviced to avoid expressions as the argument for the abs function.      *
//******************************************************************************************
char *analyzeCmd(const char *par, const char *val)
{
  String argument;        // Argument as string
  String value;           // Value of an argument as a string
  int ivalue;             // Value of argument as an integer
  static char reply[250]; // Reply to client, will be returned
  uint8_t oldvol;         // Current volume
  bool relative;          // Relative argument (+ or -)
  int inx;                // Index in string

  strcpy(reply, "Command accepted"); // Default reply
  argument = chomp(par);             // Get the argument
  if (argument.length() == 0)        // Empty commandline (comment)?
  {
    return reply; // Ignore
  }
  argument.toLowerCase();                 // Force to lower case
  value = chomp(val);                     // Get the specified value
  ivalue = value.toInt();                 // Also as an integer
  ivalue = abs(ivalue);                   // Make it absolute
  relative = argument.indexOf("up") == 0; // + relative setting?
  if (argument.indexOf("down") == 0)      // - relative setting?
  {
    relative = true;  // It's relative
    ivalue = -ivalue; // But with negative value
  }
  if (value.startsWith("http://")) // Does (possible) URL contain "http://"?
  {
    value.remove(0, 7); // Yes, remove it
  }
  if (value.length())
  {
    dbgprint("Command: %s with parameter %s",
             argument.c_str(), value.c_str());
  }
  else
  {
    dbgprint("Command: %s (without parameter)",
             argument.c_str());
  }
  if (argument.indexOf("volume") >= 0) // Volume setting?
  {
    // Volume may be of the form "upvolume", "downvolume" or "volume" for relative or absolute setting
    oldvol = vs1053player.getVolume(); // Get current volume
    if (relative)                      // + relative setting?
    {
      ini_block.reqvol = oldvol + ivalue; // Up by 0.5 or more dB
    }
    else
    {
      ini_block.reqvol = ivalue; // Absolue setting
    }
    if (ini_block.reqvol > 100)
    {
      ini_block.reqvol = 100; // Limit to normal values
    }
    sprintf(reply, "Volume is now %d", // Reply new volume
            ini_block.reqvol);
  }
  else if (argument == "mute") // Mute request
  {
    muteflag = true; // Request volume to zero
  }
  else if (argument == "unmute") // Unmute request?
  {
    muteflag = false; // Request normal volume
  }
  else if (argument.indexOf("preset") >= 0) // Preset station?
  {
    if (!argument.startsWith("preset_")) // But not a station URL
    {
      if (relative) // Relative argument?
      {
        ini_block.newpreset += ivalue; // Yes, adjust currentpreset
      }
      else
      {
        ini_block.newpreset = ivalue; // Otherwise set preset station
      }
      sprintf(reply, "Preset is now %d", // Reply new preset
              ini_block.newpreset);
      playlist_num = 0;
    }
  }
  else if (argument == "stop") // Stop requested?
  {
    if (is_playing())
    {
      datamode = STOPREQD; // Request STOP
    }
    else
    {
      strcpy(reply, "Command not accepted!"); // Error reply
    }
  }
  else if (argument == "resume") // Request to resume?
  {
    if (datamode == STOPPED) // Yes, are we stopped?
    {
      hostreq = true; // Yes, request restart
    }
  }
  else if (argument == "station") // Station in the form address:port
  {
    if (is_playing())
    {
      datamode = STOPREQD; // Request STOP
    }
    host = value;   // Save it for storage and selection later
    hostreq = true; // Force this station as new preset
    sprintf(reply,
            "New preset station %s accepted", // Format reply
            host.c_str());
  }
  else if (argument == "status") // Status request
  {
    if (datamode == STOPPED)
    {
      sprintf(reply, "Player stopped"); // Format reply
    }
    else
    {
      sprintf(reply, "%s - %s", icyname.c_str(),
              icystreamtitle.c_str()); // Streamtitle from metadata
    }
  }
  else if (argument.startsWith("reset")) // Reset request
  {
    resetRequest = true; // Reset all
  }
  else if (argument == "testfile") // Testfile command?
  {
    testfilename = value; // Yes, set file to test accordingly
  }
  else if (argument == "test") // Test command
  {
    sprintf(reply, "Free memory is %d, ringbuf %d, stream %d",
            system_get_free_heap_size(), rcount, mp3client->available());
  }
  // Commands for bass/treble control
  else if (argument.startsWith("tone")) // Tone command
  {
    if (argument.indexOf("ha") > 0) // High amplitue? (for treble)
    {
      ini_block.rtone[0] = ivalue; // Yes, prepare to set ST_AMPLITUDE
    }
    if (argument.indexOf("hf") > 0) // High frequency? (for treble)
    {
      ini_block.rtone[1] = ivalue; // Yes, prepare to set ST_FREQLIMIT
    }
    if (argument.indexOf("la") > 0) // Low amplitue? (for bass)
    {
      ini_block.rtone[2] = ivalue; // Yes, prepare to set SB_AMPLITUDE
    }
    if (argument.indexOf("lf") > 0) // High frequency? (for bass)
    {
      ini_block.rtone[3] = ivalue; // Yes, prepare to set SB_FREQLIMIT
    }
    reqtone = true; // Set change request
    sprintf(reply, "Parameter for bass/treble %s set to %d",
            argument.c_str(), ivalue);
  }
  else if (argument == "rate") // Rate command?
  {
    vs1053player.AdjustRate(ivalue); // Yes, adjust
  }
  else if (argument == "analog") // Show analog request?
  {
    sprintf(reply, "Analog input = %d units", // Read the analog input for test
            analogRead(A0));
  }
  else if (argument.startsWith("wifi")) // WiFi SSID and passwd?
  {
    inx = value.indexOf("/"); // Find separator between ssid and password
    // Was this the strongest SSID or the only acceptable?
    if (acceptableNetworksCount == 1)
    {
      ini_block.ssid = value.substring(0, inx); // Only one.  Set as the strongest
    }
    if (value.substring(0, inx) == ini_block.ssid)
    {
      ini_block.passwd = value.substring(inx + 1); // Yes, set password
    }
  }
  else if (argument == "getnetworks") // List all WiFi networks?
  {
    sprintf(reply, networks.c_str()); // Reply is SSIDs
  }
  else
  {
    sprintf(reply, "%s called with illegal parameter: %s",
            NAME, argument.c_str());
  }
  return reply; // Return reply to the caller
}

//******************************************************************************************
//                             H A N D L E C M D                                           *
//******************************************************************************************
// Handling of the various commands from remote (case sensitive). All commands have the    *
// form "/?parameter[=value]".  Example: "/?volume=50".                                    *
// The startpage will be returned if no arguments are given.                               *
// Multiple parameters are ignored.  An extra parameter may be "version=<random number>"   *
// in order to prevent browsers like Edge and IE to use their cache.  This "version" is    *
// ignored.                                                                                *
// Example: "/?upvolume=5&version=0.9775479450590543"                                      *
// The save and the list commands are handled specially.                                   *
//******************************************************************************************
void handleCmd(AsyncWebServerRequest *request)
{
  AsyncWebParameter *p;   // Points to parameter structure
  static String argument; // Next argument in command
  static String value;    // Value of an argument
  const char *reply;      // Reply to client
  //uint32_t         t ;                                // For time test
  int params;    // Number of params
  static File f; // Handle for writing /radio.ini to SPIFFS

  //t = millis() ;                                      // Timestamp at start
  params = request->params(); // Get number of arguments
  if (params == 0)            // Any arguments
  {
    if (NetworkFound)
    {
      handleFSf(request, String("/index.html")); // No parameters, send the startpage
    }
    else
    {
      handleFSf(request, String("/config.html")); // Or the configuration page if in AP mode
    }
    return;
  }
  p = request->getParam(0); // Get pointer to parameter structure
  argument = p->name();     // Get the argument
  argument.toLowerCase();   // Force to lower case
  value = p->value();       // Get the specified value
  // For the "save" command, the contents is the value of the next parameter
  if (argument.startsWith("save") && (params > 1))
  {
    reply = "Error saving " INIFILENAME; // Default reply
    p = request->getParam(1);            // Get pointer to next parameter structure
    if (p->isPost())                     // Does it have a POST?
    {
      f = SPIFFS.open(INIFILENAME, "w"); // Save to inifile
      if (f)
      {
        f.print(p->value());
        f.close();
        reply = dbgprint("%s saved", INIFILENAME);
      }
    }
  }
  else if (argument.startsWith("list")) // List all presets?
  {
    dbgprint("list request from browser");
    request->send(200, "text/plain", presetlist); // Send the reply
    return;
  }
  else
  {
    reply = analyzeCmd(argument.c_str(), // Analyze it
                       value.c_str());
  }
  request->send(200, "text/plain", reply); // Send the reply
}
