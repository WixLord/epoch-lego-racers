// Prototype companion: modern encrypted networking stays outside the legacy x86 game.
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <conio.h>
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steamnetworkingcustomsignaling.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;

std::wstring Wide(const std::string& value)
{
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), nullptr, 0);
    if (!size && !value.empty()) throw std::runtime_error("Invalid UTF-8");
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), (int)value.size(), result.data(), size);
    return result;
}

std::string Encode(const void* data, int size)
{
    DWORD length = 0;
    CryptBinaryToStringA((const BYTE*)data, size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &length);
    std::string result(length, 0);
    CryptBinaryToStringA((const BYTE*)data, size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, result.data(), &length);
    result.resize(length);
    return result;
}

std::vector<char> Decode(const std::string& value)
{
    DWORD length = 0;
    if (value.size() > 8192 || !CryptStringToBinaryA(value.c_str(), (DWORD)value.size(), CRYPT_STRING_BASE64, nullptr, &length, nullptr, nullptr))
        throw std::runtime_error("Invalid signal");
    std::vector<char> result(length);
    if (!CryptStringToBinaryA(value.c_str(), (DWORD)value.size(), CRYPT_STRING_BASE64, (BYTE*)result.data(), &length, nullptr, nullptr))
        throw std::runtime_error("Invalid signal");
    return result;
}

struct InternetHandle {
    HINTERNET value;
    explicit InternetHandle(HINTERNET h) : value(h) { if (!h) throw std::runtime_error("HTTP initialization failed"); }
    ~InternetHandle() { WinHttpCloseHandle(value); }
    operator HINTERNET() const { return value; }
};

class Api {
    InternetHandle session{WinHttpOpen(L"EpochOnline/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0)};
    std::wstring host, path;
    INTERNET_PORT port;
    bool secure;
public:
    std::string token;
    explicit Api(const std::string& base) {
        auto url = Wide(base);
        URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
        parts.dwHostNameLength = parts.dwUrlPathLength = (DWORD)-1;
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) throw std::runtime_error("Invalid service URL");
        host.assign(parts.lpszHostName, parts.dwHostNameLength);
        path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
        port = parts.nPort; secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
        if (!secure && host != L"127.0.0.1") throw std::runtime_error("HTTPS is required");
        WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);
    }
    Json Post(const std::string& action, const Json& data = Json::object()) {
        InternetHandle connection(WinHttpConnect(session, host.c_str(), port, 0));
        auto endpoint = path + L"/" + Wide(action);
        InternetHandle request(WinHttpOpenRequest(connection, L"POST", endpoint.c_str(), nullptr, nullptr, nullptr, secure ? WINHTTP_FLAG_SECURE : 0));
        DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects));
        auto headers = L"Content-Type: application/json\r\nAuthorization: Bearer " + Wide(token) + L"\r\n";
        auto body = data.dump();
        if (!WinHttpSendRequest(request, headers.c_str(), (DWORD)-1, body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) || !WinHttpReceiveResponse(request, nullptr))
            throw std::runtime_error("Room service connection failed");
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
        std::string response;
        char chunk[4096]; DWORD read;
        while (WinHttpReadData(request, chunk, sizeof(chunk), &read) && read) {
            response.append(chunk, read);
            if (response.size() > 1024 * 1024) throw std::runtime_error("Oversized server response");
        }
        auto result = Json::parse(response);
        if (status != 200) throw std::runtime_error(result.value("error", "Room request failed"));
        return result;
    }
};

