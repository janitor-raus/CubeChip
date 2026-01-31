/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <algorithm>

#include "BasicInput.hpp"
#include "ExecPolicy.hpp"

/*==================================================================*/

void BasicKeyboard::update_states() noexcept {
	std::copy_n(EXEC_POLICY(unseq)
		m_new_state, TOTALKEYS, m_old_state);

	std::copy_n(EXEC_POLICY(unseq)
		SDL_GetKeyboardState(nullptr), TOTALKEYS, m_new_state);
}

void BasicMouse::update_states() noexcept {
	m_old_state = m_new_state;

	const auto old_X = m_pos_X, old_Y = m_pos_Y;
	m_new_state = SDL_GetMouseState(&m_pos_X, &m_pos_Y);
	m_rel_X = m_pos_X - old_X; m_rel_Y = m_pos_Y - old_Y;
}
