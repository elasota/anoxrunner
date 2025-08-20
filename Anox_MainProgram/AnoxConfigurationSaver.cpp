#include "AnoxConfigurationSaver.h"

#include "rkit/Core/Optional.h"


namespace anox
{
	rkit::Vector<ConfigBuilderKeyValuePair> &ConfigBuilderKeyValueTable::Modify()
	{
		return m_values;
	}

	rkit::ConstSpan<ConfigBuilderKeyValuePair> ConfigBuilderKeyValueTable::GetValues() const
	{
		return m_values.ToSpan();
	}

	rkit::Ordering Compare(const ConfigBuilderKeyValueTable &other);

	rkit::SpanIterator<ConfigBuilderValue_t> ConfigBuilderValueList::begin()
	{
		return m_values.ToSpan().begin();
	}

	rkit::ConstSpanIterator<ConfigBuilderValue_t> ConfigBuilderValueList::begin() const
	{
		return m_values.ToSpan().begin();
	}


	rkit::SpanIterator<ConfigBuilderValue_t> ConfigBuilderValueList::end()
	{
		return m_values.ToSpan().end();
	}

	rkit::ConstSpanIterator<ConfigBuilderValue_t> ConfigBuilderValueList::end() const
	{
		return m_values.ToSpan().end();
	}

	rkit::Vector<ConfigBuilderValue_t> &ConfigBuilderValueList::Modify()
	{
		return m_values;
	}

	rkit::ConstSpan<ConfigBuilderValue_t> ConfigBuilderValueList::GetValues() const
	{
		return m_values.ToSpan();
	}

	ConfigurationValueRootState::ConfigurationValueRootState(ConfigBuilderValue_t &&root)
		: m_root(std::move(root))
	{
	}

	IConfigurationValueView ConfigurationValueRootState::GetRoot() const
	{
		return CreateViewOfValue(m_root);
	}

	IConfigurationValueView ConfigurationValueRootState::CreateViewOfValue(const ConfigBuilderValue_t &value)
	{
		IConfigurationValueView result;
		result.m_userdataPtr = const_cast<ConfigBuilderValue_t *>(&value);
		result.m_userdataUInt = 0;
		result.m_getType = GetTypeCB;

		switch (value.GetType())
		{
		case ConfigurationValueType::kUInt64:
			result.m_getValueFuncs.m_getUInt64 = GetUInt64CB;
			break;
		case ConfigurationValueType::kInt64:
			result.m_getValueFuncs.m_getInt64 = GetInt64CB;
			break;
		case ConfigurationValueType::kFloat64:
			result.m_getValueFuncs.m_getFloat64 = GetFloat64CB;
			break;
		case ConfigurationValueType::kString:
			result.m_getValueFuncs.m_getString = GetStringCB;
			break;
		case ConfigurationValueType::kArray:
			result.m_getValueFuncs.m_getArray = GetArrayCB;
			break;
		case ConfigurationValueType::kKeyValueTable:
			result.m_getValueFuncs.m_keyValueTableFuncs = &ms_keyValueTableFuncs;
			break;
		default:
			result.m_getValueFuncs = {};
			break;
		}

		return result;
	}

	ConfigurationValueType ConfigurationValueRootState::GetTypeCB(const IConfigurationValueView &view)
	{
		return static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetType();
	}

	uint64_t ConfigurationValueRootState::GetUInt64CB(const IConfigurationValueView &view)
	{
		return static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<uint64_t>();
	}

	int64_t ConfigurationValueRootState::GetInt64CB(const IConfigurationValueView &view)
	{
		return static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<int64_t>();
	}

	double ConfigurationValueRootState::GetFloat64CB(const IConfigurationValueView &view)
	{
		return static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<double>();
	}

	rkit::StringView ConfigurationValueRootState::GetStringCB(const IConfigurationValueView &view)
	{
		return static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<rkit::String>();
	}

	rkit::CallbackSpan<IConfigurationValueView, const void *> ConfigurationValueRootState::GetArrayCB(const IConfigurationValueView &view)
	{
		const ConfigBuilderValueList &list = static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<ConfigBuilderValueList>();
		
		return rkit::CallbackSpan<IConfigurationValueView, const void *>(GetArrayElementCB, &list, list.GetValues().Count());
	}

	IConfigurationValueView ConfigurationValueRootState::GetArrayElementCB(void const *const &userdata, size_t index)
	{
		const ConfigBuilderValueList *list = static_cast<const ConfigBuilderValueList *>(userdata);

		return CreateViewOfValue(list->GetValues()[index]);
	}

	rkit::Optional<IConfigurationValueView> ConfigurationValueRootState::GetValueFromKeyCB(const IConfigurationValueView &view, const IConfigurationValueView &key)
	{
		const ConfigBuilderKeyValueTable &kvt = static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<ConfigBuilderKeyValueTable>();

		for (const ConfigBuilderKeyValuePair &kvPair : kvt.GetValues())
		{
			if (CreateViewOfValue(kvPair.m_key) == key)
			{
				return CreateViewOfValue(kvPair.m_value);
			}
		}

		return rkit::Optional<IConfigurationValueView>();
	}

	rkit::CallbackSpan<IConfigurationKeyValuePair, const void *> ConfigurationValueRootState::GetKeyValuePairsCB(const IConfigurationValueView &view)
	{
		const ConfigBuilderKeyValueTable &kvt = static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<ConfigBuilderKeyValueTable>();

		return rkit::CallbackSpan<IConfigurationKeyValuePair, const void *>(GetKeyValueTableKeyValuePairCB, &kvt, kvt.GetValues().Count());
	}

	rkit::CallbackSpan<IConfigurationValueView, const void *> ConfigurationValueRootState::GetKeysCB(const IConfigurationValueView &view)
	{
		const ConfigBuilderKeyValueTable &kvt = static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<ConfigBuilderKeyValueTable>();

		return rkit::CallbackSpan<IConfigurationValueView, const void *>(GetKeyValueTableKeyCB, &kvt, kvt.GetValues().Count());
	}

	IConfigurationKeyValuePair ConfigurationValueRootState::GetKeyValueTableKeyValuePairCB(void const *const &userdata, size_t index)
	{
		const ConfigBuilderKeyValueTable &kvt = *static_cast<const ConfigBuilderKeyValueTable *>(userdata);

		const ConfigBuilderKeyValuePair &kvp = kvt.GetValues()[index];

		IConfigurationKeyValuePair result;
		result.m_key = CreateViewOfValue(kvp.m_key);
		result.m_value = CreateViewOfValue(kvp.m_value);

		return result;
	}

	IConfigurationValueView ConfigurationValueRootState::GetKeyValueTableKeyCB(void const *const &userdata, size_t index)
	{
		const ConfigBuilderKeyValueTable &kvt = *static_cast<const ConfigBuilderKeyValueTable *>(userdata);

		const ConfigBuilderKeyValuePair &kvp = kvt.GetValues()[index];

		return CreateViewOfValue(kvp.m_key);
	}

	IConfigurationKeyValueTableFuncs ConfigurationValueRootState::ms_keyValueTableFuncs =
	{
		GetValueFromKeyCB,
		GetKeyValuePairsCB,
		GetKeysCB,
	};
}
