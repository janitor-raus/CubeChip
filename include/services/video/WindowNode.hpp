/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <cstddef>
#include <cstring>
#include <string_view>

/*==================================================================*/

union SDL_Event;

struct WindowNode {
	using EventStatus = bool;
	using EventCallback = EventStatus(*)(const SDL_Event&) noexcept;

	enum : bool {
		EVENT_EXIT = false, // signal to stop event parsing
		EVENT_OKAY = true,  // signal to continue parsing
	};

	enum LinkAction : bool {
		DESTROY_LINKED,
		SEVER_LINK_ONLY,
	};

	enum LinkNotify : bool {
		TEARDOWN_PHASE,
		REBUILD_PHASE
	};

	struct ShortKey {
		static constexpr std::size_t size = 16;
		char data[size]{};

		constexpr ShortKey() noexcept = default;
		constexpr explicit ShortKey(const char* src) noexcept {
			if (!src || src[0] == '\0') { return; }
			for (std::size_t i = 0; i < (size - 1); ++i) {
				if ((data[i] = src[i]) == '\0') { return; }
			}

			// if we reached this point, the string was
			// too long to fit and has to be truncated!
			data[size - 2] = '~';
			data[size - 1] = '\0';
		}

		constexpr operator /***/ char* () /***/ noexcept { return data; }
		constexpr operator const char* () const noexcept { return data; }

		bool operator==(const ShortKey& other) const noexcept {
			return std::strcmp(data, other.data) == 0;
		}

		struct Hash {
			std::size_t operator()(const ShortKey& key) const noexcept {
				return std::hash<std::string_view>{}(key.data);
			}
		};
	};

public:
	virtual ~WindowNode() noexcept = default;

	// Arrow exposes the public polymorphic WindowNode interface.
	/***/ WindowNode* operator->() /***/ noexcept { return this; }
	// Arrow exposes the public polymorphic WindowNode interface.
	const WindowNode* operator->() const noexcept { return this; }

	virtual void drop_linked(LinkAction) noexcept = 0;
	virtual void notify_link(LinkNotify) noexcept = 0;

	virtual void on_present() noexcept = 0;

	// If the return value is EVENT_EXIT, the WindowNode instance this was called on
	// must be treated as destroyed -- do not access it or any of its state afterward.
	// At present (and by default), only SDL_EVENT_WINDOW_CLOSE_REQUESTED triggers this.
	virtual EventStatus on_event(const SDL_Event&, EventCallback = nullptr) noexcept = 0;
};
