#pragma once

#include "CoreDefs.h"
#include "UtilitiesDriver.h"

#include "PathProto.h"
#include "String.h"

namespace rkit
{
	template<class T>
	class Span;

	template<bool TIsAbsolute, class TPathTraits>
	class BasePath;

	enum class PathValidationResult
	{
		kInvalid,
		kConvertible,
		kValid,
	};

	template<uint32_t TTraitFlags>
	struct DefaultPathTraits
	{
		// Type of a character
		typedef Utf8Char_t Char_t;

		// Canonical delimiter
		static const Char_t kDefaultDelimiter = u8'/';

		// Encoding
		static const CharacterEncoding kEncoding = CharacterEncoding::kUTF8;

		// Returns true if the specified character is a delimiter
		static bool IsDelimiter(Char_t ch);

		// Checks if a component is valid and canonical
		static PathValidationResult ValidateComponent(const BaseStringSliceView<Char_t, kEncoding> &span, bool isAbsolute, bool isFirst);

		// Converts a component to a canonical component
		static void MakeComponentValid(const Span<Char_t> &component, bool isAbsolute, bool isFirst);
	};

#if RKIT_PLATFORM == RKIT_PLATFORM_WIN32
	struct OSPathTraits : public DefaultPathTraits<PathTraitFlags::kIsCaseInsensitive>
	{
		typedef OSPathChar_t Char_t;
		static const Char_t kDefaultDelimiter = '\\';
		static const CharacterEncoding kEncoding = CharacterEncoding::kUTF16;

		static bool IsDelimiter(Char_t ch);
		static PathValidationResult ValidateComponent(const BaseStringSliceView<Char_t, kEncoding> &span, bool isAbsolute, bool isFirst);
		static void MakeComponentValid(const Span<Char_t> &component, bool isAbsolute, bool isFirst);
	};

	#define RKIT_OS_PATH_LITERAL(value) u ## value
#else
#error "Unknown platform FS traits"
#endif

	template<bool TIsAbsolute, class TPathTraits>
	class BasePathIterator
	{
	public:
		typedef typename TPathTraits::Char_t Char_t;
		static const Char_t kDefaultDelimiter = TPathTraits::kDefaultDelimiter;
		typedef BaseStringSliceView<Char_t, TPathTraits::kEncoding> ComponentView_t;

		BasePathIterator() = delete;

		BasePathIterator(const BasePath<TIsAbsolute, TPathTraits> &path, size_t pos);

		BasePathIterator<TIsAbsolute, TPathTraits> &operator++();
		BasePathIterator<TIsAbsolute, TPathTraits> operator++(int);

		BasePathIterator<TIsAbsolute, TPathTraits> &operator--();
		BasePathIterator<TIsAbsolute, TPathTraits> operator--(int);

		BaseStringSliceView<typename TPathTraits::Char_t, TPathTraits::kEncoding> operator*() const;

		bool operator==(const BasePathIterator<TIsAbsolute, TPathTraits> &other) const;
		bool operator!=(const BasePathIterator<TIsAbsolute, TPathTraits> &other) const;

	private:
		void UpdateEndPos();

		const BasePath<TIsAbsolute, TPathTraits> &m_path;

		size_t m_strPos;
		size_t m_endPos;
	};

	template<bool TIsAbsolute, class TPathTraits>
	class BasePathSliceView
	{
	public:
		typedef typename TPathTraits::Char_t Char_t;
		static const CharacterEncoding kEncoding = TPathTraits::kEncoding;
		static const Char_t kDefaultDelimiter = TPathTraits::kDefaultDelimiter;
		typedef BaseStringSliceView<Char_t, TPathTraits::kEncoding> ComponentView_t;
		typedef BaseStringSliceView<Char_t, TPathTraits::kEncoding> LastComponentView_t;

		BasePathSliceView();
		explicit BasePathSliceView(const BaseStringSliceView<Char_t, TPathTraits::kEncoding> &slice);

		template<size_t TLength>
		BasePathSliceView(const Char_t(&charsArray)[TLength]);

		const Char_t *GetChars() const;
		size_t Length() const;

		ComponentView_t operator[](size_t index) const;

		BasePathSliceView<TIsAbsolute, TPathTraits> AbsSlice(size_t numComponents) const;
		BasePathSliceView<false, TPathTraits> RelSlice(size_t firstComponent, size_t numComponents) const;

		size_t NumComponents() const;
		LastComponentView_t LastComponent() const;

		BaseStringSliceView<Char_t, TPathTraits::kEncoding> ToStringSliceView() const;

		bool operator==(const BasePathSliceView<TIsAbsolute, TPathTraits> &path) const;
		bool operator!=(const BasePathSliceView<TIsAbsolute, TPathTraits> &path) const;

		bool operator<(const BasePathSliceView<TIsAbsolute, TPathTraits> &path) const;
		bool operator>(const BasePathSliceView<TIsAbsolute, TPathTraits> &path) const;

		bool operator<=(const BasePathSliceView<TIsAbsolute, TPathTraits> &path) const;
		bool operator>=(const BasePathSliceView<TIsAbsolute, TPathTraits> &path) const;

	private:
		BaseStringSliceView<Char_t, kEncoding> m_view;
	};

