/*
 * alexa_glue.c
 *
 * Alexa02 port: connects the LXR drum engine (unchanged firmware code) to
 * the VCV Rack module. It stands in for the parts of the firmware that talk
 * to hardware: trigger and accent jacks, CV inputs, the front panel, the
 * SD card kit loader and the audio codec.
 *
 * Copyright (C) 2012-2026 Erica Synths (engine), 2026 Haer (port)
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <string.h>
#include "alexa_engine.h"
#include "Parameters.h"
#include "menu/menu.h"
#include "menu/CcNr2Text.h"
#include "Preset/InitKit.h"
#include "Soundengine/config.h"
#include "Soundengine/FxBus.h"
#include "Soundengine/ModMatrix_Processor.h"
#include "Soundengine/DSPAudio/mixer.h"
#include "Soundengine/DSPAudio/DrumVoice.h"
#include "Soundengine/DSPAudio/Snare.h"
#include "Soundengine/DSPAudio/HiHat.h"
#include "Soundengine/DSPAudio/CymbalVoice.h"
#include "Soundengine/DSPAudio/BufferTools.h"
#include "Soundengine/DSPAudio/modulationNode.h"
#include "Soundengine/DSPAudio/ResonantFilter.h"
#include "Soundengine/DSPAudio/lfo.h"
#include "Soundengine/DSPAudio/FX/FxRamBuffer.h"
#include "Soundengine/MIDI/MidiParser.h"
#include "Soundengine/MIDI/MidiMessages.h"
#include "Soundengine/MIDI/ParameterArray.h"

#if (OUTPUT_DMA_SIZE != ALEXA_BLOCK)
#error "block size mismatch"
#endif

/* ------------------------------------------------------------------ */
/* state that the firmware keeps in other files                        */
/* ------------------------------------------------------------------ */
uint8_t activeParameters[NUM_PARAMS];   /* the current (morphed) kit */
uint8_t preset_morphValue = 0;
uint8_t menu_activePage = 0;

static float   alexa_cvValues[ALEXA_NUM_CV] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
static uint8_t alexa_accent[ALEXA_NUM_TRIGS] = {63, 63, 63, 63, 63, 63, 63};
static int     alexa_matrixMorph = -1;

/* the firmware mixer's decimation state lives in mixer.c */
extern float   mixer_decimation_cnt[6];
extern int16_t mixer_voice_samples[6];
extern uint8_t paramToModTarget[END_OF_SOUND_PARAMETERS];
extern uint8_t midiParser_originalCcValues[0xff];
extern ModulationNode cvModulators[NUM_MOD_SLOTS];
extern uint32_t lxr_rngState;

/* ------------------------------------------------------------------ */
/* stand-ins for hardware functions the engine calls                   */
/* ------------------------------------------------------------------ */
float adc_getCvValue(uint8_t idx) { return idx < ALEXA_NUM_CV ? alexa_cvValues[idx] : 0.5f; }
uint8_t Trigger_getAccent(uint8_t voice) { return voice < ALEXA_NUM_TRIGS ? alexa_accent[voice] : 63; }
void uart_sendFrontpanelByte(uint8_t data) { (void)data; }
void dio_mute(uint8_t voice, uint8_t onOff) { (void)voice; (void)onOff; }
void menu_switchSubPage(uint8_t subPageNr, uint8_t toggle) { (void)subPageNr; (void)toggle; }
uint8_t menu_getSubPage() { return 0; }
void voiceControl_processQueue() {}

/* The mod matrix can modulate the kit morph. On the hardware this starts a
 * morph towards the morph kit; here the module does the morphing, so the
 * value is handed over. */
void preset_morph(uint8_t morph, uint8_t forceResetIndex)
{
	(void)forceResetIndex;
	preset_morphValue = morph;
	alexa_matrixMorph = morph;
}

uint8_t preset_getMorphValue(uint16_t index, uint8_t morph)
{
	(void)morph;
	return activeParameters[index];
}

void alexa_setMorphBase(uint8_t value)
{
	activeParameters[PAR_MORPH] = value;
}

int alexa_takeMatrixMorph(void)
{
	const int m = alexa_matrixMorph;
	alexa_matrixMorph = -1;
	return m;
}

/* ------------------------------------------------------------------ */
/* kits                                                                */
/* ------------------------------------------------------------------ */
int alexa_kitSize(void) { return END_OF_SOUND_PARAMETERS; }

