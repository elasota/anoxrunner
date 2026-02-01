#include "rkit/Core/StringView.h"

#include "rkit/Render/RenderDefs.h"

#include "AnoxGraphicsSettings.h"

#define ANOX_SETTING_RETURN_ENUM(enumType, enumValue)	\
	do {\
		outMainType = ::rkit::data::RenderRTTIMainType::enumType;\
		outValue = static_cast<unsigned int>(::rkit::render::enumType::enumValue);\
		return true;\
	} while(false)
	

namespace anox
{
	bool RestartRequiringGraphicsSettings::ResolveConfigEnum(const rkit::StringView &configName, rkit::data::RenderRTTIMainType &outMainType, unsigned int &outValue) const
	{
		if (configName == u8"CFG_DepthCompareOp")
		{
			ANOX_SETTING_RETURN_ENUM(ComparisonFunction, Less);
			return true;
		}

		if (configName == u8"CFG_DepthStencilTargetFormat")
		{
			ANOX_SETTING_RETURN_ENUM(DepthStencilFormat, DepthUNorm16);
			return true;
		}

		if (configName == u8"CFG_RenderTargetFormat")
		{
			ANOX_SETTING_RETURN_ENUM(RenderTargetFormat, RGBA_UNorm8);
			return true;
		}

		return false;
	}

	bool RestartRequiringGraphicsSettings::ResolveConfigFloat(const rkit::StringView &configName, double &outValue) const
	{
		return false;
	}

	bool RestartRequiringGraphicsSettings::ResolveConfigSInt(const rkit::StringView &configName, int64_t &outValue) const
	{
		return false;
	}

	bool RestartRequiringGraphicsSettings::ResolveConfigUInt(const rkit::StringView &configName, uint64_t &outValue) const
	{
		return false;
	}
}
