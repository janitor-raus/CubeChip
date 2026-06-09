/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "SHA1.hpp"
#include "Thread.hpp"

#include <optional>
#include <string>
#include <atomic>

/*==================================================================*/

/**
 * @brief Incrementally hashes a memory buffer using SHA1, enabling controlled
 *        block-wise processing and thread-safe progress tracking.
 *
 * Unlike the one-shot 'SHA1::from()' methods, SHA1_Stream processes data in
 * discrete steps via 'advance()', making it suitable for background threading
 * without blocking the calling thread for long periods.
 *
 * @note 'progress()' is safe to call concurrently with 'advance()'. All other
 *       methods assume exclusive access and should not be called while
 *       'advance()' is running on another thread.
 */
class SHA1_Stream {
	SHA1 m_sha1;

	std::size_t m_data_size;
	std::atomic_size_t
		m_processed;

	auto get_processed() const noexcept {
		return m_processed.load(std::memory_order::relaxed);
	}

	void set_processed(std::size_t value) noexcept {
		m_processed.store(value, std::memory_order::relaxed);
	}

public:
	SHA1_Stream(std::size_t size_in_bytes = 0) noexcept
		: m_data_size(size_in_bytes)
		, m_processed(0)
	{}

	auto size() const noexcept { return m_data_size; }

	/**
	 * @brief Processes the next chunk of data and advances the hash state.
	 *
	 * @param data       Pointer to the start of the full data buffer.
	 * @param block_step Number of SHA1 blocks (64 bytes each) to process per call.
	 *                   Defaults to 64 (4KB per advance). Clamped to a minimum of 1.
	 * @return The completed SHA1 hash string if all data has been processed,
	 *         or std::nullopt if more data remains.
	 */
	[[nodiscard("The resulting SHA1 string may be lost!")]]
	auto advance(const char* data, std::size_t block_step = 64) noexcept
		-> std::optional<std::string>
	{
		const auto processed_bytes = get_processed();
		if (processed_bytes >= m_data_size) { return std::nullopt; }

		const auto length = std::min((block_step ? block_step : 1)
			* SHA1::c_block_bytes, m_data_size - processed_bytes);

		m_sha1.update(data + processed_bytes, length);
		set_processed(processed_bytes + length);

		if (processed_bytes + length >= m_data_size) {
			return m_sha1.final();
		} else {
			return std::nullopt;
		}
	}

	/**
	 * @brief Resets the hash state and progress, keeping the current data size.
	 */
	void reset() noexcept {
		set_processed(0);
		m_sha1.reset();
	}

	/**
	 * @brief Prepares the stream for a new data buffer of the given size,
	 *        resetting all state.
	 *
	 * @param size_in_bytes Size of the new data buffer in bytes.
	 */
	void setup(std::size_t size_in_bytes) noexcept {
		m_data_size = size_in_bytes;
		reset();
	}

	/**
	 * @brief Returns the current hashing progress as a value in [0.0, 1.0].
	 *        Returns 1.0 if the data size is zero.
	 *
	 * @note Safe to call concurrently with advance().
	 */
	float progress() const noexcept {
		return (m_data_size == 0) ? 1.0f
			: float(get_processed()) / float(m_data_size);
	}
};

/*==================================================================*/

/**
 * @brief Manages a background thread for incremental SHA1 hashing via SHA1_Stream,
 *        exposing progress and completion callbacks.
 *
 * Owns both the SHA1_Stream and the worker Thread. The thread can be cancelled at
 * any time via 'reset()', which blocks until the current 'advance()' step completes.
 *
 * @note Not thread-safe. All methods must be called from the owning thread.
 *       Progress can be observed indirectly via 'progress()', which delegates
 *       to SHA1_Stream's thread-safe 'progress()' method.
 */
class SHA1_ThreadedWidget {
	SHA1_Stream stream;
	Thread      thread;

	// Thread activity flag
	std::atomic_bool busy = false;

	static constexpr auto relaxed =
		std::memory_order::relaxed;

	void reset_thread() noexcept {
		if (thread.joinable()) {
			thread.request_stop();
			thread.join();
			busy.store(false, relaxed);
		}
	}

public:
	SHA1_ThreadedWidget(std::size_t size_in_bytes = 0) noexcept
		: stream(size_in_bytes)
	{}

	bool  running()  const noexcept { return busy.load(relaxed); }
	float progress() const noexcept { return stream.progress(); }

	/**
	 * @brief Stops the worker thread if running, and resets the stream state
	 *        while keeping the current data size.
	 *
	 * @note Blocks until the current advance() step completes.
	 */
	void reset() noexcept {
		reset_thread();
		stream.reset();
	}

	/**
	 * @brief Stops the worker thread if running, and prepares the stream
	 *        for a new data buffer of the given size.
	 *
	 * @param size Size of the new data buffer in bytes.
	 * @note Blocks until the current advance() step completes.
	 */
	void setup(std::size_t size) noexcept {
		reset_thread();
		stream.setup(size);
	}

	/**
	 * @brief Starts the background hashing thread.
	 *
	 * @param data     Pointer to the data buffer to hash. Must remain valid
	 *                 for the duration of the thread.
	 * @param on_done  Callable invoked with the completed SHA1 hash string
	 *                 when hashing finishes. Called from the worker thread.
	 * @param on_step  Callable invoked with the current progress [0.0, 1.0]
	 *                 after each advance() step. Called from the worker thread.
	 */
	template <
		std::invocable<std::string> OnDone,
		std::invocable<float> OnStep
	>
	void start(const char* data, OnDone&& on_done, OnStep&& on_step) noexcept {
		busy.store(true, relaxed);

		thread = Thread([ this, data,
			on_done = std::forward<OnDone>(on_done),
			on_step = std::forward<OnStep>(on_step)
		](StopToken token) noexcept {
			while (!token.stop_requested()) {
				if (auto hash = stream.advance(data)) {
					on_done(std::move(*hash));
					busy.store(false, relaxed);
					return;
				}
				on_step(stream.progress());
			}
			busy.store(false, relaxed);
		});
	}

	/**
	 * @brief Starts the background hashing thread without a progress callback.
	 *
	 * @param data    Pointer to the data buffer to hash. Must remain valid
	 *                for the duration of the thread.
	 * @param on_done Callable invoked with the completed SHA1 hash string
	 *                when hashing finishes. Called from the worker thread.
	 */
	template <std::invocable<std::string> OnDone>
	void start(const char* data, OnDone&& on_done) noexcept {
		start(data, std::forward<OnDone>(on_done), [](float) {});
	}
};
