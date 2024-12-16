#pragma once

#include "rkit/Core/NoCopy.h"

#include "IncludeVulkan.h"

#include "VulkanAPI.h"

namespace rkit::render::vulkan
{
	template<class TOwner, class TObjectType, class TDestroyFuncPtr, TDestroyFuncPtr(VulkanAPI:: *TDestroyFuncMember), class TNullResolver>
	class AutoObjectWriteback;

	template<class T>
	struct AutoObjectHandleNullResolver
	{
		static T GetNull();
	};

	template<class T>
	struct AutoObjectPtrNullResolver
	{
		static T GetNull();
	};

	template<class TOwner>
	struct AutoObjectOwnershipContext
	{
		const VulkanAPI *m_api = nullptr;
		TOwner m_owner = nullptr;
		const VkAllocationCallbacks *m_allocCallbacks = nullptr;
	};

	template<class TOwner, class TObjectType, class TDestroyFuncPtr, TDestroyFuncPtr (VulkanAPI::*TDestroyFuncMember), class TNullResolver>
	class AutoObject final : public NoCopy
	{
	public:
		AutoObject();
		explicit AutoObject(const AutoObjectOwnershipContext<TOwner> &ownerContext, const TObjectType &object);
		AutoObject(AutoObject&& other);
		~AutoObject();

		AutoObject& operator=(AutoObject &&other);

		operator TObjectType() const;

		void Reset();

		TObjectType *ReceiveNewObject(const AutoObjectOwnershipContext<TOwner> &ownerContext);

	private:
		const AutoObjectOwnershipContext<TOwner> *m_ownerContext;
		TObjectType m_object;
	};
}

#include <utility>

namespace rkit::render::vulkan
{

	template<class T>
	T AutoObjectHandleNullResolver<T>::GetNull()
	{
		return static_cast<T>(VK_NULL_HANDLE);
	}

	template<class T>
	T AutoObjectPtrNullResolver<T>::GetNull()
	{
		return nullptr;
	}

	template<class TOwner, class TObjectType, class TDestroyFuncPtr, TDestroyFuncPtr(VulkanAPI:: *TDestroyFuncMember), class TNullResolver>
	AutoObject<TOwner, TObjectType, TDestroyFuncPtr, TDestroyFuncMember, TNullResolver>::AutoObject()
		: m_ownerContext(nullptr)
		, m_object(TNullResolver::GetNull())
	{
	}

	template<class TOwner, class TObjectType, class TDestroyFuncPtr, TDestroyFuncPtr(VulkanAPI:: *TDestroyFuncMember), class TNullResolver>
	AutoObject<TOwner, TObjectType, TDestroyFuncPtr, TDestroyFuncMember, TNullResolver>::AutoObject(const AutoObjectOwnershipContext<TOwner> &ownerContext, const TObjectType &object)
		: m_ownerContext(&ownerContext)
		, m_object(object)
	{
	}

	template<class TOwner, class TObjectType, class TDestroyFuncPtr, TDestroyFuncPtr(VulkanAPI:: *TDestroyFuncMember), class TNullResolver>
	AutoObject<TOwner, TObjectType, TDestroyFuncPtr, TDestroyFuncMember, TNullResolver>::AutoObject(AutoObject &&other)
		: m_ownerContext(other.m_ownerContext)
		, m_object(other.m_object)
	{
		other.m_ownerContext = nullptr;
		other.m_object = TNullResolver::GetNull();
	}

	template<class TOwner, class TObjectType, class TDestroyFuncPtr, TDestroyFuncPtr(VulkanAPI:: *TDestroyFuncMember), class TNullResolver>
	AutoObject<TOwner, TObjectType, TDestroyFuncPtr, TDestroyFuncMember, TNullResolver>::~AutoObject()
	{
		Reset();
	}

	template<class TOwner, class TObjectType, class TDestroyFuncPtr, TDestroyFuncPtr(VulkanAPI:: *TDestroyFuncMember), class TNullResolver>
	void AutoObject<TOwner, TObjectType, TDestroyFuncPtr, TDestroyFuncMember, TNullResolver>::Reset()
	{
		if (m_ownerContext && m_object != TNullResolver::GetNull())
			(m_ownerContext->m_api->*TDestroyFuncMember)(m_ownerContext->m_owner, m_object, m_ownerContext->m_allocCallbacks);

		m_ownerContext = nullptr;
		m_object = TNullResolver::GetNull();
	}

	template<class TOwner, class TObjectType, class TDestroyFuncPtr, TDestroyFuncPtr(VulkanAPI:: *TDestroyFuncMember), class TNullResolver>
	TObjectType* AutoObject<TOwner, TObjectType, TDestroyFuncPtr, TDestroyFuncMember, TNullResolver>::ReceiveNewObject(const AutoObjectOwnershipContext<TOwner> &ownerContext)
	{
		Reset();

		m_ownerContext = &ownerContext;
		return &m_object;
	}

	template<class TOwner, class TObjectType, class TDestroyFuncPtr, TDestroyFuncPtr(VulkanAPI:: *TDestroyFuncMember), class TNullResolver>
	AutoObject<TOwner, TObjectType, TDestroyFuncPtr, TDestroyFuncMember, TNullResolver>::operator TObjectType() const
	{
		return m_object;
	}

	template<class TOwner, class TObjectType, class TDestroyFuncPtr, TDestroyFuncPtr(VulkanAPI:: *TDestroyFuncMember), class TNullResolver>
	AutoObject<TOwner, TObjectType, TDestroyFuncPtr, TDestroyFuncMember, TNullResolver> &AutoObject<TOwner, TObjectType, TDestroyFuncPtr, TDestroyFuncMember, TNullResolver>::operator=(AutoObject &&other)
	{
		Reset();

		m_ownerContext = other.m_ownerContext;
		m_object = other.m_object;

		other.m_ownerContext = nullptr;
		other.m_object = TNullResolver::GetNull();

		return *this;
	}
}

#define RKIT_VK_DECLARE_AUTO_TYPE(ownerType, type, defaultResolver)	\
	typedef ::rkit::render::vulkan::AutoObject<ownerType, Vk ## type, PFN_vkDestroy ## type, &::rkit::render::vulkan::VulkanAPI::vkDestroy ##type, defaultResolver> Auto ## type ## _t

#define RKIT_VK_DECLARE_DEVICE_DISPATCHABLE_HANDLE_AUTO_TYPE(type)	RKIT_VK_DECLARE_AUTO_TYPE(VkDevice, type, ::rkit::render::vulkan::AutoObjectPtrNullResolver<Vk ## type>)
#define RKIT_VK_DECLARE_DEVICE_NON_DISPATCHABLE_HANDLE_AUTO_TYPE(type)	RKIT_VK_DECLARE_AUTO_TYPE(VkDevice, type, ::rkit::render::vulkan::AutoObjectHandleNullResolver<Vk ## type>)

#define RKIT_VK_DECLARE_INSTANCE_DISPATCHABLE_HANDLE_AUTO_TYPE(type)	RKIT_VK_DECLARE_AUTO_TYPE(VkInstance, type, ::rkit::render::vulkan::AutoObjectPtrNullResolver<Vk ## type>)
#define RKIT_VK_DECLARE_INSTANCE_NON_DISPATCHABLE_HANDLE_AUTO_TYPE(type)	RKIT_VK_DECLARE_AUTO_TYPE(VkInstance, type, ::rkit::render::vulkan::AutoObjectHandleNullResolver<Vk ## type>)

RKIT_VK_DECLARE_INSTANCE_NON_DISPATCHABLE_HANDLE_AUTO_TYPE(DebugUtilsMessengerEXT);
