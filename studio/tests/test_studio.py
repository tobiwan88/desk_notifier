import numpy as np
from fastapi.testclient import TestClient
from PIL import Image

from eyes_studio.app import create_app
from eyes_studio.devices import Registry
from mock_device import TOKEN, MockDevice


def gif_bytes(tmp_path):
    frames = []
    for i in range(4):
        a = np.zeros((50, 50, 3), np.uint8)
        a[5 + 8 * i:20 + 8 * i, 10:30] = (255, 0, 0)
        frames.append(Image.fromarray(a))
    p = tmp_path / "t.gif"
    frames[0].save(p, save_all=True, append_images=frames[1:], duration=100)
    return p.read_bytes()


def client(tmp_path, mock):
    reg = Registry(default_token=TOKEN)
    reg.add("eyes-mock", mock.host)
    return TestClient(create_app(reg, tmp_path, mdns=False))


def test_pages_and_devices(tmp_path):
    with MockDevice() as m:
        c = client(tmp_path, m)
        assert "Eyes Studio" in c.get("/").text
        d = c.get("/api/devices").json()
        assert d[0]["id"] == "eyes-mock" and d[0]["status"]["hostname"] == "eyes-mock"


def test_control_proxy(tmp_path):
    with MockDevice() as m:
        c = client(tmp_path, m)
        assert c.post("/api/devices/eyes-mock/eye", json={"mood": "happy"}).status_code == 200
        assert c.post("/api/devices/eyes-mock/play", json={"clip": "a", "loop": True}).status_code == 200
        assert c.post("/api/devices/eyes-mock/stop").status_code == 200
        assert m.eye_cmds == [{"mood": "happy"}] and m.play_cmds[0]["clip"] == "a" and m.stops == 1
        assert c.post("/api/devices/nope/stop").status_code == 404


def test_prepare_send_delete(tmp_path):
    with MockDevice() as m:
        c = client(tmp_path, m)
        r = c.post("/api/prepare", files={"file": ("t.gif", gif_bytes(tmp_path), "image/gif")}, data={"device": "eyes-mock"})
        assert r.status_code == 200, r.text
        p = r.json()
        assert p["frames"] == 4 and p["size"] > 100
        assert c.get(p["preview"]).headers["content-type"] == "image/gif"
        assert c.post("/api/devices/eyes-mock/clips/blob", json={"prepared": p["id"]}).status_code == 200
        assert m.clips["blob"][:4] == b"EYCL"
        assert c.get("/api/devices/eyes-mock/clips").json()["clips"][0]["name"] == "blob"
        assert c.delete("/api/devices/eyes-mock/clips/blob").status_code == 204
        assert c.delete("/api/devices/eyes-mock/clips/blob").status_code == 404


def test_prepare_rejects_garbage(tmp_path):
    with MockDevice() as m:
        c = client(tmp_path, m)
        r = c.post("/api/prepare", files={"file": ("x.gif", b"not an image", "image/gif")})
        assert r.status_code == 422
