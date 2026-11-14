#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# CRITICAL: how loud each kind is (tests/kinds3 wrote one WAV a kind), so that the kinds of one sound can be trimmed to
# its first. No ear here: two measures and their mean. K-weighted (ITU-R BS.1770: it hardly lowers the lows) and
# A-weighted (it lowers them a lot); a struck or held sound by its loudest 400 ms, a machine or a layer's engine (tests/eng3) by its
# loudest three seconds.   loud3.py DIR [DIR_LOW DIR_HIGH]   (the two others: the same kinds at their variant's two ends)
import sys, glob, wave, numpy as np
from scipy.signal import lfilter, bilinear, zpk2tf
def load(p):
    w = wave.open(p); sr = w.getframerate(); a = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float) / 32768; return a.reshape(-1, 2), sr
def kweight(x, fs):
    f0, G, Q = 1681.974450955533, 3.999843853973347, 0.7071752369554196; K = np.tan(np.pi * f0 / fs); Vh = 10 ** (G / 20); Vb = Vh ** 0.4996667741545416
    a0 = 1 + K / Q + K * K; b = [(Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0]; a = [1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0]
    y = lfilter(b, a, x, axis=0)
    f0, Q = 38.13547087602444, 0.5003270373238773; K = np.tan(np.pi * f0 / fs); a = [1, 2 * (K * K - 1) / (1 + K / Q + K * K), (1 - K / Q + K * K) / (1 + K / Q + K * K)]
    return lfilter([1, -2, 1], a, y, axis=0)
def aweight(x, fs):
    f1, f2, f3, f4 = 20.598997, 107.65265, 737.86223, 12194.217; z = [0, 0, 0, 0]; p = [-2 * np.pi * f1] * 2 + [-2 * np.pi * f4] * 2 + [-2 * np.pi * f2, -2 * np.pi * f3]
    b, a = zpk2tf(z, p, 1); w = 2 * np.pi * 1000; g = abs(np.polyval(b, 1j * w) / np.polyval(a, 1j * w)); bz, az = bilinear(b / g, a, fs); return lfilter(bz, az, x, axis=0)
def loudest(x, fs, secs):
    n = int(secs * fs); h = n // 4; e = (x ** 2).sum(1); c = np.concatenate([[0], np.cumsum(e)])
    m = max((c[i + n] - c[i]) / n for i in range(0, max(1, len(e) - n), h)); return 10 * np.log10(m + 1e-12)
def measure(p):
    a, fs = load(p); fam = int(p.split('/')[-1][:2]); secs = 3.0 if fam in (0, 1, 2, 3) or fam >= 14 else 0.4
    return 0.5 * (loudest(kweight(a, fs), fs, secs) + loudest(aweight(a, fs), fs, secs)), 20 * np.log10(np.abs(a).max() + 1e-9)
dirs = sys.argv[1:]; ref = {}
for p in sorted(glob.glob(dirs[0] + '/[0-9]*.wav')):
    n = p.split('/')[-1][:-4]; fam, kind = int(n[:2]), int(n.split('-')[2]); L, pk = measure(p)
    if kind == 0: ref[fam] = L
    ext = ''
    if len(dirs) == 3:
        lo = measure(dirs[1] + '/' + n + '.wav')[0]; hi = measure(dirs[2] + '/' + n + '.wav')[0]; ext = '   variant 0: %+5.1f   variant 100: %+5.1f' % (lo - L, hi - L)
    print('%-26s %6.1f  peak %6.1f  vs its first %+5.1f dB   trim x%.2f%s' % (n[3:], L, pk, L - ref[fam], 10 ** ((ref[fam] - L) / 20), ext))
