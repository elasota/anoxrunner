#pragma once

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/Result.h"
#include "rkit/Core/SpanProtos.h"
#include "rkit/Core/String.h"
#include "rkit/Core/TypeTaggedUnion.h"
#include "rkit/Core/Vector.h"

#include "AnoxConfigurationState.h"

#include <stdint.h>

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox
{
	struct ConfigBuilderKeyValuePair;
	class ConfigBuilderKeyValueTable;
	class ConfigBuilderValueList;
}

namespace anox { namespace priv {
	struct ConfigBuilderKeyValueTablePreds
	{
		template<class TPred>
		static bool CompareWithPred(const ConfigBuilderKeyValueTable &a, const ConfigBuilderKeyValueTable &b, const TPred &pred);
	};
} }

namespace anox
{
	typedef rkit::TypeTaggedUnion<ConfigurationValueType,
		uint64_t,
		int64_t,
		double,
		rkit::String,
		ConfigBuilderValueList,
		ConfigBuilderKeyValueTable
	> ConfigBuilderValue_t;

	class ConfigBuilderKeyValueTable final : public rkit::CompareWithOrderingOperatorsMixin<ConfigBuilderKeyValueTable, priv::ConfigBuilderKeyValueTablePreds>
	{
	public:
		ConfigBuilderKeyValueTable() = default;
		ConfigBuilderKeyValueTable(ConfigBuilderKeyValueTable &&other) = default;

		ConfigBuilderKeyValueTable &operator=(ConfigBuilderKeyValueTable &&other) = default;

		rkit::Vector<ConfigBuilderKeyValuePair> &Modify();
		rkit::ConstSpan<ConfigBuilderKeyValuePair> GetValues() const;

		rkit::Ordering Compare(const ConfigBuilderKeyValueTable &other);

	private:
		rkit::Vector<ConfigBuilderKeyValuePair> m_values;
	};

	class ConfigBuilderValueList final : public rkit::CompareWithOrderingOperatorsMixin<ConfigBuilderValueList, priv::ConfigBuilderKeyValueTablePreds>
	{
	public:
		ConfigBuilderValueList() = default;
		ConfigBuilderValueList(ConfigBuilderValueList &&other) = default;
		~ConfigBuilderValueList() = default;

		ConfigBuilderValueList &operator=(ConfigBuilderValueList &&other) = default;

		rkit::SpanIterator<ConfigBuilderValue_t> begin();
		rkit::ConstSpanIterator<ConfigBuilderValue_t> begin() const;

		rkit::SpanIterator<ConfigBuilderValue_t> end();
		rkit::ConstSpanIterator<ConfigBuilderValue_t> end() const;

		rkit::Vector<ConfigBuilderValue_t> &Modify();
		rkit::ConstSpan<ConfigBuilderValue_t> GetValues() const;

		rkit::Ordering Compare(const ConfigBuilderValueList &other);

	private:
		rkit::Vector<ConfigBuilderValue_t> m_values;
	};

	struct ConfigBuilderKeyValuePair
	{
		ConfigBuilderValue_t m_key;
		ConfigBuilderValue_t m_value;
	};

	class ConfigurationValueRootState final : public IConfigurationState
	{
	public:
		ConfigurationValueRootState() = delete;
		explicit ConfigurationValueRootState(ConfigBuilderValue_t &&root);

		IConfigurationValueView GetRoot() const override;

	private:
		static IConfigurationValueView CreateViewOfValue(const ConfigBuilderValue_t &value);
		static ConfigurationValueType GetTypeCB(const IConfigurationValueView &view);
		static uint64_t GetUInt64CB(const IConfigurationValueView &view);
		static int64_t GetInt64CB(const IConfigurationValueView &view);
		static double GetFloat64CB(const IConfigurationValueView &view);
		static rkit::StringView GetStringCB(const IConfigurationValueView &view);
		static rkit::CallbackSpan<IConfigurationValueView, const void *> GetArrayCB(const IConfigurationValueView &view);

		static IConfigurationValueView GetArrayElementCB(void const* const& userdata, size_t index);

		static rkit::Optional<IConfigurationValueView> GetValueFromKeyCB(const IConfigurationValueView &view, const IConfigurationValueView &key);
		static rkit::CallbackSpan<IConfigurationKeyValuePair, const void *> GetKeyValuePairsCB(const IConfigurationValueView &view);
		static rkit::CallbackSpan<IConfigurationValueView, const void *> GetKeysCB(const IConfigurationValueView &view);

		static IConfigurationKeyValuePair GetKeyValueTableKeyValuePairCB(void const *const &userdata, size_t index);
		static IConfigurationValueView GetKeyValueTableKeyCB(void const *const &userdata, size_t index);

		ConfigBuilderValue_t m_root;

		static IConfigurationKeyValueTableFuncs ms_keyValueTableFuncs;
	};
}

namespace anox
{
}
