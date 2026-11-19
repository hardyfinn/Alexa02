// Alexa01: one voice of Alexa02 (drum, snare, cymbal or hat) with every
// setting on its own knob, six memories, and morphing from the knobs to a
// memory or through all six.
#include "plugin.hpp"
#include "Layout01.hpp"
#include "VoiceSlots.hpp"
#include "Kits.hpp"
#include "FactoryKits.hpp"
#include "Engine.hpp"
#include <osdialog.h>
#include <mutex>
#include <random>

using namespace lxr;

struct Alexa01 : Module, EngineUser {
	enum ParamId {
		ENUMS(SLOT_PARAM, NUM_SLOTS),
		ENUMS(TYPE_PARAM, NUM_VOICE_TYPES),
		ENUMS(MEM_PARAM, 6),
		STORE_PARAM,
		RECALL_PARAM,
		MUTATE_PARAM,
		PLAY_PARAM,
		MORPH_PARAM,
		MORPH_ATT_PARAM,
		MODE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		TRIG_INPUT,
		OPEN_INPUT,
		ACC_INPUT,
		VOCT_INPUT,
		MORPH_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(TYPE_LIGHT, NUM_VOICE_TYPES),
		ENUMS(MEM_LIGHT, 6),
		STORE_LIGHT,
		RECALL_LIGHT,
		PLAY_LIGHT,
		LIGHTS_LEN
	};

	struct Memory {
		bool used = false;
		int type = VT_DRUM;
		float v[NUM_SLOTS] = {};
	};

	int type = VT_DRUM;
	float knobsByType[NUM_VOICE_TYPES][NUM_SLOTS] = {};  // knob positions kept for each type
	Memory mem[6];
	int dest = 0;               // the memory MORPH heads for, and MUTATE writes
	int armed = 0;              // 0 = none, 1 = STORE, 2 = RECALL (waiting for a memory button)
	float mutateAmount = 1.f;
	int lastSlot = -1;          // the knob turned last, for the screen
	float lastKnob[NUM_SLOTS] = {};

	int16_t sent[KIT_SIZE];     // what the engine has now (-1 = must send)
	int engineVoice = 0;
	bool vOctPatched = false;
	float morphShown = 0.f;
	float morphPos[NUM_SLOTS] = {};  // the values playing now, 0..1, for the screen

	int16_t vbuf[6][ALEXA_BLOCK];
	int16_t lbuf[ALEXA_BLOCK], rbuf[ALEXA_BLOCK];
	int bufPos = ALEXA_BLOCK;
	float hist[4] = {};
	double phase = 0.0;

	dsp::SchmittTrigger trigIn, openIn;
	bool pendingTrig = false, pendingOpen = false;
	uint8_t pendingVel = 63;
	dsp::BooleanTrigger typeBtn[NUM_VOICE_TYPES], memBtn[6], storeBtn, recallBtn, mutateBtn, playBtn;
	dsp::PulseGenerator hitPulse;
	float blink = 0.f;

	std::mutex requestMutex;
	std::vector<std::function<void()>> requests;
	std::mt19937 rng{std::random_device{}()};

