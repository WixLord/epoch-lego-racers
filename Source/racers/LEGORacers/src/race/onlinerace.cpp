// Winsock must precede legacy engine headers that include windows.h.
// clang-format off
#include <winsock2.h>
#include <windows.h>
// clang-format on

#include "race/onlinerace.h"

#include "camera/golcamera.h"
#include "golfontbase.h"
#include "golstring.h"
#include "race/racestate.h"
#include "render/gold3drenderdevice.h"
#include "surface/golrendertarget.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace OnlineRace
{
namespace
{
const unsigned kMagic = 0x45504f43;
const unsigned kVersion = 2;
const unsigned kBuildSize = 0x22d;
// Fixed-size little-endian Windows protocol, explicitly versioned.
struct Packet {
	unsigned magic, version, kind, session, sequence, acknowledged;
	unsigned char build[kBuildSize];
	char padding[3];
	float position[3], direction[3], up[3], speed;
};
struct Sample {
	Packet packet;
	unsigned received;
};
SOCKET connection = INVALID_SOCKET;
sockaddr_in peer = {};
bool winsock = false, host = false, linked = false;
unsigned playerCount = 0, localSlot = 0, receivedBuilds = 0;
unsigned sequence = 0, lastSend = 0, lastBuildSend = 0;
struct PeerState {
	bool acknowledged, ready;
	unsigned lastSequence, lastReceive, sampleCount;
	unsigned char build[kBuildSize];
	Sample samples[32];
	Racer* racer;
};
PeerState peers[6] = {};
Packet buildPacket = {};
char bridgeToken[33] = {};
RaceState* race = NULL;
char failure[180] = "";

void Send(unsigned p_kind)
{
	if (connection == INVALID_SOCKET || !peer.sin_port) {
		return;
	}
	Packet packet = buildPacket;
	packet.magic = kMagic;
	packet.version = kVersion;
	packet.kind = p_kind;
	packet.session = localSlot + 1;
	packet.sequence = ++sequence;
	packet.acknowledged = receivedBuilds;
	if (p_kind == 2 && race) {
		Racer* local = race->GetRacer(0);
		GolVec3 position, direction, up;
		local->m_visuals.m_carEntity->GetPosition(&position);
		local->m_visuals.m_bodyModelEntity->GetAxes(&direction, &up);
		memcpy(packet.position, &position, 12);
		memcpy(packet.direction, &direction, 12);
		memcpy(packet.up, &up, 12);
		packet.speed = local->m_physics.m_forwardSpeed;
	}
	char envelope[32 + sizeof(Packet)];
	memcpy(envelope, bridgeToken, 32);
	memcpy(envelope + 32, &packet, sizeof(packet));
	sendto(connection, envelope, sizeof(envelope), 0, reinterpret_cast<const sockaddr*>(&peer), sizeof(peer));
}

bool ValidBuild(const unsigned char* p_data)
{
	// Validate fixed payload structure before handing it to the legacy builder.
	if (!memchr(p_data + 0x21, 0, 8)) {
		return false;
	}
	unsigned count = p_data[0x29] | (p_data[0x2a] << 8);
	if (count == 0 || count > 64) {
		return false;
	}
	for (unsigned i = 0; i < count; ++i) {
		const unsigned char* piece = p_data + 0x2b + 8 * i;
		if (piece[4] > 3 || (piece[5] & 127) > 31) {
			return false;
		}
	}
	return true;
}

void Receive()
{
	for (unsigned budget = 0; budget < 64; ++budget) {
		Packet packet;
		sockaddr_in sender;
		int senderSize = sizeof(sender);
		char envelope[32 + sizeof(Packet)];
		int size =
			recvfrom(connection, envelope, sizeof(envelope), 0, reinterpret_cast<sockaddr*>(&sender), &senderSize);
		if (size == SOCKET_ERROR) {
			break;
		}
		if (size != sizeof(envelope) || memcmp(envelope, bridgeToken, 32)) {
			continue;
		}
		memcpy(&packet, envelope + 32, sizeof(packet));
		if (packet.magic != kMagic || packet.version != kVersion || packet.kind < 1 || packet.kind > 3 ||
			packet.session == 0 || packet.session > playerCount || packet.session == localSlot + 1) {
			continue;
		}
		if (sender.sin_addr.s_addr != peer.sin_addr.s_addr || sender.sin_port != peer.sin_port) {
			continue;
		}
		unsigned slot = packet.session - 1;
		PeerState& state = peers[slot];
		if (!(receivedBuilds & (1u << slot))) {
			if (packet.kind != 1 || !ValidBuild(packet.build)) {
				continue;
			}
			memcpy(state.build, packet.build, kBuildSize);
			receivedBuilds |= 1u << slot;
		}
		state.lastReceive = GetTickCount();
		if (packet.acknowledged & (1u << localSlot)) {
			state.acknowledged = true;
		}
		if (packet.kind == 3) {
			state.ready = true;
		}
		if (packet.kind != 2 || !race || static_cast<int>(packet.sequence - state.lastSequence) <= 0) {
			continue;
		}
		bool valid = true;
		for (unsigned i = 0; i < 3; ++i) {
			if (!_finite(packet.position[i]) || fabs(packet.position[i]) > 1000000.0f ||
				!_finite(packet.direction[i]) || !_finite(packet.up[i])) {
				valid = false;
			}
		}
		if (!_finite(packet.speed) || fabs(packet.speed) > 10000.0f) {
			valid = false;
		}
		float d2 = 0, u2 = 0, dot = 0;
		for (unsigned i = 0; i < 3; ++i) {
			d2 += packet.direction[i] * packet.direction[i];
			u2 += packet.up[i] * packet.up[i];
			dot += packet.direction[i] * packet.up[i];
		}
		if (!valid || d2 < .5f || d2 > 1.5f || u2 < .5f || u2 > 1.5f || fabs(dot) > .2f) {
			continue;
		}
		state.lastSequence = packet.sequence;
		if (state.sampleCount == 30) {
			memmove(state.samples, state.samples + 1, 29 * sizeof(Sample));
			--state.sampleCount;
		}
		state.samples[state.sampleCount].packet = packet;
		state.samples[state.sampleCount++].received = GetTickCount();
	}
}
} // namespace

bool Requested()
{
	const char* mode = getenv("EPOCH_ONLINE");
	return mode && (!strcmp(mode, "host") || !strcmp(mode, "join"));
}
bool Active()
{
	return linked;
}
bool IsHost()
{
	return host;
}
unsigned Count()
{
	return playerCount;
}
unsigned GridSlot(unsigned index)
{
	return index == 0 ? localSlot : index - 1 < localSlot ? index - 1 : index;
}
bool IsRemote(const Racer* p_racer)
{
	if (!linked) {
		return false;
	}
	for (unsigned i = 0; i < playerCount; ++i) {
		if (i != localSlot && peers[i].racer == p_racer) {
			return true;
		}
	}
	return false;
}
const char* Error()
{
	return failure;
}

void Close()
{
	if (connection != INVALID_SOCKET) {
		closesocket(connection);
	}
	connection = INVALID_SOCKET;
	if (winsock) {
		WSACleanup();
	}
	winsock = false;
	linked = false;
	race = NULL;
	playerCount = 0;
	receivedBuilds = 0;
	memset(peers, 0, sizeof(peers));
	memset(&peer, 0, sizeof(peer));
}

bool ExchangeBuilds(const unsigned char* p_local, unsigned char p_remote[5][0x22d])
{
	Close();
	strcpy(failure, "Online connection failed. Please return to the online lobby and try again.");
	if (!ValidBuild(p_local)) {
		strcpy(failure, "Select a saved custom racer with a car for the online prototype.");
		return false;
	}
	const char* bridgePort = getenv("EPOCH_BRIDGE_PORT");
	const char* token = getenv("EPOCH_BRIDGE_TOKEN");
	const char* countText = getenv("EPOCH_PLAYER_COUNT");
	const char* slotText = getenv("EPOCH_PLAYER_SLOT");
	if (!bridgePort || !token || strlen(token) != 32 || !countText || !slotText) {
		return false;
	}
	playerCount = atoi(countText);
	localSlot = atoi(slotText);
	unsigned port = atoi(bridgePort);
	if (playerCount < 2 || playerCount > 6 || localSlot >= playerCount || !port || port > 65535) {
		Close();
		return false;
	}
	memcpy(bridgeToken, token, 33);
	WSADATA data;
	if (WSAStartup(MAKEWORD(2, 2), &data)) {
		return false;
	}
	winsock = true;
	host = !strcmp(getenv("EPOCH_ONLINE"), "host");
	connection = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (connection == INVALID_SOCKET) {
		Close();
		return false;
	}
	sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(connection, reinterpret_cast<sockaddr*>(&address), sizeof(address))) {
		Close();
		return false;
	}
	unsigned long nonblocking = 1;
	ioctlsocket(connection, FIONBIO, &nonblocking);
	peer.sin_family = AF_INET;
	peer.sin_port = htons(static_cast<unsigned short>(port));
	peer.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	receivedBuilds = 1u << localSlot;
	sequence = 0;
	memset(&buildPacket, 0, sizeof(buildPacket));
	memcpy(buildPacket.build, p_local, kBuildSize);
	unsigned start = GetTickCount(), sent = start - 250;
	while (GetTickCount() - start < 120000) {
		if (GetTickCount() - sent >= 250) {
			Send(1);
			sent = GetTickCount();
		}
		Receive();
		bool complete = receivedBuilds == (1u << playerCount) - 1;
		for (unsigned i = 0; i < playerCount; ++i) {
			if (i != localSlot && !peers[i].acknowledged) {
				complete = false;
			}
		}
		if (complete) {
			for (unsigned i = 1; i < playerCount; ++i) {
				memcpy(p_remote[i - 1], peers[GridSlot(i)].build, kBuildSize);
			}
			Send(1);
			linked = true;
			lastSend = 0;
			lastBuildSend = 0;
			return true;
		}
		MSG message;
		while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
			if (message.message == WM_QUIT) {
				PostQuitMessage(0);
				Close();
				return false;
			}
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
			strcpy(failure, "Online connection cancelled.");
			Close();
			return false;
		}
		Sleep(10);
	}
	strcpy(
		failure,
		"Not all racers connected within two minutes. Everyone must select their racer and start Single Race."
	);
	Close();
	return false;
}

