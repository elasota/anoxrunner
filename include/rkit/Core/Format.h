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
	};
}

namespace rkit { namespace priv {
	struct DefaultFormatters
	{
		static void FormatSignedInt(IFormatStringWriter<char> &writer, intmax_t value);
		static void FormatUnsignedInt(IFormatStringWriter<char> &writer, uintmax_t value);
		static void FormatFloat(IFormatStringWriter<char> &writer, float f);
		static void FormatDouble(IFormatStringWriter<char> &writer, double f);
		static void FormatCString(IFormatStringWriter<char> &writer, const char *str);
		static void WFormatCString(IFormatStringWriter<wchar_t> &writer, const wchar_t *str);
	};

	class AsciiToWCharConverter final : public IFormatStringWriter<char>
	{
	public:
		AsciiToWCharConverter() = delete;
		AsciiToWCharConverter(const AsciiToWCharConverter &) = delete;

		explicit AsciiToWCharConverter(IFormatStringWriter<wchar_t> &wcharWriter);

		void WriteChars(const Span<const char> &chars) override;

	private:
		IFormatStringWriter<wchar_t> &m_wcharWriter;
	};

	template<class T, size_t TSize, bool TIsIntegral, bool TIsSigned>
	struct DefaultFormatter
	{
		template<class TChar>
		inline static void Format(IFormatStringWriter<TChar> &writer, const T &value)
		{
			value.FormatValue(writer);
		}
	};

	template<class TSignedInt, size_t TSize>
	struct DefaultFormatter<TSignedInt, TSize, true, true>
	{
		inline static void Format(IFormatStringWriter<char> &writer, TSignedInt value)
		{
			return DefaultFormatters::FormatSignedInt(writer, value);
		}

		inline static void Format(IFormatStringWriter<wchar_t> &writer, TSignedInt value)
		{
			return DefaultFormatters::FormatSignedInt(AsciiToWCharConverter(writer), value);
		}
	};

	template<class TUnsignedInt, size_t TSize>
	struct DefaultFormatter<TUnsignedInt, TSize, true, false>
	{
		static void Format(IFormatStringWriter<char> &writer, TUnsignedInt value)
		{
			DefaultFormatters::FormatSignedInt(writer, value);
		}

		static void Format(IFormatStringWriter<wchar_t> &writer, TUnsignedInt value)
		{
			DefaultFormatters::FormatSignedInt(AsciiToWCharConverter(writer), value);
		}
	};
} }

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
	struct Formatter<const char *>
	{
		inline static void Format(IFormatStringWriter<char> &writer, const char *value)
		{
			priv::DefaultFormatters::FormatCString(writer, value);
		}
	};

	template<>
	struct Formatter<char *>
	{
		inline static void Format(IFormatStringWriter<char> &writer, char *value)
		{
			Formatter<const char *>::Format(writer, value);
		}
	};

	template<>
	struct Formatter<const wchar_t *>
	{
		inline static void Format(IFormatStringWriter<wchar_t> &writer, const wchar_t *value)
		{
			priv::DefaultFormatters::WFormatCString(writer, value);
		}
	};

	template<>
	struct Formatter<wchar_t *>
	{
		inline static void Format(IFormatStringWriter<wchar_t> &writer, wchar_t *value)
		{
			Formatter<const wchar_t *>::Format(writer, value);
		}
	};
}


namespace rkit { namespace priv {
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
} }

#include "Span.h"
#include "UtilitiesDriver.h"

namespace rkit
{
	template<class TChar, class... TParameter>
	typename SizedFormatParameterList<TChar, TypeListSize<typename TypeList<TParameter...> >::kSize> CreateFormatParameterList(const TParameter &... params)
	{
		return SizedFormatParameterList<TChar, TypeListSize<TypeList<TParameter...> >::kSize>(params...);
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
}



namespace rkit { namespace priv {
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

	inline void DefaultFormatters::FormatSignedInt(IFormatStringWriter<char> &writer, intmax_t value)
	{
		GetDrivers().m_utilitiesDriver->FormatSignedInt(writer, value);
	}

	inline void DefaultFormatters::FormatUnsignedInt(IFormatStringWriter<char> &writer, uintmax_t value)
	{
		GetDrivers().m_utilitiesDriver->FormatUnsignedInt(writer, value);
	}

	inline void DefaultFormatters::FormatFloat(IFormatStringWriter<char> &writer, float f)
	{
		GetDrivers().m_utilitiesDriver->FormatFloat(writer, f);
	}

	inline void DefaultFormatters::FormatDouble(IFormatStringWriter<char> &writer, double f)
	{
		GetDrivers().m_utilitiesDriver->FormatDouble(writer, f);
	}

	inline void DefaultFormatters::FormatCString(IFormatStringWriter<char> &writer, const char *str)
	{
		GetDrivers().m_utilitiesDriver->FormatCString(writer, str);
	}

	inline void DefaultFormatters::WFormatCString(IFormatStringWriter<wchar_t> &writer, const wchar_t *str)
	{
		GetDrivers().m_utilitiesDriver->WFormatCString(writer, str);
	}
} }
