#!/usr/bin/python3

import numpy as np
import soundfile as sf
import math
import random
import scipy.signal as sg
from notes import pitch

SAMPLE_RATE = 44100
BITS = 16

def bandpass(wav, low, high, pass_type=4):

    low = low / (SAMPLE_RATE / 2)
    high = high / (SAMPLE_RATE / 2)

    b, a = sg.butter(pass_type, (low, high), 'band')
    data = np.ndarray(shape=(len(wav),), buffer=np.array(wav))
    return list(sg.filtfilt(b, a, data))

def lowpass(wav, low, pass_type=4):

    low = low / (SAMPLE_RATE / 2)

    b, a = sg.butter(pass_type, low, 'low')
    data = np.ndarray(shape=(len(wav),), buffer=np.array(wav))
    return list(sg.filtfilt(b, a, data))

def highpass(wav, high, pass_type=4):

    high = high / (SAMPLE_RATE / 2)

    b, a = sg.butter(pass_type, high, 'high')
    data = np.ndarray(shape=(len(wav),), buffer=np.array(wav))
    return list(sg.filtfilt(b, a, data))

def convert(wav, scale=True, silence_ms=None):
    if scale:
        i = abs(max(wav))
        j = abs(min(wav))
        m = max(i, j)
        wav = [i / m for i in wav]
    
    if silence_ms is not None:
        wav = silence(silence_ms) + wav + silence(silence_ms)

    return np.ndarray(shape=(len(wav),), buffer=np.array(wav))

def white_noise(milliseconds=1, samples=None):

    if samples is not None:
        res = []
        for i in range(0, samples):
            res.append(random.gauss(0, 1))
        return res
    else:
        res = []
        for i in range(0, int((milliseconds / 1000) * SAMPLE_RATE)):
            res.append(random.gauss(0, 1))

        return res[0:int((milliseconds * SAMPLE_RATE) / 1000)]

def brown_noise(milliseconds):

    res = []
    n = 0.0
    for i in range(0, int((milliseconds / 1000) * SAMPLE_RATE)):
        res.append(n)
        n +=random.gauss(0, 1)

    return res[0:int((milliseconds * SAMPLE_RATE) / 1000)]

def noise(milliseconds=1, samples=None):

    if samples is not None:
        res = []
        for i in range(0, samples):
            res.append((2 * random.random()) - 1)
        return res
    else:
        res = []
        for i in range(0, int((milliseconds / 1000) * SAMPLE_RATE)):
            res.append((2 * random.random()) - 1)

        return res[0:int((milliseconds * SAMPLE_RATE) / 1000)]

def sawtooth(frequency, milliseconds):
    samples_per_cycle = int(SAMPLE_RATE / frequency)
    res = []

    while (len(res) / SAMPLE_RATE) < (milliseconds / 1000):
        for i in range(0, samples_per_cycle):
            res.append(((i * 2) / samples_per_cycle) - 1.0)

    return res

def triangle(frequency, milliseconds):
    samples_per_cycle = int(SAMPLE_RATE / frequency)
    res = []

    while (len(res) / SAMPLE_RATE) < (milliseconds / 1000):
        x = int(samples_per_cycle / 2)

        for i in range(0, x):
            res.append(((i * 2) / x) - 1.0)

        for i in range(0, x):
            res.append(1.0 - ((i * 2) / x))

    return res

def sine_wave(frequency, milliseconds):
    samples_per_cycle = SAMPLE_RATE / frequency
    res = []
    samples_per_cycle = int(samples_per_cycle)

    while (len(res) / SAMPLE_RATE) < (milliseconds / 1000):
        for i in range(0, samples_per_cycle):
            res.append(math.sin((float(i) / float(samples_per_cycle)) * math.pi * 2.0))

    return res

def noisy_sine_wave(frequency, milliseconds, noise_ratio):
    samples_per_cycle = SAMPLE_RATE / frequency
    res = []

    while (len(res) / SAMPLE_RATE) < (milliseconds / 1000):
        for i in range(0, int(samples_per_cycle)):
            res.append(math.sin((float(i) / samples_per_cycle) * math.pi * 2.0))
        samples_per_cycle += (random.random() - 0.5) * noise_ratio

    return res

def frequency_at_time(sf, ef, t, length):
    return sf + ((ef - sf) * (t / length))

def sine_wave_gliss(start_frequency, end_frequency, milliseconds):

    seconds = milliseconds / 1000.0
    num_samples = int(SAMPLE_RATE * seconds)

    res = []
    i = 0
    while True:
        t = (i / num_samples) * seconds
        cycle_time = 1.0 / frequency_at_time(start_frequency, end_frequency, t, seconds)
        a = math.sin((t  / cycle_time) * (2.0 * math.pi))
        res.append(a)

        if i >= num_samples:
            if abs(a) < 1:
                break
        i += 1

    return res