	Alexa01() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		struct SlotQuantity : ParamQuantity {
			int slot() { return paramId - SLOT_PARAM; }
			std::string getLabel() override {
				Alexa01* m = dynamic_cast<Alexa01*>(module);
				const SlotDef& d = SLOTS[slot()];
				std::string name = std::string(d.section) + " " + d.longName;
				if (m && d.param[m->type] < 0) name += std::string(" (not used by the ") + VOICE_TYPE_NAMES[m->type] + ")";
				return name;
			}
			std::string getDisplayValueString() override {
				Alexa01* m = dynamic_cast<Alexa01*>(module);
				const int t = m ? m->type : VT_DRUM;
				const int p = SLOTS[slot()].param[t];
				if (p < 0) return "-";
				return valueText(p, Alexa01::toByte(p, getValue()));
			}
			std::string getUnit() override { return ""; }
		};
		for (int s = 0; s < NUM_SLOTS; s++) {
			configParam<SlotQuantity>(SLOT_PARAM + s, 0.f, 1.f, 0.f, SLOTS[s].longName);
		}
		for (int t = 0; t < NUM_VOICE_TYPES; t++) configButton(TYPE_PARAM + t, std::string("Voice type: ") + VOICE_TYPE_NAMES[t]);
		for (int i = 0; i < 6; i++) configButton(MEM_PARAM + i, "Memory " + std::to_string(i + 1) + " (choose as morph target; after STORE or RECALL, store or recall it)");
		configButton(STORE_PARAM, "Store: then press a memory to save the knobs into it");
		configButton(RECALL_PARAM, "Recall: then press a memory to load it onto the knobs");
		configButton(MUTATE_PARAM, "Mutate: put a variation of the knobs into the chosen memory");
		configButton(PLAY_PARAM, "Play");
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "Morph", "%", 0.f, 100.f);
		configParam(MORPH_ATT_PARAM, -1.f, 1.f, 0.f, "Morph CV amount", "%", 0.f, 100.f);
		configSwitch(MODE_PARAM, 0.f, 1.f, 1.f, "Morph mode", {"Scan: knobs, then memories 1 to 6", "Memory: knobs to the chosen memory"});
		configInput(TRIG_INPUT, "Trigger (closed hat for the hat)");
		configInput(OPEN_INPUT, "Open hat trigger (hat only; others play as TRIG)");
		configInput(ACC_INPUT, "Accent (0-5V)");
		configInput(VOCT_INPUT, "1V/oct (0V = C4)");
		configInput(MORPH_INPUT, "Morph CV (10V = full)");
		configOutput(OUT_OUTPUT, "Voice");

		for (int i = 0; i < KIT_SIZE; i++) sent[i] = -1;
		for (int t = 0; t < NUM_VOICE_TYPES; t++) initKnobs(t, knobsByType[t]);
		for (int s = 0; s < NUM_SLOTS; s++) {
			params[SLOT_PARAM + s].setValue(knobsByType[VT_DRUM][s]);
			paramQuantities[SLOT_PARAM + s]->defaultValue = knobsByType[VT_DRUM][s];
			lastKnob[s] = knobsByType[VT_DRUM][s];
		}
		initEngine();
	}

	// ------------------------------------------------------------ values
	static uint8_t toByte(int p, float v) {
		const int mx = std::max(1, paramMax(p));
		return (uint8_t)clamp((int)std::lround(clamp(v, 0.f, 1.f) * mx), 0, mx);
	}
	static float fromByte(int p, int b) {
		return clamp(b / (float)std::max(1, paramMax(p)), 0.f, 1.f);
	}

	static const Kit& initKit() {
		static Kit k;
		static bool done = false;
		if (!done) { k.fromBytes(FACTORY_KITS[0].data, FACTORY_KITS[0].size); done = true; }
		return k;
	}

	// knob positions for a voice of a kit
	static void knobsFromKit(const Kit& k, int kitVoice, float* out) {
		for (int s = 0; s < NUM_SLOTS; s++) {
			const int p = slotParamForKitVoice(s, kitVoice);
			out[s] = p >= 0 ? fromByte(p, k.p[p]) : 0.f;
		}
	}
	static void initKnobs(int t, float* out) { knobsFromKit(initKit(), VOICE_OF_TYPE[t], out); }

	void readKnobs(float* out) {
		for (int s = 0; s < NUM_SLOTS; s++) out[s] = params[SLOT_PARAM + s].getValue();
	}
	void setKnobs(const float* v) {
		for (int s = 0; s < NUM_SLOTS; s++) {
			params[SLOT_PARAM + s].setValue(v[s]);
			lastKnob[s] = v[s];
		}
	}

	// switch voice type; the knobs of each type are kept, so switching back restores them
	void setType(int t, const float* knobs = nullptr) {
		readKnobs(knobsByType[type]);
		type = t;
		if (knobs) std::copy(knobs, knobs + NUM_SLOTS, knobsByType[t]);
		setKnobs(knobsByType[t]);
		float init[NUM_SLOTS];
		initKnobs(t, init);
		for (int s = 0; s < NUM_SLOTS; s++) paramQuantities[SLOT_PARAM + s]->defaultValue = init[s];
		for (int i = 0; i < KIT_SIZE; i++) sent[i] = -1;
	}

	void onReset() override {
		for (int t = 0; t < NUM_VOICE_TYPES; t++) initKnobs(t, knobsByType[t]);
		type = VT_DRUM;
		setKnobs(knobsByType[VT_DRUM]);
		for (int i = 0; i < 6; i++) mem[i] = Memory();
		dest = 0; armed = 0;
		params[MORPH_PARAM].setValue(0.f);
		for (int i = 0; i < KIT_SIZE; i++) sent[i] = -1;
	}

	void request(std::function<void()> f) {
		std::lock_guard<std::mutex> lock(requestMutex);
		requests.push_back(f);
	}

	// ------------------------------------------------------------ memories
	void store(int i) {
		mem[i].used = true;
		mem[i].type = type;
		readKnobs(mem[i].v);
	}
	void recall(int i) {
		if (!mem[i].used) return;
		setType(mem[i].type, mem[i].v);
	}
	// a variation of the knobs; level is left alone
	void mutateInto(int i, float amount) {
		float k[NUM_SLOTS];
		readKnobs(k);
		std::normal_distribution<float> gauss(0.f, 1.f);
		std::uniform_real_distribution<float> uni(0.f, 1.f);
		for (int s = 0; s < NUM_SLOTS; s++) {
			const int p = SLOTS[s].param[type];
			if (p < 0 || s == 29) continue;  // 29 = MIX LEVEL
			const ParamKindInfo& kind = PARAM_KINDS[p];
			if (kind.kind == K_MENU || kind.kind == K_ONOFF) {
				if (uni(rng) < 0.2f * amount) {
					const int mx = paramMax(p);
					k[s] = fromByte(p, (int)(uni(rng) * (mx + 1)));
				}
			}
			else k[s] = clamp(k[s] + gauss(rng) * (22.f / 127.f) * amount, 0.f, 1.f);
		}
		mem[i].used = true;
		mem[i].type = type;
		std::copy(k, k + NUM_SLOTS, mem[i].v);
	}

	// ------------------------------------------------------------ morph
	// the value of every knob slot, 0..1, at morph position m
	void morphedValues(float m, float* out) {
		float knobs[NUM_SLOTS];
		readKnobs(knobs);
		const bool scan = params[MODE_PARAM].getValue() < 0.5f;
		// the points the morph travels through: the knobs, then the target memory
		// (memory mode) or every stored memory in turn (scan mode)
		const float* pts[7];
		int n = 0;
		pts[n++] = knobs;
		if (scan) {
			for (int i = 0; i < 6; i++)
				if (mem[i].used) pts[n++] = mem[i].v;
		}
		else if (mem[dest].used) pts[n++] = mem[dest].v;
		if (n == 1 || m <= 0.f) {
			std::copy(knobs, knobs + NUM_SLOTS, out);
			return;
		}
		const float x = clamp(m, 0.f, 1.f) * (n - 1);
		const int seg = std::min((int)x, n - 2);
		const float f = x - seg;
		for (int s = 0; s < NUM_SLOTS; s++) out[s] = pts[seg][s] + (pts[seg + 1][s] - pts[seg][s]) * f;
		if (m >= 1.f) std::copy(pts[n - 1], pts[n - 1] + NUM_SLOTS, out);  // exactly the last point
	}

	// ------------------------------------------------------------ process
	void process(const ProcessArgs& args) override {
		{
			std::lock_guard<std::mutex> lock(requestMutex);
			for (auto& f : requests) f();
			requests.clear();
		}

		for (int t = 0; t < NUM_VOICE_TYPES; t++)
			if (typeBtn[t].process(params[TYPE_PARAM + t].getValue() > 0.f) && t != type) setType(t);
		if (storeBtn.process(params[STORE_PARAM].getValue() > 0.f)) armed = armed == 1 ? 0 : 1;
		if (recallBtn.process(params[RECALL_PARAM].getValue() > 0.f)) armed = armed == 2 ? 0 : 2;
		for (int i = 0; i < 6; i++)
			if (memBtn[i].process(params[MEM_PARAM + i].getValue() > 0.f)) {
				if (armed == 1) store(i);
				else if (armed == 2) recall(i);
				dest = i;
				armed = 0;
			}
		if (mutateBtn.process(params[MUTATE_PARAM].getValue() > 0.f)) mutateInto(dest, mutateAmount);

		const bool play = playBtn.process(params[PLAY_PARAM].getValue() > 0.f);
		const float acc = clamp(inputs[ACC_INPUT].getVoltage() / 5.f, 0.f, 1.f);
		if (trigIn.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f) || play) {
			pendingTrig = true;
			pendingVel = (uint8_t)(63 + 64 * acc);
			hitPulse.trigger(0.06f);
		}
		if (openIn.process(inputs[OPEN_INPUT].getVoltage(), 0.1f, 1.f)) {
			pendingOpen = true;
			pendingVel = (uint8_t)(63 + 64 * acc);
			hitPulse.trigger(0.06f);
		}

		phase += ALEXA_ENGINE_FS * args.sampleTime;
		while (phase >= 1.0) {
			phase -= 1.0;
			if (bufPos >= ALEXA_BLOCK) renderBlock();
			hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = hist[3];
			hist[3] = vbuf[engineVoice][bufPos] / 32768.f;
			bufPos++;
		}
		const float t = (float)phase;
		const float* y = hist;
		const float c1 = 0.5f * (y[2] - y[0]);
		const float c2 = y[0] - 2.5f * y[1] + 2.f * y[2] - 0.5f * y[3];
		const float c3 = 0.5f * (y[3] - y[0]) + 1.5f * (y[1] - y[2]);
		outputs[OUT_OUTPUT].setVoltage(5.f * (((c3 * t + c2) * t + c1) * t + y[1]));

		blink += args.sampleTime * 3.f;
		if (blink >= 1.f) blink -= 1.f;
		for (int t2 = 0; t2 < NUM_VOICE_TYPES; t2++) lights[TYPE_LIGHT + t2].setBrightness(t2 == type ? 1.f : 0.f);
		for (int i = 0; i < 6; i++) {
			float b = mem[i].used ? 0.25f : 0.f;
			if (i == dest) b = mem[i].used ? 1.f : 0.5f;
			if (armed) b = blink < 0.5f ? (mem[i].used ? 0.6f : 0.15f) : 0.f;
			lights[MEM_LIGHT + i].setBrightness(b);
		}
		lights[STORE_LIGHT].setBrightness(armed == 1 ? 1.f : 0.f);
		lights[RECALL_LIGHT].setBrightness(armed == 2 ? 1.f : 0.f);
		lights[PLAY_LIGHT].setBrightness(hitPulse.process(args.sampleTime) ? 1.f : 0.f);
	}

	void renderBlock() {
		bufPos = 0;
		// remember which knob moved last, for the screen
		for (int s = 0; s < NUM_SLOTS; s++) {
			const float v = params[SLOT_PARAM + s].getValue();
			if (std::fabs(v - lastKnob[s]) > 1e-6f) {
				lastKnob[s] = v;
				lastSlot = s;
			}
		}

		float m = params[MORPH_PARAM].getValue()
			+ params[MORPH_ATT_PARAM].getValue() * inputs[MORPH_INPUT].getVoltage() / 10.f;
		m = clamp(m, 0.f, 1.f);
		morphShown = m;
		float vals[NUM_SLOTS];
		morphedValues(m, vals);
		std::copy(vals, vals + NUM_SLOTS, morphPos);

		std::lock_guard<std::mutex> lock(gEngineMutex);
		acquireEngine();

		const int newVoice = VOICE_OF_TYPE[type];
		if (newVoice != engineVoice) {
			if (vOctPatched) alexa_setVOct((uint8_t)engineVoice, -1.f);
			vOctPatched = false;
			engineVoice = newVoice;
		}
		for (int s = 0; s < NUM_SLOTS; s++) {
			const int p = SLOTS[s].param[type];
			if (p < 0) continue;
			const uint8_t val = toByte(p, vals[s]);
			if (sent[p] != val) {
				alexa_setParam((uint16_t)p, val);
				sent[p] = val;
			}
		}

		const bool patched = inputs[VOCT_INPUT].isConnected();
		if (patched) {
			const float hz = dsp::FREQ_C4 * std::pow(2.f, clamp(inputs[VOCT_INPUT].getVoltage(), -5.f, 5.f));
			alexa_setVOct((uint8_t)engineVoice, hz);
		}
		else if (vOctPatched) alexa_setVOct((uint8_t)engineVoice, -1.f);
		vOctPatched = patched;

		const uint8_t accLevel = (uint8_t)(63 + 64 * clamp(inputs[ACC_INPUT].getVoltage() / 5.f, 0.f, 1.f));
		alexa_setAccent((uint8_t)engineVoice, accLevel);
		if (type == VT_HAT) alexa_setAccent(6, accLevel);

		// the hat's trigger 5 is closed, 6 is open; the other voices only have one
		if (pendingTrig) alexa_trigger((uint8_t)engineVoice, pendingVel);
		if (pendingOpen) alexa_trigger((uint8_t)(type == VT_HAT ? 6 : engineVoice), pendingVel);
		pendingTrig = pendingOpen = false;

		alexa_renderBlock(vbuf, lbuf, rbuf, 0);
		alexa_takeMatrixMorph();
	}

	// ------------------------------------------------------------ save/load
	json_t* dataToJson() override {
		json_t* root = json_object();
		readKnobs(knobsByType[type]);
		json_object_set_new(root, "type", json_integer(type));
		json_t* kt = json_array();
		for (int t = 0; t < NUM_VOICE_TYPES; t++) {
			json_t* a = json_array();
			for (int s = 0; s < NUM_SLOTS; s++) json_array_append_new(a, json_real(knobsByType[t][s]));
			json_array_append_new(kt, a);
		}
		json_object_set_new(root, "knobsByType", kt);
		json_t* ms = json_array();
		for (int i = 0; i < 6; i++) {
			json_t* o = json_object();
			json_object_set_new(o, "used", json_boolean(mem[i].used));
			json_object_set_new(o, "type", json_integer(mem[i].type));
			json_t* a = json_array();
			for (int s = 0; s < NUM_SLOTS; s++) json_array_append_new(a, json_real(mem[i].v[s]));
			json_object_set_new(o, "v", a);
			json_array_append_new(ms, o);
		}
		json_object_set_new(root, "memories", ms);
		json_object_set_new(root, "dest", json_integer(dest));
		json_object_set_new(root, "mutateAmount", json_real(mutateAmount));
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "knobsByType")))
			for (int t = 0; t < NUM_VOICE_TYPES; t++) {
				json_t* a = json_array_get(j, t);
				for (int s = 0; s < NUM_SLOTS && a; s++)
					if (json_t* v = json_array_get(a, s)) knobsByType[t][s] = clamp((float)json_number_value(v), 0.f, 1.f);
			}
		if ((j = json_object_get(root, "type"))) type = clamp((int)json_integer_value(j), 0, NUM_VOICE_TYPES - 1);
		if ((j = json_object_get(root, "memories")))
			for (int i = 0; i < 6; i++) {
				json_t* o = json_array_get(j, i);
				if (!o) continue;
				mem[i].used = json_boolean_value(json_object_get(o, "used"));
				mem[i].type = clamp((int)json_integer_value(json_object_get(o, "type")), 0, NUM_VOICE_TYPES - 1);
				json_t* a = json_object_get(o, "v");
				for (int s = 0; s < NUM_SLOTS && a; s++)
					if (json_t* v = json_array_get(a, s)) mem[i].v[s] = clamp((float)json_number_value(v), 0.f, 1.f);
			}
		if ((j = json_object_get(root, "dest"))) dest = clamp((int)json_integer_value(j), 0, 5);
		if ((j = json_object_get(root, "mutateAmount"))) mutateAmount = (float)json_number_value(j);
		// the knob params themselves come back from the patch; keep lastKnob in step
		for (int s = 0; s < NUM_SLOTS; s++) lastKnob[s] = params[SLOT_PARAM + s].getValue();
		for (int i = 0; i < KIT_SIZE; i++) sent[i] = -1;
	}

	// ------------------------------------------------------------ files
	void loadVoiceFromKit(const Kit& k, int kitVoice) {
		float v[NUM_SLOTS];
		knobsFromKit(k, kitVoice, v);
		const int t = typeOfKitVoice(kitVoice);
		request([this, t, v]() {
			float copy[NUM_SLOTS];
			std::copy(v, v + NUM_SLOTS, copy);
			setType(t, copy);
		});
	}

	void loadSndFile(int kitVoice) {
		osdialog_filters* filters = osdialog_filters_parse("LXR kit:snd,SND");
		char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, filters);
		osdialog_filters_free(filters);
		if (!path) return;
		std::string p = path;
		std::free(path);
		FILE* f = std::fopen(p.c_str(), "rb");
		if (!f) return;
		std::vector<uint8_t> data(4096);
		const size_t n = std::fread(data.data(), 1, data.size(), f);
		std::fclose(f);
		Kit k;
		if (!k.fromBytes(data.data(), n)) return;
		loadVoiceFromKit(k, kitVoice);
	}

	// a whole kit (the firmware's init kit) with this voice in its place
	void saveSndFile() {
		Kit k = initKit();
		k.setName(std::string("A01") + VOICE_TYPE_NAMES[type]);
		for (int s = 0; s < NUM_SLOTS; s++) {
			const int p = SLOTS[s].param[type];
			if (p >= 0) k.p[p] = toByte(p, params[SLOT_PARAM + s].getValue());
		}
		std::string name = std::string(k.name) + ".snd";
		osdialog_filters* filters = osdialog_filters_parse("LXR kit:snd");
		char* path = osdialog_file(OSDIALOG_SAVE, nullptr, name.c_str(), filters);
		osdialog_filters_free(filters);
		if (!path) return;
		std::string p = path;
		std::free(path);
		if (system::getExtension(p).empty()) p += ".snd";
		std::vector<uint8_t> bytes = k.toBytes();
		FILE* f = std::fopen(p.c_str(), "wb");
		if (!f) return;
		std::fwrite(bytes.data(), 1, bytes.size(), f);
		std::fclose(f);
	}
};

