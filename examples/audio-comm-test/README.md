# Audio Communication Test

This example demonstrates a simple audio communication setup between a client and server using libdatachannel. It includes comprehensive logging instrumentation to show each step of the WebRTC negotiation process.

## Features

- **Client-Server Architecture**: Client initiates connection to server
- **Data Channel Communication**: Client opens a data channel and sends a test message
- **Audio Track with Debug Tone**: Client creates an audio track and sends a 440Hz sine wave tone
- **Comprehensive Logging**: Each step of the negotiation is logged with status indicators:
  - `[✓]` Success
  - `[✗]` Error
  - `[*]` Information
  - `[!]` Warning/Notice
  - `[->]` Outgoing message
  - `[<-]` Incoming message
  - `[+]` New resource created

## Requirements

- libdatachannel compiled with media support (default)
- libdatachannel compiled with WebSocket support (default)
- A WebSocket signaling server running on `localhost:8080`
- (Optional) Audio playback utility for hearing the tone on server side:
  - `aplay` (ALSA - usually pre-installed on Linux)
  - `paplay` (PulseAudio)
  - `ffplay` (FFmpeg)

## Building

The examples are built automatically when you build libdatachannel:

```bash
cd libdatachannel
cmake -B build -DCMAKE_BUILD_TYPE=Release
cd build
make -j2
```

The binaries will be created in `build/examples/audio-comm-test/`:
- `audio-comm-server`
- `audio-comm-client`

## Running the Test

### Step 1: Start the Signaling Server

First, start the Python signaling server on port 8080:

```bash
cd examples/signaling-server-python
python3 signaling-server.py 8080
```

The signaling server will output:
```
Listening on 127.0.0.1:8080
```

### Step 2: Start the Server

In a new terminal, start the audio communication server:

```bash
cd build/examples/audio-comm-test
./audio-comm-server
```

You should see:
```
=== Audio Communication Test - Server ===
Initializing logger...
Configuration: No STUN server (local only)
Setting up WebSocket callbacks...
Connecting to signaling server: ws://localhost:8080/server
[✓] WebSocket connected to signaling server
[✓] Server is ready and waiting for clients

Server is running. Press Ctrl+C to exit.
Waiting for client connections...
```

### Step 3: Start the Client

In another terminal, start the audio communication client:

```bash
cd build/examples/audio-comm-test
./audio-comm-client
```

The client will automatically:
1. Connect to the signaling server
2. Create a peer connection to the server
3. Open a data channel
4. Send a test message: "Hello from client - audio test!"
5. Create an audio track
6. Generate and send a 440Hz sine wave tone (1 second duration)

### Expected Output

#### Server Output:
```
[<-] Received message from client: client
    Message type: offer
[+] Creating new peer connection for client: client
[*] Setting remote description (type: offer)
[*] Gathering state for client: InProgress
[->] Sending local description to client (type: answer)
[*] Gathering state for client: Complete
[✓] ICE gathering complete for client
[->] Sending ICE candidate to client
[*] PeerConnection state for client: Connecting
[*] PeerConnection state for client: Connected
[✓] Successfully connected to client: client
[+] DataChannel received from client with label: "test"
[✓] DataChannel opened from client
[✓] Message received from client: "Hello from client - audio test!"
[+] Audio track received from client
    Direction: RecvOnly
    Mid: 1
[✓] Audio track opened from client
[*] Starting audio playback through speakers...
[✓] Audio data received from client (size: 972 bytes)
[✓] Audio data received from client (size: 972 bytes)
[✓] Audio data received from client (size: 972 bytes)
...
[*] Playing audio through speakers (aplay/paplay)...
[✓] Audio playback started successfully
```

**If audio playback is working, you should hear a 440Hz tone playing through your computer speakers!**

#### Client Output:
```
=== Audio Communication Test - Client ===
Initializing logger...
Configuration: No STUN server (local only)
Setting up WebSocket callbacks...
Connecting to signaling server: ws://localhost:8080/client
[✓] WebSocket connected to signaling server
[✓] Signaling ready

Step 1: Creating peer connection to server...
[->] Sending local description (type: offer)
[*] Gathering state: InProgress
[*] Gathering state: Complete
[✓] ICE gathering complete
[->] Sending ICE candidate

Step 2: Creating DataChannel...
Waiting for DataChannel to open...
[<-] Received message from server
    Message type: answer
[*] Setting remote description (type: answer)
[*] PeerConnection state: Connecting
[<-] Received message from server
    Message type: candidate
[*] Adding remote candidate (mid: 0)
[*] PeerConnection state: Connected
[✓] Successfully connected to server
[✓] DataChannel opened successfully

Step 3: Sending test message...
[✓] Sent message: "Hello from client - audio test!"

Step 4: Creating audio track...
[*] Adding audio track to peer connection
Waiting for audio track to open...
[✓] Audio track opened successfully

Step 5: Generating and sending debug audio tone...
[*] Generating debug tone: 440 Hz, 1 seconds
[✓] Generated 96000 bytes of audio data
[*] Preparing to send audio data as RTP packets
[✓] Sent 100 audio RTP packets (96000 bytes total)

=== Test Complete ===
[✓] Successfully completed all steps:
    1. Created peer connection
    2. Opened data channel
    3. Sent test message
    4. Created audio track
    5. Sent debug audio tone

Keeping connection alive for 5 seconds...
Cleaning up...
[✓] Client finished successfully
```

## Troubleshooting

### "WebSocket error" or connection issues
- Ensure the signaling server is running on `localhost:8080`
- Check that no firewall is blocking port 8080

### "Timeout waiting for DataChannel to open"
- The signaling server may not be running
- Check the signaling server logs for errors
- Ensure both client and server can reach the signaling server

### Audio not playing on server
- Install an audio playback utility: `sudo apt-get install alsa-utils` (for aplay) or `sudo apt-get install pulseaudio-utils` (for paplay)
- Check that your audio system is working: `speaker-test -t sine -f 440 -l 1`
- The server will still log "Audio data received" messages even if playback fails

### Build errors
- Ensure libdatachannel was built with media support enabled (default)
- Ensure libdatachannel was built with WebSocket support enabled (default)
- Try a clean rebuild: `rm -rf build && cmake -B build && cd build && make`

## Technical Details

### Audio Format
- Sample Rate: 48 kHz
- Channels: Mono (1 channel)
- Format: 16-bit signed PCM
- Codec: Opus (payload type 111)
- Packet Size: 960 samples (20ms at 48kHz)

### Signaling Protocol
The example uses a simple JSON-based signaling protocol over WebSocket:
- Offer/Answer messages contain SDP descriptions
- ICE candidates are exchanged after the initial offer/answer
- Messages are routed by client ID

### Connection Flow
1. Client and server connect to signaling server with unique IDs
2. Client creates PeerConnection and DataChannel (becomes offerer)
3. Client generates and sends SDP offer through signaling server
4. Server receives offer, creates PeerConnection, generates SDP answer
5. ICE candidates are exchanged bidirectionally
6. WebRTC connection establishes (DTLS handshake, SRTP keys derived)
7. DataChannel opens, message is sent
8. Audio track is added, audio data flows

## Notes

- This example uses local connections (no STUN server) for simplicity
- In production, you would use a proper STUN/TURN server for NAT traversal
- The audio generation is a simple sine wave for testing purposes
- Real applications would use proper Opus encoding (this example uses raw PCM for demonstration)
