#pragma once

#include "TypeList.h"

#include <type_traits>
#include <stdint.h>

namespace rkit
{
	template<class T>
	class Span;

	template<class TChar>
	struct IFormatStringWriter;
}

namespace rkit
{
	template<class TChar>
	struct IFormatStringWriter
	{
		virtual void WriteChars(const Span<const TChar> &chars) = 0;
		virtual CharacterEncoding GetEncoding() const = 0;
	};
}

namespace rkit::priv
{
	template<class T, size_t TSize, bool TIsIntegral, bool TIsSigned>
	struct DefaultFormatter
	{
		template<class TChar>
		static void Format(IFormatStringWriter<TChar> &writer, const T &value);
	};

	template<class TSignedInt, size_t TSize>
	struct DefaultFormatter<TSignedInt, TSize, true, true>
	{
		static void Format(IFormatStringWriter<Utf8Char_t> &writer, TSignedInt value);
	};

	template<class TUnsignedInt, size_t TSize>
	struct DefaultFormatter<TUnsignedInt, TSize, true, false>
	{
		static void Format(IFormatStringWriter<Utf8Char_t> &writer, TUnsignedInt value);
	};
}

namespace rkit
{
	template<class TChar>
	struct FormatParameter
	{
		const void *m_dataPtr;
		void (*m_formatCallback)(IFormatStringWriter<TChar> &writer, const void *dataPtr);
	};

	template<class TChar>
	using FormatParameterList = Span<const FormatParameter<TChar>>;

	template<class TChar, size_t TSize>
	class SizedFormatParameterList
	{
	public:
		template<class... TParameter>
		SizedFormatParameterList(const TParameter &... parameter);

		SizedFormatParameterList(SizedFormatParameterList &&) = default;

		operator FormatParameterList<TChar>() const;

	private:
		SizedFormatParameterList() = delete;
		SizedFormatParameterList(const SizedFormatParameterList &) = delete;

		FormatParameter<TChar> m_formatters[TSize];
	};

	template<class TChar>
	class SizedFormatParameterList<TChar, 0>
	{
	public:
		operator FormatParameterList<TChar>() const;
	};

	template<class TType>
	struct Formatter : public priv::DefaultFormatter<TType, sizeof(TType), std::is_integral<TType>::value, std::is_signed<TType>::value>
	{
	};

	template<>
	struct Formatter<const Utf8Char_t *>
	{
		static void Format(IFormatStringWriter<Utf8Char_t> &writer, const Utf8Char_t *value);
	};

	template<>
	struct Formatter<Utf8Char_t *>
	{
		inline static void Format(IFormatStringWriter<Utf8Char_t> &writer, Utf8Char_t *value)
		{
			Formatter<const Utf8Char_t *>::Format(writer, value);
		}
	};

	template<size_t TArraySize>
	struct Formatter<const Utf8Char_t[TArraySize]>
	{
		inline static void Format(IFormatStringWriter<Utf8Char_t> &writer, const Utf8Char_t(&value)[TArraySize])
		{
			Formatter<const Utf8Char_t *>::Format(writer, value);
		}
	};

	template<size_t TArraySize>
	struct Formatter<Utf8Char_t[TArraySize]>
	{
		inline static void Format(IFormatStringWriter<char> &writer, const Utf8Char_t(&value)[TArraySize])
		{
			Formatter<const Utf8Char_t *>::Format(writer, value);
		}
	};
}


namespace rkit::priv
{
	template<class TChar, class TType>
	struct FormatterCreator
	{
		typedef void (*RefCallback_t)(IFormatStringWriter<TChar> &writer, const TType &value);
		typedef void (*ValueCallback_t)(IFormatStringWriter<TChar> &writer, TType value);

		static FormatParameter<TChar> Create(const TType &type, RefCallback_t);
		static FormatParameter<TChar> Create(const TType &type, ValueCallback_t);

