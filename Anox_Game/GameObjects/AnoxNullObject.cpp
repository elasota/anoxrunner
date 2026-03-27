#include "AnoxWorldObjectFactory.h"

#include "anox/Data/EntityStructs.h"

#include "rkit/Core/Coroutine.h"

namespace anox::game
{
	template<>
	rkit::ResultCoroutine WorldObjectInstantiator<data::EClass_info_null>::SpawnObject(rkit::ICoroThread &thread, rkit::RCPtr<WorldObject> &outObject, const WorldObjectSpawnParams &spawnParams, const data::EClass_info_null &spawnDef)
	{
		if (true)
			CORO_THROW(rkit::ResultCode::kNotYetImplemented);

		CORO_RETURN_OK;
	}
}
