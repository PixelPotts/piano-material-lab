"""Integration checks of PCM produced by piano.exe --test, no mocked synth."""
import wave
from pathlib import Path
import numpy as np

root = Path(__file__).resolve().parent
signals = {}
report = []
for name in ("note", "held", "repeat", "silence"):
    with wave.open(str(root / ("test-" + name + ".wav"))) as f:
        assert (f.getnchannels(), f.getsampwidth(), f.getframerate()) == (1, 2, 44100)
        a = np.frombuffer(f.readframes(f.getnframes()), dtype="<i2").astype(float) / 32768
    assert len(a) == 44100 * 8
    assert np.max(abs(a)) < .99, "Clipping: " + name
    signals[name] = a
    report.append(f"{name}: 8 sec mono 44100 Hz; peak {np.max(abs(a)):.6f}; no clipping")

def rms(a):
    return np.sqrt(np.mean(a*a))

assert np.max(abs(signals["silence"])) == 0
assert rms(signals["note"][:44100]) > .01
assert rms(signals["note"][3*44100:]) < 1/32768
assert rms(signals["held"][6*44100:]) > 5/32768
assert rms(signals["held"][6*44100:]) < rms(signals["held"][:44100])
assert rms(signals["repeat"][6*44100:]) > .01
np.testing.assert_array_equal(signals["note"][:44100],signals["held"][:44100])
a=signals["held"][4410:48510]*np.hanning(44100)
spec=abs(np.fft.rfft(a,n=262144))
freq=np.fft.rfftfreq(262144,1/44100)
band=(freq>255)&(freq<270)
peak=freq[band][np.argmax(spec[band])]
assert abs(peak-261.625565)<.5
report.append(f"Fundamental peak: {peak:.3f} Hz (target 261.626 Hz)")
report.append("PASS: silence, pitch, sustain decay, damper release, repeated strikes, PCM format, no clipping.")
report.append("Scope: generated PCM tested; physical speakers and perceptual piano realism not verified.")
print("\n".join(report))
(root/"sound-validation.txt").write_text("\n".join(report)+"\n")
