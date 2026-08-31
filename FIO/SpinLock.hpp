#pragma once
#include <atomic>

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
				while (lock.test_and_set(std::memory_order_relaxed))
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
}
