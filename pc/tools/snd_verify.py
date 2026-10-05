#!/usr/bin/env python3
"""Numeric checks of the PC port's sound output (docs/pc_port.md, "Sound: what plays").

Nobody has to listen for these: they compare what the program played with what
it should have played, from three independent sources:

  the content file     the wave data itself, decoded here in Python
  --dump-waves         every wave decoded by the mixer's decoder (reference WAVs)
  NEWSCHANNEL_AX_LOG   what nw4r::snd asked AX for, per audio frame and voice:
                       sample address, pitch ratio, envelope, mix volumes
  --audio-dump / --render-sounds   the samples that came out

Subcommands:

  waves WAVES_DIR CONTENT_FILE
      Decodes every DSP-ADPCM wave of WAVES_DIR/waves.txt from the content file
      (orig/HAGE/contents/09.app) and compares it, sample for sample, with the
      WAV the mixer's decoder wrote.

  stats WAV [--ax-log LOG]
      Level statistics of a dump: peak, RMS per 100 ms, DC offset, clipping,
      silence ratio, stereo balance, and the bursts (start, length, dominant
      frequency), each with the sound that nw4r::snd started just before it.

  render RENDER_DIR WAVES_DIR
      For every sound of `newschannel --render-sounds RENDER_DIR` (run with
      NEWSCHANNEL_AX_LOG=RENDER_DIR/ax.log): rebuilds the expected output from
      the reference waves and the logged voice parameters (position, ratio,
      envelope, mix volumes; linear interpolation) and compares it with the
      rendered WAV: correlation, gain, delay, and whether every voice's
      address advanced by its pitch ratio.

  dump WAV LOG WAVES_DIR
      The same for an --audio-dump of the running game, per started sound.

The output files are derived from the game's assets: keep them below build/.
"""

import argparse
import os
import re
import sys
import wave as wavemod

import numpy as np

FRAME = 96      # samples per audio frame
RATE = 32000
EFFECTS = False  # the dump has the game's aux effects in it (reverb): levels are not exact


# --- files ---------------------------------------------------------------------------

def read_wav(path):
    with wavemod.open(path, "rb") as w:
        rate, channels, frames = w.getframerate(), w.getnchannels(), w.getnframes()
        data = np.frombuffer(w.readframes(frames), dtype="<i2")
    return rate, data.reshape(-1, channels).astype(np.int32)


def read_waves_list(waves_dir):
    waves = []
    with open(os.path.join(waves_dir, "waves.txt")) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = line.split()
            w = dict(file=int(p[0]), index=int(p[1]), format=int(p[2]), rate=int(p[3]), samples=int(p[4]),
                     loop=int(p[5]), loop_start=int(p[6]), channels=int(p[7]), offset=int(p[8]),
                     bytes=int(p[9]), wav=p[10], archive_offset=int(p[11]))
            if len(p) >= 34:
                h = [int(x, 16) for x in p[12:34]]
                w["coefs"] = [x - 0x10000 if x & 0x8000 else x for x in h[:16]]
                w["pred_scale"], w["yn1"], w["yn2"] = h[16], h[17], h[18]
            waves.append(w)
    return waves


