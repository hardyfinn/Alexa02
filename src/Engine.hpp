// The LXR engine keeps its state in global variables, as on the hardware.
// Every Alexa module owns a full copy of that state and swaps it in (only when
// a different module than last time needs the engine), under one lock.
#pragma once
#include <mutex>
#include <vector>
#include <cstdint>
#include "alexa_engine.h"

struct EngineUser;
extern std::mutex gEngineMutex;
extern EngineUser* gEngineOwner;

struct EngineUser {
	std::vector<uint8_t> engineState;
	bool engineReady = false;

	// call with gEngineMutex held
	void acquireEngine() {
		if (gEngineOwner == this) return;
		if (gEngineOwner) alexa_stateSave(gEngineOwner->engineState.data());
		if (engineReady) alexa_stateLoad(engineState.data());
		gEngineOwner = this;
	}
	// a fresh engine with the firmware's init kit
	void initEngine() {
		engineState.resize(alexa_stateSize());
		std::lock_guard<std::mutex> lock(gEngineMutex);
		acquireEngine();
		alexa_engineInit();
		engineReady = true;
	}
	virtual ~EngineUser() {
		std::lock_guard<std::mutex> lock(gEngineMutex);
		if (gEngineOwner == this) gEngineOwner = nullptr;
	}
};
