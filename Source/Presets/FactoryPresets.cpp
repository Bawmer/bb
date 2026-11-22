#include "Presets/FactoryPresets.h"

namespace bl
{
// Gain staging assumes program material around -18 dBFS RMS / -6 dBFS peak.
const std::vector<RackPresetDef>& factoryPresets()
{
    static const std::vector<RackPresetDef> p = {
        //===================================================== Vocal Lead
        { "Vocal Lead", "Modern Pop Lead", {
            { "eq4",     { { "High Pass", 90 }, { "Low-Mid Gain", -2.5 }, { "Low-Mid Freq", 320 }, { "High-Mid Gain", 1.5 }, { "High-Mid Freq", 3200 }, { "High Gain", 3 }, { "High Freq", 11000 } } },
            { "fet",     { { "Input", 9 }, { "Output", -3 }, { "Attack", 0.25 }, { "Release", 250 }, { "Ratio", 0 } } },
            { "deess",   { { "Frequency", 6500 }, { "Threshold", -32 }, { "Range", 7 } } },
            { "opto",    { { "Peak Reduction", 35 }, { "Gain", 2 } } },
            { "tubeeq",  { { "High Freq", 5 }, { "High Boost", 3 }, { "Bandwidth", 7 }, { "Tube Drive", 2 } } },
            { "limiter", { { "Ceiling", -1 }, { "Release", 60 } } } } },
        { "Vocal Lead", "Warm R&B Lead", {
            { "preamp",  { { "Drive", 6 }, { "Color", 2 }, { "Low Cut", 70 }, { "Tilt", -0.5 } } },
            { "opto",    { { "Peak Reduction", 45 }, { "Gain", 3 }, { "Emphasis", 20 } } },
            { "eq4",     { { "Low-Mid Gain", -2 }, { "Low-Mid Freq", 450 }, { "High-Mid Gain", 1 }, { "High-Mid Freq", 2200 }, { "High Gain", 2 }, { "High Freq", 9000 } } },
            { "deess",   { { "Frequency", 7000 }, { "Threshold", -30 }, { "Range", 6 } } },
            { "tape",    { { "Drive", 2 }, { "Head Bump", 30 } } } } },
        { "Vocal Lead", "Aggressive Rap", {
            { "eq4",     { { "High Pass", 110 }, { "Low-Mid Gain", -3 }, { "Low-Mid Freq", 380 }, { "High-Mid Gain", 2.5 }, { "High-Mid Freq", 2800 }, { "High-Mid Q", 1.2 } } },
            { "fet",     { { "Input", 14 }, { "Output", -6 }, { "Attack", 0.1 }, { "Release", 120 }, { "Ratio", 4 } }, 70.0 },
            { "deess",   { { "Frequency", 5800 }, { "Threshold", -34 }, { "Range", 9 } } },
            { "preamp",  { { "Drive", 9 }, { "Color", 1 } } },
            { "limiter", { { "Input", 2 }, { "Ceiling", -1 }, { "Release", 40 } } } } },
        { "Vocal Lead", "Airy Ballad", {
            { "eq4",     { { "High Pass", 80 }, { "Low-Mid Gain", -1.5 }, { "Low-Mid Freq", 250 }, { "High Gain", 2 }, { "High Freq", 12000 } } },
            { "opto",    { { "Peak Reduction", 40 }, { "Gain", 2.5 } } },
            { "tubeeq",  { { "High Freq", 6 }, { "High Boost", 4 }, { "Bandwidth", 9 }, { "Atten Freq", 2 }, { "High Atten", 1 }, { "Tube Drive", 3 } } },
            { "deess",   { { "Frequency", 7500 }, { "Threshold", -33 }, { "Range", 8 } } } } },
        { "Vocal Lead", "Vintage Tube Lead", {
            { "preamp",  { { "Drive", 10 }, { "Color", 2 }, { "Low Cut", 60 } } },
            { "tubeeq",  { { "Low Freq", 3 }, { "Low Atten", 3 }, { "High Freq", 4 }, { "High Boost", 4 }, { "Bandwidth", 6 }, { "Tube Drive", 5 } } },
            { "opto",    { { "Peak Reduction", 50 }, { "Gain", 3.5 } } },
            { "tape",    { { "Drive", 4 }, { "Speed", 0 }, { "Head Bump", 40 }, { "Wow/Flutter", 8 } } } } },

        //===================================================== Vocal Background
        { "Vocal Background", "Tucked Harmonies", {
            { "eq4",     { { "High Pass", 150 }, { "Low-Mid Gain", -3 }, { "Low-Mid Freq", 400 }, { "High-Mid Gain", -1.5 }, { "High-Mid Freq", 3000 }, { "High Gain", 1.5 } } },
            { "vca",     { { "Threshold", -22 }, { "Ratio", 2 }, { "Attack", 3 }, { "Release", 4 }, { "Makeup", 3 } } },
            { "deess",   { { "Frequency", 6000 }, { "Threshold", -36 }, { "Range", 10 }, { "Mode", 1 } } },
            { "imager",  { { "Width", 140 } } } } },
        { "Vocal Background", "Wide Choir", {
            { "eq4",     { { "High Pass", 120 }, { "Low-Mid Gain", -2 }, { "High Gain", 2.5 }, { "High Freq", 10000 } } },
            { "opto",    { { "Peak Reduction", 45 }, { "Gain", 3 } } },
            { "imager",  { { "Width", 170 }, { "Mono Bass", 150 } } },
            { "tape",    { { "Drive", 1 }, { "Wow/Flutter", 10 } } } } },
        { "Vocal Background", "Lo-Fi Stack", {
            { "eq4",     { { "High Pass", 300 }, { "Low Pass", 6000 }, { "High-Mid Gain", 3 }, { "High-Mid Freq", 1800 } } },
            { "preamp",  { { "Drive", 14 }, { "Color", 1 }, { "Output", -3 } } },
            { "tape",    { { "Drive", 6 }, { "Speed", 0 }, { "Wow/Flutter", 35 }, { "Hiss", 1 }, { "Output", -2 } } },
            { "vca",     { { "Threshold", -20 }, { "Ratio", 2 }, { "Attack", 2 }, { "Release", 1 }, { "Makeup", 2 } } } } },
        { "Vocal Background", "Soft Ad-Libs", {
            { "eq4",     { { "High Pass", 140 }, { "Low-Mid Gain", -2 }, { "High-Mid Gain", -1 }, { "High-Mid Freq", 2500 } } },
            { "fet",     { { "Input", 8 }, { "Output", -4 }, { "Ratio", 1 } }, 60.0 },
            { "deess",   { { "Frequency", 6500 }, { "Threshold", -35 }, { "Range", 9 } } },
            { "imager",  { { "Width", 120 } } } } },
        { "Vocal Background", "Doubles Glue", {
            { "eq4",     { { "High Pass", 130 }, { "Low-Mid Gain", -2.5 }, { "Low-Mid Freq", 300 } } },
            { "vca",     { { "Threshold", -18 }, { "Ratio", 1 }, { "Attack", 4 }, { "Release", 4 }, { "Makeup", 2 } } },
            { "deess",   { { "Frequency", 7000 }, { "Threshold", -34 }, { "Range", 8 } } } } },

        //===================================================== Drum Bus
        { "Drum Bus", "Punchy Glue", {
            { "vca",       { { "Threshold", -16 }, { "Ratio", 2 }, { "Attack", 4 }, { "Release", 4 }, { "Makeup", 3 }, { "SC High Pass", 90 } } },
            { "eq4",       { { "Low Gain", 2 }, { "Low Freq", 70 }, { "Low-Mid Gain", -2 }, { "Low-Mid Freq", 400 }, { "High Gain", 2 }, { "High Freq", 10000 } } },
            { "transient", { { "Attack", 25 } } },
            { "limiter",   { { "Ceiling", -0.5 }, { "Release", 30 } } } } },
        { "Drum Bus", "Smashed Room", {
            { "fet",       { { "Input", 20 }, { "Output", -10 }, { "Attack", 0.05 }, { "Release", 100 }, { "Ratio", 4 } }, 40.0 },
            { "eq4",       { { "Low Gain", 3 }, { "Low Freq", 80 }, { "High Gain", 3 }, { "High Freq", 8000 } } },
            { "preamp",    { { "Drive", 8 }, { "Color", 1 } } } } },
        { "Drum Bus", "Tape Crunch", {
            { "tape",      { { "Drive", 8 }, { "Speed", 1 }, { "Head Bump", 70 }, { "Bias", -20 }, { "Output", -1 } }, 100.0, 0.0, 0.0, 1 },
            { "vca",       { { "Threshold", -14 }, { "Ratio", 1 }, { "Attack", 4 }, { "Release", 2 }, { "Makeup", 2 } } },
            { "transient", { { "Attack", 15 }, { "Sustain", -10 } } } } },
        { "Drum Bus", "Tight & Gated", {
            { "gate",      { { "Threshold", -38 }, { "Range", 18 }, { "Mode", 2 }, { "Attack", 0.2 }, { "Hold", 20 }, { "Release", 90 } } },
            { "transient", { { "Attack", 35 }, { "Sustain", -30 }, { "Speed", 0 } } },
            { "vca",       { { "Threshold", -12 }, { "Ratio", 1 }, { "Attack", 3 }, { "Release", 1 }, { "Makeup", 1.5 } } } } },
        { "Drum Bus", "Big Low End", {
            { "tubeeq",    { { "Low Freq", 2 }, { "Low Boost", 5 }, { "Low Atten", 3 }, { "High Freq", 4 }, { "High Boost", 2 }, { "Tube Drive", 4 } }, 100.0, 0.0, -1.0 },
            { "vca",       { { "Threshold", -14 }, { "Ratio", 1 }, { "Attack", 4 }, { "Release", 4 }, { "Makeup", 1 }, { "SC High Pass", 120 } } },
            { "imager",    { { "Mono Bass", 120 } } },
            { "limiter",   { { "Ceiling", -0.5 }, { "Release", 40 } } } } },

        //===================================================== Bass
        { "Bass", "Solid DI", {
            { "eq4",     { { "High Pass", 35 }, { "Low Gain", 2 }, { "Low Freq", 80 }, { "Low-Mid Gain", -2.5 }, { "Low-Mid Freq", 250 }, { "High-Mid Gain", 2 }, { "High-Mid Freq", 900 } } },
            { "opto",    { { "Peak Reduction", 50 }, { "Gain", 3 } } },
            { "preamp",  { { "Drive", 6 }, { "Color", 1 } } } } },
        { "Bass", "Growl", {
            { "preamp",  { { "Drive", 16 }, { "Color", 2 }, { "Output", -3 } }, 100.0, 0.0, 0.0, 1 },
            { "fet",     { { "Input", 10 }, { "Output", -4 }, { "Attack", 0.4 }, { "Release", 200 }, { "Ratio", 1 } } },
            { "eq4",     { { "High Pass", 30 }, { "High-Mid Gain", 3 }, { "High-Mid Freq", 1200 }, { "Low Pass", 8000 } } } } },
        { "Bass", "808 Control", {
            { "eq4",     { { "High Pass", 28 }, { "Low Gain", 1.5 }, { "Low Freq", 55 } } },
            { "vca",     { { "Threshold", -12 }, { "Ratio", 2 }, { "Attack", 4 }, { "Release", 2 }, { "Makeup", 2 } } },
            { "tape",    { { "Drive", 5 }, { "Head Bump", 60 } } },
            { "imager",  { { "Width", 0 } } },
            { "limiter", { { "Ceiling", -1 }, { "Release", 50 } } } } },
        { "Bass", "Vintage Upright", {
            { "tubeeq",  { { "Low Freq", 3 }, { "Low Boost", 3 }, { "Low Atten", 4 }, { "High Freq", 1 }, { "High Boost", 3 }, { "Bandwidth", 6 }, { "Tube Drive", 4 } } },
            { "opto",    { { "Peak Reduction", 40 }, { "Gain", 2 } } },
            { "tape",    { { "Drive", 3 }, { "Speed", 0 } } } } },
        { "Bass", "Synth Bass Tight", {
            { "eq4",       { { "High Pass", 30 }, { "Low-Mid Gain", -2 }, { "Low-Mid Freq", 350 } } },
            { "transient", { { "Attack", 20 }, { "Sustain", -15 } } },
            { "vca",       { { "Threshold", -10 }, { "Ratio", 2 }, { "Attack", 3 }, { "Release", 1 }, { "Makeup", 1 } } },
            { "imager",    { { "Mono Bass", 200 } } } } },

        //===================================================== Mix Bus
        { "Mix Bus", "Gentle Glue", {
            { "vca",     { { "Threshold", -10 }, { "Ratio", 1 }, { "Attack", 5 }, { "Release", 4 }, { "Makeup", 1.5 }, { "SC High Pass", 60 } } },
            { "eq4",     { { "Low Gain", 0.8 }, { "Low Freq", 60 }, { "High Gain", 1 }, { "High Freq", 12000 } } } } },
        { "Mix Bus", "Console Sum", {
            { "preamp",  { { "Drive", 4 }, { "Color", 1 } } },
            { "vca",     { { "Threshold", -8 }, { "Ratio", 0 }, { "Attack", 5 }, { "Release", 4 }, { "Makeup", 1 } } },
            { "tape",    { { "Drive", 1 }, { "Speed", 2 }, { "Head Bump", 25 } } } } },
        { "Mix Bus", "Tape Warmth", {
            { "tape",    { { "Drive", 3 }, { "Speed", 2 }, { "Head Bump", 40 }, { "Bias", 10 } }, 100.0, 0.0, 0.0, 1 },
            { "tubeeq",  { { "Low Freq", 1 }, { "Low Boost", 2 }, { "High Freq", 5 }, { "High Boost", 2 }, { "Bandwidth", 8 }, { "Tube Drive", 2 } } } } },
        { "Mix Bus", "Wide & Clear", {
            { "eq4",     { { "Low-Mid Gain", -1 }, { "Low-Mid Freq", 300 }, { "High Gain", 1.5 }, { "High Freq", 11000 } } },
            { "imager",  { { "Width", 120 }, { "Mono Bass", 110 } } },
            { "vca",     { { "Threshold", -10 }, { "Ratio", 1 }, { "Attack", 4 }, { "Release", 4 }, { "Makeup", 1 } } } } },
        { "Mix Bus", "Punch Bus", {
            { "transient", { { "Attack", 12 } } },
            { "vca",       { { "Threshold", -12 }, { "Ratio", 1 }, { "Attack", 5 }, { "Release", 2 }, { "Makeup", 2 }, { "SC High Pass", 90 } } },
            { "eq4",       { { "Low Gain", 1 }, { "Low Freq", 70 }, { "High Gain", 1 }, { "High Freq", 10000 } } } } },

        //===================================================== Master Glue
        { "Master Glue", "Streaming Master", {
            { "eq4",     { { "High Pass", 25 }, { "Low-Mid Gain", -0.8 }, { "Low-Mid Freq", 280 }, { "High Gain", 1 }, { "High Freq", 12000 } } },
            { "vca",     { { "Threshold", -8 }, { "Ratio", 0 }, { "Attack", 5 }, { "Release", 4 }, { "Makeup", 1 }, { "SC High Pass", 60 } } },
            { "imager",  { { "Mono Bass", 100 } } },
            { "limiter", { { "Input", 4 }, { "Ceiling", -1 }, { "Release", 80 }, { "True Peak", 1 } } } } },
        { "Master Glue", "Loud Club", {
            { "vca",     { { "Threshold", -12 }, { "Ratio", 1 }, { "Attack", 4 }, { "Release", 4 }, { "Makeup", 2 }, { "SC High Pass", 90 } } },
            { "tape",    { { "Drive", 2 }, { "Speed", 2 } }, 100.0, 0.0, 0.0, 1 },
            { "imager",  { { "Mono Bass", 120 }, { "Width", 110 } } },
            { "limiter", { { "Input", 8 }, { "Ceiling", -0.3 }, { "Release", 40 }, { "True Peak", 1 } } } } },
        { "Master Glue", "Analog Master", {
            { "tubeeq",  { { "Low Freq", 1 }, { "Low Boost", 1.5 }, { "Low Atten", 1 }, { "High Freq", 6 }, { "High Boost", 1.5 }, { "Bandwidth", 9 }, { "Tube Drive", 2 } } },
            { "vca",     { { "Threshold", -9 }, { "Ratio", 0 }, { "Attack", 5 }, { "Release", 4 }, { "Makeup", 1 } } },
            { "tape",    { { "Drive", 1 }, { "Speed", 2 }, { "Head Bump", 20 } } },
            { "limiter", { { "Input", 3 }, { "Ceiling", -1 }, { "Release", 100 } } } } },
        { "Master Glue", "Transparent Safety", {
            { "limiter", { { "Input", 0 }, { "Ceiling", -1 }, { "Release", 120 }, { "True Peak", 1 } } } } },
        { "Master Glue", "Dynamic Acoustic", {
            { "eq4",     { { "High Pass", 30 }, { "High Gain", 0.8 }, { "High Freq", 14000 } } },
            { "opto",    { { "Peak Reduction", 20 }, { "Gain", 1 } } },
            { "limiter", { { "Input", 2 }, { "Ceiling", -1 }, { "Release", 200 }, { "True Peak", 1 } } } } },
    };
    return p;
}

std::vector<const char*> factoryBanks()
{
    return { "Vocal Lead", "Vocal Background", "Drum Bus", "Bass", "Mix Bus", "Master Glue" };
}
} // namespace bl
