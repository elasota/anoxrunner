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
	class IConfigurationArrayView;
	class IConfigurationKeyValueTableView;

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
		rkit::StringSliceView (*m_getString)(const IConfigurationValueView &view);
		rkit::CallbackSpan<IConfigurationValueView, const void *> (*m_getArray)(const IConfigurationValueView &view);
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

		bool GetAs(uint8_t &outUInt) const;
		bool GetAs(uint16_t &outUInt) const;
		bool GetAs(uint32_t &outUInt) const;
		bool GetAs(uint64_t &outUInt) const;

		bool GetAs(int8_t &outInt) const;
		bool GetAs(int16_t &outInt) const;
		bool GetAs(int32_t &outInt) const;
		bool GetAs(int64_t &outInt) const;

		bool GetAs(float &outFloat) const;
		bool GetAs(double &outFloat) const;

		bool GetAs(rkit::StringSliceView &outString) const;
		bool GetAs(IConfigurationArrayView &outArray) const;
		bool GetAs(IConfigurationKeyValueTableView &outKeyValueTable) const;

		template<class T>
		rkit::Result Get(T &outValue) const;

	private:
		template<class T>
		bool GetAsUInt(T &outUInt) const;

		template<class T>
		bool GetAsSInt(T &outSInt) const;
	};

	class IConfigurationArrayView
	{
	public:
		IConfigurationArrayView();
		explicit IConfigurationArrayView(const rkit::CallbackSpan<IConfigurationValueView, const void *> &array);

	private:
		rkit::CallbackSpan<IConfigurationValueView, const void *> m_array;
	};

	class IConfigurationKeyValueTableView
	{
	public:
		IConfigurationKeyValueTableView();
		explicit IConfigurationKeyValueTableView(const IConfigurationValueView &view);

		template<class TKey>
		rkit::Result GetValueFromKey(const TKey &key, IConfigurationValueView &outValue);

		rkit::Optional<IConfigurationValueView> GetValueFromKey(const IConfigurationValueView &key);
		rkit::Optional<IConfigurationValueView> GetValueFromKey(const rkit::StringSliceView &key);

	private:
		static rkit::StringSliceView DerefStringSliceViewCB(const IConfigurationValueView &view);

		template<ConfigurationValueType TType>
		static ConfigurationValueType GetStaticTypeCB(const IConfigurationValueView &view);

		static rkit::Optional<IConfigurationValueView> GetValueFromKeyCB(const IConfigurationValueView &view, const IConfigurationValueView &key);
		static rkit::CallbackSpan<IConfigurationKeyValuePair, const void *> GetKeyValuePairsCB(const IConfigurationValueView &view);
		static rkit::CallbackSpan<IConfigurationValueView, const void *> GetKeysCB(const IConfigurationValueView &view);

		static const IConfigurationKeyValueTableFuncs ms_keyValueTableFuncs;

		IConfigurationValueView m_view;
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

#include "rkit/Core/RKitAssert.h"

namespace anox
{
	template<class T>
	rkit::Result IConfigurationValueView::Get(T &outValue) const
	{
		if (!this->GetAs(outValue))
			return rkit::ResultCode::kInvalidParameter;

		return rkit::ResultCode::kOK;
	}

	inline IConfigurationArrayView::IConfigurationArrayView()
	{
	}

	inline IConfigurationArrayView::IConfigurationArrayView(const rkit::CallbackSpan<IConfigurationValueView, const void *> &array)
		: m_array(array)
	{
	}

	inline IConfigurationKeyValueTableView::IConfigurationKeyValueTableView()
		: m_view{}
	{
		m_view.m_getType = GetStaticTypeCB<ConfigurationValueType::kKeyValueTable>;
		m_view.m_getValueFuncs.m_keyValueTableFuncs = &ms_keyValueTableFuncs;
	}

	inline IConfigurationKeyValueTableView::IConfigurationKeyValueTableView(const IConfigurationValueView &view)
		: m_view(view)
	{
		RKIT_ASSERT(view.m_getType(view) == ConfigurationValueType::kKeyValueTable);
	}

	inline bool IConfigurationValueView::GetAs(uint8_t &outUInt) const
	{
		return this->GetAsUInt(outUInt);
	}

	inline bool IConfigurationValueView::GetAs(uint16_t &outUInt) const
	{
		return this->GetAsUInt(outUInt);
	}

	inline bool IConfigurationValueView::GetAs(uint32_t &outUInt) const
	{
		return this->GetAsUInt(outUInt);
	}

	inline bool IConfigurationValueView::GetAs(uint64_t &outUInt) const
	{
		return this->GetAsUInt(outUInt);
	}

	inline bool IConfigurationValueView::GetAs(int8_t &outInt) const
	{
		return this->GetAsSInt(outInt);
	}

	inline bool IConfigurationValueView::GetAs(int16_t &outInt) const
	{
		return this->GetAsSInt(outInt);
	}

	inline bool IConfigurationValueView::GetAs(int32_t &outInt) const
	{
		return this->GetAsSInt(outInt);
	}

	inline bool IConfigurationValueView::GetAs(int64_t &outInt) const
	{
		return this->GetAsSInt(outInt);
	}

	template<class TKey>
	inline rkit::Result IConfigurationKeyValueTableView::GetValueFromKey(const TKey &key, IConfigurationValueView &outValue)
	{
		rkit::Optional<IConfigurationValueView> result = this->GetValueFromKey(key);
		if (!result.IsSet())
			return rkit::ResultCode::kKeyNotFound;

		outValue = result.Get();
		return rkit::ResultCode::kOK;
	}

	template<ConfigurationValueType TType>
	ConfigurationValueType IConfigurationKeyValueTableView::GetStaticTypeCB(const IConfigurationValueView &view)
	{
		return TType;
	}
}
