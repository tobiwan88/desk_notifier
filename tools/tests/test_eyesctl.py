import json

import pytest

from eyes_client import Device, DeviceError
from eyesctl import main
from mock_device import ANIMATIONS, IRISES, SPECIALS, STYLES, TOKEN, MockDevice


@pytest.fixture
def dev():
    with MockDevice() as m:
        yield m


def test_client_basic(dev):
    d = Device(dev.host, TOKEN)
    assert d.status()["hostname"] == "eyes-mock"
    d.eye(play="happy", look=[0, 1])
    d.play("wink", loop=True)
    d.stop()
    assert dev.eye_cmds == [{"play": "happy", "look": [0, 1]}]
    assert dev.play_cmds[0]["clip"] == "wink" and dev.play_cmds[0]["loop"] is True
    assert dev.stops == 1


def test_client_animations(dev):
    a = Device(dev.host, TOKEN).animations()
    assert a["styles"] == STYLES and a["irises"] == IRISES and a["animations"] == ANIMATIONS
    assert a["specials"] == SPECIALS and len(a["animations"]) == 12
    assert a["state"] == {"style": "realistic", "iris": "amber", "auto": False, "anim": None}


def test_client_eye_state_and_status(dev):
    d = Device(dev.host, TOKEN)
    d.eye(style="anime", iris="odd", auto=True, play="roll")
    assert d.status()["eye"] == {"style": "anime", "iris": "odd", "auto": True, "anim": "roll"}
    assert d.animations()["state"]["anim"] == "roll"


def test_client_rejects_bad_names(dev):
    d = Device(dev.host, TOKEN)
    with pytest.raises(DeviceError) as e:
        d.eye(play="nope")
    assert e.value.status == 400 and e.value.code == "E_BAD_JSON"
    with pytest.raises(DeviceError):
        d.eye(style="robo")
    with pytest.raises(DeviceError):
        d.eye(mood="happy")  # old API is gone
    assert dev.eye_cmds == []


def test_upload_list_delete(dev):
    d = Device(dev.host, TOKEN)
    d.upload("a", b"EYCL" + b"\0" * 10)
    assert d.clips()["clips"][0]["name"] == "a"
    d.delete("a")
    assert d.clips()["clips"] == []
    with pytest.raises(DeviceError) as e:
        d.upload("bad", b"nope")
    assert e.value.code == "E_BAD_CLIP"
    with pytest.raises(DeviceError) as e:
        d.delete("missing")
    assert e.value.status == 404


def test_wrong_token(dev):
    with pytest.raises(DeviceError) as e:
        Device(dev.host, "bad").eye(play="happy")
    assert e.value.status == 401


def test_cli(dev, tmp_path, capsys):
    base = ["--host", dev.host, "--token", TOKEN]
    assert main(base + ["status"]) == 0
    assert json.loads(capsys.readouterr().out)["version"] == "mock"
    assert main(base + ["eye", '{"play":"startle","look":[0,0.5,800]}']) == 0
    assert dev.eye_cmds[-1] == {"play": "startle", "look": [0, 0.5, 800]}
    clip = tmp_path / "x.clip"
    clip.write_bytes(b"EYCL1234")
    assert main(base + ["upload", str(clip)]) == 0
    assert main(base + ["play", "x", "--loop"]) == 0
    assert main(base + ["delete", "x"]) == 0
    assert main(base + ["delete", "x"]) == 1
    assert main(base + ["smoke"]) == 0


def test_cli_anim_and_style(dev, capsys):
    base = ["--host", dev.host, "--token", TOKEN]
    assert main(base + ["anim", "wink"]) == 0
    assert dev.eye_cmds[-1] == {"play": "wink"}
    assert main(base + ["anim", "sparkle"]) == 0
    assert main(base + ["style", "anime"]) == 0
    assert dev.eye_cmds[-1] == {"style": "anime"}
    assert main(base + ["style", "cute", "--iris", "violet", "--save"]) == 0
    assert dev.eye_cmds[-1] == {"style": "cute", "iris": "violet", "save": True}
    assert dev.eye_state["style"] == "cute" and dev.eye_state["iris"] == "violet"
    assert main(base + ["style", "robo"]) == 1  # rejected by the device
    assert main(base + ["anim", "nope"]) == 1
    assert main(base + ["style", "flat", "--iris", "pink"]) == 1


def test_cli_smoke_sequence(dev):
    assert main(["--host", dev.host, "--token", TOKEN, "smoke"]) == 0
    assert dev.eye_cmds == [
        {"style": "realistic"},
        {"play": "wink"}, {"play": "glance"}, {"play": "happy"},
        {"play": "random"},
        {"auto": True},
    ]
    assert dev.eye_state["auto"] is True and dev.eye_state["anim"] == "random"


def test_cli_soak_short(dev, tmp_path, capsys):
    base = ["--host", dev.host, "--token", TOKEN]
    csv_path = tmp_path / "soak.csv"
    # 0.001 h is about 3.6 s; the CLI sleeps 0.2-2 s per loop, so this runs a few requests.
    rc = main(base + ["soak", "--hours", "0.001", "--csv", str(csv_path)])
    assert rc in (0, 1)
    assert "soak done" in capsys.readouterr().out
    assert csv_path.read_text().startswith("t,uptime_s")
    assert dev.eye_cmds and all(set(c) <= {"look", "play"} for c in dev.eye_cmds)
    assert all(c.get("play") in ANIMATIONS + ["random"] for c in dev.eye_cmds if "play" in c)
    assert all(len(c["look"]) == 2 for c in dev.eye_cmds if "look" in c)
