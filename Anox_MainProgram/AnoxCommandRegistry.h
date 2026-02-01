#pragma once

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/String.h"
#include "rkit/Core/HashValue.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox
{
	class AnoxCommandStackBase;

	struct AnoxRegisteredCommand
	{
		typedef rkit::coro::MethodStarter<void(AnoxCommandStackBase &, const rkit::ISpan<rkit::ByteStringView> &args)> MethodStarter_t;

		inline const MethodStarter_t &AsyncCall() const
		{
			return m_methodStarter;
		}

		MethodStarter_t m_methodStarter;
	};

	struct AnoxRegisteredAlias
	{
		rkit::ByteString m_text;
	};

	enum class AnoxConsoleVarType
	{
		kString,
		kFloat32,
		kFloat64,
		kInt8,
		kInt16,
		kInt32,
		kInt64,
		kUInt8,
		kUInt16,
		kUInt32,
		kUInt64,
	};

	struct AnoxRegisteredConsoleVar
	{
		AnoxConsoleVarType m_varType;
		void *m_value;
	};

	class AnoxPrehashedRegistryKeyView
	{
	public:
		AnoxPrehashedRegistryKeyView() = default;
		AnoxPrehashedRegistryKeyView(const rkit::ByteStringSliceView &name);

		template<size_t TSize>
		AnoxPrehashedRegistryKeyView(const rkit::Utf8Char_t (&chars)[TSize]);

		const rkit::ByteStringSliceView &GetName() const;
		rkit::HashValue_t GetHash() const;

		static uint8_t NormalizeChar(uint8_t ch);

		AnoxPrehashedRegistryKeyView &operator=(const AnoxPrehashedRegistryKeyView &other) = default;

	private:
		static rkit::HashValue_t InitHash(const rkit::ByteStringSliceView &name);

		rkit::ByteStringSliceView m_name;
		rkit::HashValue_t m_hash = 0;
	};

	class AnoxCommandRegistryBase
	{
	public:
		virtual ~AnoxCommandRegistryBase() {}

		virtual rkit::Result RegisterCommand(const AnoxPrehashedRegistryKeyView &name, const AnoxRegisteredCommand::MethodStarter_t &methodStarter) = 0;
		virtual rkit::Result RegisterAlias(const AnoxPrehashedRegistryKeyView &name, const rkit::ByteStringSliceView &commandText) = 0;
		virtual rkit::Result RegisterConsoleVar(const AnoxPrehashedRegistryKeyView &name, AnoxConsoleVarType varType, void *varValue) = 0;

		virtual const AnoxRegisteredCommand *FindCommand(const AnoxPrehashedRegistryKeyView &name) const = 0;
		virtual const AnoxRegisteredAlias *FindAlias(const AnoxPrehashedRegistryKeyView &name) const = 0;
		virtual const AnoxRegisteredConsoleVar *FindConsoleVar(const AnoxPrehashedRegistryKeyView &name) const = 0;

		virtual rkit::Result TrySetCVar(const AnoxRegisteredConsoleVar &cvar, const AnoxPrehashedRegistryKeyView &str, bool &outSetOK) const = 0;

		static bool RequiresEscape(const rkit::ByteStringSliceView &token);
		static rkit::Result EscapeToken(rkit::ByteString &outString, const rkit::ByteStringSliceView &token);

		static rkit::Result Create(rkit::UniquePtr<AnoxCommandRegistryBase> &outRegistry);
	};
}

#include "rkit/Core/StringView.h"

namespace anox
{
	template<size_t TSize>
	AnoxPrehashedRegistryKeyView::AnoxPrehashedRegistryKeyView(const rkit::Utf8Char_t(&chars)[TSize])
		: m_name(rkit::StringView(chars).RemoveEncoding())
		, m_hash(InitHash(m_name))
	{
	}

	inline AnoxPrehashedRegistryKeyView::AnoxPrehashedRegistryKeyView(const rkit::ByteStringSliceView &name)
		: m_name(name)
		, m_hash(InitHash(m_name))
	{
	}

	inline const rkit::ByteStringSliceView &AnoxPrehashedRegistryKeyView::GetName() const
	{
		return m_name;
	}

	inline rkit::HashValue_t AnoxPrehashedRegistryKeyView::GetHash() const
	{
		return m_hash;
	}

	inline uint8_t AnoxPrehashedRegistryKeyView::NormalizeChar(uint8_t ch)
	{
		if (ch >= 'A' && ch <= 'Z')
			return ch - 'A' + 'a';
		else
			return ch;
	}
}
