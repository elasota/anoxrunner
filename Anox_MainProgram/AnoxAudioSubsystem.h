#pragma once

#include "rkit/Core/Opaque.h"
#include "rkit/Core/Result.h"

namespace rkit
{
	template<class T>
	class UniquePtr;
}

namespace anox
{
	class AudioSubsystemImpl;

	class AudioSubsystem final : public rkit::Opaque<AudioSubsystemImpl>
	{
	public:
		rkit::Result Init();

		static rkit::Result Create(rkit::UniquePtr<AudioSubsystem> &outSubsystem);

	private:

	};
}
