#include "AnoxRecordJobRunner.h"

#include "rkit/Render/CommandAllocator.h"

void anox::priv::TypedRecordJobRunnerHelper::CastCommandAllocator(rkit::render::IBaseCommandAllocator &commandAllocator, rkit::render::IGraphicsCommandAllocator *&outCommandAllocator)
{
	outCommandAllocator = commandAllocator.ToGraphicsCommandAllocator();
}

void anox::priv::TypedRecordJobRunnerHelper::CastCommandAllocator(rkit::render::IBaseCommandAllocator &commandAllocator, rkit::render::IGraphicsComputeCommandAllocator *&outCommandAllocator)
{
	outCommandAllocator = commandAllocator.ToGraphicsComputeCommandAllocator();
}

void anox::priv::TypedRecordJobRunnerHelper::CastCommandAllocator(rkit::render::IBaseCommandAllocator &commandAllocator, rkit::render::IComputeCommandAllocator *&outCommandAllocator)
{
	outCommandAllocator = commandAllocator.ToComputeCommandAllocator();
}

void anox::priv::TypedRecordJobRunnerHelper::CastCommandAllocator(rkit::render::IBaseCommandAllocator &commandAllocator, rkit::render::ICopyCommandAllocator *&outCommandAllocator)
{
	outCommandAllocator = commandAllocator.ToCopyCommandAllocator();
}
