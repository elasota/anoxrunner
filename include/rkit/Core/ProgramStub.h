#pragma once

#include "DriverModuleStub.h"
#include "ProgramDriver.h"
#include "SimpleObjectAllocation.h"

namespace rkit
{
	struct ISimpleProgram
	{
		virtual ~ISimpleProgram() {}

		virtual Result Run() = 0;
	};

	template<class TProgram>
	class ProgramStubDriver final : public rkit::IProgramDriver
	{
	public:
		rkit::Result InitDriver(const DriverInitParameters *) override;
		void ShutdownDriver() override;

		Result InitProgram() override;
		Result RunFrame(bool &outIsExiting) override;
		void ShutdownProgram() override;

	private:
		static SimpleObjectAllocation<ISimpleProgram> ms_program;
	};

	template<class TProgram>
	class SimpleProgramModule final : public DriverModuleStub<ProgramStubDriver<TProgram>, rkit::IProgramDriver, &rkit::Drivers::m_programDriver>
	{
	};
}

#include "NewDelete.h"

template<class TProgram>
rkit::Result rkit::ProgramStubDriver<TProgram>::InitDriver(const DriverInitParameters *)
{
	UniquePtr<ISimpleProgram> program;
	RKIT_CHECK(New<TProgram>(program));

	ms_program = program.Detach();

	RKIT_RETURN_OK;
}

template<class TProgram>
void rkit::ProgramStubDriver<TProgram>::ShutdownDriver()
{
	SafeDelete(ms_program);
}

template<class TProgram>
rkit::Result rkit::ProgramStubDriver<TProgram>::InitProgram()
{
	return ms_program->Run();
}

template<class TProgram>
rkit::Result rkit::ProgramStubDriver<TProgram>::RunFrame(bool &outIsExiting)
{
	outIsExiting = true;
	RKIT_RETURN_OK;
}

template<class TProgram>
void rkit::ProgramStubDriver<TProgram>::ShutdownProgram()
{
}

template<class TProgram>
rkit::SimpleObjectAllocation<rkit::ISimpleProgram> rkit::ProgramStubDriver<TProgram>::ms_program;