	private:
		static void ThunkRefFormatter(IFormatStringWriter<TChar> &writer, const void *dataPtr);
		static void ThunkValueFormatter(IFormatStringWriter<TChar> &writer, const void *dataPtr);
	};
}

#include "CoreLib.h"
#include "Span.h"
#include "Drivers.h"
#include "UtilitiesDriver.h"

namespace rkit
{
	inline void Formatter<const Utf8Char_t *>::Format(IFormatStringWriter<Utf8Char_t> &writer, const Utf8Char_t *value)
	{
		::rkit::text::formatters::FormatUtf8String(writer, value);
	}

	template<class TChar, class... TParameter>
	typename SizedFormatParameterList<TChar, TypeListSize<typename TypeList<TParameter...> >::kValue> CreateFormatParameterList(const TParameter &... params)
	{
		return SizedFormatParameterList<TChar, TypeListSize<TypeList<TParameter...> >::kValue>(params...);
	}

	template<class TChar, size_t TSize>
	template<class... TParameter>
	inline SizedFormatParameterList<TChar, TSize>::SizedFormatParameterList(const TParameter &... parameter)
		: m_formatters { priv::FormatterCreator<TChar, TParameter>::Create(parameter, Formatter<TParameter>::Format)... }
	{
	}


	template<class TChar>
	SizedFormatParameterList<TChar, 0>::operator FormatParameterList<TChar>() const
	{
		return FormatParameterList<TChar>();
	}

	template<class TChar, size_t TSize>
	inline SizedFormatParameterList<TChar, TSize>::operator FormatParameterList<TChar>() const
	{
		return FormatParameterList<TChar>(m_formatters, TSize);
	}

	inline void FormatString(IFormatStringWriter<Utf8Char_t> &writer, const StringSliceView &fmt, const FormatParameterList<Utf8Char_t> &paramList)
	{
		::rkit::text::FormatUtf8String(writer, fmt.GetChars(), fmt.Length(), paramList.Ptr(), paramList.Count());
	}
}

namespace rkit::priv
{
	template<class TChar, class TType>
	FormatParameter<TChar> FormatterCreator<TChar, TType>::Create(const TType &type, RefCallback_t)
	{
		return FormatParameter<TChar> { &type, ThunkRefFormatter };
	}

	template<class TChar, class TType>
	FormatParameter<TChar> FormatterCreator<TChar, TType>::Create(const TType &type, ValueCallback_t)
	{
		return FormatParameter<TChar> { &type, ThunkValueFormatter };
	}

	template<class TChar, class TType>
	inline void FormatterCreator<TChar, TType>::ThunkRefFormatter(IFormatStringWriter<TChar> &writer, const void *dataPtr)
	{
		const RefCallback_t cb = Formatter<TType>::Format;
		cb(writer, *static_cast<const TType *>(dataPtr));
	}

	template<class TChar, class TType>
	inline void FormatterCreator<TChar, TType>::ThunkValueFormatter(IFormatStringWriter<TChar> &writer, const void *dataPtr)
	{
		const ValueCallback_t cb = Formatter<TType>::Format;
		cb(writer, *static_cast<const TType *>(dataPtr));
	}


	template<class T, size_t TSize, bool TIsIntegral, bool TIsSigned>
	template<class TChar>
	void DefaultFormatter<T, TSize, TIsIntegral, TIsSigned>::Format(IFormatStringWriter<TChar> &writer, const T &value)
	{
		return value.FormatValue(writer);
	}

	template<class TSignedInt, size_t TSize>
	void DefaultFormatter<TSignedInt, TSize, true, true>::Format(IFormatStringWriter<Utf8Char_t> &writer, TSignedInt value)
	{
		return ::rkit::text::formatters::FormatSignedInt(writer, value);
	}

	template<class TUnsignedInt, size_t TSize>
	void DefaultFormatter<TUnsignedInt, TSize, true, false>::Format(IFormatStringWriter<Utf8Char_t> &writer, TUnsignedInt value)
	{
		return ::rkit::text::formatters::FormatUnsignedInt(writer, value);
	}
}
