#include "AnoxConfigurationSaver.h"

#include "rkit/Core/Optional.h"

namespace anox { namespace priv {
	struct ConfigReadWriteFuncs
	{
		template<ConfigurationValueType TType>
		static ConfigurationValueType GetFixedType(const IConfigurationValueView &)
		{
			return TType;
		}

		static rkit::StringSliceView GetStringFromString(const IConfigurationValueView &view)
		{
			return *static_cast<const rkit::String *>(view.m_userdataPtr);
		}

		static rkit::StringSliceView GetStringFromStringSliceView(const IConfigurationValueView &view)
		{
			return *static_cast<const rkit::StringSliceView *>(view.m_userdataPtr);
		}
	};

	struct ConfigBuilderValueFuncs
	{
		static IConfigurationValueView CreateViewOfValue(const ConfigBuilderValue_t &value);
		static rkit::Result SetValue(ConfigBuilderValue_t &value, const IConfigurationValueView &view);

		static IConfigurationValueView CreateViewOfStringSliceView(const rkit::StringSliceView &value);

	private:
		static rkit::Result SetArray(ConfigBuilderValueList &list, const rkit::ISpan<IConfigurationValueView> &values);
		static rkit::Result SetKeyValueTable(ConfigBuilderKeyValueTable &list, const rkit::ISpan<IConfigurationKeyValuePair> &values);

		static ConfigurationValueType GetTypeCB(const IConfigurationValueView &view);
		static uint64_t GetUInt64CB(const IConfigurationValueView &view);
		static int64_t GetInt64CB(const IConfigurationValueView &view);
		static double GetFloat64CB(const IConfigurationValueView &view);
		static rkit::StringSliceView GetStringCB(const IConfigurationValueView &view);
		static rkit::StringSliceView GetStringSliceCB(const IConfigurationValueView &view);
		static rkit::CallbackSpan<IConfigurationValueView, const void *> GetArrayCB(const IConfigurationValueView &view);

		static IConfigurationValueView GetArrayElementCB(void const *const &userdata, size_t index);

		static rkit::Optional<IConfigurationValueView> GetValueFromKeyCB(const IConfigurationValueView &view, const IConfigurationValueView &key);
		static rkit::CallbackSpan<IConfigurationKeyValuePair, const void *> GetKeyValuePairsCB(const IConfigurationValueView &view);
		static rkit::CallbackSpan<IConfigurationValueView, const void *> GetKeysCB(const IConfigurationValueView &view);

		static IConfigurationKeyValuePair GetKeyValueTableKeyValuePairCB(void const *const &userdata, size_t index);
		static IConfigurationValueView GetKeyValueTableKeyCB(void const *const &userdata, size_t index);

		static IConfigurationKeyValueTableFuncs ms_keyValueTableFuncs;
	};
} }

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
		return priv::ConfigBuilderValueFuncs::CreateViewOfValue(m_root);
	}


	IConfigurationValueView ConfigValueReadWrite<rkit::String>::Read(const rkit::String &value)
	{
		IConfigurationValueView result = {};
		result.m_getType = priv::ConfigReadWriteFuncs::GetFixedType<ConfigurationValueType::kString>;
		result.m_userdataPtr = const_cast<rkit::String *>(&value);
		result.m_getValueFuncs.m_getString = priv::ConfigReadWriteFuncs::GetStringFromString;

		return result;
	}

	rkit::Result ConfigValueReadWrite<rkit::String>::Write(rkit::String &value, const IConfigurationValueView &src)
	{
		if (src.m_getType(src) != ConfigurationValueType::kString)
			return rkit::ResultCode::kDataError;

		return value.Set(src.m_getValueFuncs.m_getString(src));
	}

	rkit::Result ConfigBuilderKeyValueTableWriter::SerializeField(const rkit::StringSliceView &key, const ConfigValueReadWriteParam &value)
	{
		IConfigurationValueView keyView = priv::ConfigBuilderValueFuncs::CreateViewOfStringSliceView(key);

		ConfigBuilderValue_t valueAsCBV;
		RKIT_CHECK(priv::ConfigBuilderValueFuncs::SetValue(valueAsCBV, value.Read()));

		rkit::ConstSpan<ConfigBuilderKeyValuePair> existingValues = m_keyValueTable.GetValues();

		size_t insertPosMinInclusive = 0;
		size_t insertPosMaxExclusive = existingValues.Count();

		while (insertPosMinInclusive < insertPosMaxExclusive)
		{
			const size_t halfSpanWidth = (insertPosMaxExclusive - insertPosMinInclusive) / 2u;
			const size_t testPos = insertPosMinInclusive + halfSpanWidth;

			const ConfigBuilderKeyValuePair &testPair = existingValues[testPos];

			const IConfigurationValueView candidateView = priv::ConfigBuilderValueFuncs::CreateViewOfValue(testPair.m_key);

			const rkit::Ordering ordering = ConfigurationValueViewComparer::Compare(keyView, candidateView);

			switch (ordering)
			{
			case rkit::Ordering::kEqual:
				m_keyValueTable.Modify()[testPos].m_value = std::move(valueAsCBV);
				return rkit::ResultCode::kOK;
			case rkit::Ordering::kLess:
				insertPosMaxExclusive = testPos;
				break;
			case rkit::Ordering::kGreater:
				insertPosMinInclusive = testPos + 1u;
				break;
			default:
				return rkit::ResultCode::kInternalError;
			}
		}

		ConfigBuilderValue_t keyAsCBV = rkit::String();
		RKIT_CHECK(keyAsCBV.GetAs<rkit::String>().Set(key));

		ConfigBuilderKeyValuePair newPair =
		{
			std::move(keyAsCBV),
			std::move(valueAsCBV)
		};

		RKIT_CHECK(m_keyValueTable.Modify().InsertAt(insertPosMinInclusive, std::move(newPair)));

		return rkit::ResultCode::kOK;
	}
}

