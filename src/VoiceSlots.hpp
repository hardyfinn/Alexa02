// Alexa01: the 32 knobs and which LXR parameter each one sets, for each of
// the four voice types. The slot order matches tools/make_panel_alexa01.py.
#pragma once
#include "Pages.hpp"

namespace lxr {

enum VoiceType { VT_DRUM, VT_SNARE, VT_CYMBAL, VT_HAT, NUM_VOICE_TYPES };
static const char* const VOICE_TYPE_NAMES[NUM_VOICE_TYPES] = {"Drum", "Snare", "Cymbal", "Hat"};
// the engine voice each type plays (drums use Drum 1)
static const int VOICE_OF_TYPE[NUM_VOICE_TYPES] = {0, 3, 4, 5};

static const int NUM_SLOTS = 32;

struct SlotDef {
	const char* section;
	const char* label;
	const char* longName;
	int param[NUM_VOICE_TYPES];  // -1 = not used by that type
	int drumStride;              // parameter step from Drum 1 to Drum 2 (for loading drums 2 and 3)
};

static const SlotDef SLOTS[NUM_SLOTS] = {
	{"OSC", "TUNE", "Tune", {PAR_COARSE1, PAR_COARSE4, PAR_COARSE5, PAR_COARSE6}, 2},
	{"OSC", "FINE", "Fine tune", {PAR_FINE1, PAR_FINE4, PAR_FINE5, PAR_FINE6}, 2},
	{"OSC", "WAVE", "Wave", {PAR_OSC_WAVE_DRUM1, PAR_OSC_WAVE_SNARE, PAR_WAVE1_CYM, PAR_WAVE1_HH}, 1},
	{"OSC", "PWM", "Pulse width", {PAR_PWM_1, PAR_PWM_4, PAR_PWM_5, PAR_PWM_6}, 1},
	{"OSC", "NOISE", "Noise colour", {-1, PAR_NOISE_FREQ1, -1, -1}, 0},
	{"OSC", "MIX", "Tone/noise mix", {-1, PAR_MIX1, -1, -1}, 0},

	{"AMP", "ATTACK", "Attack", {PAR_VELOA1, PAR_VELOA4, PAR_VELOA5, PAR_VELOA6}, 2},
	{"AMP", "DECAY", "Decay", {PAR_VELOD1, PAR_VELOD4, PAR_VELOD5, PAR_VELOD6_CLOSED}, 2},
	{"AMP", "OPEN", "Open hat decay", {-1, -1, -1, PAR_VELOD6_OPEN}, 0},
	{"AMP", "SLOPE", "Decay shape", {PAR_VOL_SLOPE1, PAR_VOL_SLOPE4, PAR_VOL_SLOPE5, PAR_VOL_SLOPE6}, 1},
	{"AMP", "REPEAT", "Repeats (clap)", {-1, PAR_REPEAT4, PAR_REPEAT5, -1}, 0},
	{"AMP", "ACCENT", "Accent sets level", {PAR_VOLUME_MOD_ON_OFF1, PAR_VOLUME_MOD_ON_OFF4, PAR_VOLUME_MOD_ON_OFF5, PAR_VOLUME_MOD_ON_OFF6}, 1},

	{"PITCH", "DECAY", "Pitch decay", {PAR_MOD_EG1, PAR_MOD_EG4, -1, -1}, 1},
	{"PITCH", "SLOPE", "Pitch decay shape", {PAR_PITCH_SLOPE1, PAR_PITCH_SLOPE4, -1, -1}, 1},
	{"PITCH", "AMOUNT", "Pitch sweep amount", {PAR_MODAMNT1, PAR_MODAMNT4, -1, -1}, 1},
	{"CLICK", "TYPE", "Click type", {PAR_TRANS1_WAVE, PAR_TRANS4_WAVE, PAR_TRANS5_WAVE, PAR_TRANS6_WAVE}, 1},
	{"CLICK", "LEVEL", "Click level", {PAR_TRANS1_VOL, PAR_TRANS4_VOL, PAR_TRANS5_VOL, PAR_TRANS6_VOL}, 1},
	{"CLICK", "PITCH", "Click pitch", {PAR_TRANS1_FREQ, PAR_TRANS4_FREQ, PAR_TRANS5_FREQ, PAR_TRANS6_FREQ}, 1},

	{"FM", "TUNE 2", "Osc 2 tune (drum: modulator)", {PAR_FM_FREQ1, -1, PAR_MOD_OSC_F1_CYM, PAR_MOD_OSC_F1}, 2},
	{"FM", "FM 2", "Osc 2 FM amount", {PAR_FMAMNT1, -1, PAR_MOD_OSC_GAIN1_CYM, PAR_MOD_OSC_GAIN1}, 2},
	{"FM", "WAVE 2", "Osc 2 wave", {PAR_MOD_WAVE_DRUM1, -1, PAR_WAVE2_CYM, PAR_WAVE2_HH}, 1},
	{"FM", "TUNE 3", "Osc 3 tune", {-1, -1, PAR_MOD_OSC_F2_CYM, PAR_MOD_OSC_F2}, 0},
	{"FM", "FM 3", "Osc 3 FM amount", {-1, -1, PAR_MOD_OSC_GAIN2_CYM, PAR_MOD_OSC_GAIN2}, 0},
	{"FM", "WAVE 3", "Osc 3 wave", {-1, -1, PAR_WAVE3_CYM, PAR_WAVE3_HH}, 0},
	{"FM", "FM/MIX", "FM or mix the two oscillators", {PAR_MIX_MOD_1, -1, -1, -1}, 1},

	{"FILTER", "CUTOFF", "Filter cutoff", {PAR_FILTER_FREQ_1, PAR_FILTER_FREQ_4, PAR_FILTER_FREQ_5, PAR_FILTER_FREQ_6}, 1},
	{"FILTER", "RESO", "Filter resonance", {PAR_RESO_1, PAR_RESO_4, PAR_RESO_5, PAR_RESO_6}, 1},
	{"FILTER", "TYPE", "Filter type", {PAR_FILTER_TYPE_1, PAR_FILTER_TYPE_4, PAR_FILTER_TYPE_5, PAR_FILTER_TYPE_6}, 1},
	{"FILTER", "DRIVE", "Filter drive", {PAR_FILTER_DRIVE_1, PAR_FILTER_DRIVE_4, PAR_FILTER_DRIVE_5, PAR_FILTER_DRIVE_6}, 1},
	{"MIX", "LEVEL", "Level", {PAR_VOL1, PAR_VOL4, PAR_VOL5, PAR_VOL6}, 1},
	{"MIX", "S.RATE", "Sample rate", {PAR_VOICE_DECIMATION1, PAR_VOICE_DECIMATION4, PAR_VOICE_DECIMATION5, PAR_VOICE_DECIMATION6}, 1},
	{"MIX", "DRIVE", "Drive", {PAR_DRIVE1, PAR_SNARE_DISTORTION, PAR_CYMBAL_DISTORTION, PAR_HAT_DISTORTION}, 1},
};

// the parameter a slot sets for one of the kit's six voices (0-2 drums, 3 snare, 4 cymbal, 5 hat)
inline int slotParamForKitVoice(int slot, int kitVoice) {
	if (kitVoice < 3) {
		const int p = SLOTS[slot].param[VT_DRUM];
		return p < 0 ? -1 : p + SLOTS[slot].drumStride * kitVoice;
	}
	return SLOTS[slot].param[kitVoice - 2];
}
inline int typeOfKitVoice(int kitVoice) { return kitVoice < 3 ? VT_DRUM : kitVoice - 2; }

} // namespace lxr