	template<bool TIsAbsolute, class TPathTraits>
	class BasePathView : public BasePathSliceView<TIsAbsolute, TPathTraits>
	{
	public:
		typedef typename TPathTraits::Char_t Char_t;
		static const Char_t kDefaultDelimiter = TPathTraits::kDefaultDelimiter;
		static const CharacterEncoding kEncoding = TPathTraits::kEncoding;
		typedef BaseStringSliceView<Char_t, TPathTraits::kEncoding> ComponentView_t;
		typedef BaseStringView<Char_t, TPathTraits::kEncoding> LastComponentView_t;

		BasePathView();
		explicit BasePathView(const BaseStringView<Char_t, TPathTraits::kEncoding> &slice);

		template<size_t TLength>
		BasePathView(const Char_t(&charsArray)[TLength]);

		LastComponentView_t LastComponent() const;

		BaseStringView<Char_t, TPathTraits::kEncoding> ToStringView() const;
	};

	template<bool TIsAbsolute, class TPathTraits>
	class BasePath
	{
	public:
		friend class BasePathIterator<TIsAbsolute, TPathTraits>;

		typedef typename TPathTraits::Char_t Char_t;
		static const CharacterEncoding kEncoding = TPathTraits::kEncoding;
		static const Char_t kDefaultDelimiter = TPathTraits::kDefaultDelimiter;
		typedef BaseStringSliceView<Char_t, kEncoding> ComponentView_t;
		typedef BaseString<Char_t, kEncoding> Component_t;
		typedef BasePathView<TIsAbsolute, TPathTraits> View_t;
		typedef BaseString<Char_t, kEncoding> String_t;

		BasePath();
		BasePath(const BasePath<TIsAbsolute, TPathTraits> &other);
		BasePath(BasePath<TIsAbsolute, TPathTraits> &&other) noexcept;

		BasePath<TIsAbsolute, TPathTraits> &operator=(const BasePath<TIsAbsolute, TPathTraits> &other);
		BasePath<TIsAbsolute, TPathTraits> &operator=(BasePath<TIsAbsolute, TPathTraits> &&other) noexcept;

		bool operator==(const View_t &other) const;
		bool operator!=(const View_t &other) const;

		BasePathIterator<TIsAbsolute, TPathTraits> begin() const;
		BasePathIterator<TIsAbsolute, TPathTraits> end() const;

		Result AppendComponent(const BaseStringSliceView<Char_t, TPathTraits::kEncoding> &str);
		Result Append(const BasePathView<false, TPathTraits> &str);

		ComponentView_t operator[](size_t index) const;
		BasePathSliceView<TIsAbsolute, TPathTraits> AbsSlice(size_t numComponents) const;
		BasePathSliceView<false, TPathTraits> RelSlice(size_t firstComponent, size_t numComponents) const;
		size_t NumComponents() const;

		const BaseString<Char_t, TPathTraits::kEncoding> &ToString() const;

		Result Set(const ComponentView_t &str);
		Result Set(const BasePathSliceView<TIsAbsolute, TPathTraits> &path);

		template<class TOtherChar, CharacterEncoding TOtherPathEncoding>
		Result SetFromEncodedString(const BaseStringSliceView<TOtherChar, TOtherPathEncoding> &str);

		Result SetFromUTF8(const StringSliceView &str);

		template<class TOtherPathTraits>
		Result ConvertFrom(const BasePathView<TIsAbsolute, TOtherPathTraits> &path);

		template<class TOtherPathTraits>
		Result ConvertFrom(const BasePath<TIsAbsolute, TOtherPathTraits> &path);

		operator View_t() const;

		const Char_t *CStr() const;
		size_t Length() const;

		static PathValidationResult Validate(const BaseStringSliceView<Char_t, kEncoding> &str);

		bool operator<(const BasePath<TIsAbsolute, TPathTraits> &other);
		bool operator>(const BasePath<TIsAbsolute, TPathTraits> &other);
		bool operator<=(const BasePath<TIsAbsolute, TPathTraits> &other);
		bool operator>=(const BasePath<TIsAbsolute, TPathTraits> &other);
		bool operator==(const BasePath<TIsAbsolute, TPathTraits> &other);
		bool operator!=(const BasePath<TIsAbsolute, TPathTraits> &other);

	private:
		Result SetConvert(const BaseStringSliceView<Char_t, kEncoding> &str);

		BaseString<Char_t, kEncoding> m_path;
	};
}

#include "Hasher.h"

namespace rkit
{
	template<bool TIsAbsolute, class TPathTraits>
	struct Hasher<BasePath<TIsAbsolute, TPathTraits>> : public DefaultSpanHasher<BasePath<TIsAbsolute, TPathTraits>>
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const BasePath<TIsAbsolute, TPathTraits> &value);
	};

	template<bool TIsAbsolute, class TPathTraits>
	struct Hasher<BasePathView<TIsAbsolute, TPathTraits>> : public DefaultSpanHasher<BasePathView<TIsAbsolute, TPathTraits>>
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const BasePathView<TIsAbsolute, TPathTraits> &value);
	};
}

#include "UtilitiesDriver.h"
#include "Drivers.h"

namespace rkit
{
	template<uint32_t TTraitFlags>
	inline bool DefaultPathTraits<TTraitFlags>::IsDelimiter(Char_t ch)
	{
		return ch == '/';
	}

