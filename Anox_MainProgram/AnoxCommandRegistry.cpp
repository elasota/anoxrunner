#include "AnoxCommandRegistry.h"

#include "rkit/Core/HashTable.h"

namespace anox
{
	class AnoxCommandRegistry final : public AnoxCommandRegistryBase
	{
	public:
		AnoxCommandRegistry();

		rkit::Result RegisterCommand(const rkit::StringSliceView &name, const AnoxRegisteredCommand::MethodStarter_t &methodStarter) override;
		rkit::Result RegisterAlias(const rkit::StringSliceView &name, const rkit::StringSliceView &commandText) override;
		rkit::Result RegisterConsoleVar(const rkit::StringSliceView &name, AnoxConsoleVarType varType, void *varValue) override;

		const AnoxRegisteredCommand *FindCommand(const rkit::StringSliceView &name, rkit::HashValue_t hash) const override;
		const AnoxRegisteredAlias *FindAlias(const rkit::StringSliceView &name, rkit::HashValue_t hash) const override;
		const AnoxRegisteredConsoleVar *FindConsoleVar(const rkit::StringSliceView &name, rkit::HashValue_t hash) const override;

	private:
		rkit::HashMap<rkit::String, AnoxRegisteredCommand> m_commands;
		rkit::HashMap<rkit::String, AnoxRegisteredAlias> m_aliases;
		rkit::HashMap<rkit::String, AnoxRegisteredConsoleVar> m_consoleVars;
	};

	AnoxCommandRegistry::AnoxCommandRegistry()
	{
	}

	rkit::Result AnoxCommandRegistry::RegisterCommand(const rkit::StringSliceView &name, const AnoxRegisteredCommand::MethodStarter_t &methodStarter)
	{
		AnoxRegisteredCommand cmd{ methodStarter };

		rkit::String key;
		RKIT_CHECK(key.Set(name));

		RKIT_CHECK(m_commands.Set(std::move(key), std::move(cmd)));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxCommandRegistry::RegisterAlias(const rkit::StringSliceView &name, const rkit::StringSliceView &commandText)
	{
		AnoxRegisteredAlias alias;

		RKIT_CHECK(alias.m_text.Set(commandText));

		rkit::String key;
		RKIT_CHECK(key.Set(name));

		RKIT_CHECK(m_aliases.Set(std::move(key), std::move(alias)));

		return rkit::ResultCode::kOK;
	}

	rkit::Result AnoxCommandRegistry::RegisterConsoleVar(const rkit::StringSliceView &name, AnoxConsoleVarType varType, void *varValue)
	{
		AnoxRegisteredConsoleVar consoleVar;

		consoleVar.m_varType = varType;
		consoleVar.m_value = varValue;

		rkit::String key;
		RKIT_CHECK(key.Set(name));

		RKIT_CHECK(m_consoleVars.Set(std::move(key), std::move(consoleVar)));

		return rkit::ResultCode::kOK;
	}

	const AnoxRegisteredCommand *AnoxCommandRegistry::FindCommand(const rkit::StringSliceView &name, rkit::HashValue_t hash) const
	{
		rkit::HashMap<rkit::String, AnoxRegisteredCommand>::ConstIterator_t it = m_commands.FindPrehashed(hash, name);

		if (it == m_commands.end())
			return nullptr;
		else
			return &it.Value();
	}

	const AnoxRegisteredAlias *AnoxCommandRegistry::FindAlias(const rkit::StringSliceView &name, rkit::HashValue_t hash) const
	{
		rkit::HashMap<rkit::String, AnoxRegisteredAlias>::ConstIterator_t it = m_aliases.FindPrehashed(hash, name);

		if (it == m_aliases.end())
			return nullptr;
		else
			return &it.Value();
	}

	const AnoxRegisteredConsoleVar *AnoxCommandRegistry::FindConsoleVar(const rkit::StringSliceView &name, rkit::HashValue_t hash) const
	{
		rkit::HashMap<rkit::String, AnoxRegisteredConsoleVar>::ConstIterator_t it = m_consoleVars.FindPrehashed(hash, name);

		if (it == m_consoleVars.end())
			return nullptr;
		else
			return &it.Value();
	}


	rkit::Result AnoxCommandRegistryBase::Create(rkit::UniquePtr<AnoxCommandRegistryBase> &outRegistry)
	{
		rkit::UniquePtr<AnoxCommandRegistry> registry;
		RKIT_CHECK(rkit::New<AnoxCommandRegistry>(registry));

		outRegistry = std::move(registry);

		return rkit::ResultCode::kOK;
	}
}
