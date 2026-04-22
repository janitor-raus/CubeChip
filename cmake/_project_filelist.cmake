
set(SHIMS_HEADERS
	"${PROJECT_INCLUDE_DIR}/shims/AtomSharedPtr.hpp"
	"${PROJECT_INCLUDE_DIR}/shims/ExecPolicy.hpp"
	"${PROJECT_INCLUDE_DIR}/shims/Expected.hpp"
	"${PROJECT_INCLUDE_DIR}/shims/HDIS_HCIS.hpp"
	"${PROJECT_INCLUDE_DIR}/shims/Thread.hpp"
)
source_group("shims" FILES ${SHIMS_HEADERS})

# ==================================================================================== #

set(FRONTEND_HEADERS
	"${PROJECT_INCLUDE_DIR}/frontend/ApplicationHost.hpp"
	"${PROJECT_INCLUDE_DIR}/frontend/MemoryEditor.hpp"
	"${PROJECT_INCLUDE_DIR}/frontend/UserInterface.hpp"
	"${PROJECT_INCLUDE_DIR}/frontend/WindowHost.hpp"
)
set(FRONTEND_SOURCES
	"${PROJECT_INCLUDE_DIR}/frontend/CubeChip.cpp" # main
	"${PROJECT_INCLUDE_DIR}/frontend/ApplicationHost.cpp"
	"${PROJECT_INCLUDE_DIR}/frontend/ApplicationHost_GUI.cpp"
	"${PROJECT_INCLUDE_DIR}/frontend/MemoryEditor.cpp"
	"${PROJECT_INCLUDE_DIR}/frontend/UserInterface.cpp"
	"${PROJECT_INCLUDE_DIR}/frontend/WindowHost.cpp"
)
source_group("frontend" FILES ${FRONTEND_HEADERS} ${FRONTEND_SOURCES})

# ==================================================================================== #

set(COMPONENTS_HEADERS
	"${PROJECT_INCLUDE_DIR}/components/Aligned.hpp"
	"${PROJECT_INCLUDE_DIR}/components/AudioDevice.hpp"
	"${PROJECT_INCLUDE_DIR}/components/AudioFilters.hpp"
	"${PROJECT_INCLUDE_DIR}/components/BasicInput.hpp"
	"${PROJECT_INCLUDE_DIR}/components/DisplayDevice.hpp"
	"${PROJECT_INCLUDE_DIR}/components/FileImage.hpp"
	"${PROJECT_INCLUDE_DIR}/components/FrameLimiter.hpp"
	"${PROJECT_INCLUDE_DIR}/components/FramePacket.hpp"
	"${PROJECT_INCLUDE_DIR}/components/SlidingRingBuffer.hpp"
	"${PROJECT_INCLUDE_DIR}/components/SimpleMRU.hpp"
	"${PROJECT_INCLUDE_DIR}/components/SimpleTimer.hpp"
	"${PROJECT_INCLUDE_DIR}/components/TripleBuffer.hpp"
	"${PROJECT_INCLUDE_DIR}/components/Voice.hpp"
	"${PROJECT_INCLUDE_DIR}/components/Well512.hpp"
)
set(COMPONENTS_SOURCES
	"${PROJECT_INCLUDE_DIR}/components/AudioDevice.cpp"
	"${PROJECT_INCLUDE_DIR}/components/AudioFilters.cpp"
	"${PROJECT_INCLUDE_DIR}/components/BasicInput.cpp"
	"${PROJECT_INCLUDE_DIR}/components/DisplayDevice.cpp"
	"${PROJECT_INCLUDE_DIR}/components/FileImage.cpp"
	"${PROJECT_INCLUDE_DIR}/components/FrameLimiter.cpp"
	"${PROJECT_INCLUDE_DIR}/components/SimpleTimer.cpp"
)
source_group("components" FILES ${COMPONENTS_HEADERS} ${COMPONENTS_SOURCES})

# ==================================================================================== #

set(UTILITIES_HEADERS
	"${PROJECT_INCLUDE_DIR}/utilities/ArrayOps.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/AssignCast.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/AttachConsole.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/ColorOps.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Concepts.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/DefaultConfig.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/EzMaths.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/FileItem.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/FriendlyUnique.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/ImLabel.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/LifetimeWrapperSDL.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Macros.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Map2D.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Millis.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Parameter.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/PathGetters.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/SettingWrapper.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/SHA1.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/SimpleFileIO.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/StringJoin.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/ThreadAffinity.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Waveforms.hpp"
)
set(UTILITIES_SOURCES
	"${PROJECT_INCLUDE_DIR}/utilities/AttachConsole.cpp"
	"${PROJECT_INCLUDE_DIR}/utilities/DefaultConfig.cpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Millis.cpp"
	"${PROJECT_INCLUDE_DIR}/utilities/LifetimeWrapperSDL.cpp"
	"${PROJECT_INCLUDE_DIR}/utilities/PathGetters.cpp"
	"${PROJECT_INCLUDE_DIR}/utilities/SHA1.cpp"
	"${PROJECT_INCLUDE_DIR}/utilities/ThreadAffinity.cpp"
)
source_group("utilities" FILES ${UTILITIES_HEADERS} ${UTILITIES_SOURCES})

