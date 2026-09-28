#pragma once

#include "rkit/Core/Atomic.h"
#include "rkit/Core/RefCounted.h"

#include "rkit/Render/Fence.h"

#include <stdint.h>

#include "AnoxLogicalQueue.h"

namespace anox
{
	struct IGraphicsSubsystem;
	class GraphicTimelinedResourceStack;

	class GraphicTimelinedResource : public rkit::RefCounted
	{
	public:
		friend class GraphicTimelinedResourceStack;

		explicit GraphicTimelinedResource(IGraphicsSubsystem &subsystem);

		void Touch(uint64_t globalSyncPoint);
		uint64_t GetGlobalSyncPoint() const;

		void RCTrackerZero() override;

		void DisposeResource();

		bool IsActiveOnQueue(LogicalQueueType queueType) const;

		void MakeActiveOnQueue(LogicalQueueType queueType) const;
		void RemoveFromQueue(LogicalQueueType queueType) const;

	private:
		void FinishRemovingFromQueue(LogicalQueueType queueType);

		static constexpr size_t kNumQueues = static_cast<size_t>(LogicalQueueType::kCount);
		using ActiveQueueFlags_t = rkit::UIntOfAtLeastSize<(kNumQueues + 7) / 8>::Type_t;

		rkit::AtomicInt<ActiveQueueFlags_t> m_activeQueues;
		rkit::StaticArray<rkit::render::TimelinePoint_t, kNumQueues> m_disposeSyncPoints;

		IGraphicsSubsystem &m_subsystem;
		rkit::AtomicInt<uint64_t> m_lastUsedGlobalSyncPoint;
		GraphicTimelinedResource *m_next = nullptr;
	};

	class GraphicTimelinedResourceStack
	{
	public:
		GraphicTimelinedResourceStack();
		GraphicTimelinedResourceStack(GraphicTimelinedResourceStack &&other);
		GraphicTimelinedResourceStack(const GraphicTimelinedResourceStack &) = delete;
		~GraphicTimelinedResourceStack();

		GraphicTimelinedResource *Pop();
		void Push(GraphicTimelinedResource *timelinedResource);
		void PushUnsafe(GraphicTimelinedResource *timelinedResource);
		bool IsEmptyUnsafe() const;

		GraphicTimelinedResourceStack &operator=(GraphicTimelinedResourceStack &&other);

	private:
		void DestroyAll();
		void TakeFrom(GraphicTimelinedResourceStack &other);

		rkit::AtomicPtr<GraphicTimelinedResource> m_first;
	};
}
