"""Runs the FL Studio bridge script against fake FL modules and checks the SysEx it sends.

Usage: python3 tests/test_fl_script.py
"""
import importlib
import json
import os
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPT_DIR = os.path.join(HERE, '..', 'FLStudio', 'Production Pal')


def make_fake_fl(fail_everything=False):
    sent = []

    def boom(*_a, **_k):
        raise RuntimeError('not in this FL version')

    names = ['Master', 'Kick', '808', 'Vox Lead é', 'Insert 4']
    peaks = [0.9, 0.7, 0.95, 0.4, 0.0]

    device = types.ModuleType('device')
    device.midiOutSysex = lambda msg: sent.append(msg)

    mixer = types.ModuleType('mixer')
    mixer.getCurrentTempo = boom if fail_everything else (lambda: 140000)
    mixer.trackCount = lambda: 5
    mixer.getTrackName = boom if fail_everything else (lambda i: names[i])
    mixer.getTrackVolume = boom if fail_everything else (lambda i, mode=0: -3.5 if mode == 1 else 0.8)
    mixer.getTrackPan = lambda i: 0.0
    mixer.isTrackMuted = lambda i: False
    mixer.getTrackPeaks = boom if fail_everything else (lambda i, mode: peaks[i])
    mixer.trackNumber = lambda: 3

    transport = types.ModuleType('transport')
    transport.isPlaying = lambda: True
    transport.isRecording = boom

    plugins = types.ModuleType('plugins')
    plugins.isValid = lambda i, s: s < 2
    plugins.getPluginName = lambda i, s: ['Fruity Balance', 'Pitcher'][s]

    ui = types.ModuleType('ui')
    ui.getFocusedFormCaption = lambda: 'Mixer'

    channels = types.ModuleType('channels')
    channels.selectedChannel = lambda: 0
    channels.getChannelName = boom

    for m in (device, mixer, transport, plugins, ui, channels):
        sys.modules[m.__name__] = m
    return sent


def load_script():
    sys.path.insert(0, SCRIPT_DIR)
    if 'device_ProductionPal' in sys.modules:
        del sys.modules['device_ProductionPal']
    return importlib.import_module('device_ProductionPal')


def decode(msg):
    assert msg[:4] == bytes([0xF0, 0x7D, 0x50, 0x50]), 'bad SysEx header'
    assert msg[-1] == 0xF7, 'missing SysEx end'
    assert all(b < 0x80 for b in msg[1:-1]), 'SysEx data must be 7-bit'
    return json.loads(msg[4:-1].decode('ascii'))


def main():
    sent = make_fake_fl()
    script = load_script()
    script.OnInit()
    script.OnIdle()
    state = decode(sent[-1])
    assert state['type'] == 'fl' and state['bpm'] == 140.0
    assert state['playing'] is True and state['recording'] is False
    assert state['master']['fx'] == ['Fruity Balance', 'Pitcher']
    assert state['selected_track']['name'] == 'Vox Lead é'
    assert [t['name'] for t in state['tracks']][:2] == ['808', 'Kick'], 'busiest tracks first'
    assert all(t['name'] != 'Insert 4' for t in state['tracks']), 'silent default inserts skipped'
    assert len(sent[-1]) < 2048, 'message stays small'

    # Throttling: an immediate second idle call sends nothing.
    n = len(sent)
    script.OnIdle()
    assert len(sent) == n, 'sends at most once a second'

    # A FL version missing most functions still produces a valid message.
    sent = make_fake_fl(fail_everything=True)
    script = load_script()
    script.OnIdle()
    state = decode(sent[-1])
    assert state['bpm'] is None and state['type'] == 'fl'

    print('FL SCRIPT TESTS PASSED')


if __name__ == '__main__':
    main()
