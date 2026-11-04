// Alexa02: the six drum voices of the open-source LXR firmware, played from
// trigger jacks, with two kits (A and B) and morphing between them.
#include "plugin.hpp"
#include "Layout.hpp"
#include "Pages.hpp"
#include "Kits.hpp"
#include "FactoryKits.hpp"
#include <osdialog.h>
#include <mutex>
#include <random>

using namespace lxr;

// ---------------------------------------------------------------------------
// The engine keeps its state in global variables, as on the hardware. Several
// modules share it by swapping their own copy in and out (only when a
// different module than last time needs it), under a lock.
struct Alexa02;
static std::mutex gEngineMutex;
static Alexa02* gEngineOwner = nullptr;

struct Alexa02 : Module {
	enum ParamId {
		ENUMS(EDIT_PARAM, 6),
		ENUMS(VOICE_PARAM, 6),
		ENUMS(PAGE_PARAM, NUM_PAGES),
		EDITAB_PARAM,
		MUTATE_PARAM,
		COPY_PARAM,
		PLAY_PARAM,
		MORPH_PARAM,
		MORPH_ATT_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(TRIG_INPUT, ALEXA_NUM_TRIGS),
		ENUMS(ACC_INPUT, 6),
		ENUMS(VOCT_INPUT, 6),
		ENUMS(VMORPH_INPUT, 6),
		ENUMS(CV_INPUT, ALEXA_NUM_CV),
		MORPH_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(VOICE_OUTPUT, 6),
		L_OUTPUT,
		R_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(VOICE_LIGHT, 6 * 2),  // green = selected, red = hit
		ENUMS(PAGE_LIGHT, NUM_PAGES),
		A_LIGHT,
		B_LIGHT,
		OH_LIGHT,
		LIGHTS_LEN
	};

	// ---- kits
	Kit kitA, kitB;
	int16_t sent[KIT_SIZE];       // what the engine has now (-1 = must send)
	bool editB = false;

	// ---- editing
	int voice = 0;
	int page = PG_OSC;
	int part = 0;
	float lastKnob[6] = {};
	int knobParam[6] = {-1, -1, -1, -1, -1, -1};
	bool syncKnobs = true;

	// ---- engine
	std::vector<uint8_t> engineState;
	bool engineReady = false;
	int16_t vbuf[6][ALEXA_BLOCK];
	int16_t lbuf[ALEXA_BLOCK], rbuf[ALEXA_BLOCK];
	int bufPos = ALEXA_BLOCK;
	float hist[8][4] = {};  // last 4 engine frames, for interpolation
	double phase = 0.0;

	// ---- inputs
	dsp::SchmittTrigger trigIn[ALEXA_NUM_TRIGS];
	bool pendingTrig[ALEXA_NUM_TRIGS] = {};
	uint8_t pendingVel[ALEXA_NUM_TRIGS] = {};
	dsp::BooleanTrigger voiceBtn[6], pageBtn[NUM_PAGES], editAbBtn, mutateBtn, copyBtn, playBtn;
	dsp::PulseGenerator hitPulse[ALEXA_NUM_TRIGS];
	bool vOctPatched[6] = {};

	// ---- morph display
	float morphShown[6] = {};
	float morphGlobalShown = 0.f;
	int matrixMorph = -1;

	// ---- requests from the menu (UI thread), handled in process()
	std::mutex requestMutex;
	std::vector<std::function<void()>> requests;

	std::mt19937 rng{std::random_device{}()};