const uint8_t* alexa_initKit(int* size)
{
	if(size) *size = INIT_KIT_SIZE;
	return initKit;
}

int alexa_numModTargets(void) { return getNumModTargets(); }

uint16_t alexa_modTargetParam(uint8_t modTargetIdx)
{
	if(modTargetIdx >= getNumModTargets()) return PAR_NONE;
	return modTargets[modTargetIdx].param;
}

/* ------------------------------------------------------------------ */
/* parameters                                                          */
/* ------------------------------------------------------------------ */

/* same path as the firmware's preset_morph_tick(): parameters below 128
 * go out as MIDI CC (with the firmware's +1 offset), 128..255 as CC2. */
static void alexa_sendToEngine(uint16_t idx, uint8_t val)
{
	MidiMsg msg;
	memset(&msg, 0, sizeof(msg));
	if(idx < 128)
	{
		msg.status = MIDI_CC;
		msg.data1 = (uint8_t)(idx + 1);
		msg.data2 = val;
		midiParser_ccHandler(msg, 1, 0, 0.f);
	}
	else if(idx <= 255)
	{
		msg.status = MIDI_CC2;
		msg.data1 = (uint8_t)(idx - 128);
		msg.data2 = val;
		midiParser_ccHandler(msg, 1, 0, 0.f);
	}
}

void alexa_setParam(uint16_t idx, uint8_t value)
{
	if(idx >= END_OF_SOUND_PARAMETERS) return;
	activeParameters[idx] = value;

	/* the mod matrix: destinations are indices into modTargets */
	if(idx >= PAR_CV_DEST_1 && idx < PAR_CV_DEST_1 + NUM_MOD_SLOTS)
	{
		modNode_setDestination(&cvModulators[idx - PAR_CV_DEST_1], alexa_modTargetParam(value));
		return;
	}
	/* amounts and sources are read straight from activeParameters by CV_process() */
	if(idx >= PAR_CV_DEST_1) return;

	/* the morph kit number has no meaning here */
	if(idx == PAR_MORPH_KIT_DEST) return;

	alexa_sendToEngine(idx, value);
}

uint8_t alexa_getParam(uint16_t idx)
{
	return idx < NUM_PARAMS ? activeParameters[idx] : 0;
}

/* ------------------------------------------------------------------ */
/* init                                                                */
/* ------------------------------------------------------------------ */
void alexa_engineInit(void)
{
	memset(activeParameters, 0, sizeof(activeParameters));
	memset(FX_BUFFER, 0, sizeof(FX_BUFFER));

	/* the same order as soundengine_init() in the firmware */
	mixer_init();
	parameterArray_init();
	memset(midi_MidiChannels, 0, 8);
	initDrumVoice();
	Snare_init();
	HiHat_init();
	Cymbal_init();
	CV_Init();

	/* load the factory kit, as the firmware does on an empty card */
	int i;
	for(i = 0; i < END_OF_SOUND_PARAMETERS; i++)
	{
		const int src = i + 8;
		alexa_setParam((uint16_t)i, src < INIT_KIT_SIZE ? initKit[src] : 0);
	}
}

/* ------------------------------------------------------------------ */
/* triggers and CV                                                     */
/* ------------------------------------------------------------------ */
void alexa_trigger(uint8_t trig, uint8_t vel)
{
	const uint8_t note = SEQ_DEFAULT_NOTE;
	if(trig < 3)       Drum_trigger(trig, vel, note);
	else if(trig == 3) Snare_trigger(vel, note);
	else if(trig == 4) Cymbal_trigger(vel, note);
	else if(trig == 5) HiHat_trigger(vel, 0, note);
	else if(trig == 6) HiHat_trigger(vel, 1, note);
}

void alexa_setVOct(uint8_t voice, float hz)
{
	if(voice < 3)       Drum_setCv1VOct(voice, hz);
	else if(voice == 3) Snare_setCv1VOct(hz);
	else if(voice == 4) Cymbal_setCv1VOct(hz);
	else if(voice == 5) HiHat_setCv1VOct(hz);
}

void alexa_setCv(uint8_t idx, float value01)
{
	if(idx < ALEXA_NUM_CV) alexa_cvValues[idx] = value01;
}

void alexa_setAccent(uint8_t voice, uint8_t value)
{
	if(voice < ALEXA_NUM_TRIGS) alexa_accent[voice] = value;
}

