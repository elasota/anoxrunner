#pragma once

#include "rkit/Core/StringView.h"

#include "rkit/Render/RenderDefs.h"

namespace rkit::render
{
	enum class ConfigurableValueState : uint8_t
	{
		Default,
		Configured,
		Explicit,
	};

	template<class T>
	struct ConfigurableValueUnion
	{
		ConfigurableValueUnion(const T &value);
		ConfigurableValueUnion(const StringView &configName);

		T m_value;
		StringView m_configName;
	};

	template<class T>
	struct ConfigurableValue
	{
		ConfigurableValue();
		ConfigurableValue(const T &value);
		ConfigurableValue(const StringView &configName);
		ConfigurableValue(const ConfigurableValue &other);
		ConfigurableValue(ConfigurableValue &&other);
		~ConfigurableValue();

		ConfigurableValue &operator=(const ConfigurableValue &other);
		ConfigurableValue &operator=(ConfigurableValue &&other);

		ConfigurableValueState m_isConfigurable;
		ConfigurableValueUnion<T> m_u;
	};

	struct SamplerDescDynamic
	{
		ConfigurableValue<Filter> m_minFilter;
		ConfigurableValue<Filter> m_magFilter;
		ConfigurableValue<MipMapMode> m_mipMapMode;
		ConfigurableValue<AddressMode> m_addressModeU;
		ConfigurableValue<AddressMode> m_addressMoveV;
		ConfigurableValue<AddressMode> m_addressModeW;
		ConfigurableValue<float> m_mipLodBias;
		ConfigurableValue<float> m_minLod;
		ConfigurableValue<float> m_maxLod;
		ConfigurableValue<bool> m_haveMaxLod;

		ConfigurableValue<AnisotropicFiltering> m_anisotropy;
		ConfigurableValue<bool> m_isCompare;

		ConfigurableValue<ComparisonFunction> m_compareFunction;
	};
}
