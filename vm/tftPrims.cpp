/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2018 John Maloney, Bernat Romagosa, and Jens Mönig

// tftPrims.cpp - Microblocks TFT screen primitives for the Citilab ED1 board
// Bernat Romagosa, November 2018

#include <Arduino.h>
#include <SPI.h>
#include <stdio.h>

#include "mem.h"
#include "interp.h"

int useTFT = false;
int touchEnabled = false;

#if defined(ARDUINO_CITILAB_ED1) || defined(ARDUINO_M5Stack_Core_ESP32) || defined(ARDUINO_M5Stick_C) || defined(ARDUINO_IOT_BUS)

	#define TFT_BLACK 0
	#define TFT_GREEN 0x7E0

	// Optional TFT_ESPI code was added by John to study performance differences
	#define USE_TFT_ESPI false // true to use TFT_eSPI library, false to use AdaFruit GFX library
	#if USE_TFT_ESPI
		#include <TFT_eSPI.h>

		TFT_eSPI tft = TFT_eSPI();

		void tftInit() {
			tft.init();
			tft.setRotation(0);
			tftClear();
			// Turn on backlight on IoT-Bus
			pinMode(33, OUTPUT);
			digitalWrite(33, HIGH);
			useTFT = true;
		}
	#elif defined(ARDUINO_CITILAB_ED1)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ST7735.h"

		#define TFT_CS	5
		#define TFT_DC	9
		#define TFT_RST	10
		#define TFT_WIDTH 128
		#define TFT_HEIGHT 128
		Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

		void tftInit() {
			tft.initR(INITR_144GREENTAB);
			tft.setRotation(0);
			tftClear();
			useTFT = true;
		}

	#elif defined(ARDUINO_M5Stack_Core_ESP32)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ILI9341.h"
		#define TFT_CS	14
		#define TFT_DC	27
		#define TFT_RST	33
		#define TFT_WIDTH 320
		#define TFT_HEIGHT 240
		Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
		void tftInit() {
			tft.begin(40000000); // Run SPI at 80MHz/2
			tft.setRotation(1);
			uint8_t m = 0x08 | 0x04; // RGB pixel order, refresh LCD right to left
			tft.sendCommand(ILI9341_MADCTL, &m, 1);
			tftClear();
			// Turn on backlight:
			pinMode(32, OUTPUT);
			digitalWrite(32, HIGH);
			useTFT = true;
		}

	#elif defined(ARDUINO_M5Stick_C)
		// Preliminary: this is not yet working...
		#include "Adafruit_GFX.h"
		#include "Adafruit_ST7735.h"

		#define TFT_CS		5
		#define TFT_DC		23
		#define TFT_RST		18

		#define TFT_WIDTH	80
		#define TFT_HEIGHT	160

		// make a subclass so we can adjust the x/y offsets
		class M5StickLCD : public Adafruit_ST7735 {
		public:
			M5StickLCD(int8_t cs, int8_t dc, int8_t rst) : Adafruit_ST7735(cs, dc, rst) {}
			void setOffsets(int colOffset, int rowOffset) {
				_xstart = _colstart = colOffset;
				_ystart = _rowstart = rowOffset;
			}
		};
		M5StickLCD tft = M5StickLCD(TFT_CS, TFT_DC, TFT_RST);

		void tftInit() {
			tft.initR(INITR_MINI160x80);
			tft.setOffsets(26, 1);
			tft.invertDisplay(true); // not sure why this is required...
			tftClear();
			useTFT = true;
		}

	#elif defined(ARDUINO_IOT_BUS)
		#include "Adafruit_GFX.h"
		#include "Adafruit_ILI9341.h"
		#include <XPT2046_Touchscreen.h>
		#include <SPI.h>

		#define HAS_TFT_TOUCH
		#define TOUCH_CS_PIN 16
		XPT2046_Touchscreen ts(TOUCH_CS_PIN);

		#define X_MIN 256
		#define X_MAX 3632
		#define Y_MIN 274
		#define Y_MAX 3579

		#define TFT_CS	5
		#define TFT_DC	27

		#define TFT_WIDTH 320
		#define TFT_HEIGHT 240
		Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

		void tftInit() {
			tft.begin();
			tft.setRotation(1);
//			tft._freq = 80000000; // this requires moving _freq to public in AdaFruit_SITFT.h
			tftClear();
			// Turn on backlight on IoT-Bus
			pinMode(33, OUTPUT);
			digitalWrite(33, HIGH);

			useTFT = true;
		}

		void touchInit() {
			ts.begin();
			ts.setCalibration(X_MIN, X_MAX, Y_MIN, Y_MAX);
			ts.setRotation(1);
			touchEnabled = true;
		}
	#endif

void tftClear() {
	tft.fillScreen(TFT_BLACK);
}

OBJ primEnableDisplay(int argCount, OBJ *args) {
	if (trueObj == args[0]) {
		tftInit();
	} else {
		useTFT = false;
	}
	return falseObj;
}