	Alexa02() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < 6; i++) {
			struct EditQuantity : ParamQuantity {
				std::string getLabel() override {
					Alexa02* m = dynamic_cast<Alexa02*>(module);
					if (!m) return "Edit " + std::to_string(paramId + 1);
					const int p = m->knobParam[paramId - EDIT_PARAM];
					if (p < 0) return "(nothing on this page)";
					return m->itemLongName(paramId - EDIT_PARAM);
				}
				std::string getDisplayValueString() override {
					Alexa02* m = dynamic_cast<Alexa02*>(module);
					if (!m) return "";
					const int p = m->knobParam[paramId - EDIT_PARAM];
					if (p < 0) return "";
					return valueText(p, m->editKit().p[p]);
				}
				std::string getUnit() override { return ""; }
			};
			configParam<EditQuantity>(EDIT_PARAM + i, 0.f, 1.f, 0.f, "Edit " + std::to_string(i + 1));
			paramQuantities[EDIT_PARAM + i]->randomizeEnabled = false;
			paramQuantities[EDIT_PARAM + i]->resetEnabled = false;
		}
		for (int v = 0; v < 6; v++) configButton(VOICE_PARAM + v, std::string("Select ") + VOICE_NAMES[v]);
		for (int p = 0; p < NUM_PAGES; p++) configButton(PAGE_PARAM + p, std::string(PAGE_NAMES[p]) + " page");
		configButton(EDITAB_PARAM, "Edit kit A or kit B");
		configButton(MUTATE_PARAM, "Mutate: make this voice in kit B a variation of kit A");
		configButton(COPY_PARAM, "Copy this voice from kit A to kit B");
		configButton(PLAY_PARAM, "Play this voice");
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "Morph A to B", "%", 0.f, 100.f);
		configParam(MORPH_ATT_PARAM, -1.f, 1.f, 0.f, "Morph CV amount", "%", 0.f, 100.f);

		static const char* trigNames[ALEXA_NUM_TRIGS] = {"Drum 1", "Drum 2", "Drum 3", "Snare", "Cymbal", "Closed hat", "Open hat"};
		for (int t = 0; t < ALEXA_NUM_TRIGS; t++) configInput(TRIG_INPUT + t, std::string(trigNames[t]) + " trigger");
		for (int v = 0; v < 6; v++) {
			configInput(ACC_INPUT + v, std::string(VOICE_NAMES[v]) + " accent (0-5V)");
			configInput(VOCT_INPUT + v, std::string(VOICE_NAMES[v]) + " 1V/oct (0V = C4)");
			configInput(VMORPH_INPUT + v, std::string(VOICE_NAMES[v]) + " morph (adds, 10V = full)");
			configOutput(VOICE_OUTPUT + v, std::string(VOICE_NAMES[v]) + " (leaves the mix when patched)");
		}
		for (int c = 0; c < ALEXA_NUM_CV; c++) configInput(CV_INPUT + c, "Mod matrix CV " + std::to_string(c + 1) + " (+-5V)");
		configInput(MORPH_INPUT, "Morph CV");
		configOutput(L_OUTPUT, "Mix left");
		configOutput(R_OUTPUT, "Mix right");

		engineState.resize(alexa_stateSize());
		loadFactory(kitA, 0);
		kitB = kitA;
		kitB.setName("Init B");
		for (int i = 0; i < KIT_SIZE; i++) sent[i] = -1;

		std::lock_guard<std::mutex> lock(gEngineMutex);
		acquireEngine();
		alexa_engineInit();
		engineReady = true;
	}

	~Alexa02() {
		std::lock_guard<std::mutex> lock(gEngineMutex);
		if (gEngineOwner == this) gEngineOwner = nullptr;
	}

	// call with gEngineMutex held
	void acquireEngine() {
		if (gEngineOwner == this) return;
		if (gEngineOwner) alexa_stateSave(gEngineOwner->engineState.data());
		if (engineReady) alexa_stateLoad(engineState.data());
		gEngineOwner = this;
	}

	// ------------------------------------------------------------ kits
	static void loadFactory(Kit& k, int idx) {
		const FactoryKit& f = FACTORY_KITS[idx];
		k.fromBytes(f.data, f.size);
	}

	Kit& editKit() { return editB ? kitB : kitA; }

	void resendAll() {
		for (int i = 0; i < KIT_SIZE; i++) sent[i] = -1;
		syncKnobs = true;
	}

	void onReset() override {
		loadFactory(kitA, 0);
		kitB = kitA;
		kitB.setName("Init B");
		editB = false;
		voice = 0; page = PG_OSC; part = 0;
		params[MORPH_PARAM].setValue(0.f);
		resendAll();
	}

	// ------------------------------------------------------------ pages
	PageDef currentPage() {
		return getPage(voice, page, part, kitA.p[PAR_FX_TYPE]);
	}

	std::string itemLongName(int i) {
		PageDef d = currentPage();
		std::string s = d.items[i].longName;
		if (page < PG_FX) s = std::string(VOICE_NAMES[voice]) + " " + s;
		return s;
	}

	void setPage(int p) {
		if (p == page) part = (part + 1) % pageParts(page);
		else { page = p; part = 0; }
		syncKnobs = true;
	}

	// ------------------------------------------------------------ mutate
	// make kit B's voice v a variation of kit A's
	void mutateVoice(int v, float amount) {
		const VoicePages& vp = voicePages();
		std::normal_distribution<float> gauss(0.f, 1.f);
		std::uniform_real_distribution<float> uni(0.f, 1.f);
		for (int p = 0; p < KIT_SIZE; p++) {
			if (vp.paramVoice[p] != v) continue;
			const int a = kitA.p[p];
			const int mx = paramMax(p);
			int out = a;
			const ParamKindInfo& k = PARAM_KINDS[p];
			if (p >= PAR_AUDIO_OUT1 && p <= PAR_AUDIO_OUT6) out = a;
			else if (p >= PAR_VOL1 && p <= PAR_VOL6) out = a;
			else if (k.kind == K_MENU || k.kind == K_ONOFF) {
				if (uni(rng) < 0.2f * amount) out = (int)(uni(rng) * (mx + 1));
			}
			else {
				float spread = 22.f * amount;
				if (p >= PAR_PAN1 && p <= PAR_PAN6) spread *= 0.5f;
				out = a + (int)std::round(gauss(rng) * spread);
			}
			kitB.p[p] = (uint8_t)clamp(out, 0, mx);
		}
		syncKnobs = true;
	}

	void copyVoice(int v, Kit& from, Kit& to) {
		const VoicePages& vp = voicePages();
		for (int p = 0; p < KIT_SIZE; p++)
			if (vp.paramVoice[p] == v) to.p[p] = from.p[p];
		syncKnobs = true;
	}

	void request(std::function<void()> f) {
		std::lock_guard<std::mutex> lock(requestMutex);
		requests.push_back(f);
	}

	// ------------------------------------------------------------ process
	void process(const ProcessArgs& args) override {
		// menu requests
		{
			std::lock_guard<std::mutex> lock(requestMutex);
			for (auto& f : requests) f();
			requests.clear();
		}

		// buttons
		for (int v = 0; v < 6; v++)
			if (voiceBtn[v].process(params[VOICE_PARAM + v].getValue() > 0.f)) {
				voice = v;
				syncKnobs = true;
			}
		for (int p = 0; p < NUM_PAGES; p++)
			if (pageBtn[p].process(params[PAGE_PARAM + p].getValue() > 0.f)) setPage(p);
		if (editAbBtn.process(params[EDITAB_PARAM].getValue() > 0.f)) {
			editB = !editB;
			syncKnobs = true;
		}
		if (mutateBtn.process(params[MUTATE_PARAM].getValue() > 0.f)) mutateVoice(voice, 1.f);
		if (copyBtn.process(params[COPY_PARAM].getValue() > 0.f)) copyVoice(voice, kitA, kitB);
		const bool play = playBtn.process(params[PLAY_PARAM].getValue() > 0.f);

		// triggers: wait for the next engine block, like the hardware's queue
		for (int t = 0; t < ALEXA_NUM_TRIGS; t++) {
			bool hit = trigIn[t].process(inputs[TRIG_INPUT + t].getVoltage(), 0.1f, 1.f);
			const int v = t < 6 ? t : 5;
			if (play && t == voice) hit = true;
			if (hit) {
				pendingTrig[t] = true;
				pendingVel[t] = accentOf(v);
				hitPulse[t].trigger(0.06f);
			}
		}

		// interpolate between engine frames (the engine runs at 44.14 kHz)
		phase += ALEXA_ENGINE_FS * args.sampleTime;
		while (phase >= 1.0) {
			phase -= 1.0;
			if (bufPos >= ALEXA_BLOCK) renderBlock();
			for (int c = 0; c < 8; c++) {
				hist[c][0] = hist[c][1]; hist[c][1] = hist[c][2]; hist[c][2] = hist[c][3];
			}
			for (int v = 0; v < 6; v++) hist[v][3] = vbuf[v][bufPos] / 32768.f;
			hist[6][3] = lbuf[bufPos] / 32768.f;
			hist[7][3] = rbuf[bufPos] / 32768.f;
			bufPos++;
		}
		const float t = (float)phase;
		auto hermite = [t](const float* y) {
			const float c0 = y[1];
			const float c1 = 0.5f * (y[2] - y[0]);
			const float c2 = y[0] - 2.5f * y[1] + 2.f * y[2] - 0.5f * y[3];
			const float c3 = 0.5f * (y[3] - y[0]) + 1.5f * (y[1] - y[2]);
			return ((c3 * t + c2) * t + c1) * t + c0;
		};
		for (int v = 0; v < 6; v++) outputs[VOICE_OUTPUT + v].setVoltage(5.f * hermite(hist[v]));
		outputs[L_OUTPUT].setVoltage(5.f * hermite(hist[6]));
		outputs[R_OUTPUT].setVoltage(5.f * hermite(hist[7]));

		// lights
		if (lightDivider.process()) {
			const float dt = args.sampleTime * lightDivider.getDivision();
			for (int v = 0; v < 6; v++) {
				const bool hit = hitPulse[v].process(dt) || (v == 5 && hitPulse[6].remaining > 0.f);
				lights[VOICE_LIGHT + 2 * v + 0].setBrightness(v == voice ? 0.8f : 0.f);
				lights[VOICE_LIGHT + 2 * v + 1].setBrightness(hit ? 1.f : 0.f);
			}
			lights[OH_LIGHT].setBrightness(hitPulse[6].process(dt) ? 1.f : 0.f);
			for (int p = 0; p < NUM_PAGES; p++) lights[PAGE_LIGHT + p].setBrightness(p == page ? 1.f : 0.f);
			lights[A_LIGHT].setBrightness(editB ? 0.f : 1.f);
			lights[B_LIGHT].setBrightness(editB ? 1.f : 0.f);
		}
	}

	dsp::ClockDivider lightDivider = makeDivider(32);
	static dsp::ClockDivider makeDivider(int n) {
		dsp::ClockDivider d;
		d.setDivision(n);
		return d;
	}

	uint8_t accentOf(int v) {
		// unpatched or 0V = the hardware's normal level (63), 5V or more = full accent (127)
		const float a = clamp(inputs[ACC_INPUT + v].getVoltage() / 5.f, 0.f, 1.f);
		return (uint8_t)(LOW_VEL + (127 - LOW_VEL) * a);
	}
	static const int LOW_VEL = 63;

	// knobs: show the kit's values, and write back what the user turns
	void updateKnobs() {
		PageDef d = currentPage();
		Kit& k = editKit();
		for (int i = 0; i < 6; i++) {
			const int p = d.items[i].param;
			if (syncKnobs || p != knobParam[i]) {
				knobParam[i] = p;
				const float v = p >= 0 ? (float)k.p[p] / std::max(1, paramMax(p)) : 0.f;
				params[EDIT_PARAM + i].setValue(v);
				lastKnob[i] = v;
				continue;
			}
			const float now = params[EDIT_PARAM + i].getValue();
			if (p >= 0 && std::fabs(now - lastKnob[i]) > 1e-6f) {
				lastKnob[i] = now;
				const int mx = paramMax(p);
				k.p[p] = (uint8_t)clamp((int)std::round(now * mx), 0, mx);
				// changing the FX type changes what the FX page shows
				if (p == PAR_FX_TYPE) syncKnobs = true;
			}
		}
		if (syncKnobs) syncKnobs = false;
	}

	void renderBlock() {
		bufPos = 0;
		updateKnobs();

		// morph positions
		float global = params[MORPH_PARAM].getValue()
			+ params[MORPH_ATT_PARAM].getValue() * inputs[MORPH_INPUT].getVoltage() / 10.f;
		global = clamp(global, 0.f, 1.f);
		if (matrixMorph >= 0) global = matrixMorph / 255.f;
		morphGlobalShown = global;
		uint8_t m8[7];
		for (int v = 0; v < 6; v++) {
			const float mv = clamp(global + inputs[VMORPH_INPUT + v].getVoltage() / 10.f, 0.f, 1.f);
			morphShown[v] = mv;
			m8[v] = (uint8_t)std::round(mv * 255.f);
		}
		m8[6] = (uint8_t)std::round(global * 255.f);

		std::lock_guard<std::mutex> lock(gEngineMutex);
		acquireEngine();

		// send changed parameters (kit A morphed towards kit B)
		const VoicePages& vp = voicePages();
		for (int p = 0; p < KIT_SIZE; p++) {
			uint8_t val;
			if (p >= PAR_CV_DEST_1 || (p >= PAR_AUDIO_OUT1 && p <= PAR_AUDIO_OUT6) || p == PAR_FX_TYPE)
				val = kitA.p[p];  // not morphed, as on the hardware
			else {
				const int v = vp.paramVoice[p];
				val = interpolate(kitA.p[p], kitB.p[p], v >= 0 ? m8[v] : m8[6]);
			}
			if (sent[p] != val) {
				alexa_setParam((uint16_t)p, val);
				sent[p] = val;
			}
		}
		alexa_setMorphBase(m8[6]);

		// pitch, CV and accent
		for (int v = 0; v < 6; v++) {
			const bool patched = inputs[VOCT_INPUT + v].isConnected();
			if (patched) {
				const float hz = dsp::FREQ_C4 * std::pow(2.f, clamp(inputs[VOCT_INPUT + v].getVoltage(), -5.f, 5.f));
				alexa_setVOct((uint8_t)v, hz);
			}
			else if (vOctPatched[v]) alexa_setVOct((uint8_t)v, -1.f);
			vOctPatched[v] = patched;
			alexa_setAccent((uint8_t)v, accentOf(v));
		}
		alexa_setAccent(6, accentOf(5));
		for (int c = 0; c < ALEXA_NUM_CV; c++)
			alexa_setCv((uint8_t)c, clamp(0.5f + inputs[CV_INPUT + c].getVoltage() / 10.f, 0.f, 1.f));

		for (int t = 0; t < ALEXA_NUM_TRIGS; t++)
			if (pendingTrig[t]) {
				alexa_trigger((uint8_t)t, pendingVel[t]);
				pendingTrig[t] = false;
			}

		uint8_t mask = 0;
		for (int v = 0; v < 6; v++)
			if (!outputs[VOICE_OUTPUT + v].isConnected()) mask |= (uint8_t)(1 << v);
		alexa_renderBlock(vbuf, lbuf, rbuf, mask);
		matrixMorph = alexa_takeMatrixMorph();
	}

	// ------------------------------------------------------------ save/load
	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "kitA", json_string(kitA.toHex().c_str()));
		json_object_set_new(root, "kitB", json_string(kitB.toHex().c_str()));
		json_object_set_new(root, "nameA", json_string(kitA.name));
		json_object_set_new(root, "nameB", json_string(kitB.name));
		json_object_set_new(root, "editB", json_boolean(editB));
		json_object_set_new(root, "voice", json_integer(voice));
		json_object_set_new(root, "page", json_integer(page));
		json_object_set_new(root, "part", json_integer(part));
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "kitA"))) kitA.fromHex(json_string_value(j));
		if ((j = json_object_get(root, "kitB"))) kitB.fromHex(json_string_value(j));
		if ((j = json_object_get(root, "nameA"))) kitA.setName(json_string_value(j));
		if ((j = json_object_get(root, "nameB"))) kitB.setName(json_string_value(j));
		if ((j = json_object_get(root, "editB"))) editB = json_boolean_value(j);
		if ((j = json_object_get(root, "voice"))) voice = clamp((int)json_integer_value(j), 0, 5);
		if ((j = json_object_get(root, "page"))) page = clamp((int)json_integer_value(j), 0, NUM_PAGES - 1);
		if ((j = json_object_get(root, "part"))) part = clamp((int)json_integer_value(j), 0, pageParts(page) - 1);
		sanitize(kitA);
		sanitize(kitB);
		resendAll();
	}

	static void sanitize(Kit& k) {
		for (int p = 0; p < KIT_SIZE; p++) k.p[p] = (uint8_t)std::min<int>(k.p[p], paramMax(p));
	}

	// ------------------------------------------------------------ files
	void loadKitFile(bool toB) {
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
		sanitize(k);
		request([this, k, toB]() {
			(toB ? kitB : kitA) = k;
			resendAll();
		});
	}

	void saveKitFile(bool fromB) {
		Kit k = fromB ? kitB : kitA;
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
// the display
struct AlexaDisplay : Widget {
	Alexa02* module = nullptr;
	std::string fontPath = asset::system("res/fonts/ShareTechMono-Regular.ttf");

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
		if (!font) return;
		const float w = box.size.x, h = box.size.y;
		const NVGcolor ink = nvgRGB(0xF2, 0xEC, 0xDF);
		const NVGcolor soft = nvgRGB(0x8F, 0x99, 0xA6);
		const NVGcolor amber = nvgRGB(0xF2, 0xA5, 0x41);
		static const NVGcolor voiceCol[6] = {nvgRGB(0xE8, 0x6A, 0x4A), nvgRGB(0xE8, 0xA0, 0x4A), nvgRGB(0xD9, 0xC6, 0x4A),
			nvgRGB(0x7B, 0xC4, 0x7F), nvgRGB(0x57, 0xB5, 0xC9), nvgRGB(0x9C, 0x8C, 0xE0)};

		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0.f);

		// preview in the module browser
		if (!module) {
			nvgFontSize(args.vg, 14.f);
			nvgFillColor(args.vg, ink);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgText(args.vg, w / 2, h / 2, "DRUM 1  OSC", nullptr);
			return;
		}
		Alexa02* m = module;
		const int voice = m->voice, page = m->page, part = m->part;
		PageDef d = m->currentPage();
		Kit& kit = m->editKit();

		// top line: voice, page, kit being edited
		nvgFontSize(args.vg, 12.f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		std::string title;
		if (page < PG_FX) {
			nvgFillColor(args.vg, voiceCol[voice]);
			title = std::string(VOICE_NAMES[voice]) + "  ";
			nvgText(args.vg, 5, 4, title.c_str(), nullptr);
		}
		float bounds[4];
		nvgTextBounds(args.vg, 5, 4, title.c_str(), nullptr, bounds);
		std::string pg = PAGE_NAMES[page];
		const int parts = pageParts(page);
		if (parts > 1) pg += " " + std::to_string(part + 1) + "/" + std::to_string(parts);
		nvgFillColor(args.vg, ink);
		nvgText(args.vg, title.empty() ? 5 : bounds[2], 4, pg.c_str(), nullptr);

		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
		nvgFillColor(args.vg, m->editB ? amber : ink);
		std::string ed = std::string("EDIT ") + (m->editB ? "B: " : "A: ") + kit.name;
		nvgText(args.vg, w - 5, 4, ed.c_str(), nullptr);

		// six cells, one above each knob
		const float cw = w / 6.f;
		bool any = false;
		for (int i = 0; i < 6; i++) {
			const int p = d.items[i].param;
			const float cx = cw * (i + 0.5f);
			if (p < 0) continue;
			any = true;
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgFontSize(args.vg, 9.5f);
			nvgFillColor(args.vg, soft);
			nvgText(args.vg, cx, 24, d.items[i].label, nullptr);
			std::string val = valueText(p, kit.p[p]);
			nvgFontSize(args.vg, val.size() > 7 ? 9.5f : 13.f);
			nvgFillColor(args.vg, ink);
			if (val.size() > 9) {
				// long mod destinations: two lines
				const size_t cut = val.find(' ', val.size() / 2 - 1);
				std::string a = cut == std::string::npos ? val : val.substr(0, cut);
				std::string b = cut == std::string::npos ? "" : val.substr(cut + 1);
				nvgText(args.vg, cx, 35, a.c_str(), nullptr);
				nvgText(args.vg, cx, 45, b.c_str(), nullptr);
			}
			else nvgText(args.vg, cx, 37, val.c_str(), nullptr);

			// small bar: where the other kit is, and where the sound is now
			const Kit& other = m->editB ? m->kitA : m->kitB;
			const int mx = std::max(1, paramMax(p));
			const float bx = cx - cw * 0.36f, bw = cw * 0.72f, by = 58;
			nvgBeginPath(args.vg);
			nvgRect(args.vg, bx, by, bw, 2.f);
			nvgFillColor(args.vg, nvgRGB(0x3A, 0x3F, 0x47));
			nvgFill(args.vg);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, bx, by, bw * kit.p[p] / mx, 2.f);
			nvgFillColor(args.vg, m->editB ? amber : ink);
			nvgFill(args.vg);
			if (other.p[p] != kit.p[p]) {
				nvgBeginPath(args.vg);
				const float ox = bx + bw * other.p[p] / mx;
				nvgRect(args.vg, ox - 0.75f, by - 2.f, 1.5f, 6.f);
				nvgFillColor(args.vg, m->editB ? ink : amber);
				nvgFill(args.vg);
			}
		}
		if (!any) {
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgFontSize(args.vg, 11.f);
			nvgFillColor(args.vg, soft);
			std::string msg = std::string("no ") + PAGE_NAMES[page] + " on the " + VOICE_NAMES[voice];
			if (page == PG_FX) msg = "FX is off: turn TYPE on part 1";
			nvgText(args.vg, w / 2, 34, msg.c_str(), nullptr);
		}

		// bottom: kit names and the morph bar with one mark per voice
		const float yb = h - 17.f;
		nvgFontSize(args.vg, 10.5f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, m->editB ? soft : ink);
		std::string an = std::string("A ") + m->kitA.name;
		nvgText(args.vg, 5, yb + 7, an.c_str(), nullptr);
		nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, m->editB ? amber : soft);
		std::string bn = std::string(m->kitB.name) + " B";
		nvgText(args.vg, w - 5, yb + 7, bn.c_str(), nullptr);

		const float mx0 = 82.f, mx1 = w - 82.f;
		nvgBeginPath(args.vg);
		nvgRect(args.vg, mx0, yb + 6.f, mx1 - mx0, 2.f);
		nvgFillColor(args.vg, nvgRGB(0x3A, 0x3F, 0x47));
		nvgFill(args.vg);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, mx0, yb + 6.f, (mx1 - mx0) * m->morphGlobalShown, 2.f);
		nvgFillColor(args.vg, amber);
		nvgFill(args.vg);
		for (int v = 0; v < 6; v++) {
			const float x = mx0 + (mx1 - mx0) * m->morphShown[v];
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, x, yb + 7.f + (v - 2.5f) * 0.f, v == voice ? 3.f : 2.f);
			nvgFillColor(args.vg, voiceCol[v]);
			nvgFill(args.vg);
		}
	}
};

