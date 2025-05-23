#pragma once

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/String.h"
#include "rkit/Core/HashValue.h"

namespace rkit
{
	struct Result;

	template<class T>
	class Span;
}

namespace anox
{
	class AnoxCommandStackBase;

	struct AnoxRegisteredCommand
	{
		typedef rkit::coro::MethodStarter<void(AnoxCommandStackBase &, const rkit::ISpan<rkit::StringView> &args)> MethodStarter_t;

		inline const MethodStarter_t &AsyncCall() const
		{
			return m_methodStarter;
		}

		MethodStarter_t m_methodStarter;
	};

	struct AnoxRegisteredAlias
	{
		rkit::String m_text;
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


	class AnoxCommandRegistryBase
	{
	public:
		virtual ~AnoxCommandRegistryBase() {}

		virtual rkit::Result RegisterCommand(const rkit::StringSliceView &name, const AnoxRegisteredCommand::MethodStarter_t &methodStarter) = 0;
		virtual rkit::Result RegisterAlias(const rkit::StringSliceView &name, const rkit::StringSliceView &commandText) = 0;
		virtual rkit::Result RegisterConsoleVar(const rkit::StringSliceView &name, AnoxConsoleVarType varType, void *varValue) = 0;

		virtual const AnoxRegisteredCommand *FindCommand(const rkit::StringSliceView &name, rkit::HashValue_t hash) const = 0;
		virtual const AnoxRegisteredAlias *FindAlias(const rkit::StringSliceView &name, rkit::HashValue_t hash) const = 0;
		virtual const AnoxRegisteredConsoleVar *FindConsoleVar(const rkit::StringSliceView &name, rkit::HashValue_t hash) const = 0;

		const AnoxRegisteredCommand *FindCommand(const rkit::StringSliceView &name) const;
		const AnoxRegisteredAlias *FindAlias(const rkit::StringSliceView &name) const;
		const AnoxRegisteredConsoleVar *FindConsoleVar(const rkit::StringSliceView &name) const;

		static rkit::Result Create(rkit::UniquePtr<AnoxCommandRegistryBase> &outRegistry);
	};
}

namespace anox
{
	inline const AnoxRegisteredCommand *AnoxCommandRegistryBase::FindCommand(const rkit::StringSliceView &name) const
	{
		const rkit::HashValue_t hash = rkit::Hasher<rkit::StringSliceView>::ComputeHash(0, name);

		return this->FindCommand(name, hash);
	}

	inline const AnoxRegisteredAlias *AnoxCommandRegistryBase::FindAlias(const rkit::StringSliceView &name) const
	{
		const rkit::HashValue_t hash = rkit::Hasher<rkit::StringSliceView>::ComputeHash(0, name);

		return this->FindAlias(name, hash);
	}

	inline const AnoxRegisteredConsoleVar *AnoxCommandRegistryBase::FindConsoleVar(const rkit::StringSliceView &name) const
	{
		const rkit::HashValue_t hash = rkit::Hasher<rkit::StringSliceView>::ComputeHash(0, name);

		return this->FindConsoleVar(name, hash);
	}
}
