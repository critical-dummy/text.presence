# name=TPC Presence Bridge

import os
import time

import general
import patterns
import transport


REPORT_DIR = os.path.join(
    os.environ.get("LOCALAPPDATA", os.path.expanduser("~")),
    "TPC",
)
REPORT_PATH = os.path.join(REPORT_DIR, "fl_studio.report")
TEMP_PATH = REPORT_PATH + ".tmp"

MIN_EMIT_INTERVAL = 0.20

_last_payload = None
_last_emit_time = 0.0


def _escape(value):
    value = str(value)
    return (
        value
        .replace("\\", "\\\\")
        .replace("\t", "\\t")
        .replace("\r", "\\r")
        .replace("\n", "\\n")
    )


def _value_line(key, value):
    return "{}={}".format(key, _escape(value))


def _build_payload():
    process_id = os.getpid()

    title = general.getProjectTitle()
    author = general.getProjectAuthor()
    genre = general.getProjectGenre()

    tempo = general.getCurrentTempo(0)
    playing = transport.isPlaying()
    recording = transport.isRecording()

    song_pos = transport.getSongPos()
    song_pos_hint = transport.getSongPosHint()
    song_length = transport.getSongLength(1)

    pattern_number = patterns.patternNumber
    pattern_name = patterns.getPatternName(pattern_number)

    progress = float(song_pos)

    lines = [
        "TPC1",
        _value_line("process_id", process_id),
        _value_line("project_title", title),
        _value_line("project_author", author),
        _value_line("project_genre", genre),
        _value_line("tempo", tempo),
        _value_line("playing", 1 if playing else 0),
        _value_line("recording", 1 if recording else 0),
        _value_line("song_pos", song_pos),
        _value_line("song_pos_hint", song_pos_hint),
        _value_line("song_length", song_length),
        _value_line("progress", progress),
        _value_line("pattern_number", pattern_number),
        _value_line("pattern_name", pattern_name),
    ]

    return "\n".join(lines) + "\n"


def _publish(force=False):
    global _last_payload
    global _last_emit_time

    now = time.monotonic()
    payload = _build_payload()

    if not force and payload == _last_payload:
        return

    if not force and now - _last_emit_time < MIN_EMIT_INTERVAL:
        return

    os.makedirs(REPORT_DIR, exist_ok=True)

    with open(TEMP_PATH, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(payload)
        handle.flush()
        os.fsync(handle.fileno())

    os.replace(TEMP_PATH, REPORT_PATH)

    _last_payload = payload
    _last_emit_time = now


def _remove_report():
    try:
        os.remove(REPORT_PATH)
    except FileNotFoundError:
        pass
    except OSError:
        pass


def OnInit():
    _publish(force=True)


def OnDeInit():
    _remove_report()


def OnIdle():
    _publish()


def OnProjectLoad(status):
    _publish(force=True)


def OnRefresh(flags):
    _publish()


def OnUpdateBeatIndicator(value):
    _publish()
