#pragma once
#include <atomic>
#include <type_traits>

namespace FIO
{
	template<typename T>
	constexpr bool is_fast_copyable_v = std::is_trivially_copyable_v<T> && (sizeof(T) <= sizeof(void*));

	template<typename T>
	class MPSCQueue
	{
		struct Node
		{
			std::atomic<Node*> Next;
			T                  Value;
		};

		std::atomic<size_t> size;
		std::atomic<Node*>  back;
		std::atomic<Node*>  front;

		MPSCQueue(MPSCQueue&&) = delete;
		MPSCQueue(const MPSCQueue&) = delete;

	public:
		MPSCQueue()
			: size(0),
			back(new Node {}),
			front(back.load(std::memory_order_relaxed))
		{
		}

		virtual ~MPSCQueue()
		{
			while (auto node = front.load(std::memory_order_relaxed))
			{
				front.store(node->Next, std::memory_order_relaxed);

				delete node;
			}
		}

		inline auto GetSize() const
		{
			return size.load();
		}

		template<typename _T = T>
		requires(std::is_move_assignable_v<_T>)
		auto Pop()
		{
			for (auto size = this->size.load(); size == 0; size = this->size.load())
				this->size.wait(0);

			auto front = this->front.load(std::memory_order_relaxed);
			auto node  = front->Next.load(std::memory_order_acquire);
			auto value = std::move(node->Value);

			this->size.fetch_sub(1);
			this->front.store(node, std::memory_order_release);

			delete front;

			return value;
		}
		template<typename _T = T>
		requires(std::is_copy_assignable_v<_T> && !std::is_move_assignable_v<_T>)
		auto Pop()
		{
			for (auto size = this->size.load(); size == 0; size = this->size.load())
				this->size.wait(0);

			auto front = this->front.load(std::memory_order_relaxed);
			auto node  = front->Next.load(std::memory_order_acquire);
			auto value = node->Value;

			this->size.fetch_sub(1);
			this->front.store(node, std::memory_order_release);

			delete front;

			return value;
		}

		template<typename _T = T>
		requires(std::is_move_assignable_v<_T>)
		bool Pop(_T& value)
		{
			auto front = this->front.load(std::memory_order_relaxed);

			if (auto node = front->Next.load(std::memory_order_acquire))
			{
				value = std::move(node->Value);

				this->size.fetch_sub(1);
				this->front.store(node, std::memory_order_release);

				delete front;

				return true;
			}

			return false;
		}
		template<typename _T = T>
		requires(std::is_copy_assignable_v<_T> && !std::is_move_assignable_v<_T>)
		bool Pop(_T& value)
		{
			auto front = this->front.load(std::memory_order_relaxed);

			if (auto node = front->Next.load(std::memory_order_acquire))
			{
				value = node->Value;

				this->size.fetch_sub(1);
				this->front.store(node, std::memory_order_release);

				delete front;

				return true;
			}

			return false;
		}

		template<typename _T = T>
		requires(is_fast_copyable_v<_T>)
		void Push(_T value)
		{
			auto node = new Node { .Value = value };
			auto back = this->back.exchange(node, std::memory_order_acq_rel);

			back->Next.store(node, std::memory_order_release);

			if (!this->size.fetch_add(1))
				this->size.notify_one();
		}
		template<typename _T = T>
		requires(std::is_move_assignable_v<_T>)
		void Push(_T&& value)
		{
			auto node = new Node { .Value = std::move(value) };
			auto back = this->back.exchange(node, std::memory_order_acq_rel);

			back->Next.store(node, std::memory_order_release);

			if (!this->size.fetch_add(1))
				this->size.notify_one();
		}
		template<typename _T = T>
		requires(std::is_copy_assignable_v<_T> && !is_fast_copyable_v<_T>)
		void Push(const _T& value)
		{
			auto node = new Node { .Value = value };
			auto back = this->back.exchange(node, std::memory_order_acq_rel);

			back->Next.store(node, std::memory_order_release);

			if (!this->size.fetch_add(1))
				this->size.notify_one();
		}

		void Clear()
		{
			for (T value; Pop(value); )
				;
		}
	};
}
