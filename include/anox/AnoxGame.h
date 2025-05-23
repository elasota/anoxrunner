#pragma once

#include "rkit/Core/CoreDefs.h"

#include <cstdint>

namespace rkit
{
	template<class T>
	class UniquePtr;

	template<class T>
	class Optional;

	namespace utils
	{
		struct IThreadPool;
	}
}

namespace anox
{
	class AnoxCommandRegistryBase;
	class AnoxKeybindManagerBase;
	class AnoxResourceManagerBase;
	struct ICaptureHarness;

	struct IAnoxGame
	{
		virtual ~IAnoxGame() {}

		virtual rkit::Result Start() = 0;
		virtual rkit::Result RunFrame() = 0;
		virtual bool IsExiting() const = 0;

		virtual ICaptureHarness *GetCaptureHarness() const = 0;
		virtual rkit::utils::IThreadPool *GetThreadPool() const = 0;
		virtual AnoxCommandRegistryBase *GetCommandRegistry() const = 0;
		virtual AnoxKeybindManagerBase *GetKeybindManager() const = 0;

		static rkit::Result Create(rkit::UniquePtr<IAnoxGame> &outGame, const rkit::Optional<uint16_t> &numThreads);
	};
}
