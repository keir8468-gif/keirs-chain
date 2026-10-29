# Keir's Chain — install on Mac (Ableton Live 12)

1. Unzip `KeirsChain-mac.zip`.
2. Open Terminal in the unzipped folder and clear the download quarantine:
       xattr -dr com.apple.quarantine "Keir's Chain.component" "Keir's Chain.vst3"
3. Copy the plug-ins:
       cp -R "Keir's Chain.component" ~/Library/Audio/Plug-Ins/Components/
       cp -R "Keir's Chain.vst3"      ~/Library/Audio/Plug-Ins/VST3/
4. Live → Settings → Plug-Ins: turn on "Use Audio Units v2" and/or "Use VST3 Plug-In System Folders", then Rescan.
5. Find it in the browser under Plug-Ins → Keir → Keir's Chain (use either the AU or VST3, not both).

## Quick test in Live
- Drop it on an audio track playing a loop. The KEY / CHORD readout should fill in within a few seconds (key firms up over ~20 s).
- BPM in the MPC Timing tile should match Live's tempo.
- Set Swing to 66% on 1/16: off-beat 16ths should push late in time with the grid.
- Push Mackie Trim up: the LEDs go green → yellow → red, the tone crunches, the level stays put.
- Brightness ±: high shelf lift/cut.
- 2000XL LED off/on: subtle top-end/grain change.
- Bypass: light goes out, signal returns clean and time-aligned.