namespace anox { namespace priv {
	IConfigurationValueView ConfigBuilderValueFuncs::CreateViewOfValue(const ConfigBuilderValue_t &value)
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

	rkit::Result ConfigBuilderValueFuncs::SetValue(ConfigBuilderValue_t &value, const IConfigurationValueView &view)
	{
		switch (view.m_getType(view))
		{
		case ConfigurationValueType::kUInt64:
			value = view.m_getValueFuncs.m_getUInt64(view);
			break;
		case ConfigurationValueType::kInt64:
			value = view.m_getValueFuncs.m_getInt64(view);
			break;
		case ConfigurationValueType::kFloat64:
			value = view.m_getValueFuncs.m_getFloat64(view);
			break;
		case ConfigurationValueType::kString:
			{
				rkit::String temp;
				RKIT_CHECK(temp.Set(view.m_getValueFuncs.m_getString(view)));
				value = std::move(temp);
			}
			break;
		case ConfigurationValueType::kArray:
			{
				ConfigBuilderValueList list;
				RKIT_CHECK(SetArray(list, view.m_getValueFuncs.m_getArray(view)));
				value = std::move(list);
			}
			break;
		case ConfigurationValueType::kKeyValueTable:
			{
				ConfigBuilderKeyValueTable table;
				RKIT_CHECK(SetKeyValueTable(table, view.m_getValueFuncs.m_keyValueTableFuncs->m_getKeyValuePairs(view)));
				value = std::move(table);
			}
			break;
		default:
			return rkit::ResultCode::kInternalError;
		}

		return rkit::ResultCode::kOK;
	}

	IConfigurationValueView ConfigBuilderValueFuncs::CreateViewOfStringSliceView(const rkit::StringSliceView &value)
	{
		IConfigurationValueView result = {};
		result.m_getType = ConfigReadWriteFuncs::GetFixedType<ConfigurationValueType::kString>;
		result.m_userdataPtr = const_cast<rkit::StringSliceView *>(&value);
		result.m_getValueFuncs.m_getString = GetStringSliceCB;

		return result;
	}

	rkit::Result ConfigBuilderValueFuncs::SetArray(ConfigBuilderValueList &list, const rkit::ISpan<IConfigurationValueView> &values)
	{
		const size_t count = values.Count();
		
		rkit::Vector<ConfigBuilderValue_t> &vector = list.Modify();

		RKIT_CHECK(vector.Resize(count));

		for (size_t i = 0; i < count; i++)
		{
			RKIT_CHECK(SetValue(vector[i], values[i]));
		}

		return rkit::ResultCode::kOK;
	}

	rkit::Result ConfigBuilderValueFuncs::SetKeyValueTable(ConfigBuilderKeyValueTable &list, const rkit::ISpan<IConfigurationKeyValuePair> &values)
	{
		const size_t count = values.Count();

		rkit::Vector<ConfigBuilderKeyValuePair> &vector = list.Modify();

		RKIT_CHECK(vector.Resize(count));

		for (size_t i = 0; i < count; i++)
		{
			IConfigurationKeyValuePair kvp = values[i];
			RKIT_CHECK(SetValue(vector[i].m_key, kvp.m_key));
			RKIT_CHECK(SetValue(vector[i].m_value, kvp.m_value));
		}

		return rkit::ResultCode::kOK;
	}