float alexa_getLfo(uint8_t voice)
{
	if(voice < 3) return voiceArray[voice].lfo.value;
	if(voice == 3) return snareVoice.lfo.value;
	if(voice == 4) return cymbalVoice.lfo.value;
	return hatVoice.lfo.value;
}

/* ------------------------------------------------------------------ */
/* rendering                                                           */
/* ------------------------------------------------------------------ */

/* the firmware's mixer_decimateBlock (it is not exported) */
static void alexa_decimateBlock(const uint8_t voiceNr, int16_t* buffer)
{
	uint8_t i;
	float phaseInc = mixer_decimation_rate[voiceNr] * mixer_decimation_rate[6];
	for(i = 0; i < OUTPUT_DMA_SIZE; i++)
	{
		mixer_decimation_cnt[voiceNr] += phaseInc;
		if(mixer_decimation_cnt[voiceNr] >= 1.f)
		{
			mixer_decimation_cnt[voiceNr] -= 1.f;
			mixer_voice_samples[voiceNr] = buffer[i];
		}
		buffer[i] = mixer_voice_samples[voiceNr];
	}
}

static inline int16_t sat16(int32_t x)
{
	return (int16_t)(x > 32767 ? 32767 : (x < -32768 ? -32768 : x));
}

/* like the firmware's mixer_addDataToOutput, with all of the hardware's
 * outputs folded into one stereo mix: stereo destinations are panned,
 * L and R destinations go hard left or right */
static void alexa_addToMix(uint8_t dest, float panL, float panR, const int16_t* data,
                           int16_t* mix, int16_t* fx)
{
	int i;
	switch(dest)
	{
	case MIXER_ROUTING_FX_BUS:
		for(i = 0; i < OUTPUT_DMA_SIZE; i++)
		{
			fx[2*i]   = sat16(fx[2*i]   + (int16_t)(data[i] * panL));
			fx[2*i+1] = sat16(fx[2*i+1] + (int16_t)(data[i] * panR));
		}
		break;
	case MIXER_ROUTING_DAC1_L:
	case MIXER_ROUTING_DAC2_L:
		for(i = 0; i < OUTPUT_DMA_SIZE; i++) mix[2*i] = sat16(mix[2*i] + data[i]);
		break;
	case MIXER_ROUTING_DAC1_R:
	case MIXER_ROUTING_DAC2_R:
		for(i = 0; i < OUTPUT_DMA_SIZE; i++) mix[2*i+1] = sat16(mix[2*i+1] + data[i]);
		break;
	default:
		for(i = 0; i < OUTPUT_DMA_SIZE; i++)
		{
			mix[2*i]   = sat16(mix[2*i]   + (int16_t)(data[i] * panL));
			mix[2*i+1] = sat16(mix[2*i+1] + (int16_t)(data[i] * panR));
		}
		break;
	}
}

