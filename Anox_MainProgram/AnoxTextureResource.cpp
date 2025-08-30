#include "AnoxTextureResource.h"
#include "AnoxLoadEntireFileJob.h"

#include "AnoxResourceManager.h"

#include "rkit/Core/JobDependencyList.h"
#include "rkit/Core/Vector.h"
#include "anox/AnoxGraphicsSubsystem.h"

namespace anox
{
	class AnoxTextureResource : public AnoxTextureResourceBase
	{
	public:
		friend class AnoxTextureResourceLoader;

	private:
		rkit::RCPtr<ITexture> m_texture;
	};

	class AnoxTextureResourceLoader final : public AnoxTextureResourceLoaderBase
	{
	public:
		rkit::Result CreateLoadJob(const rkit::RCPtr<AnoxTextureResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const override;
		rkit::Result CreateResourceObject(rkit::UniquePtr<AnoxTextureResourceBase> &outResource) const override;
	};

	struct AnoxTextureResourceLoaderState final : public rkit::RefCounted
	{
		rkit::Vector<uint8_t> m_data;
	};

	rkit::Result AnoxTextureResourceLoader::CreateLoadJob(const rkit::RCPtr<AnoxTextureResourceBase> &resource, const AnoxResourceLoaderSystems &systems, const rkit::data::ContentID &key, rkit::RCPtr<rkit::Job> &outJob) const
	{
		rkit::RCPtr<AnoxTextureResourceLoaderState> state;
		RKIT_CHECK(rkit::New<AnoxTextureResourceLoaderState>(state));

		rkit::RCPtr<rkit::Job> loadJob;
		RKIT_CHECK(CreateLoadEntireFileJob(loadJob, state.FieldRef(&AnoxTextureResourceLoaderState::m_data), *systems.m_fileSystem, key));

		RKIT_CHECK(systems.m_graphicsSystem->CreateAsyncCreateTextureJob(&outJob, resource.StaticCast<AnoxTextureResource>()->m_texture, std::move(state->m_data), loadJob));

		return rkit::ResultCode::kOK;
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
