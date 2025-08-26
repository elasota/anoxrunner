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

	template<class TSrc, class TDest>
	struct ConfigReadWriteConverter
	{
		static IConfigurationValueView Read(const void *value);
		static rkit::Result Write(void *value, const IConfigurationValueView &src);
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
		ConfigBuilderValue_t m_root;
	};

	template<class T>
	struct ConfigValueReadWrite
	{
	};

	template<>
	struct ConfigValueReadWrite<rkit::String>
	{
		static IConfigurationValueView Read(const rkit::String &value);
		static rkit::Result Write(rkit::String &value, const IConfigurationValueView &src);
	};

	template<>
	struct ConfigValueReadWrite<ConfigBuilderValue_t>
	{
		static IConfigurationValueView Read(const ConfigBuilderValue_t &value);
		static rkit::Result Write(ConfigBuilderValue_t &value, const IConfigurationValueView &src);
	};

	class ConfigValueReadWriteParam
	{
	public:
		typedef IConfigurationValueView (*ReadFunc_t)(const void *value);
		typedef rkit::Result (*WriteFunc_t)(void *value, const IConfigurationValueView &src);

		template<class T>
		ConfigValueReadWriteParam(T &ref);

		IConfigurationValueView Read() const;
		rkit::Result Write(const IConfigurationValueView &value) const;

	private:
		ConfigValueReadWriteParam() = delete;
		ConfigValueReadWriteParam(const ConfigValueReadWriteParam &) = delete;

		template<class T>
		static IConfigurationValueView AutoReadCB(const void *value);

		template<class T>
		static rkit::Result AutoWriteCB(void *value, const IConfigurationValueView &src);

		void *m_value;
		ReadFunc_t m_readFunc;
		WriteFunc_t m_writeFunc;
	};

	struct IConfigKeyValueTableSerializer
	{
		virtual rkit::Result SerializeField(const rkit::StringSliceView &key, const ConfigValueReadWriteParam &value) = 0;
	};

	class ConfigBuilderKeyValueTableWriter final : public IConfigKeyValueTableSerializer
	{
	public:
		explicit ConfigBuilderKeyValueTableWriter(ConfigBuilderKeyValueTable &keyValueTable);

		rkit::Result SerializeField(const rkit::StringSliceView &key, const ConfigValueReadWriteParam &value) override;

	private:
		ConfigBuilderKeyValueTableWriter() = delete;

		ConfigBuilderKeyValueTable &m_keyValueTable;
	};
}

namespace anox
{
	inline ConfigBuilderKeyValueTableWriter::ConfigBuilderKeyValueTableWriter(ConfigBuilderKeyValueTable &keyValueTable)
		: m_keyValueTable(keyValueTable)
	{
	}

	template<class T>
	ConfigValueReadWriteParam::ConfigValueReadWriteParam(T &ref)
		: m_value(&ref)
		, m_readFunc(AutoReadCB<T>)
		, m_writeFunc(AutoWriteCB<T>)
	{
	}

	inline IConfigurationValueView ConfigValueReadWriteParam::Read() const
	{
		return m_readFunc(m_value);
	}

	inline rkit::Result ConfigValueReadWriteParam::Write(const IConfigurationValueView &value) const
	{
		return m_writeFunc(m_value, value);
	}

	template<class T>
	IConfigurationValueView ConfigValueReadWriteParam::AutoReadCB(const void *value)
	{
		return ConfigValueReadWrite<T>::Read(*static_cast<const T *>(value));
	}

	template<class T>
	rkit::Result ConfigValueReadWriteParam::AutoWriteCB(void *value, const IConfigurationValueView &src)
	{
		return ConfigValueReadWrite<T>::Write(*static_cast<T *>(value), src);
	}
}
