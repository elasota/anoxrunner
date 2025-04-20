#pragma once

#include "rkit/Core/StringProto.h"

#include "rkit/Data/RenderDataHandler.h"

namespace anox
{
	struct RestartRequiringGraphicsSettings
	{
		bool ResolveConfigEnum(const rkit::StringView &configName, rkit::data::RenderRTTIMainType &outMainType, unsigned int &outValue) const;

		bool ResolveConfigFloat(const rkit::StringView &configName, double &outValue) const;
		bool ResolveConfigSInt(const rkit::StringView &configName, int64_t &outValue) const;
		bool ResolveConfigUInt(const rkit::StringView &configName, uint64_t &outValue) const;
	};

	struct RuntimeGraphicsSettings
	{
	};

	struct GraphicsSettings
	{
		RestartRequiringGraphicsSettings m_restartRequired;
		RuntimeGraphicsSettings m_runtime;
	};
}