class Lobby {
public:
    Api api;
    std::string id;
    std::mutex mutex;
    Json room, ice;
    std::deque<Json> incoming;
    std::deque<std::pair<std::string, Json>> outgoing;
    std::string failure;
    std::atomic<bool> stop{false};
    std::thread worker;
    Clock::time_point raceTime = Clock::time_point::max();
    explicit Lobby(const std::string& url) : api(url) {}
    ~Lobby() { stop = true; if (worker.joinable()) worker.join(); try { api.Post("leave"); } catch (...) {} }
    void Queue(std::string action, Json data = Json::object()) {
        std::lock_guard<std::mutex> lock(mutex);
        if (outgoing.size() < 128) outgoing.emplace_back(action, data);
    }
    void Run() {
        worker = std::thread([this] {
            auto refreshed = Clock::now();
            int failures = 0;
            while (!stop) {
                try {
                    std::deque<std::pair<std::string, Json>> pending;
                    { std::lock_guard<std::mutex> lock(mutex); pending.swap(outgoing); }
                    for (const auto& item : pending) {
                        try { api.Post(item.first, item.second); }
                        catch (const std::exception& e) { std::cerr << "Room action: " << e.what() << std::endl; }
                    }
                    auto result = api.Post("poll");
                    auto next = result.at("room");
                    { std::lock_guard<std::mutex> lock(mutex);
                        room = next;
                        for (const auto& signal : result.at("signals")) if (incoming.size() < 128) incoming.push_back(signal);
                        if (room.at("phase") == "lobby") raceTime = Clock::time_point::max();
                        if (room.at("phase") == "in_progress" && raceTime == Clock::time_point::max()) {
                            auto delay = std::max<int64_t>(0, room.at("startAt").get<int64_t>() - room.at("serverTime").get<int64_t>());
                            raceTime = Clock::now() + std::chrono::milliseconds(delay);
                        }
                    }
                    bool active = false;
                    for (const auto& member : next.at("members")) if (member.at("id") == id) active = !member.value("waiting", false);
                    if (active && (Clock::now() - refreshed > std::chrono::minutes(5) || ice.at("turn").empty())) {
                        auto updated = api.Post("ice");
                        { std::lock_guard<std::mutex> lock(mutex); ice = updated; }
                        refreshed = Clock::now();
                    }
                    failures = 0;
                } catch (const std::exception& e) {
                    if (++failures >= 3) { std::lock_guard<std::mutex> lock(mutex); failure = e.what(); stop = true; }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        });
    }
};

class Signal : public ISteamNetworkingConnectionSignaling {
    Lobby& lobby;
    std::string peer;
public:
    Signal(Lobby& owner, std::string target) : lobby(owner), peer(target) {}
    bool SendSignal(HSteamNetConnection, const SteamNetConnectionInfo_t&, const void* data, int size) override {
        if (size <= 0 || size > 6144 || lobby.stop) return false;
        lobby.Queue("signal", {{"to", peer}, {"data", Encode(data, size)}});
        return true;
    }
    void Release() override { delete this; }
};

class ReceiveContext : public ISteamNetworkingSignalingRecvContext {
public:
    Lobby& lobby;
    std::string sender;
    std::map<std::string, HSteamNetConnection>& peers;
    ReceiveContext(Lobby& l, std::string from, std::map<std::string, HSteamNetConnection>& p) : lobby(l), sender(from), peers(p) {}
    ISteamNetworkingConnectionSignaling* OnConnectRequest(HSteamNetConnection connection, const SteamNetworkingIdentity& identity, int port) override {
        const char* name = identity.GetGenericString();
        if (!name || sender != name || port != 0 || peers.count(sender)) return nullptr;
        if (SteamNetworkingSockets()->AcceptConnection(connection) != k_EResultOK) return nullptr;
        peers[sender] = connection;
        return new Signal(lobby, sender);
    }
    void SendRejectionSignal(const SteamNetworkingIdentity&, const void*, int) override {}
};

void ConfigureIce(const Json& ice, bool relayOnly)
{
    auto utils = SteamNetworkingUtils();
    utils->SetGlobalConfigValueString(k_ESteamNetworkingConfig_P2P_STUN_ServerList,
                                     ice.at("stun").empty() ? "" : ice.at("stun")[0].get<std::string>().c_str());
    if (!ice.at("turn").empty()) {
        auto turn = ice.at("turn")[0];
        utils->SetGlobalConfigValueString(k_ESteamNetworkingConfig_P2P_TURN_ServerList, turn.at("address").get<std::string>().c_str());
        utils->SetGlobalConfigValueString(k_ESteamNetworkingConfig_P2P_TURN_UserList, turn.at("username").get<std::string>().c_str());
        utils->SetGlobalConfigValueString(k_ESteamNetworkingConfig_P2P_TURN_PassList, turn.at("password").get<std::string>().c_str());
    }
    utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_P2P_Transport_ICE_Enable,
        relayOnly ? k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_Relay : k_nSteamNetworkingConfig_P2P_Transport_ICE_Enable_All);
    utils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_TimeoutInitial, 30000);
}