def siren(frequency, shift_hz, cycle_ms, length_ms):
    res = []
    length = 0

    while length < length_ms:
        res += square_wave_gliss(frequency, frequency + shift_hz, cycle_ms)
        length += cycle_ms

    return res

def silence(milliseconds):
    return [0]*int(SAMPLE_RATE * milliseconds / 1000)

def square_wave(frequency, milliseconds):
    samples_per_cycle = int(SAMPLE_RATE / frequency)
    res = []
    while (len(res) / SAMPLE_RATE) < (milliseconds / 1000):
        res += [-1.0]*int(samples_per_cycle/2)
        res += [1.0]*int(samples_per_cycle/2)

    return res

def square_wave_gliss(start_frequency, end_frequency, milliseconds):

    seconds = milliseconds / 1000.0
    num_samples = int(SAMPLE_RATE * seconds)

    res = []
    n = 0

    while n < num_samples:
        t = (n / num_samples) * seconds
        cycle_time = 1.0 / frequency_at_time(start_frequency, end_frequency, t, seconds)
        cycle_samples = int(cycle_time * SAMPLE_RATE)

        for i in range(0, int(cycle_samples / 2)):
            res.append(1)
        for i in range(0, int(cycle_samples / 2)):
            res.append(-1)

        n += cycle_samples

    return res

def mix(*args):
    res = []
    m = -1
    for w in args:
        if m == -1 or len(w) < m:
            m = len(w)

    for i in range(0, m):
        total = 0
        for w in args:
            total += w[i]
        res.append(total / len(args))

    return res

def dim_from_zero(wav):
    res = []
    for i in range(0, len(wav)):
        res.append(wav[i] * (i / len(wav)))
    return res

def dim_to_zero(wav):
    res = []
    for i in range(0, len(wav)):
        res.append(wav[i] * ((len(wav) - i) / len(wav)))
    return res

def dim(wav, percent):
    res = []
    for i in range(int(percent*len(wav)), len(wav)):
        res.append(wav[i] * ((len(wav) - i) / len(wav)))
    return res

def phase_shift(wav, offset):
    two = []
    for i in range(0, len(wav)):
        j = (i + offset) % len(wav)
        two.append(wav[j])
    return mix(wav, two)

def vol(wav, percent, clip=False):
    res = []
    for i in range(0, len(wav)):
        if clip and wav[i] < 0:
            res.append(max(wav[i] * percent, -1))
        elif clip and wav[i] > 0:
            res.append(min(wav[i] * percent, 1))
        else:
            res.append(wav[i] * percent)
    return res

def pew():
    #wav = phase_shift(square_wave_gliss(300, 1400, 150), 10)
    wav = phase_shift(square_wave_gliss(700, 400, 150), 10)
    return shape(wav, 0.01)

def cross_fade(one, two, samples):
    if len(one) < (samples * 2) or len(two) < (samples * 2):
        raise Exception("not enough samples to cross fade %s %s %s" % (len(one), len(two), samples))

    a = one
    for i in range(0, samples):
        a[-i] = a[-i] * (i / samples)

    b = two
    for i in range(0, samples):
        b[i] = b[i] * (i / samples)

    return a + b 

def scale():
    res = []
    for octave in range(3, 6):
        for name, freqs in pitch.items():
            if len(name) == 1:
                if len(res) > 0:
                    res = cross_fade(res, sine_wave(freqs[octave], 100), 100)
                else:
                    res = sine_wave(freqs[octave], 100)
    return res

def sine_harmonic(frequency, milliseconds):

    wav = sine_wave(frequency, milliseconds)

    for harmonic in range(2, 8):
        f = frequency * harmonic
        new = vol(sine_wave(f, milliseconds), 1 / (harmonic * harmonic))
        wav = mix(wav, new)

    return wav

def shape(wav, attack_percent):
    res = []
    cp = int(attack_percent * len(wav))
    for i in range(0, len(wav)):
        if i <= cp:
            res.append(wav[i] * (i/cp))
        else:
            res.append(wav[i] * (1-((i-cp)/(len(wav)-cp))))

    return res

def game_over():
    res = []
    notes = ['C', 'D', 'E', 'F', 'G', 'A', 'B']
    for note in notes:
        for octave in range(3, 6):
            res += square_wave(pitch[note][octave], 50)
    res += square_wave(pitch['C'][6], 100)
    res += shape(mix(square_wave(pitch['C'][2], 800), square_wave(pitch['C'][3], 800)), 0.1)
    return res

def game_start():
    res = []
    notes = ['C', 'E', 'G']
    for octave in range(3, 6):
        for note in notes:
            res += shape(square_wave(pitch[note][octave], 100), 0.1)
    res += shape(square_wave(pitch['C'][6], 100), 0.1)

    notes = ['C', 'B', 'A', 'G', 'F', 'E', 'D', 'C']

    for i in range(0, len(notes)):
        for j in range(0, 3):
            if (i + j) < len(notes):
                res += shape(square_wave(pitch[notes[i + j]][4], 60), 0.1)
            
    return res

