#pragma once

#include "Scene.generated.h"

namespace anox::game
{
	class Scene : public ObjectRTTI<Scene>
	{
	public:
		rkit::Result Initialize(SceneHandle handle);
		
		rkit::ResultCoroutine OnFrame(rkit::ICoroThread &thread) override;
	};
}
