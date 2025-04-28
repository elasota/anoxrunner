#pragma once

#include "rkit/Core/CoreDefs.h"

namespace rkit
{
	template<class T>
	struct UniquePtr;
}

namespace anox
{
	struct IConfigurationState;
	struct ISessionState;

	struct ICaptureHarness
	{
		virtual ~ICaptureHarness() {}

		virtual rkit::Result GetConfigurationState(const IConfigurationState **outConfigStatePtr) = 0;
		virtual rkit::Result GetSessionState(const IConfigurationState **outConfigStatePtr) = 0;
	};
}
