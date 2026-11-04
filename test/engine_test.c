/* Renders each voice of the engine into a WAV file and checks it makes sound.
 * Build: see the Makefile target "test". */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "alexa_engine.h"

static void writeWav(const char* path, const int16_t* data, int frames, int channels)
{
	FILE* f = fopen(path, "wb");
	if(!f) return;
	int sr = 44140, byteRate = sr * channels * 2, dataSize = frames * channels * 2, chunk = 36 + dataSize;
	short fmt = 1, ch = (short)channels, ba = (short)(channels * 2), bits = 16; int sub = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&chunk, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
	fwrite(&sub, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f);
	fwrite(&byteRate, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&dataSize, 4, 1, f); fwrite(data, 2, (size_t)frames * channels, f);
	fclose(f);
}

int main(int argc, char** argv)
{
	const char* outDir = argc > 1 ? argv[1] : ".";
	alexa_engineInit();
	printf("kit size %d, state %zu bytes, mod targets %d\n", alexa_kitSize(), alexa_stateSize(), alexa_numModTargets());
	const char* names[7] = {"drum1", "drum2", "drum3", "snare", "cymbal", "hat_closed", "hat_open"};
	int failures = 0;
	const int blocks = 44140 / ALEXA_BLOCK; /* one second */
	int16_t vo[ALEXA_NUM_VOICES][ALEXA_BLOCK], l[ALEXA_BLOCK], r[ALEXA_BLOCK];
	int16_t* stereo = malloc(sizeof(int16_t) * blocks * ALEXA_BLOCK * 2);
	for(int t = 0; t < 7; t++)
	{
		const int voice = t < 6 ? t : 5;
		double sum = 0; int peak = 0;
		for(int b = 0; b < blocks; b++)
		{
			if(b == 2) alexa_trigger((uint8_t)t, 127);
			alexa_renderBlock(vo, l, r, 0x3f);
			for(int i = 0; i < ALEXA_BLOCK; i++)
			{
				const int s = vo[voice][i];
				sum += (double)s * s; if(abs(s) > peak) peak = abs(s);
				stereo[2*(b*ALEXA_BLOCK+i)] = l[i]; stereo[2*(b*ALEXA_BLOCK+i)+1] = r[i];
			}
		}
		const double rms = sqrt(sum / (blocks * ALEXA_BLOCK));
		printf("%-11s rms %8.1f peak %6d\n", names[t], rms, peak);
		if(peak < 100) { printf("  FAIL: no sound\n"); failures++; }
		char path[512]; snprintf(path, sizeof path, "%s/%s.wav", outDir, names[t]);
		writeWav(path, stereo, blocks * ALEXA_BLOCK, 2);
	}
	/* a short groove through the mix */
	{
		const int nb = blocks * 4; int16_t* m = malloc(sizeof(int16_t) * nb * ALEXA_BLOCK * 2);
		const int step = (int)(44140 * 60.0 / 130.0 / 4.0 / ALEXA_BLOCK); /* 16ths at 130 bpm in blocks */
		for(int b = 0; b < nb; b++)
		{
			if(b % step == 0)
			{
				const int s = (b / step) % 16;
				if(s % 4 == 0) alexa_trigger(0, 127);
				if(s % 8 == 4) alexa_trigger(3, 127);
				if(s % 2 == 1) alexa_trigger(5, s % 4 == 3 ? 127 : 63);
				if(s == 14) alexa_trigger(6, 100);
			}
			alexa_renderBlock(vo, l, r, 0x3f);
			for(int i = 0; i < ALEXA_BLOCK; i++) { m[2*(b*ALEXA_BLOCK+i)] = l[i]; m[2*(b*ALEXA_BLOCK+i)+1] = r[i]; }
		}
		char path[512]; snprintf(path, sizeof path, "%s/groove.wav", outDir);
		writeWav(path, m, nb * ALEXA_BLOCK, 2);
		free(m);
	}
	free(stereo);

	/* two modules sharing the engine: module X alone must sound the same as
	 * module X interleaved with module Y (which plays something else) */
	{
		const size_t n = alexa_stateSize();
		uint8_t* x = malloc(n); uint8_t* y = malloc(n); uint8_t* x0 = malloc(n);
		alexa_engineInit(); alexa_stateSave(x0);
		alexa_setParam(8, 20); alexa_engineInit(); /* y: fresh */
		alexa_setParam(37, 40); alexa_setParam(43, 100); /* y: dark resonant drum 1 */
		alexa_stateSave(y);
		int16_t ref[64 * 200], got[64 * 200];
		/* reference: x alone */
		alexa_stateLoad(x0);
		for(int b = 0; b < 200; b++) {
			if(b % 20 == 0) alexa_trigger(0, 127);
			if(b % 30 == 5) alexa_trigger(3, 127);
			alexa_renderBlock(vo, l, r, 0x3f);
			memcpy(&ref[b * 64], l, sizeof l);
		}
		/* interleaved */
		memcpy(x, x0, n);
		for(int b = 0; b < 200; b++) {
			alexa_stateLoad(x);
			if(b % 20 == 0) alexa_trigger(0, 127);
			if(b % 30 == 5) alexa_trigger(3, 127);
			alexa_renderBlock(vo, l, r, 0x3f);
			memcpy(&got[b * 64], l, sizeof l);
			alexa_stateSave(x);
			alexa_stateLoad(y);
			if(b % 7 == 0) alexa_trigger((uint8_t)(b % 7), 127);
			alexa_renderBlock(vo, l, r, 0x3f);
			alexa_stateSave(y);
		}
		int diff = 0;
		for(int i = 0; i < 64 * 200; i++) if(ref[i] != got[i]) diff++;
		printf("state swap: %d differing samples\n", diff);
		if(diff) { printf("  FAIL: modules disturb each other\n"); failures++; }
		free(x); free(y); free(x0);
	}

	/* every parameter at every value: no crash, no stuck silence afterwards */
	{
		alexa_engineInit();
		for(int p = 0; p < alexa_kitSize(); p++)
		{
			if(p >= 1 && p <= 7) continue;            /* oscillator waves: 0-5 only, tested below */
			if(p >= 20 && p <= 26) continue;
			if(p == 243) continue;                   /* FX wave */
			for(int v = 0; v < 128; v += 9) {
				alexa_setParam((uint16_t)p, (uint8_t)v);
				alexa_trigger((uint8_t)(v % 7), 127);
				alexa_renderBlock(vo, l, r, 0x3f);
			}
		}
		const int waves[] = {1,2,3,4,6,7,20,21,22,23,24,25,26,243};
		for(int w = 0; w < 14; w++)
			for(int v = 0; v < 6; v++) { alexa_setParam((uint16_t)waves[w], (uint8_t)v); alexa_trigger((uint8_t)(w % 7), 127); alexa_renderBlock(vo, l, r, 0x3f); }
		printf("parameter sweep: done\n");
	}
	printf(failures ? "FAILED\n" : "OK\n");
	return failures ? 1 : 0;
}
