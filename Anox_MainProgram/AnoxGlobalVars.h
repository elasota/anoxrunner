#pragma once

#include "rkit/Core/Result.h"
#include "rkit/Core/String.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace anox
{
	struct IConfigurationState;
}

namespace anox { namespace game {
	struct GlobalVars final
	{
		rkit::String m_mapName;

		rkit::Result Save(rkit::UniquePtr<IConfigurationState> &state) const;
	};
} }
