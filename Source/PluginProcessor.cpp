#include "PluginProcessor.h"
#include <algorithm>
#include <cmath>

FreeRateArp::FreeRateArp()
    : AudioProcessor (BusesProperties()), parameters (*this, nullptr, "Parameters", makeParameters())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout FreeRateArp::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::NormalisableRange<float> rateRange (0.1f, 100.0f, 0.01f);
    rateRange.setSkewForCentre (4.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "rate", 1 }, "Rate", rateRange, 4.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gate", 1 }, "Gate",
        juce::NormalisableRange<float> (1.0f, 99.0f, 1.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode",
        juce::StringArray { "Chord", "Played order" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "gateMode", 1 }, "Gate unit",
        juce::StringArray { "%", "ms" }, 0));
    juce::NormalisableRange<float> msRange (1.0f, 10000.0f, 1.0f);
    msRange.setSkewForCentre (125.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gateMs", 1 }, "Gate time", msRange, 125.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));
    return layout;
}

void FreeRateArp::prepareToPlay (double rate, int)
{
    sampleRateHz = rate > 0.0 ? rate : 44100.0;
    previousInterval = 0.0;
    resetPending = true;
    wasPlaying = false;
}

void FreeRateArp::releaseResources() { resetPending = true; }
void FreeRateArp::reset() { resetPending = true; }

void FreeRateArp::stopSounding (juce::MidiBuffer& output, int offset)
{
    for (int i = 0; i < soundingCount; ++i)
        output.addEvent (juce::MidiMessage::noteOff (sounding[i].channel, sounding[i].pitch), offset);
    soundingCount = 0;
    untilOff = 0.0;
}

void FreeRateArp::clearNotes (juce::MidiBuffer& output, int offset)
{
    stopSounding (output, offset);
    heldCount = 0;
    nextNote = 0;
    untilPulse = 0.0;
}

void FreeRateArp::removeHeld (int channel, int pitch)
{
    for (int i = 0; i < heldCount; ++i)
    {
        if (held[i].channel != channel || held[i].pitch != pitch)
            continue;
        for (int j = i + 1; j < heldCount; ++j)
            held[j - 1] = held[j];
        --heldCount;
        if (i < nextNote) --nextNote;
        if (nextNote >= heldCount) nextNote = 0;
        return;
    }
}

void FreeRateArp::handleInput (const juce::MidiMessage& message, juce::MidiBuffer& output, int offset)
{
    if (message.isNoteOn())
    {
        const bool first = heldCount == 0;
        removeHeld (message.getChannel(), message.getNoteNumber());
        if (heldCount < maxNotes)
            held[heldCount++] = { message.getChannel(), message.getNoteNumber(), message.getVelocity() };
        if (first) untilPulse = 0.0;
    }
    else if (message.isNoteOff())
    {
        removeHeld (message.getChannel(), message.getNoteNumber());
        for (int i = soundingCount - 1; i >= 0; --i)
        {
            if (sounding[i].channel == message.getChannel() && sounding[i].pitch == message.getNoteNumber())
            {
                output.addEvent (juce::MidiMessage::noteOff (sounding[i].channel, sounding[i].pitch), offset);
                sounding[i] = sounding[--soundingCount];
            }
        }
        if (heldCount == 0) { nextNote = 0; untilPulse = 0.0; }
    }
    else
    {
        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            for (int i = heldCount - 1; i >= 0; --i)
                if (held[i].channel == message.getChannel())
                    removeHeld (held[i].channel, held[i].pitch);
            for (int i = soundingCount - 1; i >= 0; --i)
                if (sounding[i].channel == message.getChannel())
                {
                    output.addEvent (juce::MidiMessage::noteOff (sounding[i].channel, sounding[i].pitch), offset);
                    sounding[i] = sounding[--soundingCount];
                }
            if (heldCount == 0) { nextNote = 0; untilPulse = 0.0; }
        }
        output.addEvent (message, offset);
    }
}

void FreeRateArp::pulse (juce::MidiBuffer& output, int offset, double interval, double gateSamples, int mode)
{
    stopSounding (output, offset);
    if (heldCount == 0) return;
    const int count = mode == 0 ? heldCount : 1;
    for (int i = 0; i < count; ++i)
    {
        const Note note = held[mode == 0 ? i : nextNote];
        sounding[soundingCount++] = note;
        output.addEvent (juce::MidiMessage::noteOn (note.channel, note.pitch, note.velocity), offset);
    }
    if (mode != 0) nextNote = (nextNote + 1) % heldCount;
    untilOff = std::max (1.0, gateSamples);
    untilPulse += interval;
}