def decode_dsp_adpcm(data, samples, coefs, yn1, yn2):
    """DSP-ADPCM: 8-byte frames, a header byte (predictor, scale) and 14 nibbles."""
    out = np.zeros(samples, dtype=np.int32)
    s16 = lambda v: v - 0x10000 if v & 0x8000 else v
    yn1, yn2 = s16(yn1), s16(yn2)
    i = 0
    for frame in range((samples + 13) // 14):
        base = frame * 8
        header = data[base]
        c1, c2 = coefs[(header >> 4) * 2], coefs[(header >> 4) * 2 + 1]
        scale = 1 << (header & 15)
        for n in range(14):
            if i >= samples:
                break
            byte = data[base + 1 + n // 2]
            nibble = (byte >> 4) if n % 2 == 0 else (byte & 15)
            if nibble >= 8:
                nibble -= 16
            value = ((nibble * scale) << 11) + 1024 + c1 * yn1 + c2 * yn2 >> 11
            value = max(-32768, min(32767, value))
            out[i] = value
            yn2, yn1 = yn1, value
            i += 1
    return out


# --- NEWSCHANNEL_AX_LOG ----------------------------------------------------------------

VOICE = re.compile(
    r"  voice +(\d+) (adpcm|pcm16|pcm8)( stream)? cur=(\w+) loop=(\w+) end=(\w+)( looped)? src=(\d+)/(\d+) "
    r"ratio=([\d.]+) ve=(\w{4})([+-]\d+) L=(\w+) R=(\w+) S=(\w+) A=(\w+)/(\w+)/(\w+) B=(\w+)/(\w+)/(\w+) "
    r"C=(\w+)/(\w+)/(\w+) lpf=(\d+) rmt=(\d+)( ENDED)?( BAD-ADDRESS)?")
START = re.compile(r"\[ax frame (\d+)\] snd start: id (\d+) (\S+) -> (.*) \(player (\w+), retrace (\d+)\)")


def read_log(path):
    frames = {}      # frame -> list of voices
    starts = []      # (frame, id, label, result, retrace)
    current = None
    with open(path, errors="replace") as f:
        for line in f:
            if line.startswith("ax frame "):
                head = line.split()[2]
                if head.endswith(":"):
                    current = None
                else:
                    current = int(head)
                    frames[current] = []
                continue
            m = VOICE.match(line)
            if m and current is not None:
                g = m.groups()
                frames[current].append(dict(
                    index=int(g[0]), format=g[1], cur=int(g[3], 16), loop=int(g[4], 16), end=int(g[5], 16),
                    looped=g[6] is not None, src=int(g[7]), coef=int(g[8]), ratio=float(g[9]),
                    ve=int(g[10], 16), delta=int(g[11]), L=int(g[12], 16), R=int(g[13], 16), S=int(g[14], 16),
                    aux=[int(x, 16) for x in g[15:24]], lpf=int(g[24]), ended=g[26] is not None,
                    bad=g[27] is not None))
                continue
            m = START.match(line)
            if m:
                starts.append((int(m.group(1)), int(m.group(2)), m.group(3), m.group(4), int(m.group(6))))
    return frames, starts


# --- expected output -------------------------------------------------------------------

class Bank:
    """The reference waves, and where each one's data is in the DSP's address space."""

    def __init__(self, waves_dir):
        self.waves = [w for w in read_waves_list(waves_dir) if w["channels"] == 1]
        for w in self.waves:
            rate, data = read_wav(w["wav"])
            w["pcm"] = data[:, 0].astype(np.float64)
        self.base = None

    def find_base(self, frames):
        """The byte address of the wave data: the value that puts the most voices at the start of a wave."""
        offsets = {w["offset"] for w in self.waves if w["format"] == 2}
        votes = {}
        seen = set()
        for number in sorted(frames):
            for v in frames[number]:
                if v["format"] != "adpcm" or (v["cur"] & 15) != 2 or v["cur"] in seen:
                    continue
                seen.add(v["cur"])
                for offset in offsets:
                    base = (v["cur"] >> 1) - 1 - offset
                    votes[base] = votes.get(base, 0) + 1
        if not votes:
            return False
        self.base = max(votes, key=votes.get)
        return True

    def locate(self, v):
        """(wave, sample index of the voice's current address), or (None, 0)."""
        if v["format"] != "adpcm" or self.base is None:
            return None, 0
        rel = (v["cur"] >> 1) - self.base
        for w in self.waves:
            if w["format"] == 2 and w["offset"] <= rel < w["offset"] + w["bytes"]:
                nibble = v["cur"] - 2 * (self.base + w["offset"])
                return w, (nibble // 16) * 14 + (nibble % 16) - 2
        return None, 0

    def sample_of(self, w, address):
        nibble = address - 2 * (self.base + w["offset"])
        return (nibble // 16) * 14 + (nibble % 16) - 2


_four_tap = {}


def four_tap(select):
    """The mixer's 4-tap table: a windowed sinc per coefficient set, 128 phases."""
    if select not in _four_tap:
        cutoff = (0.25, 0.375, 0.5)[select]
        table = np.zeros((128, 4))
        for phase in range(128):
            for k in range(4):
                t = phase / 128.0 - (k - 1)
                window = 0.0 if abs(t) >= 2 else 0.5 + 0.5 * np.cos(np.pi * t / 2)
                table[phase, k] = 2 * cutoff * np.sinc(2 * cutoff * t) * window
            table[phase] /= table[phase].sum()
        _four_tap[select] = table
    return _four_tap[select]


class Run:
    """One note on one AX voice: the unrolled read position and its fraction."""

    def __init__(self, wave, start_sample, v, bank):
        self.wave = wave
        self.read = float(start_sample)   # samples read so far (unrolled over loops)
        self.frac = 0.0
        self.end = bank.sample_of(wave, v["end"])
        self.loop_start = bank.sample_of(wave, v["loop"]) if v["looped"] else None
        self.max_error = 0
        self.frames = 0
        self.ratios = set()

    def index(self, read):
        """Unrolled read counts -> sample indices of the wave (-1: silence)."""
        read = np.asarray(read, dtype=np.int64)
        out = np.where(read < 0, -1, read)
        if self.loop_start is not None:
            length = self.end - self.loop_start + 1
            over = read > self.end
            out = np.where(over, self.loop_start + (read - self.end - 1) % max(length, 1), out)
        else:
            out = np.where(read > self.end, -1, out)
        return out

    def fetch(self, read):
        index = self.index(read)
        pcm = self.wave["pcm"]
        ok = (index >= 0) & (index < len(pcm))
        return np.where(ok, pcm[np.clip(index, 0, len(pcm) - 1)], 0.0)

    def frame(self, v, model=False):
        """The 96 samples the voice should add to (left, right) in this audio frame.

        model=False: linear interpolation of the reference wave, which knows
        nothing of the mixer's resampler. model=True: the mixer's own 4-tap
        filter (the table of src/pc/audio/ax_dsp.cpp, recomputed here)."""
        ratio = v["ratio"]
        self.ratios.add(round(ratio, 4))
        self.frames += 1
        if v["src"] == 2:      # no rate conversion
            samples = self.fetch(self.read + np.arange(FRAME))
            self.read += FRAME
        else:
            # The DSP holds the last four samples it read; the 4-tap filter is
            # centred between the second and the third, the linear one between
            # the two oldest (src/pc/audio/ax_dsp.cpp).
            delay = 3 if v["src"] == 0 else 4
            steps = self.frac + ratio * np.arange(1, FRAME + 1)
            position = self.read - delay + steps
            lower = np.floor(position)
            t = position - lower
            if model and v["src"] == 0:
                taps = four_tap(min(v["coef"], 2))[np.minimum((t * 128).astype(np.int64), 127)]
                samples = sum(self.fetch(lower + k - 1) * taps[:, k] for k in range(4))
            else:
                samples = self.fetch(lower) * (1 - t) + self.fetch(lower + 1) * t
            total = self.frac + ratio * FRAME
            self.read += np.floor(total)
            self.frac = total - np.floor(total)
        # The envelope is logged after the frame, and steps once per sample.
        ve_before = v["ve"] - FRAME * v["delta"]
        envelope = (ve_before + v["delta"] * np.arange(FRAME)) / 32768.0
        samples = samples * envelope
        return samples * (v["L"] / 32768.0), samples * (v["R"] / 32768.0)


def expected_output(frames, first, last, bank, model=False):
    """Expected (left, right) for audio frames first..last, and statistics of the voices."""
    count = last - first + 1
    left = np.zeros(count * FRAME)
    right = np.zeros(count * FRAME)
    runs = {}
    stats = dict(notes=0, unmapped=0, max_error=0, voice_frames=0, aux_frames=0, resync=0, pitches=[], bad=0,
                 lpf=0, voices_peak=0)
    finished = []
    for number in range(first, last + 1):
        voices = frames.get(number, [])
        stats["voices_peak"] = max(stats["voices_peak"], len(voices))
        present = set()
        for v in voices:
            present.add(v["index"])
            stats["voice_frames"] += 1
            stats["bad"] += 1 if v["bad"] else 0
            stats["lpf"] += 1 if v["lpf"] else 0
            if any(v["aux"]):
                stats["aux_frames"] += 1
            wave, sample = bank.locate(v)
            run = runs.get(v["index"])
            if run is not None and wave is not None and run.wave is wave:
                predicted = int(run.index(np.array([int(run.read)]))[0])
                error = abs(predicted - sample)
                if error > 8:
                    # Not where this note would be: another note on the same voice.
                    finished.append(run)
                    run = None
                else:
                    run.max_error = max(run.max_error, error)
                    if error:
                        stats["resync"] += 1
                        run.read += sample - predicted
            elif run is not None:
                finished.append(run)
                run = None
            if run is None:
                if wave is None:
                    stats["unmapped"] += 1
                    runs.pop(v["index"], None)
                    continue
                run = Run(wave, sample, v, bank)
                runs[v["index"]] = run
                stats["notes"] += 1
                stats["pitches"].append((wave, v["ratio"]))
            l, r = run.frame(v, model)
            at = (number - first) * FRAME
            left[at:at + FRAME] += l
            right[at:at + FRAME] += r
            if v["ended"]:
                finished.append(run)
                del runs[v["index"]]
        for index in [i for i in runs if i not in present]:
            finished.append(runs.pop(index))
    finished.extend(runs.values())
    stats["max_error"] = max([r.max_error for r in finished], default=0)
    return left, right, stats


def compare(expected, actual, max_lag):
    """Best alignment of `actual` to `expected`: (correlation, gain, lag in samples)."""
    n = len(expected)
    if n == 0 or not np.any(expected):
        return None
    best = None
    padded = np.concatenate([np.zeros(max_lag), actual.astype(np.float64), np.zeros(max_lag + n)])
    energy = float(np.dot(expected, expected))
    for lag in range(-max_lag, max_lag + 1):
        segment = padded[max_lag + lag:max_lag + lag + n]
        dot = float(np.dot(expected, segment))
        if best is None or dot > best[0]:
            best = (dot, lag, float(np.dot(segment, segment)))
    dot, lag, actual_energy = best
    if actual_energy == 0:
        return 0.0, 0.0, lag
    return dot / np.sqrt(energy * actual_energy), dot / energy, lag


def cents(wave, ratio):
    """Pitch of a note against the wave's own rate: (semitones, rounded; cents off that semitone)."""
    pitch = ratio * RATE / wave["rate"]
    semitones = 12 * np.log2(pitch)
    return int(round(semitones)), 100 * (semitones - round(semitones))


# --- subcommands -----------------------------------------------------------------------

def cmd_waves(args):
    waves = read_waves_list(args.waves_dir)
    content = open(args.content, "rb").read()
    archive = content.find(b"RSAR\xfe\xff")
    if archive < 0:
        sys.exit("no sound archive (RSAR) in %s" % args.content)
    checked = different = 0
    total_samples = 0
    for w in waves:
        if w["format"] != 2 or "coefs" not in w:
            continue
        at = archive + w["archive_offset"]
        data = content[at:at + w["bytes"]]
        mine = decode_dsp_adpcm(data, w["samples"], w["coefs"], w["yn1"], w["yn2"])
        rate, theirs = read_wav(w["wav"])
        theirs = theirs[:, 0]
        checked += 1
        total_samples += w["samples"]
        if rate != w["rate"] or len(theirs) != len(mine) or np.any(theirs != mine):
            different += 1
            bad = int(np.argmax(theirs[:len(mine)] != mine[:len(theirs)])) if len(theirs) and len(mine) else -1
            print("wave %d/%d: DIFFERENT (first at sample %d of %d)" % (w["file"], w["index"], bad, w["samples"]))
        elif args.verbose:
            x = mine.astype(np.float64)
            print("wave %2d/%-3d %5d Hz %7d samples %s peak %5d rms %7.1f dc %6.1f" % (
                w["file"], w["index"], w["rate"], w["samples"], "loop" if w["loop"] else "    ",
                np.max(np.abs(mine)), np.sqrt(np.mean(x * x)), np.mean(x)))
    print("%d DSP-ADPCM waves, %d samples: %d differ between the Python decoder and the mixer's decoder" % (
        checked, total_samples, different))
    return 1 if different else 0


def dominant_frequency(x, rate):
    if len(x) < 64:
        return 0.0
    x = x - np.mean(x)
    spectrum = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    spectrum[0] = 0
    return float(np.argmax(spectrum)) * rate / len(x)


def wav_stats(data, rate):
    mono = data.mean(axis=1)
    out = {}
    out["seconds"] = len(data) / rate
    out["peak"] = int(np.max(np.abs(data))) if len(data) else 0
    out["clipped"] = int(np.sum(np.abs(data) >= 32767))
    out["dc"] = [float(np.mean(data[:, c])) for c in range(data.shape[1])]
    out["rms"] = [float(np.sqrt(np.mean(data[:, c].astype(np.float64) ** 2))) for c in range(data.shape[1])]
    out["silence"] = float(np.mean(np.max(np.abs(data), axis=1) <= 2)) if len(data) else 1.0
    # Full-scale noise has a flat spectrum and no sample-to-sample correlation.
    x = mono - np.mean(mono)
    energy = float(np.dot(x, x))
    out["autocorr1"] = float(np.dot(x[1:], x[:-1]) / energy) if energy > 0 else 0.0
    return out


def find_bursts(data, rate, threshold=24, gap=0.15):
    loud = np.max(np.abs(data), axis=1) > threshold
    indices = np.flatnonzero(loud)
    bursts = []
    if len(indices) == 0:
        return bursts
    start = previous = indices[0]
    for i in indices[1:]:
        if i - previous > gap * rate:
            bursts.append((start, previous + 1))
            start = i
        previous = i
    bursts.append((start, previous + 1))
    return bursts


def cmd_stats(args):
    rate, data = read_wav(args.wav)
    if data.shape[1] != 2:
        sys.exit("expected a stereo WAV")
    s = wav_stats(data, rate)
    print("%s: %.2f s, %d Hz" % (args.wav, s["seconds"], rate))
    print("  peak %d, clipped samples %d, DC offset L %.1f R %.1f, RMS L %.1f R %.1f, silent %.1f %%, "
          "lag-1 autocorrelation %.3f" % (s["peak"], s["clipped"], s["dc"][0], s["dc"][1], s["rms"][0], s["rms"][1],
                                          100 * s["silence"], s["autocorr1"]))
    starts = []
    frame_offset = 0
    if args.ax_log:
        frames, starts = read_log(args.ax_log)
    bursts = find_bursts(data, rate)
    print("  %d bursts (louder than 24, gaps under 150 ms joined):" % len(bursts))
    print("  %8s %8s %6s %7s %7s %6s %8s  %s" % ("start", "length", "peak", "rmsL", "rmsR", "dc", "dominant",
                                                 "sound started before it (audio frame, retrace)"))
    for a, b in bursts:
        x = data[a:b].astype(np.float64)
        mono = x.mean(axis=1)
        label = ""
        if starts:
            at_frame = a / FRAME + frame_offset
            before = [st for st in starts if st[0] <= at_frame + 2]
            if before:
                st = before[-1]
                label = "%d %s (frame %d = %.2f s, retrace %d)%s" % (
                    st[1], st[2], st[0], st[0] * FRAME / rate, st[4], "" if st[3] == "ok" else " " + st[3])
        print("  %7.2fs %7.0fms %6d %7.1f %7.1f %6.1f %6.0fHz  %s" % (
            a / rate, (b - a) * 1000.0 / rate, np.max(np.abs(x)), np.sqrt(np.mean(x[:, 0] ** 2)),
            np.sqrt(np.mean(x[:, 1] ** 2)), np.mean(mono), dominant_frequency(mono, rate), label))
    if args.rms:
        window = rate // 10
        print("  RMS per 100 ms (left/right), windows that are not silent:")
        for i in range(0, len(data) - window + 1, window):
            x = data[i:i + window].astype(np.float64)
            l, r = np.sqrt(np.mean(x[:, 0] ** 2)), np.sqrt(np.mean(x[:, 1] ** 2))
            if l > 1 or r > 1:
                print("    %6.1fs %8.1f %8.1f" % (i / rate, l, r))
    return 0


def report_sound(name, expected_l, expected_r, actual, stats, max_lag, verbose, model=None):
    result_l = compare(expected_l, actual[:, 0], max_lag)
    result_r = compare(expected_r, actual[:, 1], max_lag)
    results = [r for r in (result_l, result_r) if r is not None]
    pitches = sorted({cents(w, ratio) for w, ratio in stats["pitches"]})
    worst_cents = max([abs(c) for _, c in pitches], default=0.0)
    if not results:
        corr = gain = lag = None
        verdict = "silent as expected" if not np.any(np.abs(actual) > 2) else "UNEXPECTED OUTPUT"
        if stats["voice_frames"] and stats["unmapped"] == 0 and not np.any(np.abs(actual) > 2):
            verdict = "silent (volume 0)"
    else:
        best = max(results, key=lambda r: r[0])
        corr = min(r[0] for r in results)
        gain, lag = best[1], best[2]
        # The same with the mixer's own resampling filter in the rebuilt
        # output: what is left then is decoding, position, level and timing.
        low = [compare(e, actual[:, c], max_lag) for c, e in enumerate(model or ()) if np.any(e)]
        low = [r for r in low if r is not None]
        corr_low = min(r[0] for r in low) if low else corr
        gain_low = max(low, key=lambda r: r[0])[1] if low else gain
        strict = stats["aux_frames"] == 0 or not EFFECTS
        verdict = "ok" if corr >= 0.9 and (not strict or (corr_low >= 0.995 and 0.97 <= gain_low <= 1.03)) \
            else "CHECK"
    if stats["unmapped"]:
        verdict += " (%d voice frames not in the bank)" % stats["unmapped"]
    if stats["bad"]:
        verdict += " BAD-ADDRESS"
    print("%-30s %5d %6d %4d %6s %6s %6s %6s %5s %5d %6d %6.1f %5d  %s" % (
        name, stats["notes"], stats["voice_frames"], stats["voices_peak"],
        "-" if corr is None else "%.3f" % corr, "-" if gain is None else "%.2f" % gain,
        "-" if corr is None else "%.3f" % corr_low, "-" if corr is None else "%.2f" % gain_low,
        "-" if lag is None else "%d" % lag, stats["max_error"], stats["aux_frames"], worst_cents,
        int(np.max(np.abs(actual))) if len(actual) else 0, verdict))
    if verbose and pitches:
        print("      notes at semitones (cents off): " + ", ".join("%+d (%+.0f)" % p for p in pitches))
    return verdict.startswith("ok") or verdict.startswith("silent")


HEADER = "%-30s %5s %6s %4s %6s %6s %6s %6s %5s %5s %6s %6s %5s  %s" % (
    "sound", "notes", "v*frm", "poly", "corr", "gain", "corrF", "gainF", "lag", "poserr", "auxfrm", "cents", "peak", "verdict")
LEGEND = """  notes   voices started (one per note and channel)       v*frm   voice frames (3 ms each)
  poly    most voices in one audio frame
  corr    correlation of the output with the output rebuilt from the reference waves (worse channel)
  gain    output level over rebuilt level                    lag     delay of the output, samples
  corrF, gainF     the same with the mixer's own 4-tap resampling filter in the rebuilt output
  poserr  largest difference between a voice's logged address and the one its pitch ratio predicts
  auxfrm  voice frames with an aux send (reverb or the game's voice effect; not in the rebuilt output)
  cents   largest distance of a note's pitch from a semitone of its wave's own rate"""


def cmd_render(args):
    bank = Bank(args.waves_dir)
    frames, starts = read_log(os.path.join(args.render_dir, "ax.log"))
    if not bank.find_base(frames):
        sys.exit("no ADPCM voice in the log")
    files = {int(name[:3]): name for name in os.listdir(args.render_dir) if re.match(r"\d{3}_.*\.wav$", name)}
    print(HEADER)
    bad = 0
    for i, (frame, sound, label, result, _) in enumerate(starts):
        if sound not in files:
            continue
        rate, actual = read_wav(os.path.join(args.render_dir, files[sound]))
        blocks = len(actual) // FRAME
        left, right, stats = expected_output(frames, frame, frame + blocks - 1, bank)
        model = expected_output(frames, frame, frame + blocks - 1, bank, True)[:2]
        if not report_sound("%3d %s" % (sound, label), left, right, actual, stats, 4 * FRAME, args.verbose, model):
            bad += 1
    print(LEGEND)
    print("%d sounds, %d to check" % (len(starts), bad))
    return 1 if bad else 0


def cmd_dump(args):
    global EFFECTS
    EFFECTS = True
    bank = Bank(args.waves_dir)
    frames, starts = read_log(args.log)
    if not bank.find_base(frames):
        sys.exit("no ADPCM voice in the log")
    rate, actual = read_wav(args.wav)
    numbers = sorted(frames)
    # Block b of the dump is audio frame b + c; find c on the whole run.
    first, last = numbers[0], numbers[-1]
    left, right, stats = expected_output(frames, first, last, bank)
    whole = None
    for c in range(-4, 5):
        at = (first + c) * FRAME
        if at < 0:
            continue
        segment = actual[at:at + len(left)]
        if len(segment) < len(left):
            segment = np.vstack([segment, np.zeros((len(left) - len(segment), 2), dtype=segment.dtype)])
        r = compare(left, segment[:, 0], FRAME // 2)
        if r is not None and (whole is None or r[0] > whole[1][0]):
            whole = (c, r)
    if whole is None:
        sys.exit("nothing to compare")
    c = whole[0]
    print("dump block = audio frame %+d; whole run: correlation %.3f, gain %.2f, lag %d samples" % (
        -c, whole[1][0], whole[1][1], whole[1][2]))
    print(HEADER)
    bad = 0
    for i, (frame, sound, label, result, retrace) in enumerate(starts):
        end = starts[i + 1][0] - 1 if i + 1 < len(starts) else last
        # A sound's voices can outlive the next start; the segment still holds
        # whatever is playing, so the comparison stays valid.
        end = max(end, frame)
        left, right, stats = expected_output(frames, frame, end, bank)
        model = expected_output(frames, frame, end, bank, True)[:2]
        at = (frame + c) * FRAME
        segment = actual[max(at, 0):max(at, 0) + len(left)]
        if len(segment) < len(left):
            segment = np.vstack([segment, np.zeros((len(left) - len(segment), 2), dtype=actual.dtype)])
        name = "%3d %s @%.2fs" % (sound, label[:16], frame * FRAME / rate)
        if not report_sound(name, left, right, segment, stats, FRAME, args.verbose, model):
            bad += 1
    print(LEGEND)
    print("(voices that started before a segment are rebuilt from where the log first shows them)")
    return 1 if bad else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("waves")
    p.add_argument("waves_dir")
    p.add_argument("content")
    p.add_argument("-v", "--verbose", action="store_true")
    p.set_defaults(func=cmd_waves)
    p = sub.add_parser("stats")
    p.add_argument("wav")
    p.add_argument("--ax-log")
    p.add_argument("--rms", action="store_true", help="also print the RMS of every 100 ms window")
    p.set_defaults(func=cmd_stats)
    p = sub.add_parser("render")
    p.add_argument("render_dir")
    p.add_argument("waves_dir")
    p.add_argument("-v", "--verbose", action="store_true")
    p.set_defaults(func=cmd_render)
    p = sub.add_parser("dump")
    p.add_argument("wav")
    p.add_argument("log")
    p.add_argument("waves_dir")
    p.add_argument("-v", "--verbose", action="store_true")
    p.set_defaults(func=cmd_dump)
    args = parser.parse_args()
    sys.exit(args.func(args))


if __name__ == "__main__":
    main()
