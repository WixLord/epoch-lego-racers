"""Epoch room/signalling service. Gameplay travels over ICE, never this HTTP API.

Run behind the existing HTTPS reverse proxy; listen only on loopback. Rooms are
ephemeral and game/version-scoped. No accounts or user files. Public rooms are opt-in.
"""
import asyncio
import base64
import hashlib
import hmac
import json
import os
import secrets
import time
from collections import deque
from dataclasses import dataclass, field

from aiohttp import web


@dataclass
class Member:
    id: str
    name: str
    token: str
    seen: float
    ready: bool = False
    loaded: bool = False
    waiting: bool = False
    finished: bool = False
    signals: deque = field(default_factory=lambda: deque(maxlen=128))


@dataclass
class Room:
    code: str
    game: str
    version: str
    capacity: int
    host: str
    created: float
    members: dict = field(default_factory=dict)
    phase: str = "lobby"
    start_at: int = 0
    public: bool = False
    name: str = "Epoch race"
    chat: deque = field(default_factory=lambda: deque(maxlen=64))
    chat_sequence: int = 0


class RoomService:
    def __init__(self, turn_secret="", turn_address="", max_rooms=8, clock=time.time):
        self.rooms = {}
        self.tokens = {}
        self.limits = {}
        self.clock = clock
        self.turn_secret = turn_secret
        self.turn_address = turn_address
        self.max_rooms = max_rooms
        # Game adapters declare what they currently support. The lobby protocol
        # supports up to six active participants per session.
        self.games = {"epoch-lego-racers": {"versions": ["prototype-2"], "maxPlayers": 6}}

    def expire(self):
        now = self.clock()
        for room in list(self.rooms.values()):
            if now - room.created > 7200 or now - room.members[room.host].seen > 60:
                # Host migration is intentionally outside the first prototype.
                self.close(room)
                continue
            for member in list(room.members.values()):
                if now - member.seen > 60:
                    del room.members[member.id]
                    self.tokens.pop(member.token, None)
                    if room.phase == "lobby":
                        self.fill_slots(room)
        for key, (_, until) in list(self.limits.items()):
            if until <= now:
                del self.limits[key]

    def close(self, room):
        self.rooms.pop(room.code, None)
        for member in room.members.values():
            self.tokens.pop(member.token, None)

    @staticmethod
    def fill_slots(room):
        active = sum(not m.waiting for m in room.members.values())
        for member in room.members.values():
            if member.waiting and active < room.capacity:
                member.waiting = False
                active += 1
            member.ready = member.loaded = member.finished = False
            member.signals.clear()

    def limit(self, key, count, seconds):
        now = self.clock()
        used, until = self.limits.get(key, (0, now + seconds))
        if now >= until:
            used, until = 0, now + seconds
        if used >= count or (key not in self.limits and len(self.limits) >= 4096):
            raise web.HTTPTooManyRequests(text="Request limit reached")
        self.limits[key] = (used + 1, until)

    def authenticate(self, request):
        token = request.headers.get("Authorization", "").removeprefix("Bearer ")
        entry = self.tokens.get(token)
        if not entry:
            raise web.HTTPUnauthorized(text="Session expired or invalid")
        room, member = entry
        self.limit(("member", member.id), 120, 10)
        member.seen = self.clock()
        return room, member

    @staticmethod
    def text(data, key, maximum):
        value = data.get(key)
        if not isinstance(value, str) or not 1 <= len(value) <= maximum or any(
            ord(c) < 32 or ord(c) == 127 for c in value
        ):
            raise web.HTTPBadRequest(text="Invalid " + key)
        return value

    def member(self, data):
        return Member(secrets.token_hex(12), self.text(data, "name", 24),
                      secrets.token_urlsafe(32), self.clock())

    def snapshot(self, room):
        return {
            "code": room.code, "game": room.game, "version": room.version,
            "capacity": room.capacity, "host": room.host, "phase": room.phase,
            "serverTime": int(self.clock() * 1000), "startAt": room.start_at,
            "members": [{"id": m.id, "name": m.name, "ready": m.ready,
                         "loaded": m.loaded, "waiting": m.waiting, "finished": m.finished} for m in room.members.values()],
            "public": room.public, "name": room.name, "chat": list(room.chat),
        }

    def credentials(self, member):
        if not self.turn_secret or not self.turn_address:
            return {"stun": [], "turn": []}
        expiry = int(self.clock()) + 900
        username = str(expiry) + ":" + member.id
        password = base64.b64encode(hmac.new(
            self.turn_secret.encode(), username.encode(), hashlib.sha1
        ).digest()).decode()
        return {"stun": [self.turn_address], "turn": [{
            "address": self.turn_address, "username": username,
            "password": password, "expires": expiry,
        }]}

    async def handle(self, request):
        self.expire()
        action = request.match_info["action"]
        if action == "health" and request.method == "GET":
            return web.json_response({"service": "epoch-online", "protocol": 1})
        if request.method != "POST":
            raise web.HTTPMethodNotAllowed(request.method, ["POST"])
        try:
            data = await request.json()
        except (ValueError, UnicodeError):
            raise web.HTTPBadRequest(text="Invalid JSON")
        if not isinstance(data, dict):
            raise web.HTTPBadRequest(text="Expected object")
        if action == "list":
            ip = request.headers.get("X-Epoch-Client-IP", request.remote or "unknown")
            self.limit(("browse", ip), 30, 60)
            game = data.get("game")
            if game is not None and not isinstance(game, str):
                raise web.HTTPBadRequest(text="Invalid game filter")
            return web.json_response({"rooms": [{
                "code": r.code, "name": r.name, "game": r.game, "version": r.version,
                "phase": r.phase, "capacity": r.capacity,
                "players": sum(not m.waiting for m in r.members.values()),
                "waiting": sum(m.waiting for m in r.members.values()),
            } for r in self.rooms.values() if r.public and (game is None or game == r.game)]})
        if action in ("create", "join"):
            # Only trust this proxy header because production listens on loopback
            # and Caddy overwrites it with the actual remote address.
            ip = request.headers.get("X-Epoch-Client-IP", request.remote or "unknown")
            self.limit(("admission", ip), 12, 60)
            game = self.text(data, "game", 64)
            version = self.text(data, "version", 64)
            policy = self.games.get(game)
            if not policy or version not in policy["versions"]:
                raise web.HTTPConflict(text="Unsupported game or network version")
            member = self.member(data)
            if action == "create":
                if len(self.rooms) >= self.max_rooms:
                    raise web.HTTPServiceUnavailable(text="Prototype room capacity reached")
                capacity = data.get("capacity", 2)
                if type(capacity) is not int or not 2 <= capacity <= min(6, policy["maxPlayers"]):
                    raise web.HTTPBadRequest(text="Unsupported player count for this game")
                alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
                code = "".join(secrets.choice(alphabet) for _ in range(10))
                while code in self.rooms:
                    code = "".join(secrets.choice(alphabet) for _ in range(10))
                room = Room(code, game, version, capacity, member.id, self.clock())
                if type(data.get("public", False)) is not bool:
                    raise web.HTTPBadRequest(text="Invalid visibility")
                room.public = data.get("public", False)
                room.name = self.text(data, "roomName", 48) if "roomName" in data else member.name + "'s race"
                self.rooms[code] = room
            else:
                code = self.text(data, "code", 10).upper()
                room = self.rooms.get(code)
                if not room or room.game != game or room.version != version:
                    raise web.HTTPNotFound(text="Room not found for this game/version")
                active = sum(not m.waiting for m in room.members.values())
                member.waiting = room.phase != "lobby" or active >= room.capacity
                if member.waiting and sum(m.waiting for m in room.members.values()) >= 6:
                    raise web.HTTPConflict(text="Waiting room is full")
                if not member.waiting:
                    for other in room.members.values():
                        other.ready = False
            room.members[member.id] = member
            self.tokens[member.token] = (room, member)
            return web.json_response({"token": member.token, "id": member.id,
                                      "room": self.snapshot(room), "ice": self.credentials(member) if not member.waiting else {"stun": [], "turn": []}})

        room, member = self.authenticate(request)
        if action == "poll":
            signals = list(member.signals)
            member.signals.clear()
            return web.json_response({"room": self.snapshot(room), "signals": signals})
        if action == "ice":
            if member.waiting:
                raise web.HTTPForbidden(text="Waiting players do not need relay access")
            return web.json_response(self.credentials(member))
        if action == "signal":
            target = room.members.get(data.get("to")) if isinstance(data.get("to"), str) else None
            encoded = self.text(data, "data", 8192)
            try:
                payload = base64.b64decode(encoded, validate=True)
            except ValueError:
                raise web.HTTPBadRequest(text="Invalid signal encoding")
            if target is None or target is member or member.waiting or target.waiting or not 1 <= len(payload) <= 6144:
                raise web.HTTPBadRequest(text="Invalid peer or signal")
            target.signals.append({"from": member.id, "data": encoded})
        elif action == "chat":
            message = self.text(data, "text", 320).strip()
            if not message:
                raise web.HTTPBadRequest(text="Empty message")
            self.limit(("chat", member.id), 8, 10)
            room.chat_sequence += 1
            room.chat.append({"sequence": room.chat_sequence, "sender": member.id,
                              "name": member.name, "text": message})
        elif action == "ready":
            if member.waiting or room.phase != "lobby" or type(data.get("ready")) is not bool:
                raise web.HTTPConflict(text="Cannot change readiness")
            member.ready = data["ready"]
        elif action == "start":
            if member.id != room.host:
                raise web.HTTPForbidden(text="Only the host can start")
            active = [m for m in room.members.values() if not m.waiting]
            if room.phase != "lobby" or len(active) < 2 or not all(m.ready for m in active):
                raise web.HTTPConflict(text="All players must be ready")
            room.phase = "loading"
        elif action == "loaded":
            if member.waiting or room.phase not in ("loading", "in_progress"):
                raise web.HTTPConflict(text="Race has not started loading")
            member.loaded = True
            if room.phase == "loading" and all(m.loaded for m in room.members.values() if not m.waiting):
                room.phase = "in_progress"
                room.start_at = int(self.clock() * 1000) + 3000
        elif action == "finished":
            if member.waiting or room.phase not in ("loading", "in_progress"):
                raise web.HTTPConflict(text="No active session to finish")
            member.finished = True
            if all(m.finished for m in room.members.values() if not m.waiting):
                room.phase = "lobby"
                room.start_at = 0
                self.fill_slots(room)
        elif action == "next":
            if member.id != room.host:
                raise web.HTTPForbidden(text="Only the host can return to the lobby")
            if room.phase not in ("loading", "in_progress"):
                raise web.HTTPConflict(text="Room is already in the lobby")
            room.phase = "lobby"
            room.start_at = 0
            # Existing racers retain their slots; waiting players fill vacancies.
            self.fill_slots(room)
        elif action == "leave":
            if member.id == room.host:
                self.close(room)
            else:
                del room.members[member.id]
                del self.tokens[member.token]
                if room.phase == "lobby":
                    self.fill_slots(room)
            return web.json_response({"left": True})
        else:
            raise web.HTTPNotFound()
        return web.json_response({"room": self.snapshot(room)})


def create_app(service):
    @web.middleware
    async def errors(request, handler):
        try:
            response = await handler(request)
        except web.HTTPException as error:
            response = web.json_response({"error": error.text}, status=error.status)
        response.headers["Cache-Control"] = "no-store"
        response.headers["X-Content-Type-Options"] = "nosniff"
        return response

    app = web.Application(client_max_size=12288, middlewares=[errors])
    app.router.add_route("*", "/online/v1/{action}", service.handle)

    async def cleanup_context(app):
        async def sweep():
            while True:
                await asyncio.sleep(10)
                service.expire()
        task = asyncio.create_task(sweep())
        yield
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass
    app.cleanup_ctx.append(cleanup_context)
    return app


if __name__ == "__main__":
    service = RoomService(os.environ.get("TURN_SECRET", ""),
                          os.environ.get("TURN_ADDRESS", ""),
                          int(os.environ.get("MAX_ROOMS", "8")))
    web.run_app(create_app(service), host="127.0.0.1", port=27889, access_log=None)
