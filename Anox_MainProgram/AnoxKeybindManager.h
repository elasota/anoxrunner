#pragma once

#include "rkit/Core/CoreDefs.h"
#include "rkit/Core/StringProto.h"

#include <cstdint>

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace anox
{
	class AnoxCommandRegistryBase;
	struct IAnoxGame;

	class AnoxKeybindManagerBase
	{
	public:
		typedef uint32_t KeyCode_t;

		virtual ~AnoxKeybindManagerBase() {}

		virtual bool ResolveKeyCode(const rkit::StringSliceView &keyName, KeyCode_t &outKeyCode) const = 0;
		virtual bool ResolveKeyName(KeyCode_t keyCode, char &outSingleChar, rkit::StringSliceView &outName) const = 0;

		virtual rkit::Result Register(AnoxCommandRegistryBase &commandRegistry) = 0;

		static rkit::Result Create(rkit::UniquePtr<AnoxKeybindManagerBase> &outManager, IAnoxGame &game);
	};
}
