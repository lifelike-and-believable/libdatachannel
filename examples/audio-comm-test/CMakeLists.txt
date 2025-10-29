cmake_minimum_required(VERSION 3.13)
if(POLICY CMP0079)
	cmake_policy(SET CMP0079 NEW)
endif()

# Build server
add_executable(datachannel-audio-comm-server server.cpp)

set_target_properties(datachannel-audio-comm-server PROPERTIES
	CXX_STANDARD 17
	OUTPUT_NAME audio-comm-server)

set_target_properties(datachannel-audio-comm-server PROPERTIES
	XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.github.paullouisageneau.libdatachannel.examples.audio-comm-server)

find_package(Threads REQUIRED)
target_link_libraries(datachannel-audio-comm-server LibDataChannel::LibDataChannel Threads::Threads nlohmann_json::nlohmann_json)

if(MSVC)
	add_custom_command(TARGET datachannel-audio-comm-server POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE_DIR:datachannel>/datachannel.dll"
		$<TARGET_FILE_DIR:datachannel-audio-comm-server>
	)
endif()

# Build client
add_executable(datachannel-audio-comm-client client.cpp)

set_target_properties(datachannel-audio-comm-client PROPERTIES
	CXX_STANDARD 17
	OUTPUT_NAME audio-comm-client)

set_target_properties(datachannel-audio-comm-client PROPERTIES
	XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER com.github.paullouisageneau.libdatachannel.examples.audio-comm-client)

find_package(Threads REQUIRED)
target_link_libraries(datachannel-audio-comm-client LibDataChannel::LibDataChannel Threads::Threads nlohmann_json::nlohmann_json)

if(MSVC)
	add_custom_command(TARGET datachannel-audio-comm-client POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE_DIR:datachannel>/datachannel.dll"
		$<TARGET_FILE_DIR:datachannel-audio-comm-client>
	)
endif()
