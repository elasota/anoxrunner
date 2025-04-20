#pragma once

#include "anox/AnoxFileSystem.h"

namespace anox
{
	class AnoxGameFileSystemBase : public IGameDataFileSystem
	{
	public:
		static rkit::Result Create(rkit::UniquePtr<AnoxGameFileSystemBase> &fileSystem);
	};
}
