#include "AnoxGlobalVars.h"

#include "AnoxConfigurationSaver.h"

namespace anox { namespace game {
	class GlobalVarsSerializer
	{
	public:
		static rkit::Result SerializeGlobals(GlobalVars &vars, IConfigKeyValueTableSerializer &serializer)
		{
			RKIT_CHECK(serializer.SerializeField(u8"mapName", vars.m_mapName));

			return rkit::ResultCode::kOK;
		}
	};


	rkit::Result GlobalVars::Save(rkit::UniquePtr<IConfigurationState> &state) const
	{
		ConfigBuilderKeyValueTable kvt;
		ConfigBuilderKeyValueTableWriter writer(kvt);

		RKIT_CHECK(GlobalVarsSerializer::SerializeGlobals(const_cast<GlobalVars &>(*this), writer));

		ConfigBuilderValue_t root;
		root = std::move(kvt);

		rkit::UniquePtr<ConfigurationValueRootState> rootState;
		RKIT_CHECK(rkit::New<ConfigurationValueRootState>(rootState, std::move(root)));

		state = std::move(rootState);

		return rkit::ResultCode::kOK;
	}

} } // anox::game

