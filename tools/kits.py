# Alexa02 factory kits: (menu title, 8 letter name, {parameter: value})
# Every kit starts from the firmware's init kit, so a kit only lists what it
# changes. Values are the raw 0..127 bytes the firmware stores.
KITS = [
    ("Boom: long sine kick, soft toms", "Boom", {
        "PAR_COARSE1": 27, "PAR_VELOD1": 52, "PAR_VOL_SLOPE1": 70, "PAR_MOD_EG1": 40, "PAR_MODAMNT1": 38,
        "PAR_COARSE2": 36, "PAR_VELOD2": 30, "PAR_MODAMNT2": 20,
        "PAR_COARSE3": 46, "PAR_VELOD3": 26, "PAR_MODAMNT3": 30, "PAR_MOD_EG3": 30,
        "PAR_OSC_WAVE_SNARE": 1, "PAR_MIX1": 80, "PAR_NOISE_FREQ1": 110, "PAR_VELOD4": 22, "PAR_FILTER_FREQ_4": 100,
        "PAR_FILTER_TYPE_6": 1, "PAR_FILTER_FREQ_6": 110, "PAR_VELOD6_CLOSED": 4, "PAR_VELOD6_OPEN": 30,
    }),
    ("Punch: short kick with click and drive", "Punch", {
        "PAR_COARSE1": 33, "PAR_VELOD1": 18, "PAR_MOD_EG1": 18, "PAR_MODAMNT1": 70, "PAR_DRIVE1": 95,
        "PAR_TRANS1_VOL": 80, "PAR_TRANS1_WAVE": 10, "PAR_TRANS1_FREQ": 70,
        "PAR_VELOD4": 16, "PAR_MIX1": 70, "PAR_TRANS4_VOL": 70, "PAR_TRANS4_WAVE": 11, "PAR_TRANS4_FREQ": 64,
        "PAR_SNARE_DISTORTION": 90,
        "PAR_FILTER_FREQ_6": 112, "PAR_VELOD6_CLOSED": 3, "PAR_VELOD6_OPEN": 22, "PAR_HAT_DISTORTION": 80,
    }),
    ("Electro: FM toms, clap, metal hats", "Electro", {
        "PAR_COARSE1": 30, "PAR_VELOD1": 30, "PAR_MOD_EG1": 22, "PAR_MODAMNT1": 60,
        "PAR_OSC_WAVE_DRUM2": 1, "PAR_COARSE2": 48, "PAR_VELOD2": 24, "PAR_FMAMNT2": 50, "PAR_FM_FREQ2": 70,
        "PAR_OSC_WAVE_DRUM3": 1, "PAR_COARSE3": 55, "PAR_VELOD3": 20, "PAR_FMAMNT3": 60, "PAR_FM_FREQ3": 90,
        "PAR_OSC_WAVE_SNARE": 4, "PAR_MIX1": 127, "PAR_REPEAT4": 3, "PAR_VELOD4": 24, "PAR_FILTER_TYPE_4": 2,
        "PAR_FILTER_FREQ_4": 92, "PAR_RESO_4": 30,
        "PAR_WAVE1_HH": 3, "PAR_WAVE2_HH": 3, "PAR_WAVE3_HH": 3, "PAR_MOD_OSC_F1": 80, "PAR_MOD_OSC_F2": 110,
        "PAR_FILTER_FREQ_6": 100, "PAR_RESO_6": 40, "PAR_VELOD6_CLOSED": 4, "PAR_VELOD6_OPEN": 26,
    }),
    ("Metal: FM clang and crushed noise", "Metal", {
        "PAR_COARSE1": 34, "PAR_VELOD1": 26, "PAR_FMAMNT1": 70, "PAR_FM_FREQ1": 37, "PAR_MOD_WAVE_DRUM1": 3,
        "PAR_OSC_WAVE_DRUM2": 2, "PAR_COARSE2": 62, "PAR_VELOD2": 34, "PAR_FMAMNT2": 90, "PAR_FM_FREQ2": 101,
        "PAR_OSC_WAVE_DRUM3": 3, "PAR_COARSE3": 70, "PAR_VELOD3": 14, "PAR_FMAMNT3": 110, "PAR_FM_FREQ3": 53,
        "PAR_VOICE_DECIMATION2": 60, "PAR_VOICE_DECIMATION3": 40,
        "PAR_OSC_WAVE_SNARE": 3, "PAR_MIX1": 60, "PAR_VELOD4": 14, "PAR_VOICE_DECIMATION4": 50,
        "PAR_WAVE1_CYM": 3, "PAR_WAVE2_CYM": 3, "PAR_WAVE3_CYM": 1, "PAR_MOD_OSC_F1_CYM": 90, "PAR_MOD_OSC_F2_CYM": 47,
        "PAR_MOD_OSC_GAIN1_CYM": 100, "PAR_MOD_OSC_GAIN2_CYM": 80, "PAR_VELOD5": 60,
        "PAR_FX_TYPE": 2, "PAR_FX_DRY_WET": 50, "PAR_FX_P1": 70, "PAR_AUDIO_OUT2": 6, "PAR_AUDIO_OUT5": 6,
    }),
    ("Dub: soft kick, rim snare, delay", "Dub", {
        "PAR_COARSE1": 29, "PAR_VELOD1": 34, "PAR_MODAMNT1": 30, "PAR_FILTER_FREQ_1": 90,
        "PAR_COARSE4": 64, "PAR_VELOD4": 8, "PAR_MIX1": 30, "PAR_TRANS4_VOL": 90, "PAR_TRANS4_WAVE": 6,
        "PAR_TRANS4_FREQ": 70, "PAR_AUDIO_OUT4": 6,
        "PAR_OSC_WAVE_DRUM3": 1, "PAR_COARSE3": 60, "PAR_VELOD3": 10, "PAR_AUDIO_OUT3": 6,
        "PAR_FILTER_FREQ_6": 96, "PAR_VELOD6_CLOSED": 6, "PAR_VELOD6_OPEN": 40,
        "PAR_FX_TYPE": 4, "PAR_FX_DRY_WET": 70, "PAR_FX_P1": 70, "PAR_FX_P2": 64, "PAR_FX_P3": 50,
        "PAR_FX_P4": 80, "PAR_FX_DELAY_TYPE": 1,
    }),
    ("Crush: lo-fi, driven filters", "Crush", {
        "PAR_COARSE1": 32, "PAR_VELOD1": 28, "PAR_FILTER_DRIVE_1": 60, "PAR_FILTER_FREQ_1": 70, "PAR_RESO_1": 60,
        "PAR_VOICE_DECIMATION_ALL": 45, "PAR_VELOD4": 20, "PAR_MIX1": 90, "PAR_FILTER_DRIVE_4": 70,
        "PAR_FILTER_TYPE_4": 2, "PAR_FILTER_FREQ_4": 85, "PAR_RESO_4": 50,
        "PAR_FILTER_FREQ_6": 90, "PAR_FILTER_DRIVE_6": 30, "PAR_VELOD6_CLOSED": 5, "PAR_VELOD6_OPEN": 28,
        "PAR_FX_TYPE": 3, "PAR_FX_DRY_WET": 127, "PAR_FX_P1": 10, "PAR_FX_P2": 50, "PAR_FX_P3": 40,
        "PAR_FX_P4": 55, "PAR_FX_RATIO": 4,
        "PAR_AUDIO_OUT1": 6, "PAR_AUDIO_OUT2": 6, "PAR_AUDIO_OUT3": 6, "PAR_AUDIO_OUT4": 6, "PAR_AUDIO_OUT5": 6, "PAR_AUDIO_OUT6": 6,
    }),
]
