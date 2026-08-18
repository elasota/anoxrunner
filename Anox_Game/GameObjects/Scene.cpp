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

		m_blocks.Resize(blocks.Count());

		rkit::ProcessParallelSpans(m_blocks.ToSpan(), blocks, [](SceneRuntimeBlock &outBlock, const ScenePackage::Block &inBlock)
			{
				outBlock.m_groups.Resize(inBlock.m_numGroups);
			});
	}

	rkit::ResultCoroutine Scene::OnFrame(rkit::ICoroThread &thread)
	{
		co_return;
	}
}
