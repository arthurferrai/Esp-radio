#ifndef __MY_CUSTOM_DISPLAY_H_DEFINED__
#define __MY_CUSTOM_DISPLAY_H_DEFINED__

#include <U8g2lib.h>
#include "asciitools.h"
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

const int CHAR_WIDTH = 5;
const int CHAR_HEIGHT = 8;
const uint8_t * FONT = u8g2_font_5x8_tf;
void setupDisplay() {
  u8g2.begin();
  u8g2.setFont(FONT);
}

void displayText(const char * str, uint8 x, uint8 y) {
  char buf [128/CHAR_WIDTH + 1];
  strncpy(buf, str, sizeof(buf));
  buf[sizeof(buf) - 1] = 0;
  utf8ascii(buf);
  u8g2.setDrawColor(1);
  u8g2.drawStr(x, y + CHAR_HEIGHT, buf);
}

void clearArea(uint8 x, uint8 y, uint8 w, uint8 h) {
  u8g2.setDrawColor(0);
  u8g2.drawBox(x,y,w,h);
}

void displayGlyph(uint8 x, uint8 y, uint16 glyph) {
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
  u8g2.drawGlyph(x, y + u8g2.getAscent(), glyph);
  u8g2.setFont(FONT);
}

void showNowPlayingInfo(const char * artist, const char * song) {
  clearArea(0,0,128,CHAR_HEIGHT*2 + 1 - u8g2.getDescent());
  displayGlyph(0, 0, 0xE5);
  displayGlyph(0, 9, 0xE1);
  displayText(artist, 9, 0);
  displayText(song, 9, CHAR_HEIGHT+1);
  u8g2.sendBuffer();
}

void showStationName(const char * name) {
  clearArea(0, 18, 128, CHAR_HEIGHT + 1 - u8g2.getDescent());
  displayGlyph(0, 18, 0xF8);
  displayText(name, 9, 18);
  u8g2.sendBuffer();
}

void showContentType(const char * contentType) {
  clearArea(0, 27, 128, CHAR_HEIGHT + 1 - u8g2.getDescent());
  displayGlyph(0, 27, 0xF9);
  displayText(contentType, 9, 27);
  u8g2.sendBuffer();
}

bool waiting = false;

void showWaitingDataIcon() {
  if (waiting) return;
  waiting = true;
  clearArea(0, 36, 128, CHAR_HEIGHT + 1 - u8g2.getDescent());
  displayGlyph(0, 36, 0xCD);
  u8g2.sendBuffer();
}

void hideWaitingDataIcon() {
  if (!waiting) return;
  waiting = false;
  clearArea(0, 36, 128, CHAR_HEIGHT + 1 - u8g2.getDescent());
  u8g2.sendBuffer();
}

void displayDebug(const char* str) {
  clearArea(0, 64 - CHAR_HEIGHT + u8g2.getDescent(), 128, CHAR_HEIGHT - u8g2.getDescent());
  displayText(str, 0, 64 - CHAR_HEIGHT + u8g2.getDescent());
  u8g2.sendBuffer();
}
#endif
