/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

module;

#include <cstddef>
#include <cstring>
#include <string_view>

export module WindowNode;

/*==================================================================*/

export union SDL_Event;

export struct WindowNode {
	using EventResult = int;
	using EventCallback = EventResult(*)(const SDL_Event&) noexcept;

	enum EventResultType {
		EVENT_CONTINUE,
		EVENT_SUCCESS,
		EVENT_FAILURE,
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
	virtual EventResult on_event(const SDL_Event&, EventCallback = nullptr) noexcept = 0;

protected:
	WindowNode* m_link_ptr{};
	struct LinkToken {};

public:
	WindowNode* get_link() const noexcept { return m_link_ptr; }
	void set_link(WindowNode* ptr, LinkToken&&) noexcept { m_link_ptr = ptr; }
};
