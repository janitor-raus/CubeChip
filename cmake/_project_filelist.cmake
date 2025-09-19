
set(SHIMS_HEADERS
	"${PROJECT_INCLUDE_DIR}/shims/AtomSharedPtr.hpp"
	"${PROJECT_INCLUDE_DIR}/shims/ExecPolicy.hpp"
	"${PROJECT_INCLUDE_DIR}/shims/Expected.hpp"
	"${PROJECT_INCLUDE_DIR}/shims/HDIS_HCIS.hpp"
	"${PROJECT_INCLUDE_DIR}/shims/Thread.hpp"
)
source_group("Shims" FILES ${SHIMS_HEADERS})

# ==================================================================================== #

set(FRONTEND_HEADERS
	"${PROJECT_INCLUDE_DIR}/frontend/FrontendHost.hpp"
	"${PROJECT_INCLUDE_DIR}/frontend/FrontendInterface.hpp"
)
set(FRONTEND_SOURCES
	"${PROJECT_INCLUDE_DIR}/frontend/CubeChip.cpp" # main
	"${PROJECT_INCLUDE_DIR}/frontend/FrontendHost.cpp"
	"${PROJECT_INCLUDE_DIR}/frontend/FrontendInterface.cpp"
)
source_group("Frontend" FILES ${FRONTEND_HEADERS} ${FRONTEND_SOURCES})

# ==================================================================================== #

set(COMPONENTS_HEADERS
	"${PROJECT_INCLUDE_DIR}/components/Aligned.hpp"
	"${PROJECT_INCLUDE_DIR}/components/AudioDevice.hpp"
	"${PROJECT_INCLUDE_DIR}/components/AudioFilters.hpp"
	"${PROJECT_INCLUDE_DIR}/components/BasicInput.hpp"
	"${PROJECT_INCLUDE_DIR}/components/FrameLimiter.hpp"
	"${PROJECT_INCLUDE_DIR}/components/Map2D.hpp"
	"${PROJECT_INCLUDE_DIR}/components/RangeIterator.hpp"
	"${PROJECT_INCLUDE_DIR}/components/SimpleRingBuffer.hpp"
	"${PROJECT_INCLUDE_DIR}/components/SimpleTimer.hpp"
	"${PROJECT_INCLUDE_DIR}/components/TripleBuffer.hpp"
	"${PROJECT_INCLUDE_DIR}/components/Voice.hpp"
	"${PROJECT_INCLUDE_DIR}/components/Well512.hpp"
)
set(COMPONENTS_SOURCES
	"${PROJECT_INCLUDE_DIR}/components/AudioDevice.cpp"
	"${PROJECT_INCLUDE_DIR}/components/AudioFilters.cpp"
	"${PROJECT_INCLUDE_DIR}/components/BasicInput.cpp"
	"${PROJECT_INCLUDE_DIR}/components/FrameLimiter.cpp"
	"${PROJECT_INCLUDE_DIR}/components/SimpleTimer.cpp"
	"${PROJECT_INCLUDE_DIR}/components/Well512.cpp"
)
source_group("Components" FILES ${COMPONENTS_HEADERS} ${COMPONENTS_SOURCES})

# ==================================================================================== #

set(UTILITIES_HEADERS
	"${PROJECT_INCLUDE_DIR}/utilities/ArrayOps.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/AssignCast.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/AttachConsole.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/ColorOps.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Concepts.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/DefaultConfig.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/EzMaths.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/FriendlyUnique.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/LifetimeWrapperSDL.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Macros.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Millis.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/PathGetters.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/SettingWrapper.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/SHA1.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/SimpleFileIO.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/ThreadAffinity.hpp"
	"${PROJECT_INCLUDE_DIR}/utilities/Typedefs.hpp"
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
source_group("Utilities" FILES ${UTILITIES_HEADERS} ${UTILITIES_SOURCES})

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
source_group("Services" FILES ${SERVICES_HEADERS} ${SERVICES_SOURCES})

# ==================================================================================== #

set(SYSTEMS_HEADERS
	"${PROJECT_INCLUDE_DIR}/systems/SystemInterface.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/CoreRegistry.hpp"
)
set(SYSTEMS_SOURCES
	"${PROJECT_INCLUDE_DIR}/systems/SystemInterface.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/CoreRegistry.cpp"
)
source_group("Systems" FILES ${SYSTEMS_HEADERS} ${SYSTEMS_SOURCES})

# ==================================================================================== #

set(SYSTEM_CHIP8_HEADERS
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Chip8_CoreInterface.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/CHIP8_MODERN.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/SCHIP_MODERN.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/SCHIP_LEGACY.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/XOCHIP.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/MEGACHIP.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/CHIP8X.hpp"
)
set(SYSTEM_CHIP8_SOURCES
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Chip8_CoreInterface.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/CHIP8_MODERN.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/SCHIP_MODERN.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/SCHIP_LEGACY.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/XOCHIP.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/MEGACHIP.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/CHIP8/Cores/CHIP8X.cpp"
)
source_group("Systems\\CHIP8" FILES ${SYSTEM_CHIP8_HEADERS} ${SYSTEM_CHIP8_SOURCES})

# ==================================================================================== #

set(SYSTEM_BYTEPUSHER_HEADERS
	"${PROJECT_INCLUDE_DIR}/systems/BYTEPUSHER/BytePusher_CoreInterface.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/BYTEPUSHER/Cores/BYTEPUSHER_STANDARD.hpp"
)
set(SYSTEM_BYTEPUSHER_SOURCES
	"${PROJECT_INCLUDE_DIR}/systems/BYTEPUSHER/BytePusher_CoreInterface.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/BYTEPUSHER/Cores/BYTEPUSHER_STANDARD.cpp"
)
source_group("Systems\\BYTEPUSHER" FILES ${SYSTEM_BYTEPUSHER_HEADERS} ${SYSTEM_BYTEPUSHER_SOURCES})

# ==================================================================================== #

set(SYSTEM_GAMEBOY_HEADERS
	"${PROJECT_INCLUDE_DIR}/systems/GAMEBOY/GameBoy_CoreInterface.hpp"
	"${PROJECT_INCLUDE_DIR}/systems/GAMEBOY/Cores/GAMEBOY_CLASSIC.hpp"
)
set(SYSTEM_GAMEBOY_SOURCES
	"${PROJECT_INCLUDE_DIR}/systems/GAMEBOY/GameBoy_CoreInterface.cpp"
	"${PROJECT_INCLUDE_DIR}/systems/GAMEBOY/Cores/GAMEBOY_CLASSIC.cpp"
)
source_group("Systems\\GAMEBOY" FILES ${SYSTEM_GAMEBOY_HEADERS} ${SYSTEM_GAMEBOY_SOURCES})
