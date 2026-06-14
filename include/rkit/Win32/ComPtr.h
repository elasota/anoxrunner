#pragma once

#include <unknwn.h>
#include <compare>

namespace rkit::win32
{
	template<class T>
	class ComPtr
	{
		template<class TOther>
		friend class ComPtr;

	public:
		ComPtr();
		ComPtr(const ComPtr<T> &other);
		ComPtr(ComPtr<T> &&other) noexcept;
		~ComPtr();

		T *operator->() const;

		T *Get() const;
		T &operator*() const;
		bool IsValid() const;

		ComPtr<T> &operator=(const ComPtr<T> &other);
		ComPtr<T> &operator=(ComPtr<T> &&other) noexcept;

		void Reset();

		template<class TOther>
		bool operator==(const ComPtr<TOther> &other) const;

		template<class TOther>
		std::strong_ordering operator<=>(const ComPtr<TOther> &other) const;

		static ComPtr<T> Wrap(T *object);

		template<class TOther>
		ComPtr<TOther> TryGetInterface() const;

		T **WriteTo();

	private:
		explicit ComPtr(T *object);

		IUnknown *AsUnknown() const;

		T *m_object;
	};
}

namespace rkit::win32::priv
{
	template<class T>
	class ComPtrWriteback
	{
	public:
		ComPtrWriteback() = delete;
		ComPtrWriteback(const ComPtrWriteback &) = delete;
		ComPtrWriteback(ComPtrWriteback &&other);
		~ComPtrWriteback();

		static ComPtrWriteback<T> Wrap(ComPtr<T> &comPtr);

		void **Receive();

	private:
		explicit ComPtrWriteback(ComPtr<T> &comPtr);

		void *m_receiver;
		ComPtr<T> *m_target;
	};

	template<class T>
	T ComPtrDeclVal(const ComPtr<T> &comPtr);

	template<class T>
	ComPtrWriteback<T> ComPtrWrapWriteback(ComPtr<T> &comPtr);
}

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/RKitAssert.h"

namespace rkit::win32
{
	template<class T>
	inline ComPtr<T>::ComPtr()
		: m_object(nullptr)
	{
	}

	template<class T>
	inline ComPtr<T>::ComPtr(const ComPtr<T> &other)
		: m_object(other.m_object)
	{
		if (m_object)
			AsUnknown()->AddRef();
	}

	template<class T>
	inline ComPtr<T>::ComPtr(ComPtr<T> &&other) noexcept
		: m_object(other.m_object)
	{
		other.m_object = nullptr;
	}

	template<class T>
	inline ComPtr<T>::~ComPtr()
	{
		if (m_object)
			AsUnknown()->Release();
	}

	template<class T>
	inline T *ComPtr<T>::operator->() const
	{
		RKIT_ASSERT(m_object != nullptr);
		return m_object;
	}

	template<class T>
	T *ComPtr<T>::Get() const
	{
		return m_object;
	}

	template<class T>
	T &ComPtr<T>::operator*() const
	{
		RKIT_ASSERT(m_object != nullptr);
		return *m_object;
	}

	template<class T>
	bool ComPtr<T>::IsValid() const
	{
		return m_object != nullptr;
	}

	template<class T>
	ComPtr<T> &ComPtr<T>::operator=(const ComPtr<T> &other)
	{
		IUnknown *oldObject = AsUnknown();
		m_object = other.m_object;
		if (m_object)
			AsUnknown()->AddRef();
		if (oldObject)
			oldObject->Release();

		return *this;
	}

	template<class T>
	ComPtr<T> &ComPtr<T>::operator=(ComPtr<T> &&other) noexcept
	{
		RKIT_ASSERT(this != &other);

		IUnknown *oldObject = AsUnknown();

		m_object = other.m_object;
		other.m_object = nullptr;

		if (oldObject)
			oldObject->Release();

		return *this;
	}

	template<class T>
	void ComPtr<T>::Reset()
	{
		IUnknown *oldObject = AsUnknown();
		if (oldObject != nullptr)
			oldObject->Release();

		m_object = nullptr;
	}

	template<class T>
	template<class TOther>
	bool ComPtr<T>::operator==(const ComPtr<TOther> &other) const
	{
		return m_object == other.m_object;
	}

	template<class T>
	template<class TOther>
	std::strong_ordering ComPtr<T>::operator<=>(const ComPtr<TOther> &other) const
	{
		return AsUnknown() <=> other.AsUnknown();
	}

	template<class T>
	ComPtr<T> ComPtr<T>::Wrap(T *object)
	{
		return ComPtr<T>(object);
	}

	template<class T>
	template<class TOther>
	ComPtr<TOther> ComPtr<T>::TryGetInterface() const
	{
		RKIT_ASSERT(m_object != nullptr);
		void *ifc = nullptr;
		if (AsUnknown()->QueryInterface(__uuidof(T), &ifc) != S_OK)
			return ComPtr<TOther>();

		return ComPtr<TOther>::Wrap(static_cast<TOther *>(ifc));
	}

	template<class T>
	T **ComPtr<T>::WriteTo()
	{
		IUnknown *oldObject = AsUnknown();
		if (oldObject)
			oldObject->Release();

		m_object = nullptr;
		return &m_object;
	}

	template<class T>
	ComPtr<T>::ComPtr(T *object)
		: m_object(object)
	{
	}

	template<class T>
	IUnknown *ComPtr<T>::AsUnknown() const
	{
		return m_object;
	}
}

namespace rkit::win32::priv
{
	template<class T>
	inline ComPtrWriteback<T>::ComPtrWriteback(ComPtrWriteback &&other)
		: m_target(other.m_target)
		, m_receiver(nullptr)
	{
		other.m_target = nullptr;
	}

	template<class T>
	ComPtrWriteback<T>::ComPtrWriteback(ComPtr<T> &comPtr)
		: m_target(&comPtr)
		, m_receiver(nullptr)
	{
	}

	template<class T>
	inline ComPtrWriteback<T>::~ComPtrWriteback()
	{
		if (m_target)
			(*m_target) = ComPtr<T>::Wrap(static_cast<T *>(m_receiver));
	}

	template<class T>
	ComPtrWriteback<T> ComPtrWriteback<T>::Wrap(ComPtr<T> &comPtr)
	{
		return ComPtrWriteback<T>(comPtr);
	}

	template<class T>
	void **ComPtrWriteback<T>::Receive()
	{
		return &m_receiver;
	}

	template<class T>
	T ComPtrDeclVal(const ComPtr<T> &comPtr)
	{
		return std::declval<T>();
	}

	template<class T>
	T ComPtrDeclVal(ComPtr<T> &&comPtr)
	{
		return std::declval<T>();
	}

	template<class T>
	ComPtrWriteback<T> ComPtrWrapWriteback(ComPtr<T> &comPtr)
	{
		return ComPtrWriteback<T>::Wrap(comPtr);
	}
}

#define RKIT_COM_IID_PPV_ARGS(expr)	\
	__uuidof(decltype(::rkit::win32::priv::ComPtrDeclVal(expr))), \
	::rkit::win32::priv::ComPtrWrapWriteback(expr).Receive()

#define RKIT_COM_CHECK(expr) \
	do { \
		if (::rkit::ImplicitCast<HRESULT>((expr)) != 0)	\
		{ \
			RKIT_THROW(::rkit::ResultCode::kComError); \
		} \
	} while (false)

#define RKIT_COM_PPV_ARG(expr) (::rkit::win32::priv::ComPtrWrapWriteback(expr).Receive())