	ConfigurationValueType ConfigBuilderValueFuncs::GetTypeCB(const IConfigurationValueView &view)
	{
		return static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetType();
	}

	uint64_t ConfigBuilderValueFuncs::GetUInt64CB(const IConfigurationValueView &view)
	{
		return static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<uint64_t>();
	}

	int64_t ConfigBuilderValueFuncs::GetInt64CB(const IConfigurationValueView &view)
	{
		return static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<int64_t>();
	}

	double ConfigBuilderValueFuncs::GetFloat64CB(const IConfigurationValueView &view)
	{
		return static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<double>();
	}

	rkit::StringSliceView ConfigBuilderValueFuncs::GetStringCB(const IConfigurationValueView &view)
	{
		return static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<rkit::String>();
	}

	rkit::StringSliceView ConfigBuilderValueFuncs::GetStringSliceCB(const IConfigurationValueView &view)
	{
		return *static_cast<const rkit::StringSliceView *>(view.m_userdataPtr);
	}

	rkit::CallbackSpan<IConfigurationValueView, const void *> ConfigBuilderValueFuncs::GetArrayCB(const IConfigurationValueView &view)
	{
		const ConfigBuilderValueList &list = static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<ConfigBuilderValueList>();
		
		return rkit::CallbackSpan<IConfigurationValueView, const void *>(GetArrayElementCB, &list, list.GetValues().Count());
	}

	IConfigurationValueView ConfigBuilderValueFuncs::GetArrayElementCB(void const *const &userdata, size_t index)
	{
		const ConfigBuilderValueList *list = static_cast<const ConfigBuilderValueList *>(userdata);

		return CreateViewOfValue(list->GetValues()[index]);
	}

	rkit::Optional<IConfigurationValueView> ConfigBuilderValueFuncs::GetValueFromKeyCB(const IConfigurationValueView &view, const IConfigurationValueView &key)
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

	rkit::CallbackSpan<IConfigurationKeyValuePair, const void *> ConfigBuilderValueFuncs::GetKeyValuePairsCB(const IConfigurationValueView &view)
	{
		const ConfigBuilderKeyValueTable &kvt = static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<ConfigBuilderKeyValueTable>();

		return rkit::CallbackSpan<IConfigurationKeyValuePair, const void *>(GetKeyValueTableKeyValuePairCB, &kvt, kvt.GetValues().Count());
	}

	rkit::CallbackSpan<IConfigurationValueView, const void *> ConfigBuilderValueFuncs::GetKeysCB(const IConfigurationValueView &view)
	{
		const ConfigBuilderKeyValueTable &kvt = static_cast<const ConfigBuilderValue_t *>(view.m_userdataPtr)->GetAs<ConfigBuilderKeyValueTable>();

		return rkit::CallbackSpan<IConfigurationValueView, const void *>(GetKeyValueTableKeyCB, &kvt, kvt.GetValues().Count());
	}

	IConfigurationKeyValuePair ConfigBuilderValueFuncs::GetKeyValueTableKeyValuePairCB(void const *const &userdata, size_t index)
	{
		const ConfigBuilderKeyValueTable &kvt = *static_cast<const ConfigBuilderKeyValueTable *>(userdata);

		const ConfigBuilderKeyValuePair &kvp = kvt.GetValues()[index];

		IConfigurationKeyValuePair result;
		result.m_key = CreateViewOfValue(kvp.m_key);
		result.m_value = CreateViewOfValue(kvp.m_value);

		return result;
	}

	IConfigurationValueView ConfigBuilderValueFuncs::GetKeyValueTableKeyCB(void const *const &userdata, size_t index)
	{
		const ConfigBuilderKeyValueTable &kvt = *static_cast<const ConfigBuilderKeyValueTable *>(userdata);

		const ConfigBuilderKeyValuePair &kvp = kvt.GetValues()[index];

		return CreateViewOfValue(kvp.m_key);
	}

	IConfigurationKeyValueTableFuncs ConfigBuilderValueFuncs::ms_keyValueTableFuncs =
	{
		GetValueFromKeyCB,
		GetKeyValuePairsCB,
		GetKeysCB,
	};
} } // anox::priv