int main(int argc, char** argv)
{
    try {
        std::map<std::string, std::string> args;
        for (int i = 1; i < argc; ++i) {
            std::string key = argv[i];
            if (key == "--host" || key == "--probe" || key == "--relay-only" || key == "--public" || key == "--list") args[key] = "1";
            else if (i + 1 < argc) args[key] = argv[++i];
            else throw std::runtime_error("Missing argument value");
        }
        bool hosting = args.count("--host"), probe = args.count("--probe"), relayOnly = args.count("--relay-only");
        std::string url = args.count("--service") ? args["--service"] : "https://epochremasters.com/online/v1";
        if (args.count("--list")) {
            Api browser(url);
            std::cout << browser.Post("list").dump(2) << std::endl;
            return 0;
        }
        if (!hosting && !args.count("--join")) throw std::runtime_error("Use --host or --join ROOMCODE, --name NAME, and --game PATH (or --probe)");
        if (!probe && !args.count("--game")) throw std::runtime_error("Specify --game with the isolated prototype executable path");
        Lobby lobby(url);
        Json request = {{"game", "epoch-lego-racers"}, {"version", "prototype-2"},
                        {"name", args.count("--name") ? args["--name"] : "Racer"}, {"capacity", 6}};
        if (!hosting) request["code"] = args["--join"];
        if (hosting && args.count("--public")) request["public"] = true;
        if (hosting && args.count("--room-name")) request["roomName"] = args["--room-name"];
        auto admitted = lobby.api.Post(hosting ? "create" : "join", request);
        lobby.id = admitted.at("id"); lobby.api.token = admitted.at("token");
        lobby.room = admitted.at("room"); lobby.ice = admitted.at("ice");
        std::cout << "ROOM " << lobby.room.at("code").get<std::string>() << std::endl;
        SteamNetworkingIdentity identity; identity.Clear(); identity.SetGenericString(lobby.id.c_str());
        SteamNetworkingErrMsg error;
        if (!GameNetworkingSockets_Init(&identity, error)) throw std::runtime_error(error);
        struct NetworkLifetime { ~NetworkLifetime() { GameNetworkingSockets_Kill(); } } networkLifetime;
        if (probe) {
            SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg,
                [](ESteamNetworkingSocketsDebugOutputType, const char* message) { std::cerr << "NET: " << message << std::endl; });
            SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_LogLevel_P2PRendezvous, k_ESteamNetworkingSocketsDebugOutputType_Msg);
        }
        ConfigureIce(lobby.ice, relayOnly);
        auto sockets = SteamNetworkingSockets();
        lobby.Run();
        std::cout << "R: ready. S: host starts. Q: leave. Keep this window open while racing." << std::endl;

        WSADATA wsa; if (WSAStartup(MAKEWORD(2, 2), &wsa)) throw std::runtime_error("Local network initialization failed");
        SOCKET local = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(local, (sockaddr*)&address, sizeof(address))) throw std::runtime_error("Cannot bind local game connection");
        int addressSize = sizeof(address); getsockname(local, (sockaddr*)&address, &addressSize);
        u_long nonblocking = 1; ioctlsocket(local, FIONBIO, &nonblocking);
        unsigned char random[16];
        if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) throw std::runtime_error("Random generator failed");
        std::string cookie; const char* hex = "0123456789abcdef";
        for (auto byte : random) { cookie += hex[byte >> 4]; cookie += hex[byte & 15]; }
        sockaddr_in gameAddress{};
        std::map<std::string, HSteamNetConnection> peers;
        std::map<std::string, bool> announced;
        bool launched = false, reportedLoaded = false, awaitingLobby = false;
        std::map<std::string, std::string> pendingReady;
        std::vector<std::string> raceIds;
        PROCESS_INFORMATION process{};
        auto began = Clock::now(), nextProbe = began;
        auto probeFinished = Clock::time_point::max();
        unsigned probesReceived = 0;
        std::map<std::string, unsigned> probeCounts;
        unsigned probePlayers = args.count("--peers") ? (unsigned)std::stoi(args["--peers"]) : 2;
        bool probePassed = false;
        std::string rosterText, iceText, statusText, inputLine, stateText;
        unsigned long long chatSequence = 0;
        bool discardInput = false;
        while (!lobby.stop) {
            sockets->RunCallbacks();
            Json room, ice; std::deque<Json> signals; Clock::time_point raceTime;
            { std::lock_guard<std::mutex> lock(lobby.mutex); room = lobby.room; ice = lobby.ice;
                signals.swap(lobby.incoming); raceTime = lobby.raceTime; }
            if (ice.dump() != iceText) { ConfigureIce(ice, relayOnly); iceText = ice.dump(); }
            std::string listing;
            for (auto& member : room.at("members")) listing += member.at("name").get<std::string>() +
                (member.value("waiting", false) ? " [next race] " : member.at("ready").get<bool>() ? " [ready] " : " [not ready] ");
            if (listing != rosterText) { std::cout << listing << std::endl; rosterText = listing; }
            std::vector<std::string> ids;
            for (auto& member : room.at("members")) if (!member.value("waiting", false)) ids.push_back(member.at("id"));
            bool participating = std::find(ids.begin(), ids.end(), lobby.id) != ids.end();
            std::string currentStatus = !participating || awaitingLobby ? "waiting" : room.at("phase").get<std::string>();
            if (currentStatus != statusText) { std::cout << "STATUS " << currentStatus << std::endl; statusText = currentStatus; }
            if (awaitingLobby && room.at("phase") == "lobby") {
                awaitingLobby = false; reportedLoaded = false; raceIds.clear(); pendingReady.clear(); gameAddress = {};
                std::cout << "Back in lobby. Ready up for the next session." << std::endl;
            }
            for (auto it = peers.begin(); it != peers.end();) {
                if (std::find(ids.begin(), ids.end(), it->first) == ids.end()) {
                    sockets->CloseConnection(it->second, 0, "Member left", false); announced.erase(it->first); it = peers.erase(it);
                } else ++it;
            }
            for (const auto& signal : signals) {
                std::string sender = signal.at("from");
                if (!participating || std::find(ids.begin(), ids.end(), sender) == ids.end()) continue;
                auto data = Decode(signal.at("data"));
                ReceiveContext context(lobby, sender, peers);
                sockets->ReceivedP2PCustomSignal(data.data(), (int)data.size(), &context);
            }
            for (const auto& id : ids) if (participating && id > lobby.id && !peers.count(id)) {
                SteamNetworkingIdentity remote; remote.Clear(); remote.SetGenericString(id.c_str());
                auto connection = sockets->ConnectP2PCustomSignaling(new Signal(lobby, id), &remote, 0, 0, nullptr);
                if (!connection) throw std::runtime_error("Cannot initiate peer connection");
                peers[id] = connection;
            }
            bool connected = participating && peers.size() + 1 == ids.size() && ids.size() > 1;
            for (const auto& item : peers) {
                SteamNetConnectionInfo_t info;
                if (!sockets->GetConnectionInfo(item.second, &info)) throw std::runtime_error("Peer connection disappeared");
                if (info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer || info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
                    throw std::runtime_error(std::string("Peer disconnected: ") + info.m_szEndDebug);
                if (info.m_eState != k_ESteamNetworkingConnectionState_Connected) connected = false;
                else if (!announced[item.first]) {
                    std::cout << "CONNECTED " << ((info.m_nFlags & k_nSteamNetworkConnectionInfoFlags_Relayed) ? "relay" : "direct") << std::endl;
                    announced[item.first] = true;
                }
            }
            if (probe && connected && Clock::now() >= nextProbe) {
                for (const auto& item : peers) sockets->SendMessageToConnection(item.second, "EPOCH-PROBE", 11, k_nSteamNetworkingSend_Unreliable, nullptr);
                nextProbe = Clock::now() + std::chrono::milliseconds(100);
            }
            bool ownReady = false;
            for (const auto& member : room.at("members"))
                if (member.at("id") == lobby.id) ownReady = member.value("ready", false);
            Json state = {{"ready", ownReady}, {"canReady", connected && currentStatus == "lobby"},
                {"canStart", hosting && connected && currentStatus == "lobby" &&
                    std::all_of(room.at("members").begin(), room.at("members").end(),
                        [](const Json& m) { return m.value("waiting", false) || m.value("ready", false); })}};
            if (state.dump() != stateText) { stateText = state.dump(); std::cout << "STATE " << stateText << std::endl; }
            for (const auto& message : room.value("chat", Json::array())) {
                auto sequence = message.at("sequence").get<unsigned long long>();
                if (sequence > chatSequence) { std::cout << "CHAT " << message.dump() << std::endl; chatSequence = sequence; }
            }
            DWORD available = 0;
            bool piped = GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_PIPE;
            bool inputReady = piped ? PeekNamedPipe(GetStdHandle(STD_INPUT_HANDLE), nullptr, 0, nullptr, &available, nullptr) && available : _kbhit() != 0;
            bool quit = false;
            if (inputReady) {
                char input[4096]; DWORD read = 0;
                if (piped) ReadFile(GetStdHandle(STD_INPUT_HANDLE), input, (DWORD)sizeof(input), &read, nullptr);
                else { input[0] = (char)_getch(); read = 1; }
                for (DWORD i = 0; i < read; ++i) {
                    char c = input[i];
                    if (c != '\r' && c != '\n') {
                        if (inputLine.size() < 4096 && !discardInput) inputLine += c;
                        else discardInput = true;
                    }
                    if (!piped || c == '\n') {
                        std::string command = discardInput ? "" : inputLine;
                        inputLine.clear(); discardInput = false;
                        if (command == "q" || command == "Q") { quit = true; break; }
                        if ((command == "r" || command == "u") && connected && currentStatus == "lobby")
                            lobby.Queue("ready", {{"ready", command == "r"}});
                        if (command == "s" && hosting && connected) lobby.Queue("start");
                        if (command.rfind("chat ", 0) == 0) {
                            try {
                                auto message = Json::parse(command.substr(5));
                                if (message.is_string()) lobby.Queue("chat", {{"text", message}});
                            } catch (const std::exception&) { std::cerr << "Invalid chat message" << std::endl; }
                        }
                    }
                }
            }
            if (quit) break;
            if (!probe && room.at("phase") == "loading" && !launched && !awaitingLobby && connected) {
                raceIds = ids;
                SetEnvironmentVariableA("EPOCH_PLAYER_COUNT", std::to_string(raceIds.size()).c_str());
                SetEnvironmentVariableA("EPOCH_PLAYER_SLOT", std::to_string(std::find(raceIds.begin(), raceIds.end(), lobby.id) - raceIds.begin()).c_str());
                auto executable = std::filesystem::absolute(std::filesystem::u8path(args["--game"]));
                if (!std::filesystem::exists(executable)) throw std::runtime_error("Game executable does not exist");
                SetEnvironmentVariableA("EPOCH_ONLINE", hosting ? "host" : "join");
                SetEnvironmentVariableA("EPOCH_BRIDGE_PORT", std::to_string(ntohs(address.sin_port)).c_str());
                SetEnvironmentVariableA("EPOCH_BRIDGE_TOKEN", cookie.c_str());
                auto command = L"\"" + executable.wstring() + L"\" -novideo -alphatrans";
                STARTUPINFOW startup{}; startup.cb = sizeof(startup);
                if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, executable.parent_path().c_str(), &startup, &process))
                    throw std::runtime_error("Cannot launch game");
                CloseHandle(process.hThread); launched = true;
                std::cout << "Select your saved racer in the game and click Join Race. Wait for everyone to join." << std::endl;
            }
            char packet[4096]; sockaddr_in sender{}; int senderSize = sizeof(sender);
            for (int budget = 0; budget < 64; ++budget) {
                int length = recvfrom(local, packet, sizeof(packet), 0, (sockaddr*)&sender, &senderSize);
                if (length < 0) break;
                if (length != 656 || sender.sin_addr.s_addr != htonl(INADDR_LOOPBACK) || memcmp(packet, cookie.data(), 32) ||
                    (gameAddress.sin_port && gameAddress.sin_port != sender.sin_port)) continue;
                gameAddress = sender;
                unsigned kind; memcpy(&kind, packet + 32 + 8, 4);
                if (kind < 1 || kind > 3 || raceIds.empty() || room.at("phase") == "lobby") continue;
                unsigned slot = (unsigned)(std::find(raceIds.begin(), raceIds.end(), lobby.id) - raceIds.begin()) + 1;
                memcpy(packet + 32 + 12, &slot, 4);
                if (kind == 3 && !reportedLoaded) { lobby.Queue("loaded"); reportedLoaded = true; }
                char wire[624]; unsigned wireSize = kind == 1 ? 624 : kind == 2 ? 64 : 24;
                memcpy(wire, packet + 32, kind == 1 ? 624 : 24);
                if (kind == 2) memcpy(wire + 24, packet + 32 + 584, 40);
                for (const auto& item : peers) sockets->SendMessageToConnection(item.second, wire, wireSize,
                    kind == 2 ? k_nSteamNetworkingSend_UnreliableNoDelay : k_nSteamNetworkingSend_Reliable, nullptr);
            }
            for (const auto& item : peers) {
                ISteamNetworkingMessage* messages[32];
                int count = sockets->ReceiveMessagesOnConnection(item.second, messages, 32);
                for (int i = 0; i < count; ++i) {
                    auto message = messages[i];
                    if (probe && message->m_cbSize == 11 && !memcmp(message->m_pData, "EPOCH-PROBE", 11)) { ++probesReceived; ++probeCounts[item.first]; }
                    if (!probe && (message->m_cbSize == 624 || message->m_cbSize == 64 || message->m_cbSize == 24) && gameAddress.sin_port) {
                        unsigned kind; memcpy(&kind, (char*)message->m_pData + 8, 4);
                        if ((kind == 1 && message->m_cbSize != 624) || (kind == 2 && message->m_cbSize != 64) || (kind == 3 && message->m_cbSize != 24) || kind < 1 || kind > 3) { message->Release(); continue; }
                        std::string envelope = cookie + std::string(624, '\0');
                        memcpy(envelope.data() + 32, message->m_pData, kind == 1 ? 624 : 24);
                        if (kind == 2) memcpy(envelope.data() + 32 + 584, (char*)message->m_pData + 24, 40);
                        auto found = std::find(raceIds.begin(), raceIds.end(), item.first);
                        unsigned slot = (unsigned)(found - raceIds.begin()) + 1;
                        if (found == raceIds.end()) { message->Release(); continue; }
                        memcpy(envelope.data() + 32 + 12, &slot, 4);
                        if (kind == 3 && (room.at("phase") != "in_progress" || Clock::now() < raceTime)) pendingReady[item.first] = envelope;
                        else sendto(local, envelope.data(), (int)envelope.size(), 0, (sockaddr*)&gameAddress, sizeof(gameAddress));
                    }
                    message->Release();
                }
            }
            if (!pendingReady.empty() && room.at("phase") == "in_progress" && Clock::now() >= raceTime) {
                for (const auto& ready : pendingReady) sendto(local, ready.second.data(), (int)ready.second.size(), 0, (sockaddr*)&gameAddress, sizeof(gameAddress));
                pendingReady.clear();
            }
            if (launched && WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0) {
                CloseHandle(process.hProcess); process.hProcess = nullptr; launched = false; awaitingLobby = true;
                gameAddress = {}; pendingReady.clear(); lobby.Queue("finished");
                std::cout << "Waiting for the other players to exit their game before the next lobby." << std::endl;
            }
            if (probe && !probePassed && probeCounts.size() + 1 == probePlayers &&
                std::all_of(probeCounts.begin(), probeCounts.end(), [](const auto& p) { return p.second >= 30; })) {
                std::cout << "PROBE PASS peers=" << probeCounts.size() << " received=" << probesReceived << std::endl;
                probePassed = true;
            }
            if (probe && Clock::now() - began > std::chrono::seconds(90)) throw std::runtime_error("Probe timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (!lobby.failure.empty()) throw std::runtime_error(lobby.failure);
        for (const auto& item : peers) sockets->CloseConnection(item.second, 0, "Leaving room", false);
        closesocket(local); WSACleanup();
        if (process.hProcess) CloseHandle(process.hProcess);
        lobby.stop = true; if (lobby.worker.joinable()) lobby.worker.join();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}
