#pragma once

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
	union ConfigurableValueUnion
	{
		ConfigurableValueUnion();
		ConfigurableValueUnion(const T &value);
		ConfigurableValueUnion(T &&value);
		ConfigurableValueUnion(const render::ConfigStringIndex_t &stringIndex);

		T m_value;
		render::ConfigStringIndex_t m_configName;
	};

	template<class T>
	struct ConfigurableValue
	{
		ConfigurableValue();
		ConfigurableValue(const T &value);
		ConfigurableValue(const render::ConfigStringIndex_t &stringIndex);
		ConfigurableValue(const ConfigurableValue &other);
		ConfigurableValue(ConfigurableValue &&other);
		~ConfigurableValue();

		ConfigurableValue &operator=(const ConfigurableValue &other);
		ConfigurableValue &operator=(ConfigurableValue &&other);

		ConfigurableValueState m_state;
		ConfigurableValueUnion<T> m_u;

	private:
		void Clear();
	};

	struct SamplerDescDynamic
	{
		ConfigurableValue<Filter> m_minFilter;
		ConfigurableValue<Filter> m_magFilter;
		ConfigurableValue<MipMapMode> m_mipMapMode;
		ConfigurableValue<AddressMode> m_addressModeU;
		ConfigurableValue<AddressMode> m_addressModeV;
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

#include <new>
#include <utility>

namespace rkit::render
{
	template<class T>
	ConfigurableValueUnion<T>::ConfigurableValueUnion()
	{
	}

	template<class T>
	ConfigurableValueUnion<T>::ConfigurableValueUnion(const T &value)
	{
		new (&m_value) T(value);
	}

	template<class T>
	ConfigurableValueUnion<T>::ConfigurableValueUnion(T &&value)
	{
		new (&m_value) T(std::move(value));
	}

	template<class T>
	ConfigurableValueUnion<T>::ConfigurableValueUnion(const ConfigStringIndex_t &configName)
	{
		new (&m_configName) ConfigStringIndex_t(configName);
	}

	template<class T>
	inline ConfigurableValue<T>::ConfigurableValue()
		: m_state(ConfigurableValueState::Default)
	{
	}

	template<class T>
	inline ConfigurableValue<T>::ConfigurableValue(const T &value)
		: m_state(ConfigurableValueState::Explicit)
		, m_u(value)
	{
	}

	template<class T>
	inline ConfigurableValue<T>::ConfigurableValue(const ConfigStringIndex_t &value)
		: m_state(ConfigurableValueState::Configured)
		, m_u(value)
	{
	}

	template<class T>
	inline ConfigurableValue<T>::ConfigurableValue(const ConfigurableValue &other)
		: m_state(other.m_state)
	{
		if (other.m_state == ConfigurableValueState::Configured)
			new (&m_u.m_configName) StringView(other.m_u.m_configName);
		else if (other.m_state == ConfigurableValueState::Explicit)
			new (&m_u.m_value) T(other.m_u.m_value);
	}

	template<class T>
	inline ConfigurableValue<T>::ConfigurableValue(ConfigurableValue &&other)
		: m_state(other.m_state)
	{
		if (other.m_state == ConfigurableValueState::Configured)
			new (&m_u.m_configName) StringView(other.m_u.m_configName);
		else if (other.m_state == ConfigurableValueState::Explicit)
			new (&m_u.m_value) T(std::move(other.m_u.m_value));
	}

	template<class T>
	inline ConfigurableValue<T>::~ConfigurableValue()
	{
		Clear();
	}

	template<class T>
	inline void ConfigurableValue<T>::Clear()
	{
		if (m_state == ConfigurableValueState::Configured)
			m_u.m_configName.~ConfigStringIndex_t();
		else if (m_state == ConfigurableValueState::Explicit)
			m_u.m_value.~T();

		m_state = ConfigurableValueState::Default;
	}

	template<class T>
	inline ConfigurableValue<T> &ConfigurableValue<T>::operator=(const ConfigurableValue &other)
	{
		if (this != &other)
		{
			Clear();

			m_state = other.m_state;
			if (other.m_state == ConfigurableValueState::Configured)
				new (&m_u.m_configName) ConfigStringIndex_t(other.m_u.m_configName);
			else if (other.m_state == ConfigurableValueState::Explicit)
				new (&m_u.m_value) T(other.m_u.m_value);
		}

		return *this;
	}

	template<class T>
	inline ConfigurableValue<T> &ConfigurableValue<T>::operator=(ConfigurableValue &&other)
	{
		Clear();

		m_state = other.m_state;
		if (other.m_state == ConfigurableValueState::Configured)
			new (&m_u.m_configName) ConfigStringIndex_t(other.m_u.m_configName);
		else if (other.m_state == ConfigurableValueState::Explicit)
			new (&m_u.m_value) T(std::move(other.m_u.m_value));

		return *this;
	}
}
