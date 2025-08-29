#include "AnoxTextureResource.h"
#include "AnoxLoadEntireFileJob.h"

#pragma once

#include "AnoxResourceManager.h"

namespace anox
{
	class AnoxTextureResource : public AnoxTextureResourceBase
	{
	public:
	};

	class AnoxTextureResourceLoader final : public AnoxTextureResourceLoaderBase
	{
	public:
		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxTextureResourceBase> &resource, AnoxResourceManagerBase &resManager, AnoxGameFileSystemBase &fileSystem, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxTextureResourceBase> &outResource) const override;
	};

	rkit::Result AnoxTextureResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxTextureResourceBase> &resource, AnoxResourceManagerBase &resManager, AnoxGameFileSystemBase &fileSystem, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxTextureResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxTextureResourceBase> &outResource) const
	{
		return rkit::New<AnoxTextureResource>(outResource);
	}

	rkit::Result AnoxTextureResourceLoaderBase::Create(rkit::RCPtr<AnoxTextureResourceLoaderBase> &outLoader)
	{
		return rkit::New<AnoxTextureResourceLoader>(outLoader);
	}
}