// ---------------------------------------------------------------------------
// the menu that opens when the screen is clicked
static void buildVoiceMenu(Menu* menu, Alexa01* m) {
	static const char* kitVoiceNames[6] = {"Drum 1", "Drum 2", "Drum 3", "Snare", "Cymbal", "Hat"};
	menu->addChild(createMenuLabel("Sound"));
	menu->addChild(createSubmenuItem("Load a voice from a factory kit", "", [=](Menu* sub) {
		for (int i = 0; i < NUM_FACTORY_KITS; i++)
			sub->addChild(createSubmenuItem(FACTORY_KITS[i].title, "", [=](Menu* sub2) {
				for (int v = 0; v < 6; v++)
					sub2->addChild(createMenuItem(kitVoiceNames[v], "", [=]() {
						Kit k;
						k.fromBytes(FACTORY_KITS[i].data, FACTORY_KITS[i].size);
						m->loadVoiceFromKit(k, v);
					}));
			}));
	}));
	menu->addChild(createSubmenuItem("Load a voice from a kit file (.snd)", "", [=](Menu* sub) {
		for (int v = 0; v < 6; v++)
			sub->addChild(createMenuItem(kitVoiceNames[v], "", [=]() { m->loadSndFile(v); }));
	}));
	menu->addChild(createMenuItem("Save as a kit file (.snd)...", "", [=]() { m->saveSndFile(); }));
	menu->addChild(createMenuItem("Reset the knobs to the init sound", "", [=]() {
		m->request([=]() {
			float v[NUM_SLOTS];
			Alexa01::initKnobs(m->type, v);
			m->setType(m->type, v);
		});
	}));

	menu->addChild(new MenuSeparator);
	menu->addChild(createMenuLabel("Memories"));
	menu->addChild(createSubmenuItem("Mutate amount", "", [=](Menu* sub) {
		static const char* names[3] = {"A little", "Some", "A lot"};
		static const float amounts[3] = {0.4f, 1.f, 2.f};
		for (int i = 0; i < 3; i++)
			sub->addChild(createCheckMenuItem(names[i], "",
				[=]() { return std::fabs(m->mutateAmount - amounts[i]) < 0.01f; },
				[=]() { m->mutateAmount = amounts[i]; }));
	}));
	menu->addChild(createMenuItem("Fill all six with variations of the knobs", "", [=]() {
		m->request([=]() { for (int i = 0; i < 6; i++) m->mutateInto(i, m->mutateAmount); });
	}));
	menu->addChild(createMenuItem("Store the knobs in all six", "", [=]() {
		m->request([=]() { for (int i = 0; i < 6; i++) m->store(i); });
	}));
	menu->addChild(createSubmenuItem("Clear a memory", "", [=](Menu* sub) {
		for (int i = 0; i < 6; i++)
			sub->addChild(createMenuItem("Memory " + std::to_string(i + 1), "", [=]() {
				m->request([=]() { m->mem[i] = Alexa01::Memory(); });
			}));
	}));
	menu->addChild(createMenuItem("Clear all memories", "", [=]() {
		m->request([=]() { for (int i = 0; i < 6; i++) m->mem[i] = Alexa01::Memory(); });
	}));
}