void FreeRateArp::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();
    const int samples = audio.getNumSamples();
    if (samples <= 0) return;
    juce::MidiBuffer output;
    if (resetPending.exchange (false) || bypassed)
    {
        clearNotes (output, 0);
        bypassed = false;
    }
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            const bool playing = position->getIsPlaying();
            if (wasPlaying && ! playing) clearNotes (output, 0);
            wasPlaying = playing;
        }
    }
    const double interval = sampleRateHz / std::clamp (static_cast<double> (parameters.getRawParameterValue ("rate")->load()), 0.1, 100.0);
    if (heldCount > 0 && previousInterval > 0.0 && interval != previousInterval)
        untilPulse = std::max (0.0, untilPulse + interval - previousInterval);
    previousInterval = interval;
    const float gate = std::clamp (parameters.getRawParameterValue ("gate")->load(), 1.0f, 99.0f);
    const bool milliseconds = parameters.getRawParameterValue ("gateMode")->load() >= 0.5f;
    const double gateSamples = milliseconds
        ? sampleRateHz * std::clamp (static_cast<double> (parameters.getRawParameterValue ("gateMs")->load()), 1.0, 10000.0) / 1000.0
        : interval * static_cast<double> (gate) / 100.0;
    const int mode = static_cast<int> (parameters.getRawParameterValue ("mode")->load());
    auto it = midi.begin();
    const auto end = midi.end();
    int position = 0;
    while (position < samples)
    {
        while (it != end && (*it).samplePosition <= position)
        {
            handleInput ((*it).getMessage(), output, position);
            ++it;
        }
        if (soundingCount > 0 && untilOff <= 0.0) stopSounding (output, position);
        if (heldCount > 0 && untilPulse <= 0.0) pulse (output, position, interval, gateSamples, mode);

        int next = samples;
        if (it != end) next = std::min (next, std::max (position + 1, (*it).samplePosition));
        if (heldCount > 0) next = std::min (next, position + std::max (1, static_cast<int> (std::ceil (untilPulse))));
        if (soundingCount > 0) next = std::min (next, position + std::max (1, static_cast<int> (std::ceil (untilOff))));
        const int advance = next - position;
        if (heldCount > 0) untilPulse -= advance;
        if (soundingCount > 0) untilOff -= advance;
        position = next;
    }
    midi.swapWith (output);
}

void FreeRateArp::processBlockBypassed (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    if (audio.getNumSamples() <= 0) return;
    juce::MidiBuffer output;
    if (resetPending.exchange (false) || ! bypassed)
        clearNotes (output, 0);
    bypassed = true;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int channel = message.getChannel();
        if (message.isNoteOn() || message.isNoteOff() || message.isAllNotesOff() || message.isAllSoundOff())
        {
            for (int i = soundingCount - 1; i >= 0; --i)
                if (sounding[i].channel == channel
                    && (message.isAllNotesOff() || message.isAllSoundOff() || sounding[i].pitch == message.getNoteNumber()))
                    sounding[i] = sounding[--soundingCount];
            if (message.isNoteOn() && soundingCount < maxNotes)
                sounding[soundingCount++] = { channel, message.getNoteNumber(), message.getVelocity() };
        }
        output.addEvent (message, metadata.samplePosition);
    }
    midi.swapWith (output);
}

juce::AudioProcessorEditor* FreeRateArp::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void FreeRateArp::getStateInformation (juce::MemoryBlock& destination)
{
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary (*xml, destination);
}

void FreeRateArp::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (parameters.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            for (const auto& item : { std::pair<const char*, float> { "gateMode", 0.0f }, { "gateMs", 125.0f } })
            {
                bool found = false;
                for (const auto& child : state)
                    if (child.getProperty ("id").toString() == item.first) { found = true; break; }
                if (! found)
                {
                    juce::ValueTree child ("PARAM");
                    child.setProperty ("id", item.first, nullptr);
                    child.setProperty ("value", item.second, nullptr);
                    state.appendChild (child, nullptr);
                }
            }
            parameters.replaceState (state);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FreeRateArp(); }