void Attach(RaceState* p_race)
{
	if (!linked) {
		return;
	}
	race = p_race;
	for (unsigned i = 1; i < playerCount; ++i) {
		PeerState& state = peers[GridSlot(i)];
		state.racer = race->GetRacer(i);
		state.sampleCount = 0;
		state.lastReceive = GetTickCount();
	}
}

bool Ready()
{
	if (!linked) {
		return true;
	}
	for (unsigned i = 0; i < playerCount; ++i) {
		if (i != localSlot && !peers[i].ready) {
			return false;
		}
	}
	return true;
}

void Update()
{
	if (!linked || !race) {
		return;
	}
	Receive();
	unsigned now = GetTickCount();
	if (now - lastSend >= 33) {
		Send(2);
		if (!Ready()) {
			Send(3);
		}
		lastSend = now;
	}
	if (!Ready() && now - lastBuildSend >= 250) {
		Send(1);
		lastBuildSend = now;
	}
	for (unsigned slot = 0; slot < playerCount; ++slot) {
		PeerState& state = peers[slot];
		if (slot == localSlot || !state.racer || !state.sampleCount) {
			continue;
		}
		Racer* remote = state.racer;
		// Render 100ms behind arrivals. Freeze on prolonged loss instead of runaway prediction.
		unsigned target = now - 100;
		unsigned a = 0;
		while (a + 1 < state.sampleCount && static_cast<int>(target - state.samples[a + 1].received) >= 0) {
			++a;
		}
		unsigned b = a + 1 < state.sampleCount ? a + 1 : a;
		float t = b != a ? static_cast<float>(target - state.samples[a].received) /
							   (state.samples[b].received - state.samples[a].received)
						 : 0;
		if (static_cast<int>(target - state.samples[a].received) < 0) {
			t = 0;
		}
		if (t > 1) {
			t = 1;
		}
		const Packet& first = state.samples[a].packet;
		const Packet& second = state.samples[b].packet;
		GolVec3 position, direction, up;
		float* pos = &position.m_x;
		float* dir = &direction.m_x;
		float* upward = &up.m_x;
		for (unsigned i = 0; i < 3; ++i) {
			pos[i] = first.position[i] + (second.position[i] - first.position[i]) * t;
			dir[i] = first.direction[i] + (second.direction[i] - first.direction[i]) * t;
			upward[i] = first.up[i] + (second.up[i] - first.up[i]) * t;
		}
		float directionLength =
			direction.m_x * direction.m_x + direction.m_y * direction.m_y + direction.m_z * direction.m_z;
		if (directionLength < .01f) {
			memcpy(&direction, second.direction, 12);
		}
		GolMath::NormalizeVector3(direction, &direction);
		float dot = direction.m_x * up.m_x + direction.m_y * up.m_y + direction.m_z * up.m_z;
		up -= direction * dot;
		float upLength = up.m_x * up.m_x + up.m_y * up.m_y + up.m_z * up.m_z;
		if (upLength < .01f) {
			memcpy(&direction, second.direction, 12);
			memcpy(&up, second.up, 12);
		}
		GolMath::NormalizeVector3(up, &up);
		remote->m_physics.m_physicsEntity.SetPosition(position);
		remote->m_physics.m_physicsEntity.SetDirectionUp(direction, up);
		remote->m_visuals.m_carEntity->SetPosition(position);
		remote->m_visuals.m_carEntity->SetDirectionUp(direction, up);
		remote->m_physics.m_forwardSpeed = first.speed + (second.speed - first.speed) * t;
	}
}

