#include "AnoxMaterialResource.h"

namespace anox
{
	class AnoxMaterialResource final : public AnoxMaterialResourceBase
	{
	public:
		explicit AnoxMaterialResource(data::MaterialResourceType materialType);

	private:
		data::MaterialResourceType m_materialType;
	};

	class AnoxMaterialResourceLoader final : public AnoxMaterialResourceLoaderBase
	{
	public:
		explicit AnoxMaterialResourceLoader(data::MaterialResourceType materialType);

		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxMaterialResourceBase> &resource, AnoxResourceManagerBase &resManager, AnoxGameFileSystemBase &fileSystem, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxMaterialResourceBase> &outResource) const override;

	private:
		data::MaterialResourceType m_materialType;
	};

	AnoxMaterialResource::AnoxMaterialResource(data::MaterialResourceType materialType)
		: m_materialType(materialType)
	{
	}

	AnoxMaterialResourceLoader::AnoxMaterialResourceLoader(data::MaterialResourceType materialType)
		: m_materialType(materialType)
	{
	}

	rkit::Result AnoxMaterialResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxMaterialResourceBase> &resource, AnoxResourceManagerBase &resManager, AnoxGameFileSystemBase &fileSystem, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		return rkit::ResultCode::kNotYetImplemented;
	}

	rkit::Result AnoxMaterialResourceLoader::CreateResourceObject(rkit::UniquePtr<AnoxMaterialResourceBase> &outResource) const
	{
		return rkit::New<AnoxMaterialResource>(outResource, m_materialType);
	}

	rkit::Result AnoxMaterialResourceLoaderBase::Create(rkit::RCPtr<AnoxMaterialResourceLoaderBase> &outLoader, data::MaterialResourceType resType)
	{
		return rkit::New<AnoxMaterialResourceLoader>(outLoader, resType);
	}
}
