#pragma once

/*____________________________________________________________________________________________________________

Original Author: skadro
Github: https://github.com/skadro-official
License: See end of file

skCrypter
		Compile-time, Usermode + Kernelmode, safe and lightweight string crypter library for C++11+

							*Not removing this part is appreciated*
____________________________________________________________________________________________________________*/

#ifdef _KERNEL_MODE
namespace std
{
	// STRUCT TEMPLATE remove_reference
	template <class _Ty>
	struct remove_reference {
		using type = _Ty;
	};

	template <class _Ty>
	struct remove_reference<_Ty&> {
		using type = _Ty;
	};

	template <class _Ty>
	struct remove_reference<_Ty&&> {
		using type = _Ty;
	};

	template <class _Ty>
	using remove_reference_t = typename remove_reference<_Ty>::type;

	// STRUCT TEMPLATE remove_const
	template <class _Ty>
	struct remove_const { // remove top-level const qualifier
		using type = _Ty;
	};

	template <class _Ty>
	struct remove_const<const _Ty> {
		using type = _Ty;
	};

	template <class _Ty>
	using remove_const_t = typename remove_const<_Ty>::type;
}
#else
#include <type_traits>
#endif

namespace skc
{
	template<class _Ty>
	using clean_type = typename std::remove_const_t<std::remove_reference_t<_Ty>>;

	template <int _size, char _key1, char _key2, typename T>
	class skCrypter
	{
	public:
		__forceinline constexpr skCrypter(T* data)
		{
			crypt(data);
		}

		__forceinline T* get()
		{
			return _storage;
		}

		__forceinline int size() // (w)char count
		{
			return _size;
		}

		__forceinline  char key()
		{
			return _key1;
		}

		__forceinline  T* encrypt()
		{
			if (!isEncrypted())
				crypt(_storage);

			return _storage;
		}

		__forceinline  T* decrypt()
		{
			if (isEncrypted())
				crypt(_storage);

			return _storage;
		}

		__forceinline bool isEncrypted()
		{
			return _storage[_size - 1] != 0;
		}

		__forceinline void clear() // set full storage to 0
		{
			for (int i = 0; i < _size; i++)
			{
				_storage[i] = 0;
			}
		}

		__forceinline operator T* ()
		{
			decrypt();

			return _storage;
		}

	private:
		__forceinline constexpr void crypt(T* data)
		{
			for (int i = 0; i < _size; i++)
			{
				_storage[i] = data[i] ^ (_key1 + i % (1 + _key2));
			}
		}

		T _storage[_size]{};
	};
}

__forceinline constexpr uintptr_t genkey() {
	constexpr auto hash_str = [](const char* str) constexpr -> uintptr_t {
		uintptr_t hash = 0xcbf29ce484222325ULL;
		for (int i = 0; str[i]; ++i) {
			hash ^= static_cast<uintptr_t>(str[i]);
			hash *= 0x100000001b3ULL;
		}
		return hash;
	};

	constexpr uintptr_t key1 = __LINE__ * 0x9e3779b9ULL;
	constexpr uintptr_t key2 = hash_str(__FILE__) + __LINE__ ^ 0x63378da2ffc2;
	constexpr uintptr_t key3 = hash_str(__TIME__) ^ __LINE__ + 0xe82bae3e4568;
	constexpr uintptr_t key4 = ((__LINE__ << 16) | (__COUNTER__)) * 0x517cc1b7ULL;

	return key1 ^ key2 ^ key3 ^ key4 + key2 + key1 ^ 0xcbf29ce484222325ULL;
}

#define skCrypt(str) skCrypt_key(str, genkey(), genkey())
#define skCrypt_key(str, key1, key2) []() { \
			constexpr static auto crypted = skc::skCrypter \
				<sizeof(str) / sizeof(str[0]), key1, key2, skc::clean_type<decltype(str[0])>>((skc::clean_type<decltype(str[0])>*)str); \
					return crypted; }()

//#define (content) (skCrypt(content).decrypt())

/*________________________________________________________________________________

MIT License

Copyright (c) 2020 skadro

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

________________________________________________________________________________*/
//#define ObfStr(input) (input)