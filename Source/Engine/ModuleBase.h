#pragma once
// Bub Leon Mix Rack — module interface and parameter specification
#include <atomic>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "DSP/Common.h"

namespace bl
{
constexpr int kNumSlots       = 8;
constexpr int kParamsPerSlot  = 12;

/** Describes one module control and maps the slot's generic 0..1 parameter onto real units. */
struct ParamSpec
{
    enum class Kind { Linear, Log, Choice, Toggle };

    std::string name;          // full name, e.g. "Threshold"
    std::string shortName;     // knob label, e.g. "THRESH"
    Kind kind = Kind::Linear;
    double minV = 0, maxV = 1, defV = 0;
    std::string unit;          // "dB", "Hz", "ms", "%", ""
    std::vector<std::string> choices;
    std::string offText;       // shown when the value sits at an "off" end (e.g. HPF at minimum)
    bool offAtMin = false, offAtMax = false;
    int decimals = 1;

    int numChoices() const { return kind == Kind::Toggle ? 2 : (int) choices.size(); }
    bool isDiscrete() const { return kind == Kind::Choice || kind == Kind::Toggle; }

    double toReal (double norm) const noexcept
    {
        norm = clampd (norm, 0.0, 1.0);
        switch (kind)
        {
            case Kind::Linear: return minV + (maxV - minV) * norm;
            case Kind::Log:    return minV * std::pow (maxV / minV, norm);
            case Kind::Choice:
            case Kind::Toggle: return (double) (int) std::lround (norm * (numChoices() - 1));
        }
        return 0.0;
    }

    double toNorm (double real) const noexcept
    {
        switch (kind)
        {
            case Kind::Linear: return clampd ((real - minV) / (maxV - minV), 0.0, 1.0);
            case Kind::Log:    return clampd (std::log (std::max (real, minV) / minV) / std::log (maxV / minV), 0.0, 1.0);
            case Kind::Choice:
            case Kind::Toggle: return numChoices() > 1 ? clampd (real / (numChoices() - 1), 0.0, 1.0) : 0.0;
        }
        return 0.0;
    }

    double defaultNorm() const noexcept { return toNorm (defV); }

    bool isOff (double real) const noexcept
    {
        const double eps = (maxV - minV) * 1.0e-4;
        return (offAtMin && real <= minV + eps) || (offAtMax && real >= maxV - eps);
    }

    std::string toText (double real) const
    {
        if (kind == Kind::Choice) { const int i = (int) real; return (i >= 0 && i < (int) choices.size()) ? choices[(size_t) i] : "-"; }
        if (kind == Kind::Toggle) return real >= 0.5 ? "On" : "Off";
        if (isOff (real)) return offText.empty() ? "Off" : offText;

        double v = real; std::string u = unit; int dec = decimals;
        if (unit == "Hz" && std::abs (v) >= 1000.0) { v /= 1000.0; u = "kHz"; dec = v < 10.0 ? 2 : 1; }
        if (unit == "ms" && std::abs (v) >= 1000.0) { v /= 1000.0; u = "s"; dec = 2; }
        if (unit == "ms" && std::abs (v) < 1.0)      { v *= 1000.0; u = "us"; dec = 0; }
        char buf[64];
        std::snprintf (buf, sizeof (buf), "%.*f", dec, v);
        std::string s (buf);
        if (s == "-0" || s == "-0.0" || s == "-0.00") s = s.substr (1);
        return u.empty() ? s : s + " " + u;
    }
};

inline ParamSpec pLin (std::string n, std::string sn, double lo, double hi, double def, std::string unit, int dec = 1)
{ ParamSpec p; p.name = n; p.shortName = sn; p.kind = ParamSpec::Kind::Linear; p.minV = lo; p.maxV = hi; p.defV = def; p.unit = unit; p.decimals = dec; return p; }
inline ParamSpec pLog (std::string n, std::string sn, double lo, double hi, double def, std::string unit, int dec = 0)
{ ParamSpec p = pLin (n, sn, lo, hi, def, unit, dec); p.kind = ParamSpec::Kind::Log; return p; }
inline ParamSpec pChoice (std::string n, std::string sn, std::vector<std::string> c, int def)
{ ParamSpec p; p.name = n; p.shortName = sn; p.kind = ParamSpec::Kind::Choice; p.choices = std::move (c); p.minV = 0; p.maxV = (double) p.choices.size() - 1; p.defV = def; return p; }
inline ParamSpec pToggle (std::string n, std::string sn, bool def)
{ ParamSpec p; p.name = n; p.shortName = sn; p.kind = ParamSpec::Kind::Toggle; p.minV = 0; p.maxV = 1; p.defV = def ? 1 : 0; return p; }
inline ParamSpec withOffMin (ParamSpec p, std::string t = "Off") { p.offAtMin = true; p.offText = std::move (t); return p; }
inline ParamSpec withOffMax (ParamSpec p, std::string t = "Off") { p.offAtMax = true; p.offText = std::move (t); return p; }

/** Visual style of a module faceplate (used by the GUI). */
enum class Faceplate { Black, Silver, Cream, Blue, Graphite, Bronze, Teal, Red };

struct ModuleInfo
{
    std::string id;            // stable id used in presets, e.g. "vca"
    std::string name;          // "VCA Bus Comp"
    std::string shortName;     // "VCA"
    std::string category;      // "Dynamics", "EQ", ...
    Faceplate faceplate = Faceplate::Black;
    uint32_t accent = 0xffe0a040;
    bool supportsOversampling = false;
    bool hasGainReduction = false;
    std::vector<ParamSpec> params;   // <= kParamsPerSlot
};

//==============================================================================
/** A rack module. prepare() runs on the message thread, everything else on the audio thread. */
class ModuleBase
{
public:
    virtual ~ModuleBase() = default;
    virtual const ModuleInfo& info() const = 0;

    /** fs is the module's own rate (base rate x oversampling factor). */
    virtual void prepare (double fs, int maxBlock, int osFactor) = 0;
    virtual void reset() = 0;

    /** Real-unit parameter values, called once per block before process(). */
    virtual void setParams (const double* real) = 0;

    /** Process two channels in place. */
    virtual void process (double* l, double* r, int n) = 0;

    /** Latency in module-rate samples (must be a multiple of the oversampling factor). */
    virtual int latency() const { return 0; }

    float getGainReductionDb() const noexcept { return grDb.load (std::memory_order_relaxed); }

protected:
    std::atomic<float> grDb { 0.0f };
    void reportGr (double db) noexcept { grDb.store ((float) std::max (0.0, db), std::memory_order_relaxed); }
};
} // namespace bl
