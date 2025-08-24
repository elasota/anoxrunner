#include "AnoxConfigurationState.h"

#include "rkit/Core/Optional.h"
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

	rkit::Optional<IConfigurationValueView> IConfigurationKeyValueTableView::GetValueFromKey(const IConfigurationValueView &key)
	{
		return m_view.m_getValueFuncs.m_keyValueTableFuncs->m_getValueFromKey(m_view, key);
	}

	rkit::Optional<IConfigurationValueView> IConfigurationKeyValueTableView::GetValueFromKey(const rkit::StringSliceView &key)
	{
		IConfigurationValueView keyView = {};
		keyView.m_getType = GetStaticTypeCB<ConfigurationValueType::kString>;
		keyView.m_userdataPtr = &key;
		keyView.m_getValueFuncs.m_getString = DerefStringSliceViewCB;

		return GetValueFromKey(keyView);
	}

	rkit::StringSliceView IConfigurationKeyValueTableView::DerefStringSliceViewCB(const IConfigurationValueView &view)
	{
		return *static_cast<const rkit::StringSliceView *>(view.m_userdataPtr);
	}

	// Defaults that return nothing, for default ctor
	rkit::Optional<IConfigurationValueView> IConfigurationKeyValueTableView::GetValueFromKeyCB(const IConfigurationValueView &view, const IConfigurationValueView &key)
	{
		return rkit::Optional<IConfigurationValueView>();
	}

	rkit::CallbackSpan<IConfigurationKeyValuePair, const void *> IConfigurationKeyValueTableView::GetKeyValuePairsCB(const IConfigurationValueView &view)
	{
		return rkit::CallbackSpan<IConfigurationKeyValuePair, const void *>();
	}

	rkit::CallbackSpan<IConfigurationValueView, const void *> IConfigurationKeyValueTableView::GetKeysCB(const IConfigurationValueView &view)
	{
		return rkit::CallbackSpan<IConfigurationValueView, const void *>();
	}

	const IConfigurationKeyValueTableFuncs IConfigurationKeyValueTableView::ms_keyValueTableFuncs =
	{
		IConfigurationKeyValueTableView::GetValueFromKeyCB,
		IConfigurationKeyValueTableView::GetKeyValuePairsCB,
		IConfigurationKeyValueTableView::GetKeysCB,
	};



	bool IConfigurationValueView::GetAs(float &outFloat) const
	{
		double d = 0.0;
		if (!GetAs(d))
			return false;

		outFloat = static_cast<float>(d);
		return true;
	}

	bool IConfigurationValueView::GetAs(double &outFloat) const
	{
		switch (m_getType(*this))
		{
		case ConfigurationValueType::kInt64:
			outFloat = static_cast<double>(m_getValueFuncs.m_getInt64(*this));
			return true;
		case ConfigurationValueType::kUInt64:
			outFloat = static_cast<double>(m_getValueFuncs.m_getUInt64(*this));
			return true;
		case ConfigurationValueType::kFloat64:
			outFloat = m_getValueFuncs.m_getFloat64(*this);
			return true;
		default:
			return false;
		}
	}

	bool IConfigurationValueView::GetAs(rkit::StringSliceView &outString) const
	{
		if (m_getType(*this) != ConfigurationValueType::kString)
			return false;

		outString = m_getValueFuncs.m_getString(*this);
		return true;
	}

	bool IConfigurationValueView::GetAs(IConfigurationArrayView &outArray) const
	{
		if (m_getType(*this) != ConfigurationValueType::kArray)
			return false;

		outArray = IConfigurationArrayView(m_getValueFuncs.m_getArray(*this));
		return true;
	}

	bool IConfigurationValueView::GetAs(IConfigurationKeyValueTableView &outKeyValueTable) const
	{
		if (m_getType(*this) != ConfigurationValueType::kKeyValueTable)
			return false;

		outKeyValueTable = IConfigurationKeyValueTableView(*this);
		return true;
	}

	template<class T>
	bool IConfigurationValueView::GetAsUInt(T &outUInt) const
	{
		switch (m_getType(*this))
		{
		case ConfigurationValueType::kInt64:
			{
				int64_t i64 = m_getValueFuncs.m_getInt64(*this);
				if (i64 < 0)
					return false;
				uint64_t u64 = static_cast<uint64_t>(i64);
				if (u64 > std::numeric_limits<T>::max())
					return false;

				outUInt = static_cast<T>(u64);
			}
			return true;
		case ConfigurationValueType::kUInt64:
			{
				uint64_t u64 = m_getValueFuncs.m_getUInt64(*this);
				if (u64 > std::numeric_limits<T>::max())
					return false;

				outUInt = static_cast<T>(u64);
			}
			return true;
		case ConfigurationValueType::kFloat64:
			{
				constexpr double maxValueExcl = static_cast<double>(std::numeric_limits<T>::max() / 2u + 1u) * 2.0;

				double d = m_getValueFuncs.m_getFloat64(*this);
				if (!(d < maxValueExcl && d >= 0))
					return false;

				outUInt = static_cast<T>(floor(d));
			}
			return true;
		default:
			return false;
		}
	}

	template<class T>
	bool IConfigurationValueView::GetAsSInt(T &outSInt) const
	{
		switch (m_getType(*this))
		{
		case ConfigurationValueType::kInt64:
			{
				int64_t i64 = m_getValueFuncs.m_getInt64(*this);
				if (i64 < std::numeric_limits<T>::min())
					return false;
				if (i64 > std::numeric_limits<T>::max())
					return false;

				outSInt = static_cast<T>(i64);
			}
			return true;
		case ConfigurationValueType::kUInt64:
			{
				uint64_t u64 = m_getValueFuncs.m_getUInt64(*this);
				if (u64 > static_cast<uint64_t>(std::numeric_limits<T>::max()))
					return false;

				outSInt = static_cast<T>(u64);
			}
			return true;
		case ConfigurationValueType::kFloat64:
			{
				constexpr double minValueIncl = static_cast<double>(std::numeric_limits<T>::min());

				double d = m_getValueFuncs.m_getFloat64(*this);

				if (d < 0)
				{
					d = ceil(d);
					if (d < minValueIncl)
						return false;
				}
				else
				{
					constexpr double maxValueExcl = -minValueIncl;

					// This catches NaN too
					if (!(d < maxValueExcl))
						return false;

					d = floor(d);
				}

				outSInt = static_cast<T>(d);
			}
			return true;
		default:
			return false;
		}
	}
}
