# Free Rate Arp

<video controls src="demo/free-rate-arp-demo.mp4"></video>

[Watch the demo](demo/free-rate-arp-demo.mp4)

A VST3 MIDI effect that retriggers held notes at a free-running rate. Select **Chord** to repeat all held notes together, or **Played order** to cycle through notes in the order pressed. Rate is 0.1–100 Hz. Gate unit selects either **Gate** as a percentage of the interval or **Gate time** in milliseconds (1–10,000 ms). A note ends at the next pulse or when its input note is released if that happens before the selected gate time. The first note plays immediately; the plugin does not follow the host tempo.

Insert the effect in a host that supports routing MIDI output from VST3 effects to an instrument. MIDI controllers and other non-note messages pass through. Sustain is handled by the receiving instrument; note selection follows the incoming note-on and note-off messages.

## Build

Install CMake 3.22 or later and a C++17 compiler. On macOS install Xcode and its command-line tools; on Windows install Visual Studio with the Desktop C++ workload. The first configure downloads JUCE 8.0.13.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Find the VST3 bundle under `build/FreeRateArp_artefacts/Release/VST3` (the exact path depends on the generator), then install it in your host's VST3 folder. On macOS this is typically `~/Library/Audio/Plug-Ins/VST3`; on Windows it is typically `C:\Program Files\Common Files\VST3`.

Pushes to `main` also build macOS Apple Silicon and Windows VST3 bundles with GitHub Actions. Download the artifact for your platform from the workflow run's **Artifacts** section, extract the downloaded ZIP and the `.tar.gz` inside it, and install the resulting `.vst3` bundle as above.

Hosts control plugin lifecycle and MIDI routing. In particular, a host that removes or stops processing a plugin without delivering its final MIDI events must clear sounding notes itself.

When the host bypasses the effect through its processing callback, the plugin releases its generated notes and passes MIDI through. On reactivation it releases notes that were still held during bypass. If the receiving instrument has sustain engaged, it controls when released notes actually stop sounding.
