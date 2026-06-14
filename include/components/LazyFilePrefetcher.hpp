/*
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <atomic>
#include <concepts>

#include "FileImage.hpp"
#include "Thread.hpp"
#include "Millis.hpp"

/*==================================================================*/

/**
 * @brief Manages a background thread to prefetch a memory-mapped file,
 *        exposing progress and completion callbacks.
 *
 * Owns a FileImage handle and the worker Thread. The thread can be cancelled at
 * any time via 'reset()', which blocks until the current prefetch chunk completes.
 * The same applies for calls to 'setup()' and 'start()' too.
 *
 * @note Not thread-safe. All methods must be called from the owning thread.
 *       Progress can be observed indirectly via 'progress()'.
 */
class LazyFilePrefetcher {
	Thread    m_thread;
	FileImage m_file_image;
	std::size_t        m_total_pages = 0;
	std::atomic_size_t m_pages_touched = 0;

	// Thread activity flag
	std::atomic_bool m_busy = false;

	static constexpr auto relaxed =
		std::memory_order::relaxed;

	void reset_thread() noexcept {
		if (m_thread.joinable()) {
			m_thread.request_stop();
			m_thread.join();
		}
	}

	void count_total_pages() noexcept {
		const auto page_size = FileImage::page_size();
		m_total_pages = (m_file_image.size() + page_size - 1) / page_size;
	}

public:
	LazyFilePrefetcher() noexcept = default;
	LazyFilePrefetcher(const FileImage& image) noexcept
		: m_file_image(image)
	{
		count_total_pages();
	}

	auto total_pages() const noexcept { return m_total_pages; }

	bool  running()  const noexcept { return m_busy.load(relaxed); }
	float progress() const noexcept {
		return m_total_pages == 0 ? 1.0f
			: float(m_pages_touched.load(relaxed)) / float(m_total_pages);
	}

	/**
	 * @brief Stops the worker thread if running, and resets the progress
	 *        while keeping the current FileImage untouched.
	 *
	 * @note Blocks until the current chunk is prefetched.
	 */
	void reset() noexcept {
		reset_thread();
		m_pages_touched = 0;
	}

	/**
	 * @brief Stops the worker thread if running, resets progress, and
	 *              copies the given FileImage handle.
	 *
	 * @param image The new FileImage to prefetch on.
	 * @note Blocks until the current chunk is prefetched.
	 */
	void setup(const FileImage& image) noexcept {
		reset();
		m_file_image = image;
		count_total_pages();
	}

	/**
	 * @brief Starts the background prefetch thread, touching pages in chunks
	 *        before sleeping for a brief moment and resuming.
	 *
	 * @param on_done  Callable invoked when prefetch finishes. Called from the
	 *                 worker thread.
	 * @param on_step  Callable invoked with the current progress [0.0, 1.0]
	 *                 after each chunk. Called from the worker thread.
	 * @param chunk_step  Amount of pages to touch before sleeping the thread.
	 *                    Used to stagger disk reads and CPU usage.
	 */
	template <
		std::invocable OnDone,
		std::invocable<float> OnStep
	>
	void start(OnDone&& on_done, OnStep&& on_step, std::size_t chunk_step = 16) noexcept {
		if (!m_file_image.valid() || chunk_step == 0) { return; }

		if (progress() > 0.0f) { reset(); }
		m_busy.store(true, relaxed);

		m_thread = Thread([ this, chunk_step,
			on_done = std::forward<OnDone>(on_done),
			on_step = std::forward<OnStep>(on_step)
		](StopToken token) noexcept {
			volatile const auto* ptr = m_file_image.data();

			for (std::size_t page = 0; page < m_total_pages; page += chunk_step) {
				const auto chunk_end = std::min(page + chunk_step, m_total_pages);
				for (std::size_t i = page; i < chunk_end;) {
					(void)*(ptr + i * FileImage::page_size());
					m_pages_touched.store(++i, relaxed);
				}

				if (m_pages_touched.load(relaxed) == m_total_pages) {
					on_done(); break;
				} else if (!token.stop_requested()) {
					on_step(progress());
					Millis::sleep_for(1);
				} else { break; }
			}
			m_busy.store(false, relaxed);
		});
	}

	/**
	 * @brief Starts the background prefetch thread.
	 *
	 * @param on_done  Callable invoked when prefetch finishes. Called from the
	 *                 worker thread.
	 * @param chunk_step  Amount of pages to touch before sleeping the thread.
	 *                    Used to stagger disk reads and CPU usage.
	 */
	template <std::invocable OnDone>
	void start(OnDone&& on_done, std::size_t chunk_step = 16) noexcept {
		start(std::forward<OnDone>(on_done), [](float) {}, chunk_step);
	}

	/**
	 * @brief Starts the background prefetch thread.
	 *
	 * @param chunk_step  Amount of pages to touch before sleeping the thread.
	 *                    Used to stagger disk reads and CPU usage.
	 */
	void start(std::size_t chunk_step = 16) noexcept {
		start([]() {}, [](float) {}, chunk_step);
	}
};
