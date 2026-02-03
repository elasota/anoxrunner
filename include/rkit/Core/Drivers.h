#pragma once

#include "CoreDefs.h"
#include "SimpleObjectAllocation.h"
#include "StringProto.h"

#include <cstdint>

namespace rkit
{
	struct IShellDriver;
	struct IModuleDriver;
	struct IMallocDriver;
	struct IPlatformDriver;
	struct IProgramDriver;
	struct ISystemDriver;
	struct IUnicodeDriver;
	struct IUtilitiesDriver;
	struct ILogDriver;
	struct Drivers;

	template<class T>
	class UniquePtr;

	struct DriverInitParameters
	{
	};

	struct ICustomDriver
	{
		virtual ~ICustomDriver() {}

		virtual Result InitDriver(const DriverInitParameters *initParams) = 0;
		virtual void ShutdownDriver() = 0;

		virtual uint32_t GetDriverNamespaceID() const = 0;
		virtual StringView GetDriverName() const = 0;
	};

	struct Drivers
	{
		SimpleObjectAllocation<IMallocDriver> m_mallocDriver;
		SimpleObjectAllocation<IShellDriver> m_shellDriver;
		SimpleObjectAllocation<IModuleDriver> m_moduleDriver;
		SimpleObjectAllocation<IProgramDriver> m_programDriver;
		SimpleObjectAllocation<ISystemDriver> m_systemDriver;
		SimpleObjectAllocation<IUnicodeDriver> m_unicodeDriver;
		SimpleObjectAllocation<IUtilitiesDriver> m_utilitiesDriver;
		SimpleObjectAllocation<ILogDriver> m_logDriver;

		ICustomDriver *FindDriver(uint32_t namespaceID, const StringView &driverName) const;
		Result RegisterDriver(UniquePtr<ICustomDriver> &&driver);
		void UnregisterDriver(ICustomDriver *driver);

	private:
		struct CustomDriverLink
		{
			SimpleObjectAllocation<ICustomDriver> m_driver;
			SimpleObjectAllocation<CustomDriverLink> m_selfAllocation;
			CustomDriverLink *m_prev;
			CustomDriverLink *m_next;
		};

		CustomDriverLink *m_firstCustomDriverLink;
		CustomDriverLink *m_lastCustomDriverLink;
	};

	Drivers &GetMutableDrivers();
	const Drivers &GetDrivers();

} // End of namespace rkit

#include "Result.h"
#include "NewDelete.h"
#include "StringView.h"
#include "UniquePtr.h"

#include <utility>

inline rkit::ICustomDriver *rkit::Drivers::FindDriver(uint32_t namespaceID, const rkit::StringView &driverName) const
{
	for (const CustomDriverLink *link = m_firstCustomDriverLink; link != nullptr; link = link->m_next)
	{
		ICustomDriver *driver = link->m_driver;
		if (driver->GetDriverNamespaceID() == namespaceID && driver->GetDriverName() == driverName)
			return driver;
	}

	return nullptr;
}

inline rkit::Result rkit::Drivers::RegisterDriver(UniquePtr<ICustomDriver> &&driver)
{
	UniquePtr<ICustomDriver> driverScoped(std::move(driver));

	UniquePtr<CustomDriverLink> driverLinkScoped;
	RKIT_CHECK(rkit::NewUPtr<CustomDriverLink>(driverLinkScoped));

	CustomDriverLink *driverLink = driverLinkScoped.Get();
	driverLink->m_selfAllocation = driverLinkScoped.Detach();
	driverLink->m_driver = driverScoped.Detach();
	driverLink->m_prev = m_lastCustomDriverLink;
	driverLink->m_next = nullptr;

	if (!m_firstCustomDriverLink)
		m_firstCustomDriverLink = driverLink;

	if (m_lastCustomDriverLink)
		m_lastCustomDriverLink->m_next = driverLink;

	m_lastCustomDriverLink = driverLink;

	RKIT_RETURN_OK;
}

inline void rkit::Drivers::UnregisterDriver(ICustomDriver *driver)
{
	for (CustomDriverLink *link = m_firstCustomDriverLink; link != nullptr; link = link->m_next)
	{
		if (link->m_driver.m_obj == driver)
		{
			if (m_firstCustomDriverLink == link)
				m_firstCustomDriverLink = link->m_next;
			if (m_lastCustomDriverLink == link)
				m_lastCustomDriverLink = link->m_prev;

			if (link->m_next)
				link->m_next->m_prev = link->m_prev;
			if (link->m_prev)
				link->m_prev->m_next = link->m_next;

			// Deallocate the link first in case the link depends on the driver
			SimpleObjectAllocation<CustomDriverLink> linkAlloc = link->m_selfAllocation;
			SimpleObjectAllocation<ICustomDriver> driverAlloc = link->m_driver;

			Delete(linkAlloc);
			Delete(driverAlloc);
			break;
		}
	}
}
