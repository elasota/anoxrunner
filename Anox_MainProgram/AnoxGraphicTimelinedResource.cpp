#include "AnoxGraphicTimelinedResource.h"

#include "anox/AnoxGraphicsSubsystem.h"

namespace anox
{
	GraphicTimelinedResource::GraphicTimelinedResource(IGraphicsSubsystem &subsystem)
		: m_subsystem(subsystem)
		, m_lastUsedGlobalSyncPoint(0)
		, m_next(nullptr)
	{
	}

	void GraphicTimelinedResource::Touch(uint64_t globalSyncPoint)
	{
		m_lastUsedGlobalSyncPoint.Max(globalSyncPoint);
	}

	uint64_t GraphicTimelinedResource::GetGlobalSyncPoint() const
	{
		return m_lastUsedGlobalSyncPoint.Get();
	}

	void GraphicTimelinedResource::RCTrackerZero()
	{
		m_subsystem.CondemnTimelinedResource(*this);
	}

	void GraphicTimelinedResource::DisposeResource()
	{
		RKIT_ASSERT(RCTrackerRefCount() == 0);
		RefCounted::RCTrackerZero();
	}

	GraphicTimelinedResourceStack::GraphicTimelinedResourceStack()
		: m_first(nullptr)
	{
	}

	GraphicTimelinedResourceStack::GraphicTimelinedResourceStack(GraphicTimelinedResourceStack &&other)
	{
		TakeFrom(other);
	}

	GraphicTimelinedResourceStack::~GraphicTimelinedResourceStack()
	{
		DestroyAll();
	}

	GraphicTimelinedResource *GraphicTimelinedResourceStack::Pop()
	{
		GraphicTimelinedResource *head = m_first.GetWeak();

		if (!head)
			return nullptr;

		GraphicTimelinedResource *next = head->m_next;

		while (!m_first.CompareExchangeWeak(head, next))
		{
			if (!head)
				return nullptr;

			next = head->m_next;
		}

		head->m_next = nullptr;
		return head;
	}

	void GraphicTimelinedResourceStack::Push(GraphicTimelinedResource *timelinedResource)
	{
		RKIT_ASSERT(timelinedResource != nullptr);

		GraphicTimelinedResource *head = m_first.GetWeak();
		timelinedResource->m_next = head;

		while (!m_first.CompareExchangeWeak(head, timelinedResource))
		{
			timelinedResource->m_next = head;
		}
	}

	void GraphicTimelinedResourceStack::PushUnsafe(GraphicTimelinedResource *timelinedResource)
	{
		RKIT_ASSERT(timelinedResource != nullptr);

		timelinedResource->m_next = m_first.Get();
		m_first.Set(timelinedResource);
	}

	bool GraphicTimelinedResourceStack::IsEmptyUnsafe() const
	{
		return m_first.GetWeak() == nullptr;
	}

	GraphicTimelinedResourceStack &GraphicTimelinedResourceStack::operator=(GraphicTimelinedResourceStack &&other)
	{
		DestroyAll();
		TakeFrom(other);
		return *this;
	}

	void GraphicTimelinedResourceStack::DestroyAll()
	{
		while (GraphicTimelinedResource *resource = m_first.GetWeak())
		{
			m_first.Set(resource->m_next);
			resource->DisposeResource();
		}
	}

	void GraphicTimelinedResourceStack::TakeFrom(GraphicTimelinedResourceStack &other)
	{
		DestroyAll();

		GraphicTimelinedResource *head = other.m_first.GetWeak();
		while (!other.m_first.CompareExchangeWeak(head, nullptr))
		{
		}

		m_first.Set(head);
	}
}
