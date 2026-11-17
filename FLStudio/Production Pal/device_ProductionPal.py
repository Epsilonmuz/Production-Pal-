# name=Production Pal
# url=https://github.com/Epsilonmuz/Production-Pal-
#
# Production Pal bridge for FL Studio (MIDI scripting API).
# About once a second it sends FL Studio's state (tempo, transport, focused
# window, selected mixer track with its effect chain, and the busiest mixer
# tracks) to the Production Pal app as a SysEx message on the "Production Pal"
# virtual MIDI port. It never changes your project.
#
# Setup: see FLStudio/README.md in the repository.
#
# Every FL API call is wrapped so a missing function in your FL version only
# drops that one field instead of stopping the script.

import device
import mixer
import transport

try:
    import plugins
except ImportError:
    plugins = None

try:
    import ui
except ImportError:
    ui = None

try:
    import channels
except ImportError:
    channels = None

try:
    import time
    _now = time.time
except Exception:
    _now = None

SEND_INTERVAL_S = 1.0
IDLE_TICKS_FALLBACK = 40          # used if the time module is unavailable
MAX_TRACKS = 14                   # busiest named tracks to report
SYSEX_HEADER = bytes([0xF0, 0x7D, 0x50, 0x50])

_last_send = 0.0
_ticks = 0


def _safe(fn, *args, default=None):
    try:
        return fn(*args)
    except Exception:
        return default


def _json_str(s):
    """JSON string literal using only 7-bit ASCII (SysEx bytes must be < 0x80)."""
    out = ['"']
    for ch in str(s):
        code = ord(ch)
        if ch == '"':
            out.append('\\"')
        elif ch == '\\':
            out.append('\\\\')
        elif code > 0xFFFF:
            out.append('?')
        elif code < 0x20 or code > 0x7E:
            out.append('\\u%04x' % code)
        else:
            out.append(ch)
    out.append('"')
    return ''.join(out)


def _json(v):
    """Tiny JSON encoder (FL's embedded Python may not ship the json module)."""
    if v is None:
        return 'null'
    if v is True:
        return 'true'
    if v is False:
        return 'false'
    if isinstance(v, (int,)):
        return str(v)
    if isinstance(v, float):
        return '%.3f' % v
    if isinstance(v, (list, tuple)):
        return '[' + ','.join(_json(x) for x in v) + ']'
    if isinstance(v, dict):
        return '{' + ','.join(_json_str(k) + ':' + _json(x) for k, x in v.items()) + '}'
    return _json_str(v)


def _tempo():
    t = _safe(mixer.getCurrentTempo)
    if t is None:
        return None
    t = float(t)
    if t > 1000:          # some versions report BPM * 1000
        t = t / 1000.0
    return round(t, 2)


def _volume_db(index):
    # mode 1 asks FL for decibels where supported; otherwise send the 0..1 fader value.
    db = _safe(mixer.getTrackVolume, index, 1)
    if isinstance(db, (int, float)) and db <= 6.0:
        return round(float(db), 1)
    return None


def _peak(index):
    p = _safe(mixer.getTrackPeaks, index, 2)          # 2 = max of left/right
    if p is None:
        l = _safe(mixer.getTrackPeaks, index, 0, default=0.0) or 0.0
        r = _safe(mixer.getTrackPeaks, index, 1, default=0.0) or 0.0
        p = max(l, r)
    return round(float(p), 3) if p is not None else None


def _slots(index):
    names = []
    if plugins is None:
        return names
    for slot in range(10):
        if _safe(plugins.isValid, index, slot, default=False):
            names.append(_safe(plugins.getPluginName, index, slot, default='?'))
        else:
            names.append(None)
    while names and names[-1] is None:
        names.pop()
    return names


def _track_info(index, with_slots=False):
    info = {
        'i': index,
        'name': _safe(mixer.getTrackName, index, default=''),
        'fader': round(float(_safe(mixer.getTrackVolume, index, default=0.0) or 0.0), 3),
        'vol_db': _volume_db(index),
        'pan': round(float(_safe(mixer.getTrackPan, index, default=0.0) or 0.0), 2),
        'muted': bool(_safe(mixer.isTrackMuted, index, default=False)),
        'peak': _peak(index),
    }
    if with_slots:
        info['fx'] = _slots(index)
    return info


def _collect():
    state = {'type': 'fl', 'v': 1}
    state['bpm'] = _tempo()
    state['playing'] = bool(_safe(transport.isPlaying, default=False))
    state['recording'] = bool(_safe(transport.isRecording, default=False))
    if ui is not None:
        state['focused_window'] = _safe(ui.getFocusedFormCaption, default=None)
    if channels is not None:
        sel = _safe(channels.selectedChannel, default=None)
        if sel is not None and sel >= 0:
            state['selected_channel'] = _safe(channels.getChannelName, sel, default=None)

    count = _safe(mixer.trackCount, default=0) or 0
    state['master'] = _track_info(0, with_slots=True)

    selected = _safe(mixer.trackNumber, default=None)
    if selected is not None and 0 < selected < count:
        state['selected_track'] = _track_info(selected, with_slots=True)

    # Busiest named inserts (skip the Master and the empty "Insert N" tracks).
    tracks = []
    for i in range(1, count):
        name = _safe(mixer.getTrackName, i, default='') or ''
        peak = _peak(i) or 0.0
        if name.startswith('Insert ') and peak <= 0.0:
            continue
        tracks.append((peak, i))
    tracks.sort(reverse=True)
    state['tracks'] = [_track_info(i) for _, i in tracks[:MAX_TRACKS]]
    return state


def _send(state):
    payload = _json(state).encode('ascii', 'replace')
    msg = SYSEX_HEADER + payload + bytes([0xF7])
    _safe(device.midiOutSysex, msg)


def OnInit():
    print('Production Pal: bridge started. Pick the "Production Pal" output on the same port number.')


def OnDeInit():
    print('Production Pal: bridge stopped.')


def OnIdle():
    global _last_send, _ticks
    if _now is not None:
        t = _now()
        if t - _last_send < SEND_INTERVAL_S:
            return
        _last_send = t
    else:
        _ticks += 1
        if _ticks < IDLE_TICKS_FALLBACK:
            return
        _ticks = 0

    _send(_collect())


def OnMidiMsg(event):
    # The script only reports; incoming notes/CC pass through to FL as usual.
    event.handled = False
