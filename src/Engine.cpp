#include "Engine.hpp"

std::mutex gEngineMutex;
EngineUser* gEngineOwner = nullptr;
