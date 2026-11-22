#pragma once
// Registry of all rack modules. Type index -1 = empty slot.
#include "Modules/ColorModules.h"
#include "Modules/EQModules.h"
#include "Modules/DynamicsModules.h"
#include "Modules/UtilityModules.h"
#include "Modules/LimiterModule.h"

namespace bl
{
struct ModuleRegistry
{
    struct Entry
    {
        const ModuleInfo& (*info)();
        std::unique_ptr<ModuleBase> (*create)();
    };

    template <typename T>
    static Entry make() { return { &T::staticInfo, [] () -> std::unique_ptr<ModuleBase> { return std::make_unique<T>(); } }; }

    static const std::vector<Entry>& all()
    {
        static const std::vector<Entry> e = {
            make<ConsolePreamp>(), make<FourBandEQ>(), make<PassiveTubeEQ>(), make<FETCompressor>(),
            make<OptoCompressor>(), make<VCABusCompressor>(), make<DeEsser>(), make<GateExpander>(),
            make<TapeSaturator>(), make<StereoImager>(), make<TransientShaper>(), make<OutputLimiter>(),
        };
        return e;
    }

    static int count() { return (int) all().size(); }

    static const ModuleInfo* info (int type)
    {
        return (type >= 0 && type < count()) ? &all()[(size_t) type].info() : nullptr;
    }

    static std::unique_ptr<ModuleBase> create (int type)
    {
        return (type >= 0 && type < count()) ? all()[(size_t) type].create() : nullptr;
    }

    static int indexOf (const std::string& id)
    {
        for (int i = 0; i < count(); ++i) if (all()[(size_t) i].info().id == id) return i;
        return -1;
    }
};
} // namespace bl
