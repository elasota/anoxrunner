#pragma once

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/Tuple.h"
#include "rkit/Core/StringProto.h"

#include <stdint.h>

namespace rkit
{
	template<class T>
	class Optional;

	template<class T, class TUserdata>
	class CallbackSpan;
}

namespace anox
{
	struct IConfigurationValueView;
	struct IConfigurationKeyValueTableFuncs;
	struct IConfigurationArrayFuncs;
	struct IConfigurationValueViewComparer;
	struct IConfigurationKeyValuePair;

	enum class ConfigurationValueType
	{
		kUInt64,
		kInt64,
		kFloat64,
		kString,
		kArray,
		kKeyValueTable,

		kUndefined,
	};

	union IConfigurationValueGetUnion
	{
		uint64_t (*m_getUInt64)(const IConfigurationValueView &view);
		int64_t (*m_getInt64)(const IConfigurationValueView &view);
		double (*m_getFloat64)(const IConfigurationValueView &view);
		rkit::StringView (*m_getString)(const IConfigurationValueView &view);
		rkit::CallbackSpan<IConfigurationValueView, const void*> (*m_getArray)(const IConfigurationValueView &view);
		const IConfigurationKeyValueTableFuncs *m_keyValueTableFuncs;
	};

	struct ConfigurationValueViewComparer
	{
		static rkit::Ordering Compare(const IConfigurationValueView &a, const IConfigurationValueView &b);
		static rkit::Ordering CompareArrays(const rkit::ISpan<IConfigurationValueView> &a, const rkit::ISpan<IConfigurationValueView> &b);
		static rkit::Ordering CompareKeyValuePairs(const rkit::ISpan<IConfigurationKeyValuePair> &a, const rkit::ISpan<IConfigurationKeyValuePair> &b);
	};

	struct IConfigurationValueView final : public rkit::CompareWithOrderingOperatorsMixin<IConfigurationValueView, ConfigurationValueViewComparer>
	{
		const void *m_userdataPtr;
		uint64_t m_userdataUInt;

		ConfigurationValueType (*m_getType)(const IConfigurationValueView &view);
		IConfigurationValueGetUnion m_getValueFuncs;
	};

	struct IConfigurationKeyValuePair
	{
		IConfigurationValueView m_key;
		IConfigurationValueView m_value;
	};

	struct IConfigurationKeyValueTableFuncs
	{
		rkit::Optional<IConfigurationValueView> (*m_getValueFromKey)(const IConfigurationValueView &view, const IConfigurationValueView &key);
		rkit::CallbackSpan<IConfigurationKeyValuePair, const void *> (*m_getKeyValuePairs)(const IConfigurationValueView &view);
		rkit::CallbackSpan<IConfigurationValueView, const void *> (*m_getKeys)(const IConfigurationValueView &view);
	};

	struct IConfigurationState
	{
		virtual ~IConfigurationState() {}

		virtual IConfigurationValueView GetRoot() const = 0;
	};
}
