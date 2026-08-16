#pragma once
#include <atomic>

namespace FIO
{
	template<typename T>
	class MPSCQueue
	{
		struct Node
		{
			std::atomic<Node*> Next;
			T                  Value;
		};

		std::atomic<Node*> back;
		std::atomic<Node*> front;

		MPSCQueue(MPSCQueue&&) = delete;
		MPSCQueue(const MPSCQueue&) = delete;

	public:
		MPSCQueue()
			: back(new Node {}),
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

		bool Pop(T& value)
		{
			auto front = this->front.load(std::memory_order_relaxed);

			if (auto node = front->Next.load(std::memory_order_acquire))
			{
				value = std::move(node->Value);

				this->front.store(node, std::memory_order_release);

				delete front;

				return true;
			}

			return false;
		}

		void Push(T&& value)
		{
			auto node = new Node { .Value = std::move(value) };
			auto back = this->back.exchange(node, std::memory_order_acq_rel);

			back->Next.store(node, std::memory_order_release);
		}

		void Clear()
		{
			for (T value; Pop(value); )
				;
		}

		template<typename ... TArgs>
		void Emplace(TArgs ... args)
		{
			auto node = new Node { .Value = T(std::forward<TArgs>(args) ...) };
			auto back = this->back.exchange(node, std::memory_order_acq_rel);

			back->Next.store(node, std::memory_order_release);
		}
	};
}
