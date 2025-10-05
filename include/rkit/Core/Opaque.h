#pragma once

namespace rkit
{
	template<class TImpl>
	class Opaque;

	template<class TBase>
	class OpaqueImplementation
	{
	public:
		OpaqueImplementation();

	protected:
		TBase &Base();
		const TBase &Base() const;

	private:
		template<class TImpl>
		static void CheckBase(Opaque<TImpl> *opaque);

		template<class TImpl>
		TBase *ResolveBase(Opaque<TImpl> *);

#if RKIT_IS_DEBUG
		TBase &m_base;
#endif
	};

	template<class TImpl>
	class Opaque
	{
	public:
		template<class... TArgs>
		explicit Opaque(TArgs... args);
		~Opaque();

	protected:
		TImpl &Impl();
		const TImpl &Impl() const;

	private:
		template<class TBase>
		TImpl *ResolveImpl(OpaqueImplementation<TBase> *);

#if RKIT_IS_DEBUG
		TImpl &m_impl;
#endif
	};
}

#include "OpaquePairing.h"
#include "RKitAssert.h"

#include <stdint.h>
#include <new>
#include <utility>

namespace rkit
{
	template<class TBase>
	OpaqueImplementation<TBase>::OpaqueImplementation()
#if RKIT_IS_DEBUG
		: m_base(Base())
#endif
	{
		static_assert(std::is_final<TBase>::value, "Opaque base must be final");
	}


	template<class TBase>
	template<class TImpl>
	void OpaqueImplementation<TBase>::CheckBase(Opaque<TImpl> *)
	{
		TImpl *impl = this;
		(void)impl;
	}

	template<class TBase>
	template<class TImpl>
	TBase *OpaqueImplementation<TBase>::ResolveBase(Opaque<TImpl> *)
	{
		TImpl *impl = static_cast<TImpl *>(this);

		typedef priv::OpaquePairing<TBase, TImpl> Pairing_t;

		return reinterpret_cast<TBase *>(reinterpret_cast<uint8_t *>(impl) - offsetof(Pairing_t, m_impl) + offsetof(Pairing_t, m_base));
	}

	template<class TBase>
	TBase &OpaqueImplementation<TBase>::Base()
	{
		return ResolveBase(static_cast<TBase *>(nullptr));
	}

	template<class TBase>
	const TBase &OpaqueImplementation<TBase>::Base() const
	{
		return const_cast<OpaqueImplementation<TBase> *>(this)->Base();
	}


	template<class TImpl>
	template<class... TArgs>
	Opaque<TImpl>::Opaque(TArgs... args)
#if RKIT_IS_DEBUG
		: m_impl(Impl())
#endif
	{
		new (&Impl()) TImpl(std::forward<TArgs>(args)...);
	}

	template<class TImpl>
	Opaque<TImpl>::~Opaque()
	{
		Impl().~TImpl();
	}

	template<class TImpl>
	TImpl &Opaque<TImpl>::Impl()
	{
		return *ResolveImpl(static_cast<TImpl>(nullptr));
	}

	template<class TImpl>
	const TImpl &Opaque<TImpl>::Impl() const
	{
		return const_cast<Opaque<TImpl> *>(this)->Impl();
	}

	template<class TImpl>
	template<class TBase>
	TImpl *Opaque<TImpl>::ResolveImpl(OpaqueImplementation<TBase> *)
	{
		TBase *base = static_cast<TBase *>(this);

		typedef priv::OpaquePairing<TBase, TImpl> Pairing_t;

		return reinterpret_cast<TImpl *>(reinterpret_cast<uint8_t *>(base) - offsetof(Pairing_t, m_base) + offsetof(Pairing_t, m_impl));
	}
}