static int color24to16b(int color24b) {
	// converts 24-bit RGB888 format to 16-bit RGB565
	int r = (color24b >> 16) & 0xFF;
	int g = (color24b >> 8) & 0xFF;
	int b = color24b & 0xFF;
	#if defined(ARDUINO_M5Stick_C)
		// color order is GBR
		return ((b << 8) & 0xF800) | ((g << 3) & 0x7E0) | ((r >> 3) & 0x1F);
	#else
		return ((r << 8) & 0xF800) | ((g << 3) & 0x7E0) | ((b >> 3) & 0x1F);
	#endif
}

static OBJ primGetWidth(int argCount, OBJ *args) {
	#ifdef TFT_WIDTH
		return int2obj(TFT_WIDTH);
	#else
		return int2obj(0);
	#endif
}

static OBJ primGetHeight(int argCount, OBJ *args) {
	#ifdef TFT_HEIGHT
		return int2obj(TFT_HEIGHT);
	#else
		return int2obj(0);
	#endif
}

static OBJ primSetPixel(int argCount, OBJ *args) {
	int x = obj2int(args[0]);
	int y = obj2int(args[1]);
	// Re-encode color from 24 bits into 16 bits
	int color16b = color24to16b(obj2int(args[2]));
	tft.drawPixel(x, y, color16b);
	return falseObj;
}

static OBJ primLine(int argCount, OBJ *args) {
	int x0 = obj2int(args[0]);
	int y0 = obj2int(args[1]);
	int x1 = obj2int(args[2]);
	int y1 = obj2int(args[3]);
	// Re-encode color from 24 bits into 16 bits
	int color16b = color24to16b(obj2int(args[4]));
	tft.drawLine(x0, y0, x1, y1, color16b);
	return falseObj;
}

static OBJ primRect(int argCount, OBJ *args) {
	int x = obj2int(args[0]);
	int y = obj2int(args[1]);
	int width = obj2int(args[2]);
	int height = obj2int(args[3]);
	// Re-encode color from 24 bits into 16 bits
	int color16b = color24to16b(obj2int(args[4]));
	int fill = (argCount > 5) ? (trueObj == args[5]) : false;
	if (fill) {
		tft.fillRect(x, y, width, height, color16b);
	} else {
		tft.drawRect(x, y, width, height, color16b);
	}
	return falseObj;
}

static OBJ primRoundedRect(int argCount, OBJ *args) {
	int x = obj2int(args[0]);
	int y = obj2int(args[1]);
	int width = obj2int(args[2]);
	int height = obj2int(args[3]);
	int radius = obj2int(args[4]);
	// Re-encode color from 24 bits into 16 bits
	int color16b = color24to16b(obj2int(args[5]));
	int fill = (argCount > 6) ? (trueObj == args[6]) : false;
	if (fill) {
		tft.fillRoundRect(x, y, width, height, radius, color16b);
	} else {
		tft.drawRoundRect(x, y, width, height, radius, color16b);
	}
	return falseObj;
}

static OBJ primCircle(int argCount, OBJ *args) {
	int x = obj2int(args[0]);
	int y = obj2int(args[1]);
	int radius = obj2int(args[2]);
	// Re-encode color from 24 bits into 16 bits
	int color16b = color24to16b(obj2int(args[3]));
	int fill = (argCount > 4) ? (trueObj == args[4]) : false;
	if (fill) {
		tft.fillCircle(x, y, radius, color16b);
	} else {
		tft.drawCircle(x, y, radius, color16b);
	}
	return falseObj;
}

static OBJ primTriangle(int argCount, OBJ *args) {
	int x0 = obj2int(args[0]);
	int y0 = obj2int(args[1]);
	int x1 = obj2int(args[2]);
	int y1 = obj2int(args[3]);
	int x2 = obj2int(args[4]);
	int y2 = obj2int(args[5]);
	// Re-encode color from 24 bits into 16 bits
	int color16b = color24to16b(obj2int(args[6]));
	int fill = (argCount > 7) ? (trueObj == args[7]) : false;
	if (fill) {
		tft.fillTriangle(x0, y0, x1, y1, x2, y2, color16b);
	} else {
		tft.drawTriangle(x0, y0, x1, y1, x2, y2, color16b);
	}
	return falseObj;
}

static OBJ primText(int argCount, OBJ *args) {
	OBJ value = args[0];
	int x = obj2int(args[1]);
	int y = obj2int(args[2]);
	// Re-encode color from 24 bits into 16 bits
	int color16b = color24to16b(obj2int(args[3]));
	int scale = (argCount > 4) ? obj2int(args[4]) : 1;
	int wrap = (argCount > 5) ? (trueObj == args[5]) : false;
	tft.setCursor(x, y);
	tft.setTextColor(color16b);
	tft.setTextSize(scale);
	tft.setTextWrap(wrap);

	if (IS_TYPE(value, StringType)) {
		tft.print(obj2str(value));
	} else if (trueObj == value) {
		tft.print("true");
	} else if (falseObj == value) {
		tft.print("false");
	} else if (isInt(value)) {
		char s[50];
		sprintf(s, "%d", obj2int(value));
		tft.print(s);
	}
	return falseObj;
}

