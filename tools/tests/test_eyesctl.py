import json

import pytest

from eyes_client import Device, DeviceError
from eyesctl import main
from mock_device import TOKEN, MockDevice


@pytest.fixture
def dev():
    with MockDevice() as m:
        yield m


def test_client_basic(dev):
    d = Device(dev.host, TOKEN)
    assert d.status()["hostname"] == "eyes-mock"
    d.eye(mood="happy", look=[0, 1])
    d.play("wink", loop=True)
    d.stop()
    assert dev.eye_cmds == [{"mood": "happy", "look": [0, 1]}]
    assert dev.play_cmds[0]["clip"] == "wink" and dev.play_cmds[0]["loop"] is True
    assert dev.stops == 1


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
        Device(dev.host, "bad").eye(mood="happy")
    assert e.value.status == 401


def test_cli(dev, tmp_path, capsys):
    base = ["--host", dev.host, "--token", TOKEN]
    assert main(base + ["status"]) == 0
    assert json.loads(capsys.readouterr().out)["version"] == "mock"
    assert main(base + ["eye", '{"mood":"angry"}']) == 0
    clip = tmp_path / "x.clip"
    clip.write_bytes(b"EYCL1234")
    assert main(base + ["upload", str(clip)]) == 0
    assert main(base + ["play", "x", "--loop"]) == 0
    assert main(base + ["delete", "x"]) == 0
    assert main(base + ["delete", "x"]) == 1
    assert main(base + ["smoke"]) == 0
