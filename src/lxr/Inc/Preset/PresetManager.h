/*
 * PresetManager.h  (Alexa02 port: slimmed down)
 *
 * The original manages kits on the SD card. In VCV Rack, kits live in the
 * patch, so only the morph interface that the sound engine calls is kept.
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef PRESETMANAGER_H_
#define PRESETMANAGER_H_
#include "main.h"
#include "string.h"
#include "Parameters.h"
#include "InitKit.h"

extern uint8_t preset_morphValue;

/** start a morph between kit A and kit B (morph 0..255) */
void preset_morph(uint8_t morph, uint8_t forceResetIndex);
uint8_t preset_getMorphValue(uint16_t index, uint8_t morph);

#endif /* PRESETMANAGER_H_ */