// ---------------------------------------------------------------------------
template <typename TLight>
struct SmallLightBezel : VCVLightBezel<TLight> {};

struct Alexa02Widget : ModuleWidget {
	Alexa02Widget(Alexa02* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Alexa02.svg")));
		using namespace layout;

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		AlexaDisplay* disp = createWidget<AlexaDisplay>(mm2px(Vec(DISP_X, DISP_Y)));
		disp->box.size = mm2px(Vec(DISP_W, DISP_H));
		disp->module = module;
		addChild(disp);

		for (int i = 0; i < 6; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(EDIT_X[i], EDIT_Y)), module, Alexa02::EDIT_PARAM + i));

		for (int v = 0; v < 6; v++) {
			const float x = VOICE_X[v];
			addParam(createLightParamCentered<VCVLightBezel<GreenRedLight>>(mm2px(Vec(x, ROW_SEL)), module,
				Alexa02::VOICE_PARAM + v, Alexa02::VOICE_LIGHT + 2 * v));
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(x, ROW_TRIG)), module, Alexa02::TRIG_INPUT + v));
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(x, ROW_ACC)), module, Alexa02::ACC_INPUT + v));
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(x, ROW_VOCT)), module, Alexa02::VOCT_INPUT + v));
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(x, ROW_MORPH)), module, Alexa02::VMORPH_INPUT + v));
			addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(x, ROW_OUT)), module, Alexa02::VOICE_OUTPUT + v));
		}
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(OH_X, ROW_TRIG)), module, Alexa02::TRIG_INPUT + 6));
		addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(OH_X + 4.6f, ROW_TRIG - 4.4f)), module, Alexa02::OH_LIGHT));

		for (int p = 0; p < NUM_PAGES; p++)
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(mm2px(Vec(PAGE_X[p % 5], PAGE_Y[p / 5])), module,
				Alexa02::PAGE_PARAM + p, Alexa02::PAGE_LIGHT + p));

		addParam(createParamCentered<VCVBezel>(mm2px(Vec(EDITAB_X, KIT_Y)), module, Alexa02::EDITAB_PARAM));
		addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(EDITAB_X - 4.6f, KIT_Y + 4.0f)), module, Alexa02::A_LIGHT));
		addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(EDITAB_X + 4.6f, KIT_Y + 4.0f)), module, Alexa02::B_LIGHT));
		addParam(createParamCentered<VCVBezel>(mm2px(Vec(RND_X, KIT_Y)), module, Alexa02::MUTATE_PARAM));
		addParam(createParamCentered<VCVBezel>(mm2px(Vec(COPY_X, KIT_Y)), module, Alexa02::COPY_PARAM));
		addParam(createParamCentered<VCVBezel>(mm2px(Vec(PLAY_X, KIT_Y)), module, Alexa02::PLAY_PARAM));

		const float cvY[5] = {ROW_TRIG, ROW_ACC, ROW_VOCT, ROW_MORPH, ROW_OUT};
		for (int c = 0; c < ALEXA_NUM_CV; c++)
			addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(CV_X, cvY[c])), module, Alexa02::CV_INPUT + c));

		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(MORPH_KNOB_X, MORPH_KNOB_Y)), module, Alexa02::MORPH_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(MORPH_ATT_X, MORPH_ATT_Y)), module, Alexa02::MORPH_ATT_PARAM));
		addInput(createInputCentered<ThemedPJ301MPort>(mm2px(Vec(MORPH_CV_X, MORPH_CV_Y)), module, Alexa02::MORPH_INPUT));

		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(OUT_L_X, OUT_L_Y)), module, Alexa02::L_OUTPUT));
		addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(OUT_R_X, OUT_R_Y)), module, Alexa02::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Alexa02* m = dynamic_cast<Alexa02*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Kits"));

		for (int side = 0; side < 2; side++) {
			const bool b = side == 1;
			menu->addChild(createSubmenuItem(b ? "Load factory kit into B" : "Load factory kit into A", "", [=](Menu* sub) {
				for (int i = 0; i < NUM_FACTORY_KITS; i++) {
					sub->addChild(createMenuItem(FACTORY_KITS[i].title, "", [=]() {
						m->request([=]() {
							Alexa02::loadFactory(b ? m->kitB : m->kitA, i);
							m->resendAll();
						});
					}));
				}
			}));
		}
		menu->addChild(createMenuItem("Load kit file (.snd) into A...", "", [=]() { m->loadKitFile(false); }));
		menu->addChild(createMenuItem("Load kit file (.snd) into B...", "", [=]() { m->loadKitFile(true); }));
		menu->addChild(createMenuItem("Save kit A as .snd...", "", [=]() { m->saveKitFile(false); }));
		menu->addChild(createMenuItem("Save kit B as .snd...", "", [=]() { m->saveKitFile(true); }));

		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Copy kit A to B", "", [=]() { m->request([=]() { m->kitB = m->kitA; m->resendAll(); }); }));
		menu->addChild(createMenuItem("Copy kit B to A", "", [=]() { m->request([=]() { m->kitA = m->kitB; m->resendAll(); }); }));
		menu->addChild(createMenuItem("Swap kits A and B", "", [=]() { m->request([=]() { std::swap(m->kitA, m->kitB); m->resendAll(); }); }));
		menu->addChild(createSubmenuItem("Mutate all voices into B", "", [=](Menu* sub) {
			static const char* names[3] = {"A little", "Some", "A lot"};
			static const float amounts[3] = {0.4f, 1.f, 2.f};
			for (int i = 0; i < 3; i++)
				sub->addChild(createMenuItem(names[i], "", [=]() {
					m->request([=]() { for (int v = 0; v < 6; v++) m->mutateVoice(v, amounts[i]); });
				}));
		}));
	}
};

Model* modelAlexa02 = createModel<Alexa02, Alexa02Widget>("Alexa02");