def click():
    one = square_wave(400, 10) + silence(50) + square_wave(400, 10) 
    two = square_wave(1200, 10) + silence(50) + square_wave(1200, 10) 
    three = noise(10) + silence(50) + noise(10) 
    return mix(one, two, three)

def menu_open():
    wav = sine_wave_gliss(300, 800, 250)
    return shape(wav, 0.01)

def menu_close():
    wav = sine_wave_gliss(800, 300, 250)
    return shape(wav, 0.01)

def damage_hit():
    return echo(compress(bandpass(noise(100), 100, 800), 1), 100, 0.4, 5)

def pulse(wav, cycle_time_ms):
    res = []
    samples_per_cycle = cycle_time_ms * SAMPLE_RATE / 1000
    n = 0
    for i in range(0, len(wav)):
        res.append(wav[i] * (n / samples_per_cycle))
        n = (n + 1) % samples_per_cycle
    return res

def echo(wav, delay_ms, decay, extend=0.5):
    res = wav + [0] * int(len(wav) * extend)
    delay = int((delay_ms / 1000) * SAMPLE_RATE)
    for i in range(0, len(res) - delay):
        new_value = res[i + delay] + (res[i] * decay)
        res[i + delay] = new_value
    return res

def egg_spawn():
    wav = lowpass(square_wave(400, 100), 200)
    l = int(len(wav) / 2)
    for i in range(0, l):
        wav[i] *= i / l
        wav[-i] *= i / l
    return wav

def egg_hatch():
    wav1 = lowpass(square_wave(1000, 100), 400)
    wav2 = vol(bandpass(white_noise(1000), 400, 500), 0.5)
    wav = mix(wav1, wav2)
    l = int(len(wav) / 2)
    for i in range(0, l):
        wav[i] *= i / l
        wav[-i] *= i / l
    return shape(compress(wav, 1.0), 0.1)

def compress(wav, push):
    res = []
    for y in wav:
        if y != 0 and y != 1 and y != -1:
            if y > 0:
                x = (1 / (1 - y)) - 1
                x += push
                y = 1 - (1 / (x + 1))
            else:
                y *= -1
                x = (1 / (1 - y)) - 1
                x += push
                y = 1 - (1 / (x + 1))
                y *= -1
        res.append(y)
    return res

def swarm_base(freq):
    ms = 10000
    wav = highpass(sawtooth(freq, ms), 600)

    pulse_ms = 100
    step = int(len(wav) * (pulse_ms / ms))

    for x in range(0, len(wav), step):
        l = step / 2
        for i in range(x, x + step):
            scale = (i - x) / l
            scale *= 0.5
            scale += 0.25
            #print(scale)
            wav[i] *= scale
            wav[-i] *= scale
    return wav

def swarm1():
    return swarm_base(50)

def swarm2():
    return compress(swarm_base(50), 0.05)

def swarm3():
    return compress(swarm_base(50), 0.1)

def swarm4():
    return compress(swarm_base(50), 0.2)

def swarm5():
    return compress(swarm_base(50), 0.3)


def egg_grow_medium():
    wav = sine_wave_gliss(400, 500, 200)
    wav = lowpass(wav, 450)
    wav = echo(wav, 200, 0.6, 5)
    return wav

def egg_grow_large():
    wav = sine_wave_gliss(500, 900, 200)
    wav = lowpass(wav, 600)
    wav = echo(wav, 200, 0.6, 5)
    return wav

def alien_kill():
    return echo(compress(bandpass(white_noise(200), 100, 400), 1), 200, 0.4, 5)

def egg_kill():
    return echo(compress(bandpass(white_noise(50), 100, 400), 1), 50, 0.4, 5)

def write_file(fn, wav):
    sf.write('sounds/' + fn + '.wav', convert(wav), SAMPLE_RATE, 'PCM_%s' % BITS)
    sf.write('sounds/' + fn + '.ogg', convert(wav), SAMPLE_RATE)

if True:
    import os
    try:
        os.mkdir('sounds')
    except:
        pass
    
    write_file('pew', pew())
    write_file('game_start', game_start())
    write_file('game_over', game_over())
    write_file('button_click', click())
    write_file('menu_open', menu_open())
    write_file('menu_close', menu_close())
    write_file('damage_hit', damage_hit())
    write_file('egg_spawn', egg_spawn())
    write_file('egg_hatch', egg_hatch())
    write_file('swarm1', swarm1())
    write_file('swarm2', swarm2())
    write_file('swarm3', swarm3())
    write_file('swarm4', swarm4())
    write_file('swarm5', swarm5())
    write_file('egg_grow_medium', egg_grow_medium())
    write_file('egg_grow_large', egg_grow_large())
    write_file('alien_kill', alien_kill())
    write_file('egg_kill', egg_kill())

# button click, egg spawn, egg grow medium, egg grow large, egg hatch, damage hit, alien kill, egg kill, game over, menu open, menu close
# additive layers of swarm sounds
#
