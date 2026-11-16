import base64
import hashlib
import hmac
import unittest
from aiohttp.test_utils import TestClient, TestServer
from server import RoomService, create_app


class RoomsTest(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.now = 1000.0
        self.service = RoomService("test-secret", "example.invalid:3478", clock=lambda: self.now)
        self.service.games["test-six"] = {"versions": ["1"], "maxPlayers": 6}
        self.client = TestClient(TestServer(create_app(self.service)))
        await self.client.start_server()

    async def asyncTearDown(self):
        await self.client.close()

    async def post(self, action, data=None, token=None, status=200):
        headers = {"Authorization": "Bearer " + token} if token else {}
        response = await self.client.post("/online/v1/" + action, json=data or {}, headers=headers)
        body = await response.json()
        self.assertEqual(response.status, status, body)
        return body

    async def create(self, game="epoch-lego-racers", version="prototype-2", capacity=2):
        return await self.post("create", dict(game=game, version=version, name="Host", capacity=capacity))

    async def join(self, host, name="Guest", **kwargs):
        room = host["room"]
        return await self.post("join", dict(game=room["game"], version=room["version"],
                                            name=name, code=room["code"]), **kwargs)

    async def test_chat_is_authenticated_bounded_and_room_scoped(self):
        host = await self.create()
        guest = await self.join(host)
        other = await self.create()
        await self.post("chat", {"text": "hello"}, status=401)
        await self.post("chat", {"text": "hello", "name": "Imposter"}, guest["token"])
        room = (await self.post("poll", token=host["token"]))["room"]
        self.assertEqual(room["chat"], [{"sequence": 1, "sender": guest["id"], "name": "Guest", "text": "hello"}])
        self.assertEqual((await self.post("poll", token=other["token"]))["room"]["chat"], [])
        for invalid in ["", "   ", "x" * 321, "x\ny", 42]:
            await self.post("chat", {"text": invalid}, guest["token"], status=400)
        for i in range(7):
            await self.post("chat", {"text": str(i)}, guest["token"])
        await self.post("chat", {"text": "flood"}, guest["token"], status=429)
        for i in range(70):
            self.now += 11
            await self.post("poll", token=host["token"])
            await self.post("chat", {"text": str(i)}, guest["token"])
        history = (await self.post("poll", token=host["token"]))["room"]["chat"]
        self.assertEqual(len(history), 64)
        self.assertEqual(history[-1]["sequence"], 78)

    async def test_six_player_lobby_host_start_and_loading_barrier(self):
        host = await self.create(capacity=6)
        peers = [host] + [await self.join(host, "P" + str(i)) for i in range(5)]
        waiter = await self.join(host)
        self.assertTrue(waiter["room"]["members"][-1]["waiting"])
        await self.post("start", token=peers[1]["token"], status=403)
        await self.post("start", token=host["token"], status=409)
        for peer in peers:
            await self.post("ready", {"ready": True}, peer["token"])
        state = await self.post("start", token=host["token"])
        self.assertEqual(state["room"]["phase"], "loading")
        late = await self.join(host)
        self.assertTrue(late["room"]["members"][-1]["waiting"])
        for peer in peers[:-1]:
            state = await self.post("loaded", token=peer["token"])
            self.assertEqual(state["room"]["phase"], "loading")
        state = await self.post("loaded", token=peers[-1]["token"])
        self.assertEqual(state["room"]["phase"], "in_progress")
        self.assertEqual(state["room"]["startAt"], 1003000)
        again = await self.post("loaded", token=peers[-1]["token"])
        self.assertEqual(again["room"]["startAt"], state["room"]["startAt"])

    async def test_game_and_version_isolation(self):
        host = await self.create()
        await self.post("join", dict(game="test-six", version="1", name="Other", code=host["room"]["code"]), status=404)
        await self.create("test-six", "1", 2)
        await self.post("join", dict(game="epoch-lego-racers", version="old", name="Old", code=host["room"]["code"]), status=409)
        await self.post("create", dict(game="epoch-lego-racers", version="prototype-2", name="Host", capacity=7), status=400)

    async def test_signals_only_to_current_room_members(self):
        host = await self.create()
        guest = await self.join(host)
        other = await self.create()
        signal = base64.b64encode(b"rendezvous").decode()
        await self.post("signal", {"to": other["id"], "data": signal}, host["token"], status=400)
        await self.post("signal", {"to": guest["id"], "data": signal}, host["token"])
        poll = await self.post("poll", token=guest["token"])
        self.assertEqual(poll["signals"], [{"from": host["id"], "data": signal}])
        self.assertEqual((await self.post("poll", token=guest["token"]))["signals"], [])
        await self.post("signal", {"to": guest["id"], "data": "%%%"}, host["token"], status=400)
        await self.post("poll", token="guessed-token", status=401)

    async def test_ephemeral_turn_credentials_and_revocation(self):
        host = await self.create()
        turn = host["ice"]["turn"][0]
        expected = base64.b64encode(hmac.new(b"test-secret", turn["username"].encode(), hashlib.sha1).digest()).decode()
        self.assertEqual(turn["password"], expected)
        self.assertEqual(turn["expires"], 1900)
        await self.post("leave", token=host["token"])
        await self.post("ice", token=host["token"], status=401)

    async def test_timeout_and_host_leave_close_rooms(self):
        host = await self.create()
        guest = await self.join(host)
        self.now += 61
        await self.post("poll", token=guest["token"], status=401)
        self.assertFalse(self.service.rooms)
        host = await self.create()
        guest = await self.join(host)
        await self.post("leave", token=host["token"])
        await self.post("poll", token=guest["token"], status=401)

    async def test_admission_limits_and_payload_bounds(self):
        for _ in range(12):
            await self.post("join", dict(game="epoch-lego-racers", version="prototype-2", name="Guest", code="NOTAROOM12"), status=404)
        await self.post("create", {}, status=429)
        self.now += 61
        host = await self.create()
        await self.post("ready", {"ready": "true"}, host["token"], status=409)
        response = await self.client.post("/online/v1/create", data="x" * 13000)
        self.assertEqual(response.status, 413)

    async def test_public_browser_waiting_player_and_next_race(self):
        private = await self.create()
        host = await self.post("create", dict(game="epoch-lego-racers", version="prototype-2", name="Host", public=True, roomName="Friends racing"))
        listing = await self.post("list")
        self.assertEqual([r["code"] for r in listing["rooms"]], [host["room"]["code"]])
        self.assertNotIn("token", str(listing))
        self.assertEqual((await self.post("list", {"game": "test-six"}))["rooms"], [])
        guest = await self.join(host)
        for peer in (host, guest):
            await self.post("ready", {"ready": True}, peer["token"])
        await self.post("start", token=host["token"])
        for peer in (host, guest):
            await self.post("loaded", token=peer["token"])
        waiter = await self.join(host, "Next racer")
        await self.post("ice", token=waiter["token"], status=403)
        await self.post("loaded", token=waiter["token"], status=409)
        await self.post("next", token=guest["token"], status=403)
        await self.post("leave", token=guest["token"])
        state = await self.post("next", token=host["token"])
        self.assertEqual(state["room"]["phase"], "lobby")
        self.assertTrue(all(not m["waiting"] and not m["ready"] for m in state["room"]["members"]))
        self.assertTrue((await self.post("ice", token=waiter["token"]))["turn"])

    async def test_all_finished_returns_to_lobby_and_resets_barrier(self):
        host = await self.create(capacity=6)
        peers = [host] + [await self.join(host, "P" + str(i)) for i in range(5)]
        for peer in peers:
            await self.post("ready", {"ready": True}, peer["token"])
        await self.post("start", token=host["token"])
        for peer in peers:
            await self.post("loaded", token=peer["token"])
        waiter = await self.join(host)
        await self.post("finished", token=waiter["token"], status=409)
        for peer in peers[:-1]:
            result = await self.post("finished", token=peer["token"])
            self.assertEqual(result["room"]["phase"], "in_progress")
        result = await self.post("finished", token=peers[-1]["token"])
        self.assertEqual(result["room"]["phase"], "lobby")
        self.assertEqual(result["room"]["startAt"], 0)
        self.assertTrue(all(not m["ready"] and not m["loaded"] and not m["finished"] for m in result["room"]["members"]))
        await self.post("leave", token=peers[-1]["token"])
        result = await self.post("poll", token=waiter["token"])
        self.assertFalse(next(m for m in result["room"]["members"] if m["id"] == waiter["id"])["waiting"])


if __name__ == "__main__":
    unittest.main()