// ---------------------------------------------------------------------------
static const NVGcolor TYPE_COL[NUM_VOICE_TYPES] = {nvgRGB(0xE8, 0x6A, 0x4A), nvgRGB(0x7B, 0xC4, 0x7F),
	nvgRGB(0x57, 0xB5, 0xC9), nvgRGB(0x9C, 0x8C, 0xE0)};

struct Alexa01Display : Widget {
	Alexa01* module = nullptr;
	std::string fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf");
	bool hover = false;

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_RIGHT) && module) {
			Menu* menu = createMenu();
			buildVoiceMenu(menu, module);
			e.consume(this);
			return;
		}
		Widget::onButton(e);
	}
	void onEnter(const EnterEvent& e) override { hover = true; }
	void onLeave(const LeaveEvent& e) override { hover = false; }

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
		if (!font) return;
		const float w = box.size.x, h = box.size.y;
		const NVGcolor ink = nvgRGB(0xF2, 0xEC, 0xDF);
		const NVGcolor soft = nvgRGB(0x8F, 0x99, 0xA6);
		const NVGcolor amber = nvgRGB(0xF2, 0xA5, 0x41);
		const NVGcolor dark = nvgRGB(0x3A, 0x3F, 0x47);
		nvgFontFaceId(args.vg, font->handle);

		if (!module) {
			nvgFontSize(args.vg, 14.f);
			nvgFillColor(args.vg, ink);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgText(args.vg, w / 2, h / 2, "DRUM", nullptr);
			return;
		}
		Alexa01* m = module;
		if (hover) {
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 1.f, 1.f, w - 2.f, h - 2.f, 3.f);
			nvgStrokeColor(args.vg, nvgRGBA(0xF2, 0xA5, 0x41, 0x90));
			nvgStrokeWidth(args.vg, 1.f);
			nvgStroke(args.vg);
		}

		// top line: type and morph mode
		nvgFontSize(args.vg, 12.f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgFillColor(args.vg, TYPE_COL[m->type]);
		std::string tn = VOICE_TYPE_NAMES[m->type];
		for (auto& c : tn) c = (char)std::toupper(c);
		nvgText(args.vg, 6, 5, tn.c_str(), nullptr);
		const bool scan = m->params[Alexa01::MODE_PARAM].getValue() < 0.5f;
		std::string mode = scan ? "SCAN 1-6" : "MORPH TO " + std::to_string(m->dest + 1);
		if (m->armed == 1) mode = "STORE IN?";
		if (m->armed == 2) mode = "RECALL?";
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		nvgFillColor(args.vg, m->armed ? amber : ink);
		nvgText(args.vg, w - 6, 5, mode.c_str(), nullptr);

		// the knob turned last and its value
		if (hover) {
			nvgFontSize(args.vg, 10.f);
			nvgFillColor(args.vg, amber);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgText(args.vg, 6, 22, "click for the menu", nullptr);
		}
		else if (m->lastSlot >= 0) {
			const SlotDef& d = SLOTS[m->lastSlot];
			const int p = d.param[m->type];
			nvgFontSize(args.vg, 10.f);
			nvgFillColor(args.vg, soft);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			std::string nm = std::string(d.section) + " " + d.label;
			nvgText(args.vg, 6, 22, nm.c_str(), nullptr);
			nvgFontSize(args.vg, 14.f);
			nvgFillColor(args.vg, ink);
			std::string val = p < 0 ? "not used" : valueText(p, Alexa01::toByte(p, m->params[Alexa01::SLOT_PARAM + m->lastSlot].getValue()));
			nvgText(args.vg, 6, 33, val.c_str(), nullptr);
		}

		// memories: filled when stored, outlined when chosen, coloured by type
		const float bw = (w - 12.f - 5 * 3.f) / 6.f, by = h - 32.f;
		for (int i = 0; i < 6; i++) {
			const float bx = 6.f + i * (bw + 3.f);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, bx, by, bw, 11.f);
			if (m->mem[i].used) {
				NVGcolor c = TYPE_COL[m->mem[i].type];
				c.a = 0.35f;
				nvgFillColor(args.vg, c);
				nvgFill(args.vg);
			}
			nvgStrokeColor(args.vg, i == m->dest ? amber : dark);
			nvgStrokeWidth(args.vg, i == m->dest ? 1.5f : 1.f);
			nvgStroke(args.vg);
			nvgFontSize(args.vg, 9.5f);
			nvgFillColor(args.vg, m->mem[i].used ? ink : soft);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgText(args.vg, bx + bw / 2, by + 6.f, std::to_string(i + 1).c_str(), nullptr);
		}

		// morph bar
		const float yb = h - 12.f;
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 6.f, yb, w - 12.f, 3.f);
		nvgFillColor(args.vg, dark);
		nvgFill(args.vg);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 6.f, yb, (w - 12.f) * m->morphShown, 3.f);
		nvgFillColor(args.vg, amber);
		nvgFill(args.vg);
	}
};

