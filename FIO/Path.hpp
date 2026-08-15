#pragma once
#include <tuple>
#include <string>
#include <utility>

namespace FIO
{
	class Path
	{
		Path() = delete;

	public:
		static bool IsFile(std::string_view path);

		static bool IsDirectory(std::string_view path);

		static bool Exists(std::string_view path);

		template<typename ... T>
		static constexpr auto Combine(T ... chunks)
		{
			std::string value;

			auto tuple = std::forward_as_tuple(std::forward<T>(chunks) ...);
			Combine_Append(value, tuple, std::make_index_sequence<sizeof...(T) - 1> {});
			Combine_Append(value, std::get<sizeof...(T) - 1>(tuple));

			return value;
		}

	private:
		static constexpr void Combine_Append(std::string& value, std::string_view chunk)
		{
			value.append(chunk);
		}
		static constexpr void Combine_Append(std::string& value, std::string_view chunk1, std::string_view chunk2)
		{
			value.append(chunk1);

#if defined(FIO_LINUX)
			if (!chunk1.ends_with('/') && !chunk2.starts_with('/'))
				value.append(1, '/');
#elif defined(FIO_WIN32)
			if (!chunk1.ends_with('/')  && !chunk2.starts_with('/') &&
				!chunk1.ends_with('\\') && !chunk2.starts_with('\\'))
				value.append(1, '\\');
#endif
		}
		template<typename ... T, size_t ... I>
		static constexpr void Combine_Append(std::string& value, const std::tuple<T ...>& tuple, std::index_sequence<I ...>)
		{
			(Combine_Append(value, std::get<I>(tuple), std::get<I + 1>(tuple)), ...);
		}
	};
}
