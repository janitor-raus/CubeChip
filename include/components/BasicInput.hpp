/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_mouse.h>

#include <concepts>

/*==================================================================*/

#define KEY(i) SDL_SCANCODE_##i
#define BTN(i) BIC_MOUSE_##i

enum BIC_Button : unsigned {
	BIC_MOUSE_LEFT   = SDL_BUTTON_LMASK,
	BIC_MOUSE_RIGHT  = SDL_BUTTON_RMASK,
	BIC_MOUSE_MIDDLE = SDL_BUTTON_MMASK,
	BIC_MOUSE_X1     = SDL_BUTTON_X1MASK,
	BIC_MOUSE_X2     = SDL_BUTTON_X2MASK,
};

/*==================================================================*/
	#pragma region BasicKeyboard Class

class BasicKeyboard final {
	static constexpr auto TOTALKEYS = 0u + SDL_SCANCODE_COUNT;

	bool m_old_state[TOTALKEYS]{};
	bool m_new_state[TOTALKEYS]{};

public:
	void update_states() noexcept;

	bool was_held   (SDL_Scancode key) const noexcept { return m_old_state[key]; }
	bool is_held    (SDL_Scancode key) const noexcept { return m_new_state[key]; }
	bool is_pressed (SDL_Scancode key) const noexcept { return !was_held(key) &&  is_held(key); }
	bool is_released(SDL_Scancode key) const noexcept { return  was_held(key) && !is_held(key); }

	template <std::same_as<SDL_Scancode>... S>
		requires (sizeof...(S) >= 1)
	bool are_all_held(S... code) const noexcept {
		return (is_held(code) && ...);
	}

	template <std::same_as<SDL_Scancode>... S>
		requires (sizeof...(S) >= 1)
	bool are_any_held(S... code) const noexcept {
		return (is_held(code) || ...);
	}
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/

/*==================================================================*/
	#pragma region BasicMouse Class

class BasicMouse final {
	unsigned m_new_state{}, m_old_state{};
	float m_pos_X{}, m_pos_Y{};
	float m_rel_X{}, m_rel_Y{};

public:
	void update_states() noexcept;

	float getRelX() const noexcept { return m_rel_X; }
	float getRelY() const noexcept { return m_rel_Y; }
	float getPosX() const noexcept { return m_pos_X; }
	float getPosY() const noexcept { return m_pos_Y; }

	bool was_held   (BIC_Button key) const noexcept { return m_old_state & key; }
	bool is_held    (BIC_Button key) const noexcept { return m_new_state & key; }
	bool is_pressed (BIC_Button key) const noexcept { return !was_held(key) &&  is_held(key); }
	bool is_released(BIC_Button key) const noexcept { return  was_held(key) && !is_held(key); }

	template <std::convertible_to<BIC_Button>... S>
		requires (sizeof...(S) >= 1)
	bool are_all_held(S... code) const noexcept {
		return (is_held(code) && ...);
	}

	template <std::convertible_to<BIC_Button>... S>
		requires (sizeof...(S) >= 1)
	bool are_any_held(S... code) const noexcept {
		return (is_held(code) || ...);
	}
};

	#pragma endregion
/*VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV*/
