/**
 * libdatachannel audio communication test - Server
 * Copyright (c) 2025
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "rtc/rtc.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cstdio>

using namespace std::chrono_literals;
using std::shared_ptr;
using std::weak_ptr;
template <class T> weak_ptr<T> make_weak_ptr(shared_ptr<T> ptr) { return ptr; }

using nlohmann::json;

std::string localId = "server";
std::unordered_map<std::string, shared_ptr<rtc::PeerConnection>> peerConnectionMap;
std::unordered_map<std::string, shared_ptr<rtc::DataChannel>> dataChannelMap;
std::unordered_map<std::string, shared_ptr<rtc::Track>> trackMap;

// Extract PCM payload from RTP-like packet and start playback once buffer is large enough
static void appendAudioAndMaybePlay(const rtc::binary &pkt, const std::string &id) {
	static std::vector<unsigned char> audioBuffer;
	static bool playbackStarted = false;

	if (pkt.size() <=12)
		return;

	const size_t add = pkt.size() -12;
	const size_t oldSize = audioBuffer.size();
	audioBuffer.resize(oldSize + add);
	// Copy after12-byte RTP header
	std::memcpy(audioBuffer.data() + oldSize,
	 reinterpret_cast<const unsigned char *>(pkt.data()) +12, add);

	if (!playbackStarted && audioBuffer.size() >48000) { // ~0.5s at48kHz16-bit mono
		playbackStarted = true;
		std::cout << "[*] Playing audio through speakers (aplay/paplay/ffplay)..." << std::endl;

		// Write audio to a temp file
		std::string tempFile = "/tmp/audio_test_" + id + ".raw";
		std::ofstream file(tempFile, std::ios::binary);
		file.write(reinterpret_cast<const char *>(audioBuffer.data()),
		 static_cast<std::streamsize>(audioBuffer.size()));
		file.close();

		// Build playback command
		std::string playCmd = std::string("aplay -f S16_LE -r48000 -c1 ") + tempFile +
		 "2>/dev/null || "
		 "paplay --raw --format=s16le --rate=48000 --channels=1 " + tempFile +
		 "2>/dev/null || "
		 "ffplay -f s16le -ar48000 -ac1 -nodisp -autoexit " + tempFile +
		 "2>/dev/null &";

		std::thread([playCmd, tempFile]() {
			int result = std::system(playCmd.c_str());
			if (result ==0) {
				std::cout << "[✓] Audio playback started successfully" << std::endl;
			} else {
				std::cout << "[!] Could not play audio (install aplay, paplay, or ffplay)" << std::endl;
			}
			std::this_thread::sleep_for(3s);
			std::remove(tempFile.c_str());
		}).detach();
	}
}

shared_ptr<rtc::PeerConnection> createPeerConnection(const rtc::Configuration &config,
 weak_ptr<rtc::WebSocket> wws,
 std::string id);

int main(int argc, char **argv) try {
	std::cout << "=== Audio Communication Test - Server ===" << std::endl;
	std::cout << "Initializing logger..." << std::endl;
	rtc::InitLogger(rtc::LogLevel::Info);

	rtc::Configuration config;
	std::cout << "Configuration: No STUN server (local only)" << std::endl;

	auto ws = std::make_shared<rtc::WebSocket>();

	std::cout << "Setting up WebSocket callbacks..." << std::endl;

	ws->onOpen([]() {
		std::cout << "[✓] WebSocket connected to signaling server" << std::endl;
		std::cout << "[✓] Server is ready and waiting for clients" << std::endl;
	});

	ws->onError([](std::string s) { std::cout << "[✗] WebSocket error: " << s << std::endl; });

	ws->onClosed([]() { std::cout << "[!] WebSocket closed" << std::endl; });

	ws->onMessage([&config, wws = make_weak_ptr(ws)](rtc::message_variant data) {
		if (!std::holds_alternative<std::string>(data))
			return;

		json message = json::parse(std::get<std::string>(data));

		auto it = message.find("id");
		if (it == message.end())
			return;

		auto id = it->get<std::string>();
		std::cout << "[<-] Received message from client: " << id << std::endl;

		it = message.find("type");
		if (it == message.end())
			return;

		auto type = it->get<std::string>();
		std::cout << " Message type: " << type << std::endl;

		shared_ptr<rtc::PeerConnection> pc;
		if (auto jt = peerConnectionMap.find(id); jt != peerConnectionMap.end()) {
			pc = jt->second;
		} else if (type == "offer") {
			std::cout << "[+] Creating new peer connection for client: " << id << std::endl;
			pc = createPeerConnection(config, wws, id);
		} else {
			return;
		}

		if (type == "offer" || type == "answer") {
			auto sdp = message["description"].get<std::string>();
			std::cout << "[*] Setting remote description (type: " << type << ")" << std::endl;
			pc->setRemoteDescription(rtc::Description(sdp, type));
		} else if (type == "candidate") {
			auto sdp = message["candidate"].get<std::string>();
			auto mid = message["mid"].get<std::string>();
			std::cout << "[*] Adding remote candidate (mid: " << mid << ")" << std::endl;
			pc->addRemoteCandidate(rtc::Candidate(sdp, mid));
		}
	});

	const std::string url = "ws://localhost:8080/" + localId;
	std::cout << "Connecting to signaling server: " << url << std::endl;
	ws->open(url);

	std::cout << "\nServer is running. Press Ctrl+C to exit." << std::endl;
	std::cout << "Waiting for client connections...\n" << std::endl;

	// Keep the server running
	while (true) {
		std::this_thread::sleep_for(1s);
	}

	return 0;

} catch (const std::exception &e) {
	std::cout << "[✗] Fatal error: " << e.what() << std::endl;
	return -1;
}

shared_ptr<rtc::PeerConnection> createPeerConnection(const rtc::Configuration &config,
 weak_ptr<rtc::WebSocket> wws,
 std::string id) {
	auto pc = std::make_shared<rtc::PeerConnection>(config);

	pc->onStateChange([id](rtc::PeerConnection::State state) {
		std::cout << "[*] PeerConnection state for " << id << ": " << state << std::endl;
		if (state == rtc::PeerConnection::State::Connected) {
			std::cout << "[✓] Successfully connected to client: " << id << std::endl;
		} else if (state == rtc::PeerConnection::State::Failed) {
			std::cout << "[✗] Connection failed for client: " << id << std::endl;
		} else if (state == rtc::PeerConnection::State::Disconnected) {
			std::cout << "[!] Client disconnected: " << id << std::endl;
		}
	});

	pc->onGatheringStateChange([id](rtc::PeerConnection::GatheringState state) {
		std::cout << "[*] Gathering state for " << id << ": " << state << std::endl;
		if (state == rtc::PeerConnection::GatheringState::Complete) {
			std::cout << "[✓] ICE gathering complete for " << id << std::endl;
		}
	});

	pc->onLocalDescription([wws, id](rtc::Description description) {
		std::cout << "[->] Sending local description to " << id
		 << " (type: " << description.typeString() << ")" << std::endl;
		json message = {{"id", id},
		 {"type", description.typeString()},
		 {"description", std::string(description)}};

		if (auto ws = wws.lock())
			ws->send(message.dump());
	});

	pc->onLocalCandidate([wws, id](rtc::Candidate candidate) {
		std::cout << "[->] Sending ICE candidate to " << id << std::endl;
		json message = {{"id", id},
		 {"type", "candidate"},
		 {"candidate", std::string(candidate)},
		 {"mid", candidate.mid()}};

		if (auto ws = wws.lock())
			ws->send(message.dump());
	});

	pc->onDataChannel([id](shared_ptr<rtc::DataChannel> dc) {
		std::cout << "[+] DataChannel received from " << id
		 << " with label: \"" << dc->label() << "\"" << std::endl;

		dc->onOpen([id]() { std::cout << "[✓] DataChannel opened from " << id << std::endl; });

		dc->onClosed([id]() { std::cout << "[!] DataChannel closed from " << id << std::endl; });

		dc->onMessage([id](rtc::message_variant data) {
			if (auto s = std::get_if<std::string>(&data)) {
				std::cout << "[✓] Message received from " << id << ": \"" << *s << "\""
				 << std::endl;
			} else if (auto b = std::get_if<rtc::binary>(&data)) {
				std::cout << "[✓] Binary message received from " << id
				 << " (size: " << b->size() << " bytes)" << std::endl;
			}
		});

		dataChannelMap.emplace(id, dc);
	});

	pc->onTrack([id](shared_ptr<rtc::Track> track) {
		std::cout << "[+] Audio track received from " << id << std::endl;
		std::cout << " Direction: " << track->direction() << std::endl;
		std::cout << " Mid: " << track->mid() << std::endl;

		track->onOpen([id]() {
			std::cout << "[✓] Audio track opened from " << id << std::endl;
			std::cout << "[*] Starting audio playback through speakers..." << std::endl;
		});

		track->onClosed([id]() { std::cout << "[!] Audio track closed from " << id << std::endl; });

		track->onMessage([id](rtc::message_variant data) {
			if (auto p = std::get_if<rtc::binary>(&data)) {
				std::cout << "[✓] Audio data received from " << id
				 << " (size: " << p->size() << " bytes)" << std::endl;
				appendAudioAndMaybePlay(*p, id);
			}
		});

		trackMap.emplace(id, track);
	});

	peerConnectionMap.emplace(id, pc);
	return pc;
}
