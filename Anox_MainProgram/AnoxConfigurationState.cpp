#include "AnoxConfigurationState.h"

#include "rkit/Core/StringView.h"

namespace anox
{
	rkit::Ordering ConfigurationValueViewComparer::Compare(const IConfigurationValueView &a, const IConfigurationValueView &b)
	{
		ConfigurationValueType typeA = a.m_getType(a);
		ConfigurationValueType typeB = b.m_getType(b);

		const rkit::StrongComparePred pred;

		if (typeA != typeB)
			return pred(typeA, typeB);

		switch (typeA)
		{
		case ConfigurationValueType::kUInt64:
			return pred(a.m_getValueFuncs.m_getUInt64(a), b.m_getValueFuncs.m_getUInt64(b));
		case ConfigurationValueType::kInt64:
			return pred(a.m_getValueFuncs.m_getInt64(a), b.m_getValueFuncs.m_getInt64(b));
		case ConfigurationValueType::kFloat64:
			return pred(a.m_getValueFuncs.m_getFloat64(a), b.m_getValueFuncs.m_getFloat64(b));
		case ConfigurationValueType::kString:
			return pred(a.m_getValueFuncs.m_getString(a), b.m_getValueFuncs.m_getString(b));
		case ConfigurationValueType::kArray:
			return CompareArrays(a.m_getValueFuncs.m_getArray(a), b.m_getValueFuncs.m_getArray(b));
		case ConfigurationValueType::kKeyValueTable:
			return CompareKeyValuePairs(a.m_getValueFuncs.m_keyValueTableFuncs->m_getKeyValuePairs(a), b.m_getValueFuncs.m_keyValueTableFuncs->m_getKeyValuePairs(b));
		default:
			return rkit::Ordering::kUnordered;
		}
	}

	rkit::Ordering ConfigurationValueViewComparer::CompareArrays(const rkit::ISpan<IConfigurationValueView> &a, const rkit::ISpan<IConfigurationValueView> &b)
	{
		const size_t countA = a.Count();
		const size_t countB = b.Count();
		const size_t lesserCount = rkit::Min(countA, countB);

		const rkit::StrongComparePred pred;

		for (size_t i = 0; i < lesserCount; i++)
		{
			const rkit::Ordering ordering = Compare(a[i], b[i]);

			if (ordering != rkit::Ordering::kEqual)
				return ordering;
		}

		return pred(countA, countB);
	}

	rkit::Ordering ConfigurationValueViewComparer::CompareKeyValuePairs(const rkit::ISpan<IConfigurationKeyValuePair> &a, const rkit::ISpan<IConfigurationKeyValuePair> &b)
	{
		const size_t countA = a.Count();
		const size_t countB = b.Count();
		const size_t lesserCount = rkit::Min(countA, countB);

		const rkit::StrongComparePred pred;

		for (size_t i = 0; i < lesserCount; i++)
		{
			const IConfigurationKeyValuePair kvpA = a[i];
			const IConfigurationKeyValuePair kvpB = b[i];

			const rkit::Ordering keyOrdering = Compare(kvpA.m_key, kvpA.m_value);

			if (keyOrdering != rkit::Ordering::kEqual)
				return keyOrdering;

			const rkit::Ordering valueOrdering = Compare(kvpA.m_value, kvpA.m_value);

			if (valueOrdering != rkit::Ordering::kEqual)
				return valueOrdering;
		}

		return pred(countA, countB);
	}
}
