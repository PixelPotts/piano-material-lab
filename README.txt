PIANO / MATERIAL LAB

Open piano.exe. The native C app has a 1440 x 960 black workbench with 42
physical knobs, one white key, live waveform and model readouts.

CONTROLS
- Hold the white key, Space or C to strike and hold the note.
- Drag knobs upward/downward. Shift makes adjustment ten times finer.
- Mouse wheel or arrow keys adjusts the selected knob; Tab selects the next.
- Double-click a knob/value or press Enter to type an exact number.
  Enter applies; Escape cancels. Out-of-range values are rejected.
- Right-click a knob to restore its default.
- TUNE C4 solves the tension for the current string material and dimensions.
- S toggles the sustain pedal between 0% and 100% lift.
- AUTO STRIKE repeats every 2.5 seconds with a one-second hold.
- Escape in the main window stops auto-strike and releases the damper.
- The monitor-level slider changes playback gain, separately from physics.

The seven groups are hammer/felt, strings/material, unison/bridge,
soundboard wood, casing/resonance, damper/pedal, and room/microphone.
Every physical control has units, a defined range and hover/selection help.
See CONTROL-GUIDE.txt for all 42 parameters and their validation status.
See MODEL.txt for equations, assumptions, model limits and research references.

PERSISTENCE
Physical settings save on close to current-settings.piano and load on reopen.
SAVE PRESET and LOAD PRESET use user-preset.piano. Copy or rename that text file
outside the app to keep several named presets. RESET ALL restores the reference
instrument. Saved presets contain all 42 physical properties, not monitor gain.

EXPORT WAV renders the current physical settings to test-note.wav, with a
one-second hold followed by release. It uses the fixed reference monitor gain.
This is compatible with the audio-model review tool below.

BUILD
Run build.bat. It downloads a checksum-verified portable Zig 0.13.0 / Clang
compiler on first use, then builds optimized
Windows C code with only system UI/audio libraries. The older Tiny C Compiler
is retained only in local archived experiments (not in Git): a test exposed a
floating-point code-generation error in that compiler. No install is needed.

VERIFICATION
piano.exe --test: four 8-second WAVs from the live renderer.
python3 verify-sound.py: signal, pitch, decay, release and clipping checks.
piano.exe --sweep: all 42 min/max parameter renders and transition checks.
engine-validation.exe: deterministic physics/continuity/preset/stress checks.
powershell -ExecutionPolicy Bypass -File check-workbench.ps1: isolated native
UI test copy; exercises all knobs, exact entry, presets and live adjustment.
Test copy outputs go to validation/runtime; your main app/settings are preserved.
WORKBENCH-VALIDATION.txt contains the consolidated results and limitations.

Local-only previous source, executable and sound are preserved under
backups/before-physical-workbench. The listener setup and API key are unchanged.

Audio-model listening tool
--------------------------
Run setup-audio-key.cmd and enter your OpenAI API key in the masked local form.
Create a key at https://platform.openai.com/api-keys and ensure API billing is
available. Do not put the key in chat or source files. Setup stores a Windows
DPAPI-protected credential at %LOCALAPPDATA%\CPiano\api-key.xml; delete that
file to remove it. This credential is accessible to your Windows user account.
Alternatively configure OPENAI_API_KEY in your environment.

Run listen.cmd to send test-note.wav to gpt-audio-1.5 and get a text assessment.
This sends audio to OpenAI and uses your API project's billing. No microphone
or loopback capture is involved. The coding agent reads the audio model's
assessment; it does not gain native hearing. Musical judgments can be wrong.
The tool uses store:false and does not log keys or encoded audio.

From WSL in this folder:
  python3 listen.py --dry-run
  python3 listen.py test-held.wav
  python3 listen.py test-note.wav --reference real-piano.wav
  python3 test-listener.py

The reference option accepts another 16-bit PCM WAV, mono/stereo, up to 30 sec.
Comparison is on original levels; level-match reference clips when appropriate.
Reports are timestamped JSON and text in audio-reviews, with model, usage,
file hashes, and the assessment. Existing reports are preserved.
Tests use the real WAV files and a local simulated HTTP API; they do not prove
live API access or perceptual audio understanding. Actual API validation is
pending until a working key is configured and a real assessment is returned.
