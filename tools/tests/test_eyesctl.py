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
    assert a["specials"] == SPECIALS and len(a["animations"]) == len(ANIMATIONS) == 31
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


def gif_file(tmp_path, name="Blink Test.gif"):
    import numpy as np
    from PIL import Image

    frames = []
    for i in range(4):
        a = np.zeros((50, 50, 3), np.uint8)
        a[5 + 8 * i:20 + 8 * i, 10:30] = (255, 0, 0)
        frames.append(Image.fromarray(a))
    p = tmp_path / name
    frames[0].save(p, save_all=True, append_images=frames[1:], duration=100)
    return p


def test_mock_pool_defaults_and_rules(dev):
    d = Device(dev.host, TOKEN)
    p = d.pool()
    assert [i["name"] for i in p["items"]] == ANIMATIONS
    assert all(i["kind"] == "anim" and i["enabled"] for i in p["items"])
    assert p["interval"] == [2.5, 7.0] and p["auto"] is True
    d.upload("fire", b"EYCL" + b"\0" * 8)
    fire = d.pool()["items"][-1]
    assert fire == {"name": "fire", "kind": "clip", "enabled": False}
    with pytest.raises(DeviceError) as e:
        d.set_pool(enabled=["wink", "x"])
    assert e.value.status == 400 and e.value.message == "unknown item: x"
    assert dev.pool_enabled == set(ANIMATIONS)  # rejected POST changes nothing
    for bad in ([0.5, 3], [5, 3], [1, 121]):
        with pytest.raises(DeviceError):
            d.set_pool(interval=bad)
    assert dev.pool_interval == [2.5, 7.0]
    p = d.set_pool(enabled=["fire", "wink"], interval=[1, 4])
    assert sorted(i["name"] for i in p["items"] if i["enabled"]) == ["fire", "wink"]
    assert p["interval"] == [1, 4]
    d.delete("fire")
    assert "fire" not in [i["name"] for i in d.pool()["items"]]
    assert dev.pool_enabled == {"wink"}


def test_client_pool_partial_updates(dev):
    d = Device(dev.host, TOKEN)
    d.set_pool(enabled=["happy"])
    assert dev.pool_enabled == {"happy"} and dev.pool_interval == [2.5, 7.0]
    d.set_pool(interval=[3, 6])
    assert dev.pool_enabled == {"happy"} and dev.pool_interval == [3, 6]


def test_cli_pool_table(dev, capsys):
    base = ["--host", dev.host, "--token", TOKEN]
    assert main(base + ["pool"]) == 0
    out = capsys.readouterr().out.splitlines()
    assert out[0].startswith("[x] wink") and "(anim)" in out[0]
    assert out[-1] == "interval 2.5-7 s, auto on"


def test_cli_pool_edit_commands(dev, capsys):
    base = ["--host", dev.host, "--token", TOKEN]
    assert main(base + ["pool", "set", "wink", "happy", "--interval", "1", "5"]) == 0
    assert dev.pool_enabled == {"wink", "happy"} and dev.pool_interval == [1, 5]
    assert "[x] happy" in capsys.readouterr().out
    assert main(base + ["pool", "add", "glance"]) == 0
    assert dev.pool_enabled == {"wink", "happy", "glance"}
    assert main(base + ["pool", "remove", "wink", "happy"]) == 0
    assert dev.pool_enabled == {"glance"}
    assert main(base + ["pool", "all"]) == 0
    assert dev.pool_enabled == set(ANIMATIONS)
    assert main(base + ["pool", "none"]) == 0
    assert dev.pool_enabled == set()
    assert main(base + ["pool", "set", "wink", "--interval", "9", "3"]) == 1  # min > max
    assert dev.pool_enabled == set() and dev.pool_interval == [1, 5]  # rejected call changes nothing


def test_cli_pool_set_rejects_unknown(dev, capsys):
    base = ["--host", dev.host, "--token", TOKEN]
    assert main(base + ["pool", "set", "wink", "nope"]) == 1
    assert "unknown item: nope" in capsys.readouterr().err
    assert main(base + ["pool", "add", "ghost"]) == 1
    assert dev.pool_enabled == set(ANIMATIONS)


def test_cli_add_uploads_clip(dev, tmp_path, capsys):
    base = ["--host", dev.host, "--token", TOKEN]
    src = gif_file(tmp_path)
    assert main(base + ["add", str(src), "--pool"]) == 0
    assert "blink_test" in dev.clips
    assert dev.clips["blink_test"][:4] == b"EYCL"
    assert "blink_test" in dev.pool_enabled
    out = capsys.readouterr().out
    assert "uploaded blink_test" in out and "4 frames" in out
    assert main(base + ["add", str(src), "--name", "my_eye", "--max-frames", "2"]) == 0
    assert dev.clips["my_eye"][:4] == b"EYCL"
    assert "my_eye" not in dev.pool_enabled
    assert main(base + ["add", str(src), "--name", "Bad Name"]) == 1
    assert main(base + ["add", str(src), "--region", "1,2,3"]) == 1
    assert main(base + ["add", str(tmp_path / "missing.gif")]) == 1
    assert "blink_test" in dev.clips and "Bad Name" not in dev.clips
