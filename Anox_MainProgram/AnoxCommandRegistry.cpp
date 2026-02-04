#include "AnoxCommandRegistry.h"

#include "rkit/Core/HashTable.h"

namespace anox
{
	class AnoxCommandRegistry final : public AnoxCommandRegistryBase
	{
	public:
		AnoxCommandRegistry();

		rkit::Result RegisterCommand(const AnoxPrehashedRegistryKeyView &name, const AnoxRegisteredCommand::MethodStarter_t &methodStarter) override;
		rkit::Result RegisterAlias(const AnoxPrehashedRegistryKeyView &name, const rkit::ByteStringSliceView &commandText) override;
		rkit::Result RegisterConsoleVar(const AnoxPrehashedRegistryKeyView &name, AnoxConsoleVarType varType, void *varValue) override;

		const AnoxRegisteredCommand *FindCommand(const AnoxPrehashedRegistryKeyView &name) const override;
		const AnoxRegisteredAlias *FindAlias(const AnoxPrehashedRegistryKeyView &name) const override;
		const AnoxRegisteredConsoleVar *FindConsoleVar(const AnoxPrehashedRegistryKeyView &name) const override;

		rkit::Result TrySetCVar(const AnoxRegisteredConsoleVar &cvar, const AnoxPrehashedRegistryKeyView &name, bool &outSetOK) const override;

	private:
		struct RegistryKey
		{
			rkit::ByteString m_str;

			bool operator==(const AnoxPrehashedRegistryKeyView &other) const;
			bool operator!=(const AnoxPrehashedRegistryKeyView &other) const;

			bool operator==(const RegistryKey &other) const;
			bool operator!=(const RegistryKey &other) const;
		};

		static rkit::Result CreateNormalizedKey(RegistryKey &outKey, const AnoxPrehashedRegistryKeyView &inKey);

		rkit::HashMap<RegistryKey, AnoxRegisteredCommand> m_commands;
		rkit::HashMap<RegistryKey, AnoxRegisteredAlias> m_aliases;
		rkit::HashMap<RegistryKey, AnoxRegisteredConsoleVar> m_consoleVars;
	};

	rkit::HashValue_t AnoxPrehashedRegistryKeyView::InitHash(const rkit::ByteStringSliceView &name)
	{
		rkit::HashValue_t result = 0;

		for (uint8_t ch : name)
			result = rkit::Hasher<uint8_t>::ComputeHash(result, NormalizeChar(ch));

		return result;
	}

	AnoxCommandRegistry::AnoxCommandRegistry()
	{
	}

