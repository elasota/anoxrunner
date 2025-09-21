#pragma once

namespace rkit
{
	template<class T>
	class Optional
	{
	public:

		Optional();
		Optional(const T &value);
		Optional(T &&value);
		Optional(const Optional<T> &value);
		Optional(Optional<T> &&value);
		~Optional();

		Optional<T> &operator=(T &&value);
		Optional<T> &operator=(const T &value);

		Optional<T> &operator=(Optional<T> &&value);
		Optional<T> &operator=(const Optional<T> &value);

		bool IsSet() const;
		T &Get();
		const T &Get() const;

		template<class... TArgs>
		void Emplace(TArgs && ...args);

		void Reset();

	private:
		struct Uninitialized { };

		union ValueUnion
		{
			ValueUnion(const Uninitialized &uninit);
			ValueUnion(const T &value);
			ValueUnion(T &&value);
			~ValueUnion();

			Uninitialized m_uninit;
			T m_value;
		};

		ValueUnion m_value;
		bool m_isSet;
	};
}

#include <utility>
#include <new>

#include "RKitAssert.h"

namespace rkit
{
	template<class T>
	Optional<T>::Optional()
		: m_isSet(false)
		, m_value(Uninitialized())
	{
	}

	template<class T>
	Optional<T>::Optional(const T &value)
		: m_isSet(true)
		, m_value(value)
	{
	}

	template<class T>
	Optional<T>::Optional(T &&value)
		: m_isSet(true)
		, m_value(std::move(value))
	{
	}

	template<class T>
	Optional<T>::Optional(const Optional<T> &other)
		: m_isSet(false)
		, m_value(Uninitialized())
	{
		if (other.m_isSet)
		{
			m_value.m_uninit.~Uninitialized();
			new (&m_value.m_value) T(other.m_value.m_value);

			m_isSet = true;
		}
	}

	template<class T>
	Optional<T>::Optional(Optional<T> &&other)
		: m_isSet(false)
		, m_value(Uninitialized())
	{
		if (other.m_isSet)
		{
			m_value.m_uninit.~Uninitialized();
			new (&m_value.m_value) T(std::move(other.m_value.m_value));

			m_isSet = true;

			other.m_value.m_value.~T();
			new (&other.m_value.m_uninit) Uninitialized();
			other.m_isSet = false;
		}
	}

	template<class T>
	Optional<T>::~Optional()
	{
		if (m_isSet)
			m_value.m_value.~T();
		else
			m_value.m_uninit.~Uninitialized();
	}


	template<class T>
	Optional<T> &Optional<T>::operator=(T &&value)
	{
		if (m_isSet)
			m_value.m_value = std::move(value);
		else
		{
			m_value.m_uninit.~Uninitialized();
			new (&m_value.m_value) T(std::move(value));
			m_isSet = true;
		}

		return *this;
	}

	template<class T>
	Optional<T> &Optional<T>::operator=(const T &value)
	{
		if (m_isSet)
			m_value.m_value = value;
		else
		{
			m_value.m_uninit.~Uninitialized();
			new (&m_value.m_value) T(value);
			m_isSet = true;
		}

		return *this;
	}

	template<class T>
	Optional<T> &Optional<T>::operator=(Optional<T> &&other)
	{
		if (this != &other)
		{
			if (m_isSet)
			{
				if (other.m_isSet)
				{
					m_value.m_value = std::move(other.m_value.m_value);
					other.m_value.m_value.~T();
					new (&other.m_value.m_uninit) Uninitialized();
					other.m_isSet = false;
				}
				else
					Reset();
			}
			else
			{
				if (other.m_isSet)
				{
					m_value.m_uninit.~Uninitialized();

					new (&m_value.m_value) T(std::move(other.m_value.m_value));
					m_isSet = true;

					other.m_value.m_value.~T();
					new (&other.m_value.m_uninit) Uninitialized();
					other.m_isSet = false;
				}
			}
		}

		return *this;
	}

	template<class T>
	Optional<T> &Optional<T>::operator=(const Optional<T> &other)
	{
		if (this != &other)
		{
			if (m_isSet)
			{
				if (other.m_isSet)
					m_value.m_value = other.m_value.m_value;
				else
					Reset();
			}
			else
			{
				if (other.m_isSet)
				{
					m_value.m_uninit.~Uninitialized();

					new (&m_value.m_value) T(other.m_value.m_value);
					m_isSet = true;
				}
			}
		}

		return *this;
	}

	template<class T>
	bool Optional<T>::IsSet() const
	{
		return m_isSet;
	}

	template<class T>
	T &Optional<T>::Get()
	{
		RKIT_ASSERT(m_isSet);
		return m_value.m_value;
	}

	template<class T>
	const T &Optional<T>::Get() const
	{
		RKIT_ASSERT(m_isSet);
		return m_value.m_value;
	}



	template<class T>
	template<class... TArgs>
	void Optional<T>::Emplace(TArgs && ...args)
	{
		if (m_isSet)
			m_value.m_value.~T();
		else
		{
			m_value.m_uninit.~Uninitialized();
			m_isSet = true;
		}

		new (&m_value.m_value) T(std::forward<TArgs>(args)...);
	}

	template<class T>
	void Optional<T>::Reset()
	{
		if (m_isSet)
		{
			m_value.m_value.~T();
			new (&m_value.m_uninit) Uninitialized();
			m_isSet = false;
		}
	}

	template<class T>
	Optional<T>::ValueUnion::ValueUnion(const Uninitialized &uninit)
		: m_uninit(uninit)
	{
	}

	template<class T>
	Optional<T>::ValueUnion::ValueUnion(const T &value)
		: m_value(value)
	{
	}

	template<class T>
	Optional<T>::ValueUnion::ValueUnion(T &&value)
		: m_value(std::move(value))
	{
	}

	template<class T>
	Optional<T>::ValueUnion::~ValueUnion()
	{
	}
}
