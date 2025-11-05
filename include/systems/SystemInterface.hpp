/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <optional>
#include <utility>
#include <vector>
#include <array>
#include <span>
#include <bit>

#include "Typedefs.hpp"
#include "Concepts.hpp"
#include "AtomSharedPtr.hpp"
#include "ExecPolicy.hpp"
#include "Thread.hpp"
#include "Millis.hpp"

#include "AssignCast.hpp"
#include "ArrayOps.hpp"
#include "Map2D.hpp"
#include "AudioDevice.hpp"
#include "Voice.hpp"
#include "SimpleTimer.hpp"
#include "BasicInput.hpp"
#include "Well512.hpp"

#include "CoreRegistry.hpp"

#include <SDL3/SDL_scancode.h>

/*==================================================================*/

enum EmuState : u8 {
	NORMAL = 0x00, // normal operation
	HIDDEN = 0x01, // window is hidden
	PAUSED = 0x02, // paused by hotkey
	HALTED = 0x04, // normal end path
	FATAL  = 0x08, // fatal error path
	BENCH  = 0x10, // benchmarking mode

	NOT_RUNNING = HIDDEN | PAUSED | HALTED | FATAL, // when emulation must wait
	CANNOT_PAUSE = HIDDEN | HALTED | FATAL, // when pausing is disallowed
};

struct SimpleKeyMapping {
	u32          idx; // index value associated with entry
	SDL_Scancode key; // primary key mapping
	SDL_Scancode alt; // alternative key mapping
};

class HomeDirManager;
class BasicVideoSpec;
class GlobalAudioBase;

/*==================================================================*/

class alignas(HDIS) SystemInterface {

	Thread mSystemThread;
	Thread mTimingThread;

	Atom<f32> mFrameBusyTime{ 0.0f };
	Atom<u8> mGlobalState{ EmuState::NORMAL };

private:
	Atom<bool> mNextFrame{};
	Atom<bool> mStopFrame{};

protected:
	SimpleTimer mTimer{};

private:
	Str mOverlayDataBuffer{};
	AtomSharedPtr<Str>
		mOverlayData{ nullptr };

protected:
	Str* getOverlayDataBuffer() noexcept
		{ return &mOverlayDataBuffer; }

protected:
	static inline HomeDirManager* HDM{};
	static inline BasicVideoSpec* BVS{};

	Well512*       RNG{};
	BasicKeyboard* Input{};

public:
	void startWorker() noexcept;
	void stopWorker() noexcept;

protected:
	void systemThreadEntry(StopToken token);
	void timingThreadEntry(StopToken token);

	Atom<f32> mBaseSystemFramerate{};
	Atom<f32> mFramerateMultiplier{ 1.0f };

	u32 mBenchedFrames{};
	u32 mElapsedFrames{};

protected:
	SystemInterface() noexcept;

public:
	virtual ~SystemInterface() noexcept = default;

public:
	static void assignComponents(
		HomeDirManager* const pHDM,
		BasicVideoSpec* const pBVS
	) noexcept {
		HDM = pHDM;
		BVS = pBVS;
	}

	// Adds a State to the System, returns previous total state.
	auto addSystemState(EmuState state) noexcept { return mGlobalState.fetch_or ( state, mo::acq_rel); }
	// Removes a State from the System, returns previous total state.
	auto subSystemState(EmuState state) noexcept { return mGlobalState.fetch_and(~state, mo::acq_rel); }
	// Toggles a State in the System, returns previous total state.
	auto xorSystemState(EmuState state) noexcept { return mGlobalState.fetch_xor( state, mo::acq_rel); }

	// Sets the total System State to the given value.
	void setSystemState(EmuState state) noexcept { mGlobalState.store(state, mo::release); }
	// Fetches the current total System State.
	auto getSystemState()         const noexcept { return mGlobalState.load(mo::acquire);  }

	// Tests if the given State is present in the System.
	bool hasSystemState(EmuState state) const noexcept { return !!(getSystemState() & state); }
	// Tests if the System is allowed to run (temporarily or not).
	bool canSystemWork()               const noexcept { return !(getSystemState() & EmuState::NOT_RUNNING); }
	// Tests if the System is allowed to (un)pause on demand.
	bool canSystemPause()              const noexcept { return !(getSystemState() & EmuState::CANNOT_PAUSE); }

	// Attempts to (un)pause the System if possible, `reason` text optional.
	std::optional<bool> tryPauseSystem() noexcept {
		if (!canSystemPause()) { return std::nullopt; }
		else {
			xorSystemState(EmuState::PAUSED);
			return !!(getSystemState() & EmuState::PAUSED);
		}
	}

	virtual s32 getMaxDisplayW() const noexcept = 0;
	virtual s32 getMaxDisplayH() const noexcept = 0;
	virtual s32 getDisplaySize() const noexcept { return getMaxDisplayW() * getMaxDisplayH(); }


protected: void setBaseSystemFramerate(f32 value) noexcept;
public:    void setFramerateMultiplier(f32 value) noexcept;

public:    f32  getBaseSystemFramerate() const noexcept;
public:    f32  getFramerateMultiplier() const noexcept;
public:    f32  getRealSystemFramerate() const noexcept;


protected:
	void setStopFrame(bool state) noexcept { mStopFrame.store(state, mo::release); }
	auto getStopFrame()     const noexcept { return mStopFrame.load(mo::acquire);  }

private:
	void declareNextFrame(bool state) noexcept {
		mNextFrame.store(state, mo::release);
		mNextFrame.notify_one();
	}
	void standbyNextFrame(bool state) noexcept {
		mNextFrame.wait(state, mo::acquire);
		mNextFrame.store(state, mo::release);
	}

protected:
	void setViewportSizes(bool cond, u32 W, u32 H, u32 mult, u32 ppad) noexcept;

	void setDisplayBorderColor(u32 color) noexcept;

	virtual void mainSystemLoop() = 0;
	virtual void initializeSystem() noexcept = 0;

	/**
	 * @brief Save the Overlay data buffer contents to the public-facing buffer as a final step.
	 * @param[in] data :: A pointer to a string object, typically the return of getOverlayDataBuffer().
	 */
	/*___*/ void saveOverlayData(const Str* data);
	/**
	 * @brief Overridable method dedicated to assembling the string of Overlay data.
	 */
	virtual Str* makeOverlayData();
	/**
	 * @brief Overridable method dedicated to controlling when/how the Overlay data is pushed to
	 *        the public-facing buffer, typically used along with saveOverlayData().
	 */
	virtual void pushOverlayData();

public:
	/**
	 * @brief Public-facing method dedicated to fetching a copy of the Overlay data string from
	 *        the public-facing buffer, thread-safe.
	 * @note This method should not be used across a DLL boundary, as it is not ABI-safe.
	 */
	Str copyOverlayData() const noexcept;
};