	template<uint32_t TTraitFlags>
	inline PathValidationResult DefaultPathTraits<TTraitFlags>::ValidateComponent(const BaseStringSliceView<Char_t, kEncoding> &span, bool isAbsolute, bool isFirst)
	{
		bool requiresFixup = false;

		if (span.Length() == 1 && span[0] == '.')
			return ((TTraitFlags & PathTraitFlags::kAllowCWD) != 0 && isFirst) ? PathValidationResult::kValid : PathValidationResult::kInvalid;

		if (span.Length() == 2 && span[0] == '.' && span[1] == '.')
			return ((TTraitFlags & PathTraitFlags::kAllowParent) != 0) ? PathValidationResult::kValid : PathValidationResult::kInvalid;

		if (TTraitFlags & PathTraitFlags::kIsCaseInsensitive)
		{
			for (Char_t ch : span)
			{
				if (ch >= 'A' && ch <= 'Z')
				{
					requiresFixup = true;
					break;
				}
			}
		}

		if (!GetDrivers().m_utilitiesDriver->DefaultIsPathComponentValid(span, isFirst, (TTraitFlags & PathTraitFlags::kAllowWildcards) != 0))
			return PathValidationResult::kInvalid;

		return requiresFixup ? PathValidationResult::kConvertible : PathValidationResult::kValid;
	}

