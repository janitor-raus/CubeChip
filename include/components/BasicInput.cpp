/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <algorithm>

#include "BasicInput.hpp"
#include "ExecPolicy.hpp"

/*==================================================================*/

struct BasicKeyboard::BufferState {
	BasicKeyboard::Buffer m_new_buf{};
	BasicKeyboard::Buffer m_old_buf{};
};

BasicKeyboard::BasicKeyboard() noexcept
	: m_buffers(std::make_unique<BufferState>())
{}

BasicKeyboard::~BasicKeyboard() noexcept = default;

void BasicKeyboard::poll_global_state() noexcept {
	std::copy_n(EXEC_POLICY(unseq) SDL_GetKeyboardState(nullptr),
		c_total_keys, get_backup_buffer()->data());

	swap_active_buffer();
}

/*==================================================================*/

bool BasicKeyboard::was_held(SDL_Scancode key) const noexcept { return m_buffers->m_old_buf[key]; }
bool BasicKeyboard::is_held (SDL_Scancode key) const noexcept { return m_buffers->m_new_buf[key]; }

bool BasicKeyboard::is_pressed (SDL_Scancode key) const noexcept { return !m_buffers->m_old_buf[key] &&  m_buffers->m_new_buf[key]; }
bool BasicKeyboard::is_released(SDL_Scancode key) const noexcept { return  m_buffers->m_old_buf[key] && !m_buffers->m_new_buf[key]; }

void BasicKeyboard::advance_state() noexcept {
	std::copy_n(EXEC_POLICY(unseq)  m_buffers->m_new_buf .data(), c_total_keys, m_buffers->m_old_buf.data());
	std::copy_n(EXEC_POLICY(unseq) (*get_active_buffer()).data(), c_total_keys, m_buffers->m_new_buf.data());
}

/*==================================================================*/

void BasicMouse::update_states() noexcept {
	m_old_state = m_new_state;

	const auto old_X = m_pos_X, old_Y = m_pos_Y;
	m_new_state = SDL_GetMouseState(&m_pos_X, &m_pos_Y);
	m_rel_X = m_pos_X - old_X; m_rel_Y = m_pos_Y - old_Y;
}
