// Alexa01's knob map: every voice of every factory kit, loaded onto the 32
// knobs and played alone, must round-trip exactly and make sound.
#include <cstdio>
#include <cmath>
#include <vector>
#include "../src/VoiceSlots.hpp"
#include "../src/Kits.hpp"
#include "../src/FactoryKits.hpp"
using namespace lxr;

int main() {
	int fails = 0;
	alexa_engineInit();
	std::vector<uint8_t> fresh(alexa_stateSize());
	alexa_stateSave(fresh.data());
	for (int k = 0; k < NUM_FACTORY_KITS; k++) {
		Kit kit;
		kit.fromBytes(FACTORY_KITS[k].data, FACTORY_KITS[k].size);
		for (int kv = 0; kv < 6; kv++) {
			const int t = typeOfKitVoice(kv), ev = VOICE_OF_TYPE[t];
			alexa_stateLoad(fresh.data());
			for (int s = 0; s < NUM_SLOTS; s++) {
				const int src = slotParamForKitVoice(s, kv), dst = SLOTS[s].param[t];
				if ((src < 0) != (dst < 0)) { std::printf("slot %d voice %d: map mismatch\n", s, kv); fails++; continue; }
				if (src < 0) continue;
				const int mx = std::max(1, paramMax(dst));
				const float knob = kit.p[src] / (float)mx;            // what the knob shows
				const int back = (int)std::lround(knob * mx);       // what it sends
				if (back != std::min<int>(kit.p[src], mx)) { std::printf("slot %d voice %d: %d -> %d\n", s, kv, kit.p[src], back); fails++; }
				alexa_setParam((uint16_t)dst, (uint8_t)back);
			}
			int16_t vo[6][ALEXA_BLOCK], l[ALEXA_BLOCK], r[ALEXA_BLOCK];
			int peak = 0;
			for (int b = 0; b < 400; b++) {
				if (b == 2) alexa_trigger((uint8_t)ev, 127);
				alexa_renderBlock(vo, l, r, 0);
				for (int i = 0; i < ALEXA_BLOCK; i++) peak = std::max(peak, std::abs((int)vo[ev][i]));
			}
			if (peak < 100) { std::printf("kit %d voice %d (%s): silent\n", k, kv, VOICE_TYPE_NAMES[t]); fails++; }
		}
	}
	std::printf("alexa01 test: %d failures\n", fails);
	return fails ? 1 : 0;
}
