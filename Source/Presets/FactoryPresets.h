#pragma once
// Factory rack presets. Values are in real units (dB, Hz, ms, %), choice values are indices.
#include <vector>
#include <utility>

namespace bl
{
struct SlotPresetDef
{
    const char* module = nullptr;   // module id, nullptr = empty
    std::vector<std::pair<const char*, double>> values;   // by parameter name
    double mix = 100.0, inTrim = 0.0, outTrim = 0.0;
    int os = 0;
};

struct RackPresetDef
{
    const char* bank;
    const char* name;
    std::vector<SlotPresetDef> slots;
    double inGain = 0.0, outGain = 0.0;
};

const std::vector<RackPresetDef>& factoryPresets();
std::vector<const char*> factoryBanks();
} // namespace bl
