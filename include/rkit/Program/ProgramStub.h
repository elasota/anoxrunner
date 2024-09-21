#pragma once

namespace rkit
{
	struct ModuleInitParameters;
	struct Result;

	template<class T>
	struct ProgramStub
	{
		static Result Init(const ModuleInitParameters *);
		static void Shutdown();

	private:
		static SimpleObjectAllocation<T> ms_programDriver;
	};
}

#include "rkit/Core/NewDelete.h"
#include "rkit/Core/Result.h"

template<class T>
rkit::Result rkit::ProgramStub<T>::Init(const ModuleInitParameters *)
{
	UniquePtr<IProgramDriver> driver;
	RKIT_CHECK(New<T>(driver));
	GetMutableDrivers().m_programDriver = driver.Detach();

	return ResultCode::kOK;
}

template<class T>
void rkit::ProgramStub<T>::Shutdown()
{
	SafeDelete(ms_programDriver);
}

template<class T>
rkit::SimpleObjectAllocation<T> rkit::ProgramStub<T>::ms_programDriver;
