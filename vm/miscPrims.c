/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Copyright 2019 John Maloney, Bernat Romagosa, and Jens Mönig

// miscPrims.c - Miscellaneous primitives
// John Maloney, May 2019

#include <Arduino.h>
#include <math.h>

#include "mem.h"
#include "interp.h"

static OBJ primSine(int argCount, OBJ *args) {
	// Returns the sine of the given angle * 2^14 (i.e. a fixed point integer with 13 bits of
	// fraction). The input is the angle in hundreths of a degree (e.g. 4500 means 45 degrees).

	const float hundrethsToRadians = 6.2831853071795864769 / 36000.0;
	return int2obj((int) round(16384.0 * sin(evalInt(args[0]) * hundrethsToRadians)));
}

// Primitives

static PrimEntry entries[] = {
	"sin", primSine,
};

void addMiscPrims() {
	addPrimitiveSet("misc", sizeof(entries) / sizeof(PrimEntry), entries);
}