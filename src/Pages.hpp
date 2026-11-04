// Alexa02: how the LXR parameters are grouped into pages, named and shown.
// The groups follow the firmware's own per-voice lists (modTargets in the
// firmware's Cc2Text.c), arranged so each page fits the six edit knobs.
#pragma once
#include <string>
#include <cstdio>
#include "ParamMeta.hpp"
#include "alexa_engine.h"

namespace lxr {

// ------------------------------------------------------------ choice lists
// names as the firmware shows them (MenuText.h), spelled out a little more
static const char* const LIST_WAVE[] = {"Sine", "Tri", "Saw", "Square", "Noise", "PWM"};
static const char* const LIST_MIXFM[] = {"FM", "Mix"};
static const char* const LIST_LFOWAVE[] = {"Sine", "Tri", "Saw up", "Saw dn", "Square", "Random", "Exp up", "Exp dn"};
static const char* const LIST_RETRIG[] = {"off", "Drum 1", "Drum 2", "Drum 3", "Snare", "Cymbal", "Hat"};
static const char* const LIST_SYNC[] = {"free", "4/1", "2/1", "1/1", "1/2", "1/3", "1/4", "1/6", "1/8", "1/12", "1/16", "1/32"};
static const char* const LIST_FILTER[] = {"LP", "HP", "BP", "Unity BP", "Notch", "Peak", "LP2", "off"};
static const char* const LIST_TRANS[] = {"Snap", "Offset", "Click", "Click 2", "Tick", "Kick", "Rim", "Drop", "Hat", "Clap", "Kick 2", "Snare", "Tom", "Snap 2"};
static const char* const LIST_OUT[] = {"Mix", "Mix 2", "Left", "Right", "Left 2", "Right 2", "FX"};
static const char* const LIST_OUTFX[] = {"Mix", "Mix 2", "Left", "Right", "Left 2", "Right 2"};
static const char* const LIST_FX[] = {"off", "Drive", "Ring", "Comp", "Delay"};
static const char* const LIST_DIST[] = {"Tube", "Fold", "Clip"};
static const char* const LIST_RATIO[] = {"1:1", "2:1", "3:1", "4:1", "6:1", "8:1"};
static const char* const LIST_REVERB[] = {"Plate"};
static const char* const LIST_DELAY[] = {"Normal", "PingPong"};
static const char* const LIST_CVSRC[] = {"CV 1", "CV 2", "CV 3", "CV 4", "CV 5",
	"LFO Dr1", "LFO Dr2", "LFO Dr3", "LFO Snr", "LFO Cym", "LFO Hat",
	"Acc Dr1", "Acc Dr2", "Acc Dr3", "Acc Snr", "Acc Cym", "Acc Hat"};

#define LXR_N(a) (int)(sizeof(a) / sizeof(a[0]))

inline int listCount(int list, const char* const** names) {
	switch (list) {
		case L_WAVE: *names = LIST_WAVE; return LXR_N(LIST_WAVE);
		case L_MIXFM: *names = LIST_MIXFM; return LXR_N(LIST_MIXFM);
		case L_LFOWAVE: *names = LIST_LFOWAVE; return LXR_N(LIST_LFOWAVE);
		case L_RETRIG: *names = LIST_RETRIG; return LXR_N(LIST_RETRIG);
		case L_SYNC: *names = LIST_SYNC; return LXR_N(LIST_SYNC);
		case L_FILTER: *names = LIST_FILTER; return LXR_N(LIST_FILTER);
		case L_TRANS: *names = LIST_TRANS; return LXR_N(LIST_TRANS);
		case L_OUT: *names = LIST_OUT; return LXR_N(LIST_OUT);
		case L_OUTFX: *names = LIST_OUTFX; return LXR_N(LIST_OUTFX);
		case L_FX: *names = LIST_FX; return LXR_N(LIST_FX);
		case L_DIST: *names = LIST_DIST; return LXR_N(LIST_DIST);
		case L_RATIO: *names = LIST_RATIO; return LXR_N(LIST_RATIO);
		case L_REVERB: *names = LIST_REVERB; return LXR_N(LIST_REVERB);
		case L_DELAY: *names = LIST_DELAY; return LXR_N(LIST_DELAY);
		case L_CVSRC: *names = LIST_CVSRC; return LXR_N(LIST_CVSRC);
		default: *names = nullptr; return 128;
	}
}

// highest value a parameter can take
inline int paramMax(int p) {
	if (p < 0 || p >= KIT_SIZE) return 127;
	const ParamKindInfo& k = PARAM_KINDS[p];
	switch (k.kind) {
		case K_ONOFF: return 1;
		case K_PCT: return 100;
		case K_TARGET: return alexa_numModTargets() - 1;
		case K_MENU: {
			const char* const* n;
			return listCount(k.list, &n) - 1;
		}
		default: return 127;
	}
}

// ------------------------------------------------------------ pages
enum Page { PG_OSC, PG_AMP, PG_PITCH, PG_FM, PG_CLICK, PG_FILTER, PG_LFO, PG_MIX, PG_FX, PG_MOD, NUM_PAGES };
static const char* const PAGE_NAMES[NUM_PAGES] = {"OSC", "AMP", "PITCH", "FM", "CLICK", "FILTER", "LFO", "MIX", "FX", "MOD"};
static const char* const VOICE_NAMES[6] = {"DRUM 1", "DRUM 2", "DRUM 3", "SNARE", "CYMBAL", "HAT"};
static const char* const VOICE_SHORT[6] = {"Dr1", "Dr2", "Dr3", "Snr", "Cym", "Hat"};

struct Item {
	int param;          // -1 = empty slot
	const char* label;  // short, fits above a knob
	const char* longName;
};

struct PageDef {
	Item items[6];
};

static const Item NONE = {-1, "", ""};

// the voice pages, built once
struct VoicePages {
	PageDef pages[6][PG_MIX + 1];
	int paramVoice[KIT_SIZE];  // which voice a parameter belongs to (-1 = whole kit)