void tftSetHugePixel(int x, int y, int state) {
	// simulate a 5x5 array of square pixels like the micro:bit LED array
	int minDimension, xInset = 0, yInset = 0;
	if (tft.width() > tft.height()) {
		minDimension = tft.height();
		xInset = (tft.width() - tft.height()) / 2;
	} else {
		minDimension = tft.width();
		yInset = (tft.height() - tft.width()) / 2;
	}
	int lineWidth = 3;
	int squareSize = (minDimension - (6 * lineWidth)) / 5;
	tft.fillRect(
		xInset + ((x - 1) * squareSize) + (x * lineWidth), // x
		yInset + ((y - 1) * squareSize) + (y * lineWidth), // y
		squareSize, squareSize,
		state ? TFT_GREEN : TFT_BLACK);
}

void tftSetHugePixelBits(int bits) {
	if (0 == bits) {
		tftClear();
	} else {
		for (int x = 1; x <= 5; x++) {
			for (int y = 1; y <= 5; y++) {
				tftSetHugePixel(x, y, bits & (1 << ((5 * (y - 1) + x) - 1)));
			}
		}
	}
}

static OBJ primTftTouched(int argCount, OBJ *args) {
	#ifdef HAS_TFT_TOUCH
		if (!touchEnabled) { touchInit(); }
		return ts.touched() ? trueObj : falseObj;
	#endif
	return falseObj;
}

static OBJ primTftTouchX(int argCount, OBJ *args) {
	#ifdef HAS_TFT_TOUCH
		if (!touchEnabled) { touchInit(); }
		if (ts.touched()) {
			TS_Point p = ts.getMappedPoint();
			return int2obj(p.x);
		}
	#endif
	return int2obj(-1);
}

static OBJ primTftTouchY(int argCount, OBJ *args) {
	#ifdef HAS_TFT_TOUCH
		if (!touchEnabled) { touchInit(); }
		if (ts.touched()) {
			TS_Point p = ts.getMappedPoint();
			return int2obj(p.y);
		}
	#endif
	return int2obj(-1);
}

static OBJ primTftTouchPressure(int argCount, OBJ *args) {
	#ifdef HAS_TFT_TOUCH
		if (!touchEnabled) { touchInit(); }
		if (ts.touched()) {
			TS_Point p = ts.getMappedPoint();
			return int2obj(p.z);
		}
	#endif
	return int2obj(-1);
}

#else // stubs

void tftInit() { }
void tftClear() { }
void tftSetHugePixel(int x, int y, int state) { }
void tftSetHugePixelBits(int bits) { }

static OBJ primEnableDisplay(int argCount, OBJ *args) { return falseObj; }
static OBJ primGetWidth(int argCount, OBJ *args) { return int2obj(0); }
static OBJ primGetHeight(int argCount, OBJ *args) { return int2obj(0); }
static OBJ primSetPixel(int argCount, OBJ *args) { return falseObj; }
static OBJ primLine(int argCount, OBJ *args) { return falseObj; }
static OBJ primRect(int argCount, OBJ *args) { return falseObj; }
static OBJ primRoundedRect(int argCount, OBJ *args) { return falseObj; }
static OBJ primCircle(int argCount, OBJ *args) { return falseObj; }
static OBJ primTriangle(int argCount, OBJ *args) { return falseObj; }
static OBJ primText(int argCount, OBJ *args) { return falseObj; }
static OBJ primTftTouched(int argCount, OBJ *args) { return falseObj; }
static OBJ primTftTouchX(int argCount, OBJ *args) { return falseObj; }
static OBJ primTftTouchY(int argCount, OBJ *args) { return falseObj; }
static OBJ primTftTouchPressure(int argCount, OBJ *args) { return falseObj; }

#endif

// Primitives

static PrimEntry entries[] = {
	{"enableDisplay", primEnableDisplay},
	{"getWidth", primGetWidth},
	{"getHeight", primGetHeight},
	{"setPixel", primSetPixel},
	{"line", primLine},
	{"rect", primRect},
	{"roundedRect", primRoundedRect},
	{"circle", primCircle},
	{"triangle", primTriangle},
	{"text", primText},
	{"tftTouched", primTftTouched},
	{"tftTouchX", primTftTouchX},
	{"tftTouchY", primTftTouchY},
	{"tftTouchPressure", primTftTouchPressure},
};

void addTFTPrims() {
	addPrimitiveSet("tft", sizeof(entries) / sizeof(PrimEntry), entries);
}
