#pragma once

#include "anox/AnoxFileSystem.h"

namespace rkit
{
	struct IJobQueue;
}

namespace anox
{
	class AnoxGameFileSystemBase : public IGameDataFileSystem
	{
	public:
		virtual rkit::IJobQueue &GetJobQueue() const = 0;

		static rkit::Result Create(rkit::UniquePtr<AnoxGameFileSystemBase> &outFileSystem, rkit::IJobQueue &jobQueue);
	};
}