	VoicePages() {
		for (int v = 0; v < 6; v++)
			for (int p = 0; p <= PG_MIX; p++)
				for (int i = 0; i < 6; i++)
					pages[v][p].items[i] = NONE;

		for (int v = 0; v < 3; v++) {
			set(v, PG_OSC, {{PAR_COARSE1 + 2 * v, "TUNE", "Coarse tune"}, {PAR_FINE1 + 2 * v, "FINE", "Fine tune"},
				{PAR_OSC_WAVE_DRUM1 + v, "WAVE", "Oscillator wave"}, {PAR_PWM_1 + v, "PWM", "Pulse width"}});
			set(v, PG_AMP, {{PAR_VELOA1 + 2 * v, "ATTACK", "Amp attack"}, {PAR_VELOD1 + 2 * v, "DECAY", "Amp decay"},
				{PAR_VOL_SLOPE1 + v, "SLOPE", "Amp decay shape"}, {PAR_VOLUME_MOD_ON_OFF1 + v, "ACC>VOL", "Accent sets level"}});
			set(v, PG_PITCH, {{PAR_MOD_EG1 + v, "DECAY", "Pitch decay"}, {PAR_PITCH_SLOPE1 + v, "SLOPE", "Pitch decay shape"},
				{PAR_MODAMNT1 + v, "AMOUNT", "Pitch sweep amount"}});
			set(v, PG_FM, {{PAR_FMAMNT1 + 2 * v, "AMOUNT", "FM amount"}, {PAR_FM_FREQ1 + 2 * v, "RATIO", "Modulator tune"},
				{PAR_MOD_WAVE_DRUM1 + v, "WAVE", "Modulator wave"}, {PAR_MIX_MOD_1 + v, "MODE", "FM or mix"}});
		}
		// snare
		set(3, PG_OSC, {{PAR_COARSE4, "TUNE", "Coarse tune"}, {PAR_FINE4, "FINE", "Fine tune"},
			{PAR_NOISE_FREQ1, "NOISE", "Noise colour"}, {PAR_MIX1, "MIX", "Tone/noise mix"},
			{PAR_OSC_WAVE_SNARE, "WAVE", "Oscillator wave"}, {PAR_PWM_4, "PWM", "Pulse width"}});
		set(3, PG_AMP, {{PAR_VELOA4, "ATTACK", "Amp attack"}, {PAR_VELOD4, "DECAY", "Amp decay"},
			{PAR_REPEAT4, "REPEAT", "Clap repeats"}, {PAR_VOL_SLOPE4, "SLOPE", "Amp decay shape"},
			{PAR_VOLUME_MOD_ON_OFF4, "ACC>VOL", "Accent sets level"}});
		set(3, PG_PITCH, {{PAR_MOD_EG4, "DECAY", "Pitch decay"}, {PAR_PITCH_SLOPE4, "SLOPE", "Pitch decay shape"},
			{PAR_MODAMNT4, "AMOUNT", "Pitch sweep amount"}});
		// cymbal
		set(4, PG_OSC, {{PAR_COARSE5, "TUNE", "Coarse tune"}, {PAR_FINE5, "FINE", "Fine tune"},
			{PAR_WAVE1_CYM, "WAVE", "Oscillator wave"}, {PAR_PWM_5, "PWM", "Pulse width"}});
		set(4, PG_AMP, {{PAR_VELOA5, "ATTACK", "Amp attack"}, {PAR_VELOD5, "DECAY", "Amp decay"},
			{PAR_REPEAT5, "REPEAT", "Repeats"}, {PAR_VOL_SLOPE5, "SLOPE", "Amp decay shape"},
			{PAR_VOLUME_MOD_ON_OFF5, "ACC>VOL", "Accent sets level"}});
		set(4, PG_FM, {{PAR_MOD_OSC_F1_CYM, "TUNE 2", "Osc 2 tune"}, {PAR_MOD_OSC_F2_CYM, "TUNE 3", "Osc 3 tune"},
			{PAR_MOD_OSC_GAIN1_CYM, "FM 2", "Osc 2 FM amount"}, {PAR_MOD_OSC_GAIN2_CYM, "FM 3", "Osc 3 FM amount"},
			{PAR_WAVE2_CYM, "WAVE 2", "Osc 2 wave"}, {PAR_WAVE3_CYM, "WAVE 3", "Osc 3 wave"}});
		// hat
		set(5, PG_OSC, {{PAR_COARSE6, "TUNE", "Coarse tune"}, {PAR_FINE6, "FINE", "Fine tune"},
			{PAR_WAVE1_HH, "WAVE", "Oscillator wave"}, {PAR_PWM_6, "PWM", "Pulse width"}});
		set(5, PG_AMP, {{PAR_VELOA6, "ATTACK", "Amp attack"}, {PAR_VELOD6_CLOSED, "CLOSED", "Closed hat decay"},
			{PAR_VELOD6_OPEN, "OPEN", "Open hat decay"}, {PAR_VOL_SLOPE6, "SLOPE", "Amp decay shape"},
			{PAR_VOLUME_MOD_ON_OFF6, "ACC>VOL", "Accent sets level"}});
		set(5, PG_FM, {{PAR_MOD_OSC_F1, "TUNE 2", "Osc 2 tune"}, {PAR_MOD_OSC_F2, "TUNE 3", "Osc 3 tune"},
			{PAR_MOD_OSC_GAIN1, "FM 2", "Osc 2 FM amount"}, {PAR_MOD_OSC_GAIN2, "FM 3", "Osc 3 FM amount"},
			{PAR_WAVE2_HH, "WAVE 2", "Osc 2 wave"}, {PAR_WAVE3_HH, "WAVE 3", "Osc 3 wave"}});

		static const int driveParam[6] = {PAR_DRIVE1, PAR_DRIVE2, PAR_DRIVE3, PAR_SNARE_DISTORTION, PAR_CYMBAL_DISTORTION, PAR_HAT_DISTORTION};
		for (int v = 0; v < 6; v++) {
			set(v, PG_CLICK, {{PAR_TRANS1_WAVE + v, "TYPE", "Click sample"}, {PAR_TRANS1_VOL + v, "LEVEL", "Click level"},
				{PAR_TRANS1_FREQ + v, "PITCH", "Click pitch"}});
			set(v, PG_FILTER, {{PAR_FILTER_FREQ_1 + v, "CUTOFF", "Filter cutoff"}, {PAR_RESO_1 + v, "RESO", "Filter resonance"},
				{PAR_FILTER_TYPE_1 + v, "TYPE", "Filter type"}, {PAR_FILTER_DRIVE_1 + v, "DRIVE", "Filter drive"}});
			set(v, PG_LFO, {{PAR_FREQ_LFO1 + v, "RATE", "LFO rate"}, {PAR_SYNC_LFO1 + v, "SYNC", "LFO clock sync"},
				{PAR_AMOUNT_LFO1 + v, "DEPTH", "LFO depth"}, {PAR_WAVE_LFO1 + v, "WAVE", "LFO wave"},
				{PAR_RETRIGGER_LFO1 + v, "RESET", "LFO reset by voice"}, {PAR_OFFSET_LFO1 + v, "PHASE", "LFO start phase"}});
			set(v, PG_MIX, {{PAR_VOL1 + v, "LEVEL", "Voice level"}, {PAR_PAN1 + panOffset(v), "PAN", "Pan"},
				{PAR_VOICE_DECIMATION1 + v, "S.RATE", "Sample rate"}, {driveParam[v], "DRIVE", "Voice drive"},
				{PAR_AUDIO_OUT1 + v, "ROUTE", "Goes to mix or FX"}});
		}

		for (int p = 0; p < KIT_SIZE; p++) paramVoice[p] = -1;
		for (int v = 0; v < 6; v++)
			for (int p = 0; p <= PG_MIX; p++)
				for (int i = 0; i < 6; i++)
					if (pages[v][p].items[i].param >= 0) paramVoice[pages[v][p].items[i].param] = v;
	}

