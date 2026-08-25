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
#ifdef FIO_WIN32
		static bool IsFile(std::wstring_view path);
#endif

		static bool IsDirectory(std::string_view path);
#ifdef FIO_WIN32
		static bool IsDirectory(std::wstring_view path);
#endif

		static bool Exists(std::string_view path);
#ifdef FIO_WIN32
		static bool Exists(std::wstring_view path);
#endif

		template<typename ... T>
		static constexpr auto Combine(std::string_view chunk, T ... chunks)
		{
			std::string value;

			auto tuple = std::forward_as_tuple(chunk, std::forward<T>(chunks) ...);
			Combine_Append(value, tuple, std::make_index_sequence<sizeof...(T)> {});
			Combine_Append(value, std::get<sizeof...(T)>(tuple));

			return value;
		}
#ifdef FIO_WIN32
		template<typename ... T>
		static constexpr auto Combine(std::wstring_view chunk, T ... chunks)
		{
			std::wstring value;

			auto tuple = std::forward_as_tuple(chunk, std::forward<T>(chunks) ...);
			Combine_AppendW(value, tuple, std::make_index_sequence<sizeof...(T)> {});
			Combine_AppendW(value, std::get<sizeof...(T)>(tuple));

			return value;
		}
#endif

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

		static constexpr void Combine_AppendW(std::wstring& value, std::wstring_view chunk)
		{
			value.append(chunk);
		}
		static constexpr void Combine_AppendW(std::wstring& value, std::wstring_view chunk1, std::wstring_view chunk2)
		{
			value.append(chunk1);

#if defined(FIO_LINUX)
			if (!chunk1.ends_with(L'/') && !chunk2.starts_with(L'/'))
				value.append(1, L'/');
#elif defined(FIO_WIN32)
			if (!chunk1.ends_with(L'/')  && !chunk2.starts_with(L'/') &&
				!chunk1.ends_with(L'\\') && !chunk2.starts_with(L'\\'))
				value.append(1, L'\\');
#endif
		}
		template<typename ... T, size_t ... I>
		static constexpr void Combine_AppendW(std::wstring& value, const std::tuple<T ...>& tuple, std::index_sequence<I ...>)
		{
			(Combine_AppendW(value, std::get<I>(tuple), std::get<I + 1>(tuple)), ...);
		}
	};
}
