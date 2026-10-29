#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

class FreeRateArp final : public juce::AudioProcessor
{
public:
    FreeRateArp();
    const juce::String getName() const override { return "Free Rate Arp"; }
    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override;
    void reset() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    struct Note { int channel = 0, pitch = 0; juce::uint8 velocity = 0; };
    static constexpr int maxNotes = 16 * 128;
    std::array<Note, maxNotes> held {}, sounding {};
    int heldCount = 0, soundingCount = 0, nextNote = 0;
    double sampleRateHz = 44100.0, untilPulse = 0.0, untilOff = 0.0;
    double previousInterval = 0.0;
    std::atomic<bool> resetPending { false };
    bool wasPlaying = false, bypassed = false;
    juce::AudioProcessorValueTreeState parameters;

    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void stopSounding (juce::MidiBuffer&, int);
    void clearNotes (juce::MidiBuffer&, int);
    void removeHeld (int channel, int pitch);
    void handleInput (const juce::MidiMessage&, juce::MidiBuffer&, int);
    void pulse (juce::MidiBuffer&, int, double interval, double gateSamples, int mode);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreeRateArp)
};
