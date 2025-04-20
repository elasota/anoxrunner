#pragma once

#include "rkit/Core/CoreDefs.h"

namespace rkit
{
	struct ModuleInitParameters;
	struct Drivers;
	struct ICustomDriver;

	template<class T>
	struct SimpleObjectAllocation;

	template<class TDriver, class TDriverInterface, SimpleObjectAllocation<TDriverInterface> Drivers::*TDriverMember>
	class DriverModuleStub
	{
	public:
		static Result Init(const ModuleInitParameters *);
		static void Shutdown();

	private:
		static SimpleObjectAllocation<TDriver> ms_driver;
	};

	template<class TDriver>
	class CustomDriverModuleStub
	{
	public:
		static Result Init(const ModuleInitParameters *);
		static void Shutdown();

	private:
		static ICustomDriver *ms_driver;
	};
}

#include "DriverModuleInitParams.h"
#include "Result.h"
#include "NewDelete.h"

#include <utility>

template<class TDriver, class TDriverInterface, ::rkit::SimpleObjectAllocation<TDriverInterface> rkit::Drivers:: *TDriverMember>
rkit::Result rkit::DriverModuleStub<TDriver, TDriverInterface, TDriverMember>::Init(const ModuleInitParameters *initParams)
{
	UniquePtr<TDriver> driver;
	RKIT_CHECK(New<TDriver>(driver));

	ms_driver = driver.Detach();
	GetMutableDrivers().*TDriverMember = ms_driver;

	const DriverInitParameters *driverInitParams = nullptr;
	if (initParams)
		driverInitParams = static_cast<const DriverModuleInitParameters *>(initParams)->m_driverInitParams;

	Result initResult = ms_driver->InitDriver(driverInitParams);
	if (!utils::ResultIsOK(initResult))
	{
		SafeDelete(ms_driver);
		GetMutableDrivers().*TDriverMember = ms_driver;
	}

	return ResultCode::kOK;
}

template<class TDriver, class TDriverInterface, ::rkit::SimpleObjectAllocation<TDriverInterface> rkit::Drivers:: *TDriverMember>
void rkit::DriverModuleStub<TDriver, TDriverInterface, TDriverMember>::Shutdown()
{
	if (ms_driver)
	{
		ms_driver->ShutdownDriver();
		SafeDelete(ms_driver);
		GetMutableDrivers().*TDriverMember = ms_driver;
	}
}

template<class TDriver, class TDriverInterface, ::rkit::SimpleObjectAllocation<TDriverInterface> rkit::Drivers:: *TDriverMember>
rkit::SimpleObjectAllocation<TDriver> rkit::DriverModuleStub<TDriver, TDriverInterface, TDriverMember>::ms_driver;

template<class TDriver>
rkit::Result rkit::CustomDriverModuleStub<TDriver>::Init(const ModuleInitParameters *initParams)
{
	UniquePtr<ICustomDriver> driver;
	RKIT_CHECK(New<TDriver>(driver));

	ICustomDriver *driverPtr = driver.Get();

	RKIT_CHECK(GetMutableDrivers().RegisterDriver(std::move(driver)));

	const DriverInitParameters *driverInitParams = nullptr;
	if (initParams)
		driverInitParams = static_cast<const DriverModuleInitParameters *>(initParams)->m_driverInitParams;

	ms_driver = driverPtr;

	return driverPtr->InitDriver(driverInitParams);
}

template<class TDriver>
void rkit::CustomDriverModuleStub<TDriver>::Shutdown()
{
	if (ms_driver)
	{
		ICustomDriver *driver = ms_driver;

		driver->ShutdownDriver();
		GetMutableDrivers().UnregisterDriver(driver);
		ms_driver = nullptr;
	}
}

template<class TDriver>
rkit::ICustomDriver *rkit::CustomDriverModuleStub<TDriver>::ms_driver = nullptr;
