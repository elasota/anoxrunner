#pragma once

#include <cstdint>

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct Result;

	template<class T>
	class Optional;
}

namespace anox
{
	struct IAnoxGame
	{
		virtual ~IAnoxGame() {}

		virtual rkit::Result Start() = 0;
		virtual rkit::Result RunFrame() = 0;
		virtual bool IsExiting() const = 0;

		static rkit::Result Create(rkit::UniquePtr<IAnoxGame> &outGame, const rkit::Optional<uint16_t> &numThreads);
	};
}
