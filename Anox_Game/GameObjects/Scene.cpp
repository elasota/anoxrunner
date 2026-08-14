#include "Scene.h"

#include "Scene.generated.inl"

#include "ScenePackage.h"

#include "World.h"

namespace anox::game
{
	rkit::Result Scene::Initialize(SceneHandle handle)
	{
		m_startTime = GetWorld().GetCurrentTimeMSec() + 99u;
		m_startTime -= m_startTime % 100u;
		m_scene = ResourceRef<SceneHandle>(std::move(handle));

		size_t numBlocks = 0;
		rkit::ConstSpan<ScenePackage::Block> blocks = m_scene->GetBlocks();

		RKIT_CHECK(m_blocks.Resize(blocks.Count()));

		auto processBlock = [](SceneRuntimeBlock &outBlock, const ScenePackage::Block &inBlock) -> rkit::Result
			{
				RKIT_CHECK(outBlock.m_groups.Resize(inBlock.m_numGroups));
				RKIT_RETURN_OK;
			};

		RKIT_CHECK(rkit::CheckedProcessParallelSpans(m_blocks.ToSpan(), blocks, processBlock));

		RKIT_RETURN_OK;
	}

	rkit::ResultCoroutine Scene::OnFrame(rkit::ICoroThread &thread)
	{
		CORO_RETURN_OK;
	}
}