void alexa_renderBlock(int16_t voiceOut[ALEXA_NUM_VOICES][ALEXA_BLOCK],
                       int16_t mixL[ALEXA_BLOCK], int16_t mixR[ALEXA_BLOCK],
                       uint8_t inMixMask)
{
	/* the same steps as mixer_calcNextSampleBlock() in the firmware */
	lfo_calcNextValue(&voiceArray[0].lfo);
	lfo_calcNextValue(&voiceArray[1].lfo);
	lfo_calcNextValue(&voiceArray[2].lfo);
	lfo_calcNextValue(&snareVoice.lfo);
	lfo_calcNextValue(&cymbalVoice.lfo);
	lfo_calcNextValue(&hatVoice.lfo);

	modNode_resetTargets();

	SVF_recalcFreq(&voiceArray[0].filter);
	SVF_recalcFreq(&voiceArray[1].filter);
	SVF_recalcFreq(&voiceArray[2].filter);
	SVF_recalcFreq(&snareVoice.filter);
	SVF_recalcFreq(&cymbalVoice.filter);
	SVF_recalcFreq(&hatVoice.filter);

	CV_process();

	calcDrumVoiceAsync(0);
	calcDrumVoiceAsync(1);
	calcDrumVoiceAsync(2);
	Snare_calcAsync();
	Cymbal_calcAsync();
	HiHat_calcAsync();

	int16_t mix[OUTPUT_DMA_SIZE*2];
	int16_t fxBusData[OUTPUT_DMA_SIZE*2];
	memset(mix, 0, sizeof(mix));
	memset(fxBusData, 0, sizeof(fxBusData));

	int v;
	for(v = 0; v < ALEXA_NUM_VOICES; v++)
	{
		int16_t* buf = voiceOut[v];
		float panL, panR;
		if(v < 3)
		{
			calcDrumVoiceSyncBlock((uint8_t)v, buf, OUTPUT_DMA_SIZE);
			panL = voiceArray[v].triggeredPanL; panR = voiceArray[v].triggeredPanR;
		}
		else if(v == 3)
		{
			Snare_calcSyncBlock(buf, OUTPUT_DMA_SIZE);
			panL = snareVoice.triggeredPanL; panR = snareVoice.triggeredPanR;
		}
		else if(v == 4)
		{
			Cymbal_calcSyncBlock(buf, OUTPUT_DMA_SIZE);
			panL = cymbalVoice.triggeredPanL; panR = cymbalVoice.triggeredPanR;
		}
		else
		{
			HiHat_calcSyncBlock(buf, OUTPUT_DMA_SIZE);
			panL = hatVoice.triggeredPanL; panR = hatVoice.triggeredPanR;
		}
		alexa_decimateBlock((uint8_t)v, buf);

		if(inMixMask & (1 << v))
			alexa_addToMix(mixer_audioRouting[v], panL, panR, buf, mix, fxBusData);
	}

	/* the FX bus adds its output to the mix; all of its routings land in the mix */
	FxBus_process(fxBusData, &mix[0], &mix[1], &mix[0], &mix[1], OUTPUT_DMA_SIZE);

	int i;
	for(i = 0; i < OUTPUT_DMA_SIZE; i++)
	{
		mixL[i] = mix[2*i];
		mixR[i] = mix[2*i+1];
	}
}

/* ------------------------------------------------------------------ */
/* state swapping for several modules                                  */
/* ------------------------------------------------------------------ */
typedef struct { void* ptr; size_t size; } AlexaRegion;

static AlexaRegion alexa_regions[] = {
	{ voiceArray, sizeof(voiceArray) },
	{ &snareVoice, sizeof(snareVoice) },
	{ &hatVoice, sizeof(hatVoice) },
	{ &cymbalVoice, sizeof(cymbalVoice) },
	{ &fxBus, sizeof(fxBus) },
	{ FX_BUFFER, sizeof(FX_BUFFER) },
	{ cvModulators, sizeof(cvModulators) },
	{ parameterArray, sizeof(Parameter) * PAR_ACTIVE_STEP },
	{ activeParameters, sizeof(activeParameters) },
	{ &preset_morphValue, sizeof(preset_morphValue) },
	{ mixer_audioRouting, sizeof(uint8_t) * 6 },
	{ mixer_decimation_rate, sizeof(float) * 7 },
	{ mixer_decimation_cnt, sizeof(float) * 6 },
	{ mixer_voice_samples, sizeof(int16_t) * 6 },
	{ midiParser_originalCcValues, 0xff },
	{ paramToModTarget, END_OF_SOUND_PARAMETERS },
	{ alexa_cvValues, sizeof(alexa_cvValues) },
	{ alexa_accent, sizeof(alexa_accent) },
	{ &alexa_matrixMorph, sizeof(alexa_matrixMorph) },
	{ &lxr_rngState, sizeof(lxr_rngState) },
};
#define ALEXA_NUM_REGIONS (sizeof(alexa_regions) / sizeof(alexa_regions[0]))

size_t alexa_stateSize(void)
{
	size_t total = 0, i;
	for(i = 0; i < ALEXA_NUM_REGIONS; i++) total += alexa_regions[i].size;
	return total;
}

void alexa_stateSave(uint8_t* dst)
{
	size_t i;
	for(i = 0; i < ALEXA_NUM_REGIONS; i++)
	{
		memcpy(dst, alexa_regions[i].ptr, alexa_regions[i].size);
		dst += alexa_regions[i].size;
	}
}

void alexa_stateLoad(const uint8_t* src)
{
	size_t i;
	for(i = 0; i < ALEXA_NUM_REGIONS; i++)
	{
		memcpy(alexa_regions[i].ptr, src, alexa_regions[i].size);
		src += alexa_regions[i].size;
	}
}
