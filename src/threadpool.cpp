#include "threadpool.h"
#include <thread>

Threadpool* Threadpool::instance = nullptr;

Threadpool::Threadpool(int _threads) {
	n_threads = _threads > 0 ?
		std::min(static_cast<unsigned int>(_threads), std::thread::hardware_concurrency()) :
		std::thread::hardware_concurrency();

	threads.resize(n_threads);

	for (int i = 0; i < n_threads; ++i) {
		threads[i].main_mutex = &main_mutex;
		threads[i].return_mutex = &return_mutex;
		threads[i].main_cond = &main_cond;
		threads[i].return_cond = &return_cond;
		threads[i].queue = &queue;
		threads[i].i = i;

		threads[i].handle = std::thread(&Threadpool::ThreadFunction, this, &threads[i]);
	}
}

Threadpool::~Threadpool() {
	{
		std::lock_guard<std::mutex> mlock(main_mutex);
		for (int i = 0; i < n_threads; ++i) {
			threads[i].stop = true;
		}
	}

	main_cond.notify_all();

	for (auto& thread : threads) {
		if (thread.handle.joinable()) {
			thread.handle.join();
		}
	}
}

void Threadpool::ThreadFunction(tp_struct* param) {
	while (true) {
		std::function<void()> task;
		{
			std::function<void()> task;
			bool should_stop = false;

			// First check without locking
			if (param->stop && param->queue->empty()) {
				break;
			}

			// Quick check if there's work without locking
			if (param->queue->empty()) {
				// Only lock and wait if necessary
				std::unique_lock<std::mutex> mlock(*param->main_mutex);
				param->main_cond->wait(mlock, [param] {
					return param->stop || !param->queue->empty();
					});

				should_stop = param->stop && param->queue->empty();

				if (!should_stop && !param->queue->empty()) {
					param->free = false;
					task = std::move(param->queue->front());
					param->queue->pop_front();
				}
				mlock.unlock();  // Explicitly unlock before potential break
			}
			else {
				// Fast path - queue has work
				std::unique_lock<std::mutex> mlock(*param->main_mutex);
				if (!param->queue->empty()) {  // Check again under lock
					param->free = false;
					task = std::move(param->queue->front());
					param->queue->pop_front();
				}
			}

			if (should_stop) {
				break;
			}

			if (task) {
				task();  // Execute without holding any locks

				// Minimal locking for status update
				{
					std::lock_guard<std::mutex> rlock(*param->return_mutex);
					param->free = true;
				}
				param->return_cond->notify_all();
			}
		}
}

}

void Threadpool::Queue(std::function<void()> function) {
	{
		std::lock_guard<std::mutex> mlock(main_mutex);
		queue.push_back(std::move(function));
	}
	// Only wake one thread - reduces contention
	main_cond.notify_one();
}

void Threadpool::Wait() {
	std::unique_lock<std::mutex> rlock(return_mutex);
	return_cond.wait(rlock, [this] {
		std::lock_guard<std::mutex> mlock(main_mutex);
		return queue.empty() &&
			std::all_of(threads.begin(), threads.end(),
				[](const tp_struct& t) { return t.free; });
		});
}
