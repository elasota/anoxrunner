#pragma once

#include "rkit/BuildSystem/BuildSystem.h"

namespace rkit
{
	template<class T>
	class UniquePtr;

	namespace buildsystem
	{
		struct IBaseBuildSystemInstance : public IBuildSystemInstance
		{
			virtual ~IBaseBuildSystemInstance() {}

			static Result Create(UniquePtr<IBuildSystemInstance> &outInstance);
		};
	}
}
