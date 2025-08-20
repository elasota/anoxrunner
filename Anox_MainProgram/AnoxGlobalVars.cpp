#include "AnoxGlobalVars.h"

#include "AnoxConfigurationSaver.h"

namespace anox { namespace game {
	rkit::Result GlobalVars::Save(rkit::UniquePtr<IConfigurationState> &state) const
	{
		ConfigBuilderValue_t root;

		rkit::UniquePtr<ConfigurationValueRootState> rootState;
		RKIT_CHECK(rkit::New<ConfigurationValueRootState>(rootState, std::move(root)));

		state = std::move(rootState);

		return rkit::ResultCode::kOK;
	}
} } // anox::game

