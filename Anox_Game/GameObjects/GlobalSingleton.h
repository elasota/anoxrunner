#pragma once

#include "GlobalSingleton.generated.h"

namespace anox::game
{
	class GlobalSingleton : public ObjectRTTI<GlobalSingleton>
	{
	public:
		rkit::ResultCoroutine OnSpawnedFromLevel(rkit::ICoroThread &thread) override;
	};
}
