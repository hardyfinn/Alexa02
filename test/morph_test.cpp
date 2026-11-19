// Checks the kit morph: 0% is kit A, 100% is exactly kit B for every setting,
// and the settings that can't be blended switch to B at 99%, not before.
#include <cstdio>
#include "../src/Pages.hpp"
#include "../src/Kits.hpp"
#include "../src/FactoryKits.hpp"
#include <vector>
#include <cstring>
using namespace lxr;

int main() {
	int fails = 0;
	for (int p = 1; p < KIT_SIZE; p++)
		for (int a = 0; a < 128; a += 3)
			for (int b = 0; b < 128; b += 5) {
				if (morphParam(p, a, b, morphByte(0.f)) != a) { fails++; }
				if (morphParam(p, a, b, morphByte(1.f)) != b) { fails++; }
				if (isSwitchedParam(p)) {
					if (morphParam(p, a, b, morphByte(0.985f)) != a) fails++;
					if (morphParam(p, a, b, morphByte(0.99f)) != b) fails++;
				}
			}
	if (!isSwitchedParam(PAR_FX_TYPE) || !isSwitchedParam(PAR_AUDIO_OUT3) || !isSwitchedParam(PAR_CV_DEST_1)
		|| isSwitchedParam(PAR_COARSE1)) fails++;

	// the reported case: with the morph at 100%, loading another kit into A
	// must not change the sound. Render kit B at 100% with two different A kits.
	alexa_engineInit();
	std::vector<uint8_t> fresh(alexa_stateSize());
	alexa_stateSave(fresh.data());
	auto render = [&fresh](int kitAIdx, int kitBIdx) {
		alexa_stateLoad(fresh.data());
		Kit a, b;
		a.fromBytes(FACTORY_KITS[kitAIdx].data, FACTORY_KITS[kitAIdx].size);
		b.fromBytes(FACTORY_KITS[kitBIdx].data, FACTORY_KITS[kitBIdx].size);
		for (int p = 0; p < KIT_SIZE; p++) {
			alexa_setParam((uint16_t)p, morphParam(p, a.p[p], b.p[p], morphByte(1.f)));
		}
		alexa_setMorphBase(morphByte(1.f));
		std::vector<int16_t> out;
		int16_t vo[6][ALEXA_BLOCK], l[ALEXA_BLOCK], r[ALEXA_BLOCK];
		for (int blk = 0; blk < 1400; blk++) {
			if (blk % 100 == 2) for (int t = 0; t < 7; t++) alexa_trigger((uint8_t)t, 127);
			alexa_renderBlock(vo, l, r, 0x3f);
			out.insert(out.end(), l, l + ALEXA_BLOCK);
			out.insert(out.end(), r, r + ALEXA_BLOCK);
		}
		return out;
	};
	for (int bk = 1; bk < NUM_FACTORY_KITS; bk++) {
		const std::vector<int16_t> ref = render(bk == 1 ? 2 : 1, bk);
		for (int ak = 0; ak < NUM_FACTORY_KITS; ak++) {
			const std::vector<int16_t> o = render(ak, bk);
			size_t diff = 0;
			for (size_t i = 0; i < o.size(); i++) diff += o[i] != ref[i];
			if (diff) { std::printf("  kit B %d at 100%%, kit A %d: %zu samples differ\n", bk, ak, diff); fails++; }
		}
	}
	std::printf("morph test: %d failures\n", fails);
	return fails ? 1 : 0;
}
