#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	struct IJobQueue;
}

namespace anox
{
	class AudioSubsystemImpl;

	class AudioSubsystem final : public rkit::Opaque<AudioSubsystemImpl>
	{
	public:
		explicit AudioSubsystem(rkit::IJobQueue &jobQueue);

		rkit::Result Init();

		static rkit::Result Create(rkit::UniquePtr<AudioSubsystem> &outSubsystem, rkit::IJobQueue& jobQueue);
		rkit::Result Update();
	};
}