	rkit::Result AnoxCommandRegistry::RegisterCommand(const AnoxPrehashedRegistryKeyView &name, const AnoxRegisteredCommand::MethodStarter_t &methodStarter)
	{
		AnoxRegisteredCommand cmd{ methodStarter };

		RegistryKey key;
		RKIT_CHECK(CreateNormalizedKey(key, name));

		RKIT_CHECK(m_commands.SetPrehashed(name.GetHash(), std::move(key), std::move(cmd)));

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxCommandRegistry::RegisterAlias(const AnoxPrehashedRegistryKeyView &name, const rkit::ByteStringSliceView &commandText)
	{
		AnoxRegisteredAlias alias;

		RKIT_CHECK(alias.m_text.Set(commandText));

		RegistryKey key;
		RKIT_CHECK(CreateNormalizedKey(key, name));

		RKIT_CHECK(m_aliases.SetPrehashed(name.GetHash(), std::move(key), std::move(alias)));

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxCommandRegistry::RegisterConsoleVar(const AnoxPrehashedRegistryKeyView &name, AnoxConsoleVarType varType, void *varValue)
	{
		AnoxRegisteredConsoleVar consoleVar;

		consoleVar.m_varType = varType;
		consoleVar.m_value = varValue;

		RegistryKey key;
		RKIT_CHECK(CreateNormalizedKey(key, name));

		RKIT_CHECK(m_consoleVars.SetPrehashed(name.GetHash(), std::move(key), std::move(consoleVar)));

		RKIT_RETURN_OK;
	}

	const AnoxRegisteredCommand *AnoxCommandRegistry::FindCommand(const AnoxPrehashedRegistryKeyView &name) const
	{
		rkit::HashMap<RegistryKey, AnoxRegisteredCommand>::ConstIterator_t it = m_commands.FindPrehashed(name.GetHash(), name);

		if (it == m_commands.end())
			return nullptr;
		else
			return &it.Value();
	}

	const AnoxRegisteredAlias *AnoxCommandRegistry::FindAlias(const AnoxPrehashedRegistryKeyView &name) const
	{
		rkit::HashMap<RegistryKey, AnoxRegisteredAlias>::ConstIterator_t it = m_aliases.FindPrehashed(name.GetHash(), name);

		if (it == m_aliases.end())
			return nullptr;
		else
			return &it.Value();
	}

	const AnoxRegisteredConsoleVar *AnoxCommandRegistry::FindConsoleVar(const AnoxPrehashedRegistryKeyView &name) const
	{
		rkit::HashMap<RegistryKey, AnoxRegisteredConsoleVar>::ConstIterator_t it = m_consoleVars.FindPrehashed(name.GetHash(), name);

		if (it == m_consoleVars.end())
			return nullptr;
		else
			return &it.Value();
	}

	rkit::Result AnoxCommandRegistry::TrySetCVar(const AnoxRegisteredConsoleVar &cvar, const AnoxPrehashedRegistryKeyView &name, bool &outSetOK) const
	{
		rkit::IUtilitiesDriver *utils = rkit::GetDrivers().m_utilitiesDriver;

		switch (cvar.m_varType)
		{
		default:
			RKIT_THROW(rkit::ResultCode::kInternalError);
		}
	}

	bool AnoxCommandRegistry::RegistryKey::operator==(const AnoxPrehashedRegistryKeyView &other) const
	{
		if (m_str.Length() != other.GetName().Length())
			return false;

		const size_t len = m_str.Length();
		const uint8_t *thisChars = m_str.CStr();
		const uint8_t *otherChars = other.GetName().GetChars();

		rkit::Optional<size_t> inexactStart;
		for (size_t i = 0; i < len; i++)
		{
			if (otherChars[i] != thisChars[i])
			{
				inexactStart = i;
				break;
			}
		}

		// Most of the time, if there is an exact match, it's case-equal too
		if (!inexactStart.IsSet())
			return true;

		for (size_t i = inexactStart.Get(); i < len; i++)
		{
			if (AnoxPrehashedRegistryKeyView::NormalizeChar(otherChars[i]) != thisChars[i])
				return false;
		}

		return true;
	}

	bool AnoxCommandRegistry::RegistryKey::operator!=(const AnoxPrehashedRegistryKeyView &other) const
	{
		return !((*this) == other);
	}

	bool AnoxCommandRegistry::RegistryKey::operator==(const RegistryKey &other) const
	{
		return m_str == other.m_str;
	}

	bool AnoxCommandRegistry::RegistryKey::operator!=(const RegistryKey &other) const
	{
		return !((*this) == other);
	}

	rkit::Result AnoxCommandRegistry::CreateNormalizedKey(RegistryKey &outKey, const AnoxPrehashedRegistryKeyView &inKey)
	{
		rkit::ByteStringSliceView candidateView = inKey.GetName();
		const uint8_t *candidateChars = candidateView.GetChars();
		size_t len = candidateView.Length();

		rkit::ByteStringConstructionBuffer cbuf;
		RKIT_CHECK(cbuf.Allocate(candidateView.Length()));

		uint8_t *cbufChars = cbuf.GetSpan().Ptr();
		for (size_t i = 0; i < len; i++)
			cbufChars[i] = AnoxPrehashedRegistryKeyView::NormalizeChar(candidateChars[i]);

		outKey.m_str = rkit::ByteString(std::move(cbuf));

		RKIT_RETURN_OK;
	}

	bool AnoxCommandRegistryBase::RequiresEscape(const rkit::ByteStringSliceView &token)
	{
		for (char c : token)
		{
			if (rkit::IsASCIIWhitespace(c))
				return true;
		}

		return false;
	}

	rkit::Result AnoxCommandRegistryBase::EscapeToken(rkit::ByteString &outString, const rkit::ByteStringSliceView &token)
	{
		rkit::ByteStringConstructionBuffer cbuf;
		RKIT_CHECK(cbuf.Allocate(token.Length() + 2));

		rkit::Span<uint8_t> chars = cbuf.GetSpan();
		chars[0] = '\"';
		chars[chars.Count() - 1] = '\"';

		rkit::CopySpanNonOverlapping(chars.SubSpan(1, chars.Count() - 2), token.ToSpan());

		outString = rkit::ByteString(std::move(cbuf));

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxCommandRegistryBase::Create(rkit::UniquePtr<AnoxCommandRegistryBase> &outRegistry)
	{
		rkit::UniquePtr<AnoxCommandRegistry> registry;
		RKIT_CHECK(rkit::New<AnoxCommandRegistry>(registry));

		outRegistry = std::move(registry);

		RKIT_RETURN_OK;
	}
}