// a dark veil over knobs the current voice type doesn't use (lets clicks through)
struct UnusedVeil : Widget {
	Alexa01* module = nullptr;
	int slot = 0;
	void draw(const DrawArgs& args) override {
		if (!module || SLOTS[slot].param[module->type] >= 0) return;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, box.size.x / 2, box.size.y / 2, box.size.x / 2);
		nvgFillColor(args.vg, nvgRGBA(0x2F, 0x33, 0x3A, 0xC0));
		nvgFill(args.vg);
	}
};

struct Alexa01Widget : ModuleWidget {
	Alexa01Widget(Alexa01* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Alexa01.svg")));
		using namespace layout01;

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		Alexa01Display* disp = createWidget<Alexa01Display>(mm2px(Vec(DISP_X, DISP_Y)));
		disp->box.size = mm2px(Vec(DISP_W, DISP_H));
		disp->module = module;
		addChild(disp);

		for (int s = 0; s < NUM_SLOTS; s++) {
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(SLOT_X[s], SLOT_Y[s])), module, Alexa01::SLOT_PARAM + s));
			UnusedVeil* veil = new UnusedVeil;
			veil->module = module;
			veil->slot = s;
			veil->box.size = mm2px(Vec(10.6f, 10.6f));
			veil->box.pos = mm2px(Vec(SLOT_X[s], SLOT_Y[s])).minus(veil->box.size.div(2));
			addChild(veil);
		}

		for (int t = 0; t < NUM_VOICE_TYPES; t++)
			addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(TYPE_X[t], TYPE_Y)), module,
				Alexa01::TYPE_PARAM + t, Alexa01::TYPE_LIGHT + t));
		for (int i = 0; i < 6; i++)
			addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(MEM_X[i], MEM_Y)), module,
				Alexa01::MEM_PARAM + i, Alexa01::MEM_LIGHT + i));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(mm2px(Vec(ACT_X[0], ACT_Y)), module, Alexa01::STORE_PARAM, Alexa01::STORE_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<GreenLight>>(mm2px(Vec(ACT_X[1], ACT_Y)), module, Alexa01::RECALL_PARAM, Alexa01::RECALL_LIGHT));
		addParam(createParamCentered<VCVBezel>(mm2px(Vec(ACT_X[2], ACT_Y)), module, Alexa01::MUTATE_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(ACT_X[3], ACT_Y)), module, Alexa01::PLAY_PARAM, Alexa01::PLAY_LIGHT));

		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(MORPH_KNOB_X, MORPH_KNOB_Y)), module, Alexa01::MORPH_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(MODE_X, MODE_Y)), module, Alexa01::MODE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(MORPH_ATT_X, MORPH_ATT_Y)), module, Alexa01::MORPH_ATT_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(MORPH_CV_X, MORPH_CV_Y)), module, Alexa01::MORPH_INPUT));

		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(JACK_X[0], JACK_Y)), module, Alexa01::TRIG_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(JACK_X[1], JACK_Y)), module, Alexa01::OPEN_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(JACK_X[2], JACK_Y)), module, Alexa01::ACC_INPUT));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(JACK_X[3], JACK_Y)), module, Alexa01::VOCT_INPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(JACK_X[4], JACK_Y)), module, Alexa01::OUT_OUTPUT));
	}
};

Model* modelAlexa01 = createModel<Alexa01, Alexa01Widget>("Alexa01");
