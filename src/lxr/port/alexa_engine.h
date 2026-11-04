/*
 * alexa_engine.h
 *
 * Alexa02 port: the interface between the VCV Rack module (C++) and the
 * LXR drum engine (C, from the open-source Erica Synths firmware).
 *
 * The engine runs at its own sample rate (ALEXA_ENGINE_FS) in blocks of
 * ALEXA_BLOCK samples, exactly like the hardware.
 *
 * Copyright (C) 2012-2026 Erica Synths (engine), 2026 Haer (port)
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef ALEXA_ENGINE_H
#define ALEXA_ENGINE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALEXA_BLOCK      64        /* = OUTPUT_DMA_SIZE of the firmware */
#define ALEXA_ENGINE_FS  44140.f   /* = REAL_FS of the firmware */
#define ALEXA_NUM_VOICES 6
#define ALEXA_NUM_TRIGS  7         /* drum 1-3, snare, cymbal, closed hat, open hat */
#define ALEXA_NUM_CV     5         /* the eurorack module's 5 CV inputs */
#define ALEXA_NUM_SLOTS  18        /* mod matrix slots */

/* number of bytes in a kit (all sound parameters, without the 8 byte name) */
int alexa_kitSize(void);
/* the factory init kit from the firmware: 8 name bytes then the parameters */
const uint8_t* alexa_initKit(int* size);

/* set up the engine (call once, after alexa_selectState) */
void alexa_engineInit(void);

/* set a sound parameter (0..kitSize-1) to a value as stored in a kit */
void alexa_setParam(uint16_t idx, uint8_t value);
uint8_t alexa_getParam(uint16_t idx);

/* trigger a voice. trig: 0-2 drums, 3 snare, 4 cymbal, 5 closed hat, 6 open hat.
 * vel 0..127 */
void alexa_trigger(uint8_t trig, uint8_t vel);

/* 1V/oct: frequency in Hz that replaces the voice's base pitch, or -1 to use the kit pitch */
void alexa_setVOct(uint8_t voice, float hz);

/* CV inputs of the mod matrix, 0..1 (0.5 = no change) */
void alexa_setCv(uint8_t idx, float value01);
/* accent levels the mod matrix reads (sources ac1..ac6), 0..127 */
void alexa_setAccent(uint8_t voice, uint8_t value);

/* LFO values for display/outputs (bipolar-ish, as the engine computes them) */
float alexa_getLfo(uint8_t voice);

/* morph requested by the mod matrix (destination "morph"), 0..255, or -1 if none
 * since the last call. Reading clears it. */
int alexa_takeMatrixMorph(void);
/* the morph position the mod matrix modulates around (0..255) */
void alexa_setMorphBase(uint8_t value);

/* render one block.
 * voiceOut: each voice after its own volume, before pan (mono)
 * mixL/mixR: the stereo mix of all voices with inMixMask bit set, plus the FX bus */
void alexa_renderBlock(int16_t voiceOut[ALEXA_NUM_VOICES][ALEXA_BLOCK],
                       int16_t mixL[ALEXA_BLOCK], int16_t mixR[ALEXA_BLOCK],
                       uint8_t inMixMask);

/* The engine keeps its state in global variables (as on the hardware).
 * To run several modules, each one keeps a private copy and swaps it in.
 * alexa_stateSize gives the total size, save/load copy it. */
size_t alexa_stateSize(void);
void alexa_stateSave(uint8_t* dst);
void alexa_stateLoad(const uint8_t* src);

/* the mod matrix destinations ("modTargets" in the firmware) */
uint16_t alexa_modTargetParam(uint8_t modTargetIdx);
int alexa_numModTargets(void);

#ifdef __cplusplus
}
#endif
#endif
