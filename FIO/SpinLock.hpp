#pragma once
#include <atomic>

#include "Thread.hpp"

namespace FIO
{
	class SpinLock
	{
		std::atomic_flag lock;

		SpinLock(SpinLock&&) = delete;
		SpinLock(const SpinLock&) = delete;

	public:
		inline SpinLock()
		{
		}

		inline ~SpinLock()
		{
		}

		inline bool IsLocked() const
		{
			return lock.test(std::memory_order_relaxed);
		}

		inline void Lock()
		{
			while (lock.test_and_set(std::memory_order_acquire))
				while (lock.test(std::memory_order_relaxed))
					;
		}
		inline void Unlock()
		{
			lock.clear(std::memory_order_release);
		}

		inline void Wait(bool prev_value = true) const
		{
			lock.wait(prev_value);
		}

		inline void Notify()
		{
			lock.notify_one();
		}
		inline void NotifyAll()
		{
			lock.notify_all();
		}
	};

	class RecursiveSpinLock
	{
		std::atomic<ThreadId> owner;
		size_t                count;

		RecursiveSpinLock(RecursiveSpinLock&&) = delete;
		RecursiveSpinLock(const RecursiveSpinLock&) = delete;

	public:
		inline RecursiveSpinLock()
			: owner(0),
			count(0)
		{
		}

		inline ~RecursiveSpinLock()
		{
		}

		inline bool IsLocked() const
		{
			return owner.load(std::memory_order_acquire) != 0;
		}

		inline void Lock()
		{
			auto thread_id = Thread::GetCurrentId();

			if (owner.load(std::memory_order_relaxed) == thread_id)
				++count;
			else
			{
				ThreadId thread_id_expected = 0;

				while (!owner.compare_exchange_weak(thread_id_expected, thread_id, std::memory_order_acquire, std::memory_order_relaxed))
					thread_id_expected = 0;

				count = 1;
			}
		}
		inline void Unlock()
		{
			if ((owner.load(std::memory_order_relaxed) == Thread::GetCurrentId()) && !--count)
				owner.store(0, std::memory_order_release);
		}
	};

	class SpinLockGuard
	{
		SpinLock* const lock;

		SpinLockGuard(SpinLockGuard&&) = delete;
		SpinLockGuard(const SpinLockGuard&) = delete;

	public:
		inline SpinLockGuard(SpinLock& lock)
			: lock(&lock)
		{
			lock.Lock();
		}

		inline ~SpinLockGuard()
		{
			lock->Unlock();
		}
	};

	class RecursiveSpinLockGuard
	{
		RecursiveSpinLock* const lock;

		RecursiveSpinLockGuard(RecursiveSpinLockGuard&&) = delete;
		RecursiveSpinLockGuard(const RecursiveSpinLockGuard&) = delete;

	public:
		inline RecursiveSpinLockGuard(RecursiveSpinLock& lock)
			: lock(&lock)
		{
			lock.Lock();
		}

		inline ~RecursiveSpinLockGuard()
		{
			lock->Unlock();
		}
	};
}
