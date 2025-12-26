/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <SDL3/SDL.h>

#include "LifetimeWrapperSDL.hpp"

/*==================================================================*/

void SDL_Deleter<SDL_Window>     ::operator()(SDL_Window*      ptr) const noexcept { SDL_DestroyWindow(ptr); }
void SDL_Deleter<SDL_Renderer>   ::operator()(SDL_Renderer*    ptr) const noexcept { SDL_DestroyRenderer(ptr); }
void SDL_Deleter<SDL_Texture>    ::operator()(SDL_Texture*     ptr) const noexcept { SDL_DestroyTexture(ptr); }
void SDL_Deleter<SDL_AudioStream>::operator()(SDL_AudioStream* ptr) const noexcept { SDL_DestroyAudioStream(ptr); }
void SDL_Deleter<unsigned>       ::operator()(unsigned*        ptr) const noexcept { SDL_free(ptr); }
void SDL_Deleter<char>           ::operator()(char*            ptr) const noexcept { SDL_free(ptr); }
void SDL_Deleter<const char>     ::operator()(const char*      ptr) const noexcept { SDL_free(const_cast<char*>(ptr)); }

template <typename T>
SDL_Holder<T>::~SDL_Holder() noexcept = default;

template class SDL_Holder<SDL_Window>;
template class SDL_Holder<SDL_Renderer>;
template class SDL_Holder<SDL_Texture>;
template class SDL_Holder<SDL_AudioStream>;
template class SDL_Holder<unsigned>;
template class SDL_Holder<char>;
template class SDL_Holder<const char>;
