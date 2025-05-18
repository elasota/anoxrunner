#pragma once

#include "rkit/Core/Coroutine.h"
#include "rkit/Core/Optional.h"
#include "rkit/Core/StringProto.h"

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
		typedef rkit::coro::MethodStarter<void(AnoxCommandStackBase &, const rkit::Span<rkit::StringView> &args)> MethodStarter_t;

		rkit::coro::MethodStarter<void(AnoxCommandStackBase &, const rkit::Span<rkit::StringView> &args)> m_methodStarter;
	};

	struct AnoxRegisteredConsoleVar
	{
		rkit::String m_value;
		rkit::Optional<float> m_string;
	};

	class AnoxCommandRegistryBase
	{
		virtual ~AnoxCommandRegistryBase() {}

		virtual rkit::Result RegisterCommand(const rkit::StringSliceView &name, uint32_t hash, const AnoxRegisteredCommand::MethodStarter_t &methodStarter) = 0;
		virtual rkit::Result RegisterAlias(const rkit::StringSliceView &name, uint32_t hash, const rkit::StringSliceView &commandText) = 0;
		virtual rkit::Result RegisterConsoleVar(const rkit::StringSliceView &name, uint32_t hash, const rkit::StringSliceView &commandText) = 0;

		virtual const AnoxRegisteredCommand::MethodStarter_t *FindCommand(const rkit::StringSliceView &name, const AnoxRegisteredCommand::MethodStarter_t &methodStarter) = 0;
	};
}