void DrawNames(GolCamera* p_camera, GolFontBase* p_font, GolD3DRenderDevice* p_renderer)
{
	if (!linked || !race || !p_camera || !p_font) {
		return;
	}
	const GolRenderTarget* target = p_renderer->GetRenderTargetInfo();
	float scale = target->GetHeightU32() / 720.0f;
	for (unsigned i = 0; i < playerCount; ++i) {
		Racer* racer = race->GetRacer(i);
		GolVec3 position, view, screen;
		racer->m_visuals.m_carEntity->GetPosition(&position);
		position.m_z += 3.5f;
		p_camera->TransformToView(&position, &view);
		if (view.m_z <= .1f) {
			continue;
		}
		p_camera->ProjectToScreen(&position, &screen);
		if (!_finite(screen.m_x) || !_finite(screen.m_y) || screen.m_x < 0 || screen.m_x > target->GetWidthU32() ||
			screen.m_y < 0 || screen.m_y > target->GetHeightU32()) {
			continue;
		}
		char name[14];
		const unsigned char* record = i == 0 ? buildPacket.build : peers[GridSlot(i)].build;
		unsigned n = 0;
		for (; n < 13; ++n) {
			unsigned c = record[n * 2] | record[n * 2 + 1] << 8;
			if (!c) {
				break;
			}
			name[n] = c >= 32 && c <= 126 ? static_cast<char>(c) : '?';
		}
		name[n] = 0;
		if (!n) {
			strcpy(name, i == 0 ? "You" : "Friend");
		}
		int width, height;
		p_font->MeasureString(name, &width, &height);
		p_font->DrawString(
			name,
			p_renderer,
			static_cast<int>(screen.m_x - width * scale * .5f),
			static_cast<int>(screen.m_y - height * scale),
			scale,
			scale,
			NULL,
			0
		);
	}
	const char* status = !Ready() ? "Waiting for all players" : NULL;
	for (unsigned i = 0; i < playerCount; ++i) {
		if (Ready() && i != localSlot && GetTickCount() - peers[i].lastReceive > 5000) {
			status = "Connection lost - waiting for racer";
		}
	}
	if (status) {
		int width, height;
		p_font->MeasureString(status, &width, &height);
		p_font->DrawString(
			status,
			p_renderer,
			static_cast<int>((target->GetWidthU32() - width * scale) * .5f),
			static_cast<int>(target->GetHeightU32() * .18f),
			scale,
			scale,
			NULL,
			0
		);
	}
}
} // namespace OnlineRace