	template<uint32_t TTraitFlags>
	inline void DefaultPathTraits<TTraitFlags>::MakeComponentValid(const Span<Char_t> &component, bool isAbsolute, bool isFirst)
	{
		if (TTraitFlags & PathTraitFlags::kIsCaseInsensitive)
		{
			for (Char_t &ch : component)
			{
				if (ch >= 'A' && ch <= 'Z')
					ch = (ch - 'A') + 'a';
			}
		}
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathIterator<TIsAbsolute, TPathTraits>::BasePathIterator(const BasePath<TIsAbsolute, TPathTraits> &path, size_t pos)
		: m_path(path)
		, m_strPos(pos)
		, m_endPos(0)
	{
		this->UpdateEndPos();
	}

	template<bool TIsAbsolute, class TPathTraits>
	void BasePathIterator<TIsAbsolute, TPathTraits>::UpdateEndPos()
	{
		const BaseString<Char_t, TPathTraits::kEncoding> &str = m_path.ToString();
		const size_t length = str.Length();
		const Char_t *chars = str.CStr();

		size_t strPos = m_strPos;

		while (strPos < length)
		{
			if (chars[strPos++] == kDefaultDelimiter)
				break;
		}

		m_endPos = strPos;
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathIterator<TIsAbsolute, TPathTraits> &BasePathIterator<TIsAbsolute, TPathTraits>::operator++()
	{
		const BaseString<Char_t, TPathTraits::kEncoding> &str = m_path.ToString();

		RKIT_ASSERT(m_strPos < str.Length());

		m_strPos = m_endPos + 1;
		this->UpdateEndPos();

		return *this;
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathIterator<TIsAbsolute, TPathTraits> BasePathIterator<TIsAbsolute, TPathTraits>::operator++(int)
	{
		BasePathIterator<TIsAbsolute, TPathTraits> result(*this);
		++(*this);
		return result;
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathIterator<TIsAbsolute, TPathTraits> &BasePathIterator<TIsAbsolute, TPathTraits>::operator--()
	{
		const BaseString<Char_t, TPathTraits::kEncoding> &str = m_path.ToString();
		const size_t length = str.Length();
		const Char_t *chars = str.CStr();

		size_t strPos = m_strPos;

		while (strPos > 0)
		{
			if (chars[strPos - 1] == kDefaultDelimiter)
				break;

			--strPos;
		}

		m_strPos = strPos;

		return *this;
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathIterator<TIsAbsolute, TPathTraits> BasePathIterator<TIsAbsolute, TPathTraits>::operator--(int)
	{
		BasePathIterator<TIsAbsolute, TPathTraits> result(*this);
		--(*this);
		return result;
	}

	template<bool TIsAbsolute, class TPathTraits>
	BaseStringSliceView<typename TPathTraits::Char_t, TPathTraits::kEncoding> BasePathIterator<TIsAbsolute, TPathTraits>::operator*() const
	{
		const BaseString<Char_t, TPathTraits::kEncoding> &str = m_path.ToString();

		return str.SubString(m_strPos, m_endPos - m_strPos);
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePathIterator<TIsAbsolute, TPathTraits>::operator==(const BasePathIterator<TIsAbsolute, TPathTraits> &other) const
	{
		return (&m_path == &other.m_path) && (m_strPos == other.m_strPos);
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePathIterator<TIsAbsolute, TPathTraits>::operator!=(const BasePathIterator<TIsAbsolute, TPathTraits> &other) const
	{
		return !((*this) == other);
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathSliceView<TIsAbsolute, TPathTraits>::BasePathSliceView()
	{
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathSliceView<TIsAbsolute, TPathTraits>::BasePathSliceView(const BaseStringSliceView<Char_t, TPathTraits::kEncoding> &view)
		: m_view(view)
	{
		RKIT_ASSERT((BasePath<TIsAbsolute, TPathTraits>::Validate(m_view) == PathValidationResult::kValid));
	}

	template<bool TIsAbsolute, class TPathTraits>
	template<size_t TLength>
	BasePathSliceView<TIsAbsolute, TPathTraits>::BasePathSliceView(const Char_t(&charsArray)[TLength])
		: m_view(charsArray)
	{
		RKIT_ASSERT((BasePath<TIsAbsolute, TPathTraits>::Validate(m_view) == PathValidationResult::kValid));
	}

	template<bool TIsAbsolute, class TPathTraits>
	const typename BasePathSliceView<TIsAbsolute, TPathTraits>::Char_t *BasePathSliceView<TIsAbsolute, TPathTraits>::GetChars() const
	{
		return m_view.GetChars();
	}

	template<bool TIsAbsolute, class TPathTraits>
	size_t BasePathSliceView<TIsAbsolute, TPathTraits>::Length() const
	{
		return m_view.Length();
	}

	template<bool TIsAbsolute, class TPathTraits>
	typename BasePathSliceView<TIsAbsolute, TPathTraits>::ComponentView_t BasePathSliceView<TIsAbsolute, TPathTraits>::operator[](size_t index) const
	{
		size_t scanPos = 0;
		while (index > 0)
		{
			if (scanPos == m_view.Length())
			{
				RKIT_ASSERT(false);
				break;
			}

			if (m_view[scanPos++] == kDefaultDelimiter)
				index--;
		}

		const size_t startPos = scanPos;
		while (scanPos < m_view.Length() && m_view[scanPos] != kDefaultDelimiter)
			scanPos++;

		return m_view.SubString(startPos, scanPos - startPos);
	}


	template<bool TIsAbsolute, class TPathTraits>
	BasePathSliceView<TIsAbsolute, TPathTraits> BasePathSliceView<TIsAbsolute, TPathTraits>::AbsSlice(size_t numComponents) const
	{
		if (numComponents == 0)
			return BasePathSliceView<TIsAbsolute, TPathTraits>();

		size_t scanPos = 0;
		for (;;)
		{
			if (scanPos == m_view.Length())
			{
				RKIT_ASSERT(numComponents == 1);
				break;
			}

			if (m_view[scanPos] == kDefaultDelimiter)
			{
				numComponents--;
				if (numComponents == 0)
					break;
			}

			scanPos++;
		}

		BaseStringSliceView<Char_t, kEncoding> slice = m_view.SubString(0, scanPos);
		return BasePathSliceView<TIsAbsolute, TPathTraits>(slice);
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathSliceView<false, TPathTraits> BasePathSliceView<TIsAbsolute, TPathTraits>::RelSlice(size_t firstComponent, size_t numComponents) const
	{
		if (numComponents == 0)
			return BasePathSliceView<TIsAbsolute, TPathTraits>();

		size_t scanPos = 0;
		while (firstComponent > 0)
		{
			if (scanPos == m_view.Length())
			{
				RKIT_ASSERT(false);
				break;
			}

			if (m_view[scanPos++] == kDefaultDelimiter)
				firstComponent--;
		}

		const size_t startPos = scanPos;

		for (;;)
		{
			if (scanPos == m_view.Length())
			{
				RKIT_ASSERT(numComponents == 1);
				break;
			}

			if (m_view[scanPos] == kDefaultDelimiter)
			{
				numComponents--;
				if (numComponents == 0)
					break;
			}

			scanPos++;
		}

		BaseStringSliceView<Char_t, TPathTraits::kEncoding> slice = m_view.SubString(startPos, scanPos - startPos);
		return BasePathSliceView<false, TPathTraits>(slice);
	}

	template<bool TIsAbsolute, class TPathTraits>
	size_t BasePathSliceView<TIsAbsolute, TPathTraits>::NumComponents() const
	{
		if (m_view.Length() == 0)
			return 0;

		size_t numComponents = 1;

		for (Char_t ch : m_view)
		{
			if (ch == kDefaultDelimiter)
				numComponents++;
		}

		return numComponents;
	}

	template<bool TIsAbsolute, class TPathTraits>
	typename BasePathSliceView<TIsAbsolute, TPathTraits>::LastComponentView_t BasePathSliceView<TIsAbsolute, TPathTraits>::LastComponent() const
	{
		if (m_view.Length() == 0)
			return LastComponentView_t();

		size_t startPos = m_view.Length() - 1;
		while (startPos > 0)
		{
			if (m_view[startPos - 1] == kDefaultDelimiter)
				break;

			startPos--;
		}

		return LastComponentView_t(m_view.SubString(startPos));
	}

	template<bool TIsAbsolute, class TPathTraits>
	BaseStringSliceView<typename TPathTraits::Char_t, TPathTraits::kEncoding> BasePathSliceView<TIsAbsolute, TPathTraits>::ToStringSliceView() const
	{
		return m_view;
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePathSliceView<TIsAbsolute, TPathTraits>::operator==(const BasePathSliceView<TIsAbsolute, TPathTraits> &other) const
	{
		return m_view == other.m_view;
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePathSliceView<TIsAbsolute, TPathTraits>::operator!=(const BasePathSliceView<TIsAbsolute, TPathTraits> &other) const
	{
		return !((*this) == other);
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePathSliceView<TIsAbsolute, TPathTraits>::operator<(const BasePathSliceView<TIsAbsolute, TPathTraits> &other) const
	{
		return m_view < other.m_view;
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePathSliceView<TIsAbsolute, TPathTraits>::operator>(const BasePathSliceView<TIsAbsolute, TPathTraits> &other) const
	{
		return other < (*this);
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePathSliceView<TIsAbsolute, TPathTraits>::operator<=(const BasePathSliceView<TIsAbsolute, TPathTraits> &other) const
	{
		return !(other < (*this));
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePathSliceView<TIsAbsolute, TPathTraits>::operator>=(const BasePathSliceView<TIsAbsolute, TPathTraits> &other) const
	{
		return !((*this) < other);
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathView<TIsAbsolute, TPathTraits>::BasePathView()
	{
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathView<TIsAbsolute, TPathTraits>::BasePathView(const BaseStringView<Char_t, TPathTraits::kEncoding> &slice)
		: BasePathSliceView<TIsAbsolute, TPathTraits>(slice)
	{
	}

	template<bool TIsAbsolute, class TPathTraits>
	template<size_t TLength>
	BasePathView<TIsAbsolute, TPathTraits>::BasePathView(const Char_t(&charsArray)[TLength])
		: BasePathSliceView<TIsAbsolute, TPathTraits>(charsArray)
	{
	}

	template<bool TIsAbsolute, class TPathTraits>
	typename BasePathView<TIsAbsolute, TPathTraits>::LastComponentView_t BasePathView<TIsAbsolute, TPathTraits>::LastComponent() const
	{
		typename BasePathSliceView<TIsAbsolute, TPathTraits>::LastComponentView_t lastComponent = BasePathSliceView<TIsAbsolute, TPathTraits>::LastComponent();

		return LastComponentView_t(lastComponent.GetChars(), lastComponent.Length());
	}

	template<bool TIsAbsolute, class TPathTraits>
	BaseStringView<typename BasePathView<TIsAbsolute, TPathTraits>::Char_t, TPathTraits::kEncoding> BasePathView<TIsAbsolute, TPathTraits>::ToStringView() const
	{
		const BaseStringSliceView<Char_t, kEncoding> stringSlice = this->ToStringSliceView();
		return BaseStringView<Char_t, kEncoding>(stringSlice.GetChars(), stringSlice.Length());
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePath<TIsAbsolute, TPathTraits>::BasePath()
	{
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePath<TIsAbsolute, TPathTraits>::BasePath(const BasePath<TIsAbsolute, TPathTraits> &other)
		: m_path(other.m_path)
	{
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePath<TIsAbsolute, TPathTraits>::BasePath(BasePath<TIsAbsolute, TPathTraits> &&other) noexcept
		: m_path(std::move(other.m_path))
	{
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePath<TIsAbsolute, TPathTraits> &BasePath<TIsAbsolute, TPathTraits>::operator=(const BasePath<TIsAbsolute, TPathTraits> &other)
	{
		m_path = other.m_path;
		return *this;
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePath<TIsAbsolute, TPathTraits> &BasePath<TIsAbsolute, TPathTraits>::operator=(BasePath<TIsAbsolute, TPathTraits> &&other) noexcept
	{
		m_path = std::move(other.m_path);
		return *this;
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePath<TIsAbsolute, TPathTraits>::operator==(const View_t &other) const
	{
		return ToString() == other.ToStringView();
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePath<TIsAbsolute, TPathTraits>::operator!=(const View_t &other) const
	{
		return !((*this) == other);
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathIterator<TIsAbsolute, TPathTraits> BasePath<TIsAbsolute, TPathTraits>::begin() const
	{
		return BasePathIterator<TIsAbsolute, TPathTraits>(*this, 0);
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathIterator<TIsAbsolute, TPathTraits> BasePath<TIsAbsolute, TPathTraits>::end() const
	{
		return BasePathIterator<TIsAbsolute, TPathTraits>(*this, m_path.Length());
	}

	template<bool TIsAbsolute, class TPathTraits>
	Result BasePath<TIsAbsolute, TPathTraits>::AppendComponent(const BaseStringSliceView<Char_t, TPathTraits::kEncoding> &str)
	{
		if (m_path.Length() == 0)
			return m_path.Set(str);

		if (str.Length() == 0)
			RKIT_RETURN_OK;

		PathValidationResult validationResult = TPathTraits::ValidateComponent(str, TIsAbsolute, false);

		if (validationResult == PathValidationResult::kInvalid)
			RKIT_THROW(ResultCode::kInvalidParameter);

		size_t totalChars = str.Length() + 1;
		RKIT_CHECK(SafeAdd<size_t>(totalChars, m_path.Length(), totalChars));

		BaseStringConstructionBuffer<Char_t> scBuf;
		RKIT_CHECK(scBuf.Allocate(totalChars));

		Span<Char_t> chars = scBuf.GetSpan();
		CopySpanNonOverlapping(chars.SubSpan(0, m_path.Length()), m_path.ToSpan());
		chars[m_path.Length()] = kDefaultDelimiter;

		Span<Char_t> newComponentSpan = chars.SubSpan(m_path.Length() + 1, chars.Count() - m_path.Length() - 1);

		CopySpanNonOverlapping(newComponentSpan, str.ToSpan());
		if (validationResult == PathValidationResult::kConvertible)
		{
			TPathTraits::MakeComponentValid(newComponentSpan, TIsAbsolute, false);
		}

		m_path.Clear();
		m_path = BaseString<Char_t, kEncoding>(std::move(scBuf));

		RKIT_RETURN_OK;
	}

	template<bool TIsAbsolute, class TPathTraits>
	Result BasePath<TIsAbsolute, TPathTraits>::Append(const BasePathView<false, TPathTraits> &str)
	{
		if (m_path.Length() == 0)
		{
			if (TIsAbsolute)
				RKIT_THROW(ResultCode::kInvalidPath);
			else
				return m_path.Set(str.ToStringView());
		}

		if (str.Length() == 0)
			RKIT_RETURN_OK;

		size_t totalChars = str.Length() + 1;
		RKIT_CHECK(SafeAdd<size_t>(totalChars, m_path.Length(), totalChars));

		BaseStringConstructionBuffer<Char_t> scBuf;
		RKIT_CHECK(scBuf.Allocate(totalChars));

		Span<Char_t> chars = scBuf.GetSpan();
		CopySpanNonOverlapping(chars.SubSpan(0, m_path.Length()), m_path.ToSpan());
		chars[m_path.Length()] = kDefaultDelimiter;
		CopySpanNonOverlapping(chars.SubSpan(m_path.Length() + 1, chars.Count() - m_path.Length() - 1), str.ToStringView().ToSpan());

		m_path.Clear();
		m_path = BaseString<Char_t, kEncoding>(std::move(scBuf));

		RKIT_RETURN_OK;
	}

	template<bool TIsAbsolute, class TPathTraits>
	typename BasePath<TIsAbsolute, TPathTraits>::ComponentView_t BasePath<TIsAbsolute, TPathTraits>::operator[](size_t index) const
	{
		return BasePathView<TIsAbsolute, TPathTraits>(m_path)[index];
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathSliceView<TIsAbsolute, TPathTraits> BasePath<TIsAbsolute, TPathTraits>::AbsSlice(size_t numComponents) const
	{
		BasePathView<TIsAbsolute, TPathTraits> view = *this;
		return view.AbsSlice(numComponents);
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePathSliceView<false, TPathTraits> BasePath<TIsAbsolute, TPathTraits>::RelSlice(size_t firstComponent, size_t numComponents) const
	{
		BasePathView<TIsAbsolute, TPathTraits> view = *this;
		return view.RelSlice(firstComponent, numComponents);
	}

	template<bool TIsAbsolute, class TPathTraits>
	size_t BasePath<TIsAbsolute, TPathTraits>::NumComponents() const
	{
		return BasePathView<TIsAbsolute, TPathTraits>(m_path).NumComponents();
	}

	template<bool TIsAbsolute, class TPathTraits>
	const BaseString<typename BasePath<TIsAbsolute, TPathTraits>::Char_t, TPathTraits::kEncoding> &BasePath<TIsAbsolute, TPathTraits>::ToString() const
	{
		return m_path;
	}

	template<bool TIsAbsolute, class TPathTraits>
	Result BasePath<TIsAbsolute, TPathTraits>::Set(const ComponentView_t &str)
	{
		switch (Validate(str))
		{
		case PathValidationResult::kValid:
			return m_path.Set(str);
		case PathValidationResult::kConvertible:
			return SetConvert(str);
		default:
			RKIT_THROW(ResultCode::kInvalidParameter);
		}
	}

	template<bool TIsAbsolute, class TPathTraits>
	Result BasePath<TIsAbsolute, TPathTraits>::Set(const BasePathSliceView<TIsAbsolute, TPathTraits> &view)
	{
		return m_path.Set(view.ToStringSliceView());
	}

	template<bool TIsAbsolute, class TPathTraits>
	template<class TOtherChar, CharacterEncoding TOtherPathEncoding>
	Result BasePath<TIsAbsolute, TPathTraits>::SetFromEncodedString(const BaseStringSliceView<TOtherChar, TOtherPathEncoding> &str)
	{
		if (str.Length() == 0)
		{
			m_path.Clear();
			RKIT_RETURN_OK;
		}

		BaseString<Char_t, TPathTraits::kEncoding> newStr;
		RKIT_CHECK(newStr.ConvertFrom(str));

		return Set(newStr);
	}

	template<bool TIsAbsolute, class TPathTraits>
	Result BasePath<TIsAbsolute, TPathTraits>::SetFromUTF8(const StringSliceView &str)
	{
		return SetFromEncodedString<Utf8Char_t, CharacterEncoding::kUTF8>(str);
	}

	template<bool TIsAbsolute, class TPathTraits>
	template<class TOtherPathTraits>
	Result BasePath<TIsAbsolute, TPathTraits>::ConvertFrom(const BasePath<TIsAbsolute, TOtherPathTraits> &path)
	{
		return this->ConvertFrom(static_cast<BasePathView<TIsAbsolute, TOtherPathTraits>>(path));
	}

	template<bool TIsAbsolute, class TPathTraits>
	template<class TOtherPathTraits>
	Result BasePath<TIsAbsolute, TPathTraits>::ConvertFrom(const BasePathView<TIsAbsolute, TOtherPathTraits> &path)
	{
		if (path.Length() == 0)
		{
			m_path.Clear();
			RKIT_RETURN_OK;
		}

		const CharacterEncoding kOtherEncoding = TOtherPathTraits::kEncoding;
		typedef typename TOtherPathTraits::Char_t OtherChar_t;

		const BaseStringView<OtherChar_t, kOtherEncoding> &otherStr = path.ToStringView();
		const ConstSpan<OtherChar_t> otherCharsSpan = otherStr.ToSpan();

		size_t newSize = 0;

		{
			size_t startPos = 0;

			for (size_t scanPos = 0; scanPos <= otherCharsSpan.Count(); scanPos++)
			{
				if (scanPos == otherCharsSpan.Count() || TOtherPathTraits::IsDelimiter(otherCharsSpan[scanPos]))
				{
					const size_t endPos = scanPos;

					const ConstSpan<OtherChar_t> chunk = otherCharsSpan.SubSpan(startPos, scanPos - startPos);

					size_t convertedChunkSize = 0;

					if (chunk.Count() > 0)
					{
						const size_t amountConsumed = text::ConvertText(nullptr, TPathTraits::kEncoding, std::numeric_limits<size_t>::max(), convertedChunkSize,
							chunk.Ptr(), kOtherEncoding, chunk.Count(), text::UnknownCharBehavior::kFail, 0);

						if (!amountConsumed)
							RKIT_THROW(ResultCode::kInvalidUnicode);
					}

					newSize = newSize + convertedChunkSize + 1;

					startPos = scanPos + 1;
				}
			}

			// Remove the last implicit delimiter
			newSize--;
		}

		BaseStringConstructionBuffer<Char_t> scBuf;

		RKIT_CHECK(scBuf.Allocate(newSize));

		Span<Char_t> outSpan = scBuf.GetSpan();

		{
			size_t startPos = 0;
			size_t emitPos = 0;

			for (size_t scanPos = 0; scanPos <= otherCharsSpan.Count(); scanPos++)
			{
				if (scanPos == otherCharsSpan.Count() || TOtherPathTraits::IsDelimiter(otherCharsSpan[scanPos]))
				{
					if (emitPos > 0)
						outSpan[emitPos++] = kDefaultDelimiter;

					const size_t endPos = scanPos;

					const ConstSpan<OtherChar_t> chunk = otherCharsSpan.SubSpan(startPos, scanPos - startPos);
					const Span<Char_t> outChunkSpan = outSpan.SubSpan(emitPos, outSpan.Count() - emitPos);

					size_t convertedChunkSize = 0;

					if (chunk.Count() > 0)
					{
						const size_t amountConsumed = text::ConvertText(outChunkSpan.Ptr(), TPathTraits::kEncoding, outChunkSpan.Count(), convertedChunkSize,
							chunk.Ptr(), kOtherEncoding, chunk.Count(), text::UnknownCharBehavior::kFail, 0);

						if (!amountConsumed)
							RKIT_THROW(ResultCode::kInvalidUnicode);
					}

					const bool isFirst = (emitPos == 0);
					const bool isLast = (scanPos == otherCharsSpan.Count());

					PathValidationResult validationResult = TPathTraits::ValidateComponent(BaseStringSliceView<Char_t, kEncoding>(outChunkSpan.SubSpan(0, convertedChunkSize)), TIsAbsolute, isFirst);

					switch (validationResult)
					{
					case PathValidationResult::kValid:
						break;
					case PathValidationResult::kConvertible:
						TPathTraits::MakeComponentValid(outChunkSpan, TIsAbsolute, isFirst);
						break;
					case PathValidationResult::kInvalid:
						RKIT_THROW(ResultCode::kInvalidPath);
					default:
						RKIT_THROW(ResultCode::kInternalError);
					}

					emitPos += convertedChunkSize;

					startPos = scanPos + 1;
				}
			}
		}

		m_path = BaseString<Char_t, kEncoding>(std::move(scBuf));

		RKIT_RETURN_OK;
	}

	template<bool TIsAbsolute, class TPathTraits>
	BasePath<TIsAbsolute, TPathTraits>::operator typename BasePath<TIsAbsolute, TPathTraits>::View_t() const
	{
		return BasePathView<TIsAbsolute, TPathTraits>(m_path);
	}

	template<bool TIsAbsolute, class TPathTraits>
	const typename BasePath<TIsAbsolute, TPathTraits>::Char_t *BasePath<TIsAbsolute, TPathTraits>::CStr() const
	{
		return m_path.CStr();
	}

	template<bool TIsAbsolute, class TPathTraits>
	size_t BasePath<TIsAbsolute, TPathTraits>::Length() const
	{
		return m_path.Length();
	}

	template<bool TIsAbsolute, class TPathTraits>
	PathValidationResult BasePath<TIsAbsolute, TPathTraits>::Validate(const BaseStringSliceView<Char_t, kEncoding> &str)
	{
		if (str.Length() == 0)
			return PathValidationResult::kValid;

		bool requiresFixup = false;

		size_t startPos = 0;

		for (size_t scanPos = 0; scanPos <= str.Length(); scanPos++)
		{
			if (scanPos == str.Length() || TPathTraits::IsDelimiter(str[scanPos]))
			{
				const bool isFirst = (startPos == 0);

				if (scanPos == startPos)
					return PathValidationResult::kInvalid;

				if (scanPos != str.Length() && str[scanPos] != kDefaultDelimiter)
					requiresFixup = true;

				const BaseStringSliceView<Char_t, kEncoding> slice = str.SubString(startPos, scanPos - startPos);

				const PathValidationResult result = TPathTraits::ValidateComponent(slice, TIsAbsolute, isFirst);

				if (result == PathValidationResult::kInvalid)
					return PathValidationResult::kInvalid;

				if (result == PathValidationResult::kConvertible)
					requiresFixup = true;

				startPos = scanPos + 1;
			}
		}

		if (requiresFixup)
			return PathValidationResult::kConvertible;

		return PathValidationResult::kValid;
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePath<TIsAbsolute, TPathTraits>::operator<(const BasePath<TIsAbsolute, TPathTraits> &other)
	{
		BasePathIterator<TIsAbsolute, TPathTraits> thisIt = this->begin();
		BasePathIterator<TIsAbsolute, TPathTraits> thisEnd = this->end();
		BasePathIterator<TIsAbsolute, TPathTraits> otherIt = other.begin();
		BasePathIterator<TIsAbsolute, TPathTraits> otherEnd = other.end();

		for (;;)
		{
			if (otherIt == otherEnd)
				return false;

			if (thisIt == thisEnd)
				return true;

			BaseStringSliceView<typename TPathTraits::Char_t, TPathTraits::kEncoding> thisSlice = *thisIt;
			BaseStringSliceView<typename TPathTraits::Char_t, TPathTraits::kEncoding> otherSlice = *otherIt;

			Ordering comparison = thisSlice.Compare(otherSlice);

			if (comparison == Ordering::kLess)
				return true;
			else if (comparison == Ordering::kGreater)
				return false;

			++thisIt;
			++otherIt;
		}
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePath<TIsAbsolute, TPathTraits>::operator>(const BasePath<TIsAbsolute, TPathTraits> &other)
	{
		return other < *this;
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePath<TIsAbsolute, TPathTraits>::operator<=(const BasePath<TIsAbsolute, TPathTraits> &other)
	{
		return !(other < (*this));
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePath<TIsAbsolute, TPathTraits>::operator>=(const BasePath<TIsAbsolute, TPathTraits> &other)
	{
		return !((*this) < other);
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePath<TIsAbsolute, TPathTraits>::operator==(const BasePath<TIsAbsolute, TPathTraits> &other)
	{
		return m_path == other.m_path;
	}

	template<bool TIsAbsolute, class TPathTraits>
	bool BasePath<TIsAbsolute, TPathTraits>::operator!=(const BasePath<TIsAbsolute, TPathTraits> &other)
	{
		return !((*this) == other);
	}

	template<bool TIsAbsolute, class TPathTraits>
	Result BasePath<TIsAbsolute, TPathTraits>::SetConvert(const BaseStringSliceView<Char_t, kEncoding> &str)
	{
		if (str.Length() == 0)
		{
			m_path.Clear();
			RKIT_RETURN_OK;
		}

		size_t numComponents = 0;
		size_t numComponentCharacters = 0;

		size_t startPos = 0;

		for (size_t scanPos = 0; scanPos <= str.Length(); scanPos++)
		{
			if (scanPos == str.Length() || TPathTraits::IsDelimiter(str[scanPos]))
			{
				numComponents++;
				numComponentCharacters += scanPos - startPos;

				startPos = scanPos + 1;
			}
		}

		BaseStringConstructionBuffer<Char_t> stringBuffer;
		RKIT_CHECK(stringBuffer.Allocate(numComponents - 1 + numComponentCharacters));

		Span<Char_t> stringSpan = stringBuffer.GetSpan();

		startPos = 0;

		size_t outPos = 0;
		for (size_t scanPos = 0; scanPos <= str.Length(); scanPos++)
		{
			const bool isEnd = (scanPos == str.Length());
			const bool isDelimiter = !isEnd && TPathTraits::IsDelimiter(str[scanPos]);

			if (isEnd || isDelimiter)
			{
				const BaseStringSliceView<Char_t, kEncoding> slice = str.SubString(startPos, scanPos - startPos);
				Span<Char_t> outSpan = stringSpan.SubSpan(outPos, slice.Length());

				CopySpanNonOverlapping(outSpan, slice.ToSpan());
				TPathTraits::MakeComponentValid(outSpan, TIsAbsolute, scanPos == 0);

				outPos += slice.Length();

				startPos = scanPos + 1;
			}

			if (isDelimiter)
				stringSpan[outPos++] = kDefaultDelimiter;
		}

		RKIT_ASSERT(outPos == stringSpan.Count());

		m_path = BaseString<Char_t, kEncoding>(std::move(stringBuffer));

		RKIT_RETURN_OK;
	}

	template<bool TIsAbsolute, class TPathTraits>
	inline HashValue_t Hasher<BasePath<TIsAbsolute, TPathTraits>>::ComputeHash(HashValue_t baseHash, const BasePath<TIsAbsolute, TPathTraits> &value)
	{
		return Hasher<BaseString<typename TPathTraits::Char_t, TPathTraits::kEncoding>>::ComputeHash(baseHash, value.ToString());
	}

	template<bool TIsAbsolute, class TPathTraits>
	inline HashValue_t Hasher<BasePathView<TIsAbsolute, TPathTraits>>::ComputeHash(HashValue_t baseHash, const BasePathView<TIsAbsolute, TPathTraits> &value)
	{
		return Hasher<BaseStringView<typename TPathTraits::Char_t, TPathTraits::kEncoding>>::ComputeHash(baseHash, value.ToStringView());
	}
}

#include "Result.h"

#if RKIT_PLATFORM == RKIT_PLATFORM_WIN32

namespace rkit
{
	inline bool OSPathTraits::IsDelimiter(Char_t ch)
	{
		return ch == '/' || ch == '\\';
	}

	inline PathValidationResult OSPathTraits::ValidateComponent(const BaseStringSliceView<Char_t, kEncoding> &span, bool isAbsolute, bool isFirst)
	{
		if (GetDrivers().m_utilitiesDriver->IsPathComponentValidOnWindows(span, isAbsolute, isFirst, false))
			return PathValidationResult::kValid;

		return PathValidationResult::kInvalid;
	}

	inline void OSPathTraits::MakeComponentValid(const Span<Char_t> &outComponent, bool isAbsolute, bool isFirst)
	{
	}
};

#endif	// RKIT_PLATFORM == RKIT_PLATFORM_WIN32
