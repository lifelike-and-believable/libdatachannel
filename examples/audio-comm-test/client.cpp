/**
 * libdatachannel audio communication test - Client
 * Copyright (c) 2025
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "rtc/rtc.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cmath>
#include <future>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace std::chrono_literals;
using std::shared_ptr;
using std::weak_ptr;
template <class T> weak_ptr<T> make_weak_ptr(shared_ptr<T> ptr) { return ptr; }

using nlohmann::json;

std::string localId = "client";
std::string remoteId = "server";
shared_ptr<rtc::PeerConnection> pc;
shared_ptr<rtc::DataChannel> dc;
shared_ptr<rtc::Track> audioTrack;

// Generate a simple sine wave tone as Opus-encoded audio
std::vector<std::byte> generateDebugTone(double frequency = 440.0, double duration = 1.0, int sampleRate = 48000) {
	std::cout << "[*] Generating debug tone: " << frequency << " Hz, " << duration << " seconds" << std::endl;
	
	// Generate raw PCM samples (16-bit signed)
	int numSamples = static_cast<int>(duration * sampleRate);
	std::vector<int16_t> samples(numSamples);
	
	for (int i = 0; i < numSamples; ++i) {
		double t = static_cast<double>(i) / sampleRate;
		double value = std::sin(2.0 * M_PI * frequency * t);
		samples[i] = static_cast<int16_t>(value * 32767.0 * 0.5); // 50% volume
	}
	
	// Convert to byte array (simulating Opus payload)
	// In a real implementation, you would encode this with libopus
	std::vector<std::byte> payload;
	payload.reserve(samples.size() * sizeof(int16_t));
	
	for (int16_t sample : samples) {
		payload.push_back(static_cast<std::byte>(sample & 0xFF));
		payload.push_back(static_cast<std::byte>((sample >> 8) & 0xFF));
	}
	
	std::cout << "[✓] Generated " << payload.size() << " bytes of audio data" << std::endl;
	return payload;
}

shared_ptr<rtc::PeerConnection> createPeerConnection(const rtc::Configuration &config,
                                                     weak_ptr<rtc::WebSocket> wws);

int main(int argc, char **argv) try {
	std::cout << "=== Audio Communication Test - Client ===" << std::endl;
	std::cout << "Initializing logger..." << std::endl;
	rtc::InitLogger(rtc::LogLevel::Info);

	rtc::Configuration config;
	std::cout << "Configuration: No STUN server (local only)" << std::endl;

	auto ws = std::make_shared<rtc::WebSocket>();

	std::promise<void> wsPromise;
	auto wsFuture = wsPromise.get_future();

	std::cout << "Setting up WebSocket callbacks..." << std::endl;

	ws->onOpen([&wsPromise]() {
		std::cout << "[✓] WebSocket connected to signaling server" << std::endl;
		wsPromise.set_value();
	});

	ws->onError([&wsPromise](std::string s) {
		std::cout << "[✗] WebSocket error: " << s << std::endl;
		wsPromise.set_exception(std::make_exception_ptr(std::runtime_error(s)));
	});

	ws->onClosed([]() {
		std::cout << "[!] WebSocket closed" << std::endl;
	});

	ws->onMessage([wws = make_weak_ptr(ws)](auto data) {
		if (!std::holds_alternative<std::string>(data))
			return;

		json message = json::parse(std::get<std::string>(data));

		auto it = message.find("id");
		if (it == message.end())
			return;

		auto id = it->get<std::string>();
		std::cout << "[<-] Received message from " << id << std::endl;

		it = message.find("type");
		if (it == message.end())
			return;

		auto type = it->get<std::string>();
		std::cout << "    Message type: " << type << std::endl;

		if (!pc) {
			std::cout << "[✗] No peer connection available" << std::endl;
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

	std::cout << "Waiting for signaling connection..." << std::endl;
	wsFuture.get();
	std::cout << "[✓] Signaling ready\n" << std::endl;

	// Create peer connection
	std::cout << "Step 1: Creating peer connection to server..." << std::endl;
	pc = createPeerConnection(config, ws);

	// Add audio track BEFORE creating data channel (to include in initial offer)
	std::cout << "\nStep 2: Creating audio track..." << std::endl;
	const rtc::SSRC ssrc = 1234;
	rtc::Description::Audio media("audio", rtc::Description::Direction::SendOnly);
	media.addOpusCodec(111); // Opus payload type
	media.addSSRC(ssrc, "audio-send");
	
	std::cout << "[*] Adding audio track to peer connection" << std::endl;
	audioTrack = pc->addTrack(media);

	std::promise<void> trackOpenPromise;
	auto trackOpenFuture = trackOpenPromise.get_future();

	audioTrack->onOpen([&trackOpenPromise]() {
		std::cout << "[✓] Audio track opened successfully" << std::endl;
		trackOpenPromise.set_value();
	});

	audioTrack->onClosed([]() {
		std::cout << "[!] Audio track closed" << std::endl;
	});

	// Create data channel (as offerer) - this will trigger the offer/answer exchange
	std::cout << "\nStep 3: Creating DataChannel..." << std::endl;
	dc = pc->createDataChannel("test");

	std::promise<void> dcOpenPromise;
	auto dcOpenFuture = dcOpenPromise.get_future();

	dc->onOpen([&dcOpenPromise]() {
		std::cout << "[✓] DataChannel opened successfully" << std::endl;
		dcOpenPromise.set_value();
	});

	dc->onClosed([]() {
		std::cout << "[!] DataChannel closed" << std::endl;
	});

	dc->onMessage([](auto data) {
		if (std::holds_alternative<std::string>(data))
			std::cout << "[<-] Received: " << std::get<std::string>(data) << std::endl;
	});

	// Wait for data channel to open
	std::cout << "Waiting for DataChannel to open..." << std::endl;
	if (dcOpenFuture.wait_for(10s) == std::future_status::timeout) {
		std::cout << "[✗] Timeout waiting for DataChannel to open" << std::endl;
		return -1;
	}

	// Wait for audio track to open
	std::cout << "\nStep 4: Waiting for audio track to open..." << std::endl;
	if (trackOpenFuture.wait_for(10s) == std::future_status::timeout) {
		std::cout << "[✗] Timeout waiting for audio track to open" << std::endl;
		return -1;
	}

	// Send test message
	std::cout << "\nStep 5: Sending test message..." << std::endl;
	const std::string testMessage = "Hello from client - audio test!";
	dc->send(testMessage);
	std::cout << "[✓] Sent message: \"" << testMessage << "\"" << std::endl;

	// Generate and send debug tone
	std::cout << "\nStep 6: Generating and sending debug audio tone..." << std::endl;
	auto audioData = generateDebugTone(440.0, 1.0, 48000);
	
	// Create RTP packet with the audio data
	// In a real implementation, you would properly packetize Opus frames into RTP
	std::cout << "[*] Preparing to send audio data as RTP packets" << std::endl;
	
	// For demonstration, we'll send the data in chunks simulating RTP packets
	const size_t chunkSize = 960; // Typical Opus frame size at 48kHz (20ms)
	size_t offset = 0;
	int packetCount = 0;
	
	while (offset < audioData.size()) {
		size_t currentChunkSize = std::min(chunkSize, audioData.size() - offset);
		
		// Create a simple RTP header (12 bytes) + payload
		std::vector<std::byte> rtpPacket;
		rtpPacket.resize(12 + currentChunkSize);
		
		// Simple RTP header
		rtpPacket[0] = std::byte(0x80); // Version 2, no padding, no extension
		rtpPacket[1] = std::byte(111);   // Opus payload type
		
		// Sequence number (big-endian)
		rtpPacket[2] = std::byte((packetCount >> 8) & 0xFF);
		rtpPacket[3] = std::byte(packetCount & 0xFF);
		
		// Timestamp (simplified, big-endian)
		uint32_t timestamp = packetCount * 960; // 20ms at 48kHz
		rtpPacket[4] = std::byte((timestamp >> 24) & 0xFF);
		rtpPacket[5] = std::byte((timestamp >> 16) & 0xFF);
		rtpPacket[6] = std::byte((timestamp >> 8) & 0xFF);
		rtpPacket[7] = std::byte(timestamp & 0xFF);
		
		// SSRC (big-endian)
		rtpPacket[8] = std::byte((ssrc >> 24) & 0xFF);
		rtpPacket[9] = std::byte((ssrc >> 16) & 0xFF);
		rtpPacket[10] = std::byte((ssrc >> 8) & 0xFF);
		rtpPacket[11] = std::byte(ssrc & 0xFF);
		
		// Copy payload
		std::copy(audioData.begin() + offset, 
		         audioData.begin() + offset + currentChunkSize,
		         rtpPacket.begin() + 12);
		
		// Send the RTP packet
		if (audioTrack && audioTrack->isOpen()) {
			audioTrack->send(rtpPacket);
		}
		
		offset += currentChunkSize;
		packetCount++;
		
		// Small delay between packets (20ms for Opus)
		std::this_thread::sleep_for(20ms);
	}
	
	std::cout << "[✓] Sent " << packetCount << " audio RTP packets (" << audioData.size() << " bytes total)" << std::endl;

	std::cout << "\n=== Test Complete ===" << std::endl;
	std::cout << "[✓] Successfully completed all steps:" << std::endl;
	std::cout << "    1. Created peer connection" << std::endl;
	std::cout << "    2. Created audio track" << std::endl;
	std::cout << "    3. Opened data channel" << std::endl;
	std::cout << "    4. Waited for audio track to open" << std::endl;
	std::cout << "    5. Sent test message" << std::endl;
	std::cout << "    6. Sent debug audio tone" << std::endl;

	std::cout << "\nKeeping connection alive for 5 seconds..." << std::endl;
	std::this_thread::sleep_for(5s);

	std::cout << "Cleaning up..." << std::endl;
	audioTrack.reset();
	dc.reset();
	pc.reset();

	std::cout << "[✓] Client finished successfully" << std::endl;
	return 0;

} catch (const std::exception &e) {
	std::cout << "[✗] Fatal error: " << e.what() << std::endl;
	return -1;
}

shared_ptr<rtc::PeerConnection> createPeerConnection(const rtc::Configuration &config,
                                                     weak_ptr<rtc::WebSocket> wws) {
	auto pc = std::make_shared<rtc::PeerConnection>(config);

	pc->onStateChange([](rtc::PeerConnection::State state) {
		std::cout << "[*] PeerConnection state: " << state << std::endl;
		if (state == rtc::PeerConnection::State::Connected) {
			std::cout << "[✓] Successfully connected to server" << std::endl;
		} else if (state == rtc::PeerConnection::State::Failed) {
			std::cout << "[✗] Connection failed" << std::endl;
		} else if (state == rtc::PeerConnection::State::Disconnected) {
			std::cout << "[!] Disconnected from server" << std::endl;
		}
	});

	pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state) {
		std::cout << "[*] Gathering state: " << state << std::endl;
		if (state == rtc::PeerConnection::GatheringState::Complete) {
			std::cout << "[✓] ICE gathering complete" << std::endl;
		}
	});

	pc->onLocalDescription([wws](rtc::Description description) {
		std::cout << "[->] Sending local description (type: " << description.typeString() << ")" << std::endl;
		json message = {{"id", remoteId},
		                {"type", description.typeString()},
		                {"description", std::string(description)}};

		if (auto ws = wws.lock())
			ws->send(message.dump());
	});

	pc->onLocalCandidate([wws](rtc::Candidate candidate) {
		std::cout << "[->] Sending ICE candidate" << std::endl;
		json message = {{"id", remoteId},
		                {"type", "candidate"},
		                {"candidate", std::string(candidate)},
		                {"mid", candidate.mid()}};

		if (auto ws = wws.lock())
			ws->send(message.dump());
	});

	return pc;
}