# ==================================================================================== #

set(SERVICES_HEADERS
	"${PROJECT_INCLUDE_DIR}/services/BasicLogger.hpp"
	"${PROJECT_INCLUDE_DIR}/services/BasicVideoSpec.hpp"
	"${PROJECT_INCLUDE_DIR}/services/GlobalAudioBase.hpp"
	"${PROJECT_INCLUDE_DIR}/services/HomeDirManager.hpp"
)
set(SERVICES_SOURCES
	"${PROJECT_INCLUDE_DIR}/services/BasicLogger.cpp"
	"${PROJECT_INCLUDE_DIR}/services/BasicVideoSpec.cpp"
	"${PROJECT_INCLUDE_DIR}/services/GlobalAudioBase.cpp"
	"${PROJECT_INCLUDE_DIR}/services/HomeDirManager.cpp"
)
source_group("services" FILES ${SERVICES_HEADERS} ${SERVICES_SOURCES})

# ==================================================================================== #

set(SYSTEMS_HEADERS
	"${PROJECT_INCLUDE_DIR}/systems/CoreRegistry.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/CoreRegistry.inl"
	"${PROJECT_INCLUDE_DIR}/systems/ISystemEmu.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/ISystemEmu_GUI.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/SystemDescriptor.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/SystemStaging.hpp"
)
set(SYSTEMS_SOURCES
	"${PROJECT_INCLUDE_DIR}/systems/CoreRegistry.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/ISystemEmu.cpp"
)
source_group("systems" FILES ${SYSTEMS_HEADERS} ${SYSTEMS_SOURCES})

# ==================================================================================== #

set(SYSTEM_CHIP8_HEADERS
	"${PROJECT_INCLUDE_DIR}/systems/chip8/IFamily_CHIP8.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/CHIP8_MODERN.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/SCHIP_MODERN.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/SCHIP_LEGACY.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/XOCHIP.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/MEGACHIP.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/CHIP8X.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/CHIP8E.hpp"
)
set(SYSTEM_CHIP8_SOURCES
	"${PROJECT_INCLUDE_DIR}/systems/chip8/IFamily_CHIP8.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/IFamily_CHIP8_GUI.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/CHIP8_MODERN.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/SCHIP_MODERN.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/SCHIP_LEGACY.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/XOCHIP.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/MEGACHIP.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/CHIP8X.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/chip8/cores/CHIP8E.cpp"
)
source_group("systems\\chip8" FILES ${SYSTEM_CHIP8_HEADERS} ${SYSTEM_CHIP8_SOURCES})

# ==================================================================================== #

set(SYSTEM_BYTEPUSHER_HEADERS
	"${PROJECT_INCLUDE_DIR}/systems/bytepusher/IFamily_BYTEPUSHER.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/bytepusher/cores/BYTEPUSHER_STANDARD.hpp"
)
set(SYSTEM_BYTEPUSHER_SOURCES
	"${PROJECT_INCLUDE_DIR}/systems/bytepusher/IFamily_BYTEPUSHER.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/bytepusher/IFamily_BYTEPUSHER_GUI.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/bytepusher/cores/BYTEPUSHER_STANDARD.cpp"
)
source_group("systems\\bytepusher" FILES ${SYSTEM_BYTEPUSHER_HEADERS} ${SYSTEM_BYTEPUSHER_SOURCES})

# ==================================================================================== #

set(SYSTEM_GAMEBOY_HEADERS
	"${PROJECT_INCLUDE_DIR}/systems/gameboy/IFamily_GAMEBOY.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/gameboy/cores/GAMEBOY_CLASSIC.hpp"
)
set(SYSTEM_GAMEBOY_SOURCES
	"${PROJECT_INCLUDE_DIR}/systems/gameboy/IFamily_GAMEBOY.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/gameboy/IFamily_GAMEBOY_GUI.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/gameboy/cores/GAMEBOY_CLASSIC.cpp"
)
source_group("systems\\gameboy" FILES ${SYSTEM_GAMEBOY_HEADERS} ${SYSTEM_GAMEBOY_SOURCES})
