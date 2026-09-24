#pragma once

#include <utility>

namespace rkit::data::priv
{
	template<class T, bool TIsAlignedPtr>
	class InvertibleData
	{
	public:
		InvertibleData();
		InvertibleData(const InvertibleData &other) = default;
		InvertibleData(InvertibleData &&other) = default;

		explicit InvertibleData(const T &value, bool isInverted);
		explicit InvertibleData(T &&value, bool isInverted);

		InvertibleData &operator=(const InvertibleData &other) = default;
		InvertibleData &operator=(InvertibleData &&other) = default;

		bool IsInverted() const;
		const T &GetValue() const;

		void Set(T &&value, bool isInverted);
		void Set(const T &value, bool isInverted);

	private:
		T m_value;
		bool m_isInverted;
	};

	template<class T>
	class InvertibleData<T *, true>
	{
	public:
		InvertibleData();
		InvertibleData(const InvertibleData &other) = default;
		InvertibleData(InvertibleData &&other) = default;

		explicit InvertibleData(T *value, bool isInverted);

		InvertibleData &operator=(const InvertibleData &other) = default;
		InvertibleData &operator=(InvertibleData &&other) = default;

		bool IsInverted() const;
		T *GetValue() const;

		void Set(T *value, bool isInverted);

	private:
		uintptr_t m_value;
	};

	template<class T>
	struct InvertibleDataTypeResolver
	{
		typedef InvertibleData<T, false> Type_t;
	};

	template<class T>
	struct InvertibleDataTypeResolver<T *>
	{
		typedef InvertibleData<T *, alignof(T) >= 2> Type_t;
	};
}

namespace rkit::data
{
	template<class T>
	class Invertible final : public priv::InvertibleDataTypeResolver<T>::Type_t
	{
	};
}

namespace rkit::data::builder
{
	template<class T>
	class InvertibleBuilder
	{
	public:
		InvertibleBuilder() = default;
		InvertibleBuilder(const InvertibleBuilder &other) = default;
		InvertibleBuilder(InvertibleBuilder&& other) = default;
		~InvertibleBuilder() = default;

		InvertibleBuilder(const T& value, bool isInverted);
		InvertibleBuilder(T&& value, bool isInverted);

		const T& GetValue() const;
		T& ModifyValue();

		bool IsInverted() const;
		void SetInverted(bool inverted);

	private:
		T m_value = T();
		bool m_isInverted = false;
	};
}

namespace rkit::data::priv
{
	template<class T, bool TIsAlignedPtr>
	InvertibleData<T, TIsAlignedPtr>::InvertibleData()
		: m_value()
		, m_isInverted(false)
	{
	}

	template<class T, bool TIsAlignedPtr>
	InvertibleData<T, TIsAlignedPtr>::InvertibleData(const T &value, bool isInverted)
		: m_value(value)
		, m_isInverted(isInverted)
	{
	}

	template<class T, bool TIsAlignedPtr>
	InvertibleData<T, TIsAlignedPtr>::InvertibleData(T &&value, bool isInverted)
		: m_value(std::move(value))
		, m_isInverted(isInverted)
	{
	}


	template<class T, bool TIsAlignedPtr>
	bool InvertibleData<T, TIsAlignedPtr>::IsInverted() const
	{
		return m_isInverted;
	}

	template<class T, bool TIsAlignedPtr>
	const T &InvertibleData<T, TIsAlignedPtr>::GetValue() const
	{
		return m_value;
	}

	template<class T, bool TIsAlignedPtr>
	void InvertibleData<T, TIsAlignedPtr>::Set(T &&other, bool isInverted)
	{
		m_value = std::move(other);
		m_isInverted = isInverted;
	}

	template<class T, bool TIsAlignedPtr>
	void InvertibleData<T, TIsAlignedPtr>::Set(const T &other, bool isInverted)
	{
		m_value = other;
		m_isInverted = isInverted;
	}

	template<class T>
	InvertibleData<T *, true>::InvertibleData()
		: m_value(0)
	{
	}

	template<class T>
	InvertibleData<T *, true>::InvertibleData(T *value, bool isInverted)
		: m_value(reinterpret_cast<uintptr_t>(value) | (isInverted ? 1 : 0))
	{
	}

	template<class T>
	bool InvertibleData<T *, true>::IsInverted() const
	{
		return (m_value & 1) != 0;
	}

	template<class T>
	T *InvertibleData<T *, true>::GetValue() const
	{
		uintptr_t addr = (m_value & static_cast<uintptr_t>(std::numeric_limits<uintptr_t>::max() - 1));
		return reinterpret_cast<T *>(addr);
	}

	template<class T>
	void InvertibleData<T *, true>::Set(T *value, bool isInverted)
	{
		m_value = (reinterpret_cast<uintptr_t>(value) | (isInverted ? 1 : 0));
	}
}

namespace rkit::data::builder
{
	template<class T>
	InvertibleBuilder<T>::InvertibleBuilder(const T &value, bool isInverted)
		: m_value(value)
		, m_isInverted(isInverted)
	{
	}

	template<class T>
	InvertibleBuilder<T>::InvertibleBuilder(T &&value, bool isInverted)
		: m_value(std::move(value))
		, m_isInverted(isInverted)
	{
	}


	template<class T>
	const T &InvertibleBuilder<T>::GetValue() const
	{
		return m_value;
	}

	template<class T>
	T &InvertibleBuilder<T>::ModifyValue()
	{
		return m_value;
	}

	template<class T>
	bool InvertibleBuilder<T>::IsInverted() const
	{
		return m_isInverted;
	}

	template<class T>
	void InvertibleBuilder<T>::SetInverted(bool inverted)
	{
		m_isInverted = inverted;
	}
}