	// pan parameters are not contiguous (two NRPN slots sit between pan 3 and pan 4)
	static int panOffset(int v) {
		return v < 3 ? v : (PAR_PAN4 - PAR_PAN1) + (v - 3);
	}

	void set(int v, int page, std::initializer_list<Item> items) {
		int i = 0;
		for (const Item& it : items) pages[v][page].items[i++] = it;
	}
};

inline const VoicePages& voicePages() {
	static VoicePages vp;
	return vp;
}

// the FX page, by part (part 0: main knobs, part 1: the rest)
inline PageDef fxPage(int fxType, int part) {
	PageDef d;
	for (int i = 0; i < 6; i++) d.items[i] = NONE;
	if (part == 1) {
		d.items[0] = {PAR_FX_ROUTING, "OUT", "FX output"};
		if (fxType == 1) d.items[1] = {PAR_FX_DIST_TYPE, "SHAPE", "Distortion type"};
		if (fxType == 2) d.items[1] = {PAR_FX_WAVE, "WAVE", "Ring mod wave"};
		if (fxType == 3) d.items[1] = {PAR_FX_RATIO, "RATIO", "Compressor ratio"};
		if (fxType == 4) d.items[1] = {PAR_FX_DELAY_TYPE, "MODE", "Delay mode"};
		return d;
	}
	d.items[0] = {PAR_FX_TYPE, "TYPE", "FX type"};
	d.items[1] = {PAR_FX_DRY_WET, "WET", "FX dry/wet"};
	switch (fxType) {
		case 1:
			d.items[2] = {PAR_FX_P1, "DRIVE", "Drive"}; d.items[3] = {PAR_FX_P2, "COLOR", "Colour"};
			d.items[4] = {PAR_FX_P3, "TONE", "Tone"}; d.items[5] = {PAR_FX_P4, "LEVEL", "Output level"};
			break;
		case 2:
			d.items[2] = {PAR_FX_P1, "FREQ", "Ring mod frequency"};
			break;
		case 3:
			d.items[2] = {PAR_FX_P1, "ATTACK", "Attack"}; d.items[3] = {PAR_FX_P2, "RELEASE", "Release"};
			d.items[4] = {PAR_FX_P3, "THRESH", "Threshold"}; d.items[5] = {PAR_FX_P4, "GAIN", "Make-up gain"};
			break;
		case 4:
			d.items[2] = {PAR_FX_P1, "TIME", "Delay time"}; d.items[3] = {PAR_FX_P2, "RANGE", "Time range"};
			d.items[4] = {PAR_FX_P3, "TONE", "Feedback tone"}; d.items[5] = {PAR_FX_P4, "FEEDBK", "Feedback"};
			break;
		default: break;
	}
	return d;
}

static const int MOD_PARTS = 9;  // 18 slots, 2 per part

inline PageDef modPage(int part) {
	PageDef d;
	for (int k = 0; k < 2; k++) {
		const int s = part * 2 + k;
		static char labels[18][3][16];
		snprintf(labels[s][0], 16, "SRC %d", s + 1);
		snprintf(labels[s][1], 16, "DEST %d", s + 1);
		snprintf(labels[s][2], 16, "AMT %d", s + 1);
		d.items[k * 3 + 0] = {PAR_CV_SRC_1 + s, labels[s][0], "Mod source"};
		d.items[k * 3 + 1] = {PAR_CV_DEST_1 + s, labels[s][1], "Mod destination"};
		d.items[k * 3 + 2] = {PAR_CV_MOD_AMT_1 + s, labels[s][2], "Mod amount"};
	}
	return d;
}

inline int pageParts(int page) {
	if (page == PG_FX) return 2;
	if (page == PG_MOD) return MOD_PARTS;
	return 1;
}

inline PageDef getPage(int voice, int page, int part, int fxType) {
	if (page == PG_FX) return fxPage(fxType, part);
	if (page == PG_MOD) return modPage(part);
	return voicePages().pages[voice][page];
}

// ------------------------------------------------------------ names and values
// a short name for a parameter, for mod destinations: "Dr1 TUNE"
inline std::string paramShortName(int p) {
	if (p == PAR_MORPH) return "MORPH";
	if (p == PAR_VOICE_DECIMATION_ALL) return "All S.RATE";
	if (p >= PAR_CV_1VOCT_1 && p <= PAR_CV_1VOCT_6) return std::string(VOICE_SHORT[p - PAR_CV_1VOCT_1]) + " V/OCT";
	const VoicePages& vp = voicePages();
	if (p >= 0 && p < KIT_SIZE && vp.paramVoice[p] >= 0) {
		const int v = vp.paramVoice[p];
		for (int pg = 0; pg <= PG_MIX; pg++)
			for (int i = 0; i < 6; i++)
				if (vp.pages[v][pg].items[i].param == p)
					return std::string(VOICE_SHORT[v]) + " " + PAGE_NAMES[pg] + " " + vp.pages[v][pg].items[i].label;
	}
	if (p >= PAR_FX_P1 && p <= PAR_FX_P4) {
		char b[16];
		snprintf(b, sizeof b, "FX P%d", p - PAR_FX_P1 + 1);
		return b;
	}
	return "?";
}

inline std::string targetName(int t) {
	if (t <= 0) return "off";
	return paramShortName(alexa_modTargetParam((uint8_t)t));
}

// the value as the firmware would show it
inline std::string valueText(int p, int value) {
	char b[32];
	if (p < 0 || p >= KIT_SIZE) return "";
	const ParamKindInfo& k = PARAM_KINDS[p];
	switch (k.kind) {
		case K_ONOFF: return value ? "on" : "off";
		case K_PCT: snprintf(b, sizeof b, "%d", value); return b;
		case K_TARGET: return targetName(value);
		case K_MENU: {
			const char* const* names;
			const int n = listCount(k.list, &names);
			if (names && value >= 0 && value < n) return names[value];
			snprintf(b, sizeof b, "%d", value);
			return b;
		}
		case K_PM63: {
			const bool semis = (p == PAR_COARSE1 || p == PAR_COARSE2 || p == PAR_COARSE3 || p == PAR_COARSE4 ||
				p == PAR_COARSE5 || p == PAR_COARSE6 || p == PAR_FM_FREQ1 || p == PAR_FM_FREQ2 || p == PAR_FM_FREQ3 ||
				p == PAR_MOD_OSC_F1_CYM || p == PAR_MOD_OSC_F2_CYM || p == PAR_MOD_OSC_F1 || p == PAR_MOD_OSC_F2);
			snprintf(b, sizeof b, "%+d", value - (semis ? 60 : 63));
			return b;
		}
		default: snprintf(b, sizeof b, "%d", value); return b;
	}
}

// the firmware's morph interpolation (presetManager.c)
inline uint8_t interpolate(uint8_t a, uint8_t b, uint8_t x) {
	const uint16_t fixedPointValue = (uint16_t)(((a * 256) + (b - a) * x));
	const uint8_t result = (uint8_t)(fixedPointValue / 256);
	return (uint8_t)((fixedPointValue & 0xff) < 0x7f ? result : result + 1);
}

} // namespace lxr
