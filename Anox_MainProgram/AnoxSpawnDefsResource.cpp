#include "AnoxSpawnDefsResource.h"

#include "AnoxEntityDefResource.h"
#include "AnoxAbstractSingleFileResource.h"

#include "rkit/Data/ContentID.h"
#include "rkit/Core/Sanitizers.h"

#include "anox/Data/EntitySpawnData.h"
#include "anox/Data/EntityStructs.h"

#include "anox/AnoxUtilitiesDriver.h"
#include "anox/AnoxModule.h"


namespace anox
{
	class AnoxSpawnDefsResource;
	struct AnoxSpawnDefsResourceLoaderState;

	struct AnoxSpawnDefsResourceLoaderState final : public AnoxAbstractSingleFileResourceLoaderState
	{
		rkit::Vector<rkit::Future<AnoxResourceRetrieveResult>> m_edefResources;
	};

	struct AnoxSpawnDefsLoaderInfo
	{
		typedef AnoxSpawnDefsResourceLoaderBase LoaderBase_t;
		typedef AnoxSpawnDefsResource Resource_t;
		typedef AnoxSpawnDefsResourceLoaderState State_t;

		static constexpr size_t kNumPhases = 2;

		class SpawnDataChunkVisitor
		{
		public:
			SpawnDataChunkVisitor(rkit::IReadStream &stream, AnoxSpawnDefsResourceLoaderState &state);

			template<class T>
			rkit::Result VisitMember(rkit::Vector<T> &array) const;

		private:
			rkit::IReadStream &m_stream;
			AnoxSpawnDefsResourceLoaderState &m_state;
		};

		static rkit::Result LoadHeaderAndQueueDependencies(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps);
		static rkit::Result LoadContents(State_t &state, Resource_t &resource);

		static bool PhaseHasDependencies(size_t phase)
		{
			switch (phase)
			{
			case 0:
				return true;
			default:
				return false;
			}
		}

		static rkit::Result LoadPhase(State_t &state, Resource_t &resource, size_t phase, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
		{
			switch (phase)
			{
			case 0:
				return LoadHeaderAndQueueDependencies(state, resource, outDeps);
			case 1:
				return LoadContents(state, resource);
			default:
				return rkit::ResultCode::kInternalError;
			}
		}

		static rkit::Result ParseEntity(void *data, const AnoxSpawnDefsResource &resource, rkit::Span<const uint8_t> entityData, const data::EntityClassDef *eclass);

		template<size_t TComponents>
		static void ParseFloatVector(void *outData, const void *inData);
	};

	class AnoxSpawnDefsResource final : public AnoxSpawnDefsResourceBase
	{
	public:
		friend struct AnoxSpawnDefsLoaderInfo;

	private:
		rkit::Vector<rkit::RCPtr<AnoxEntityDefResourceBase>> m_userEntityDefs;
		rkit::Vector<SpawnDef> m_spawnDefs;

		data::EntitySpawnDataChunks m_chunks;

		rkit::Vector<uint8_t> m_objectDataBuffer;
	};

	AnoxSpawnDefsLoaderInfo::SpawnDataChunkVisitor::SpawnDataChunkVisitor(rkit::IReadStream &stream, AnoxSpawnDefsResourceLoaderState &state)
		: m_stream(stream)
		, m_state(state)
	{
	}

	template<class T>
	rkit::Result AnoxSpawnDefsLoaderInfo::SpawnDataChunkVisitor::VisitMember(rkit::Vector<T> &array) const
	{
		rkit::endian::LittleUInt32_t countData;
		RKIT_CHECK(m_stream.ReadOneBinary(countData));

		const uint32_t count = countData.Get();
		RKIT_CHECK(array.Resize(count));

		if (count > 0)
		{
			RKIT_CHECK(m_stream.ReadAllSpan(array.ToSpan()));
		}

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxSpawnDefsLoaderInfo::LoadHeaderAndQueueDependencies(State_t &state, Resource_t &resource, rkit::traits::TraitRef<rkit::VectorTrait<rkit::RCPtr<rkit::Job>>> outDeps)
	{
		{
			rkit::ReadOnlyMemoryStream stream(state.m_fileContents.ToSpan());

			data::EntitySpawnDataFile spawnDataFile;
			RKIT_CHECK(stream.ReadOneBinary(spawnDataFile));

			if (spawnDataFile.m_fourCC.Get() != data::EntitySpawnDataFile::kFourCC || spawnDataFile.m_version.Get() != data::EntitySpawnDataFile::kVersion)
				return rkit::ResultCode::kDataError;

			RKIT_CHECK(resource.m_chunks.VisitAllChunks(SpawnDataChunkVisitor(stream, state)));
		}

		state.m_fileContents.Reset();

		RKIT_CHECK(state.m_edefResources.Resize(resource.m_chunks.m_entityDefContentIDs.Count()));

		AnoxResourceManagerBase *resManager = state.m_systems.m_resManager;

		RKIT_CHECK((rkit::CheckedProcessParallelSpans(state.m_edefResources.ToSpan(), resource.m_chunks.m_entityDefContentIDs.ToSpan(),
			[resManager, &outDeps]
			(rkit::Future<AnoxResourceRetrieveResult> &outResourceFuture, const rkit::data::ContentID &inContentID)
			-> rkit::Result
			{
				rkit::RCPtr<rkit::Job> depsJob;
				RKIT_CHECK(resManager->GetContentIDKeyedResource(&depsJob, outResourceFuture, resloaders::kEntityDefTypeCode, inContentID));
				RKIT_CHECK(outDeps.AppendRValue(std::move(depsJob)));

				RKIT_RETURN_OK;
			})));

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxSpawnDefsLoaderInfo::LoadContents(State_t &state, Resource_t &resource)
	{
		const size_t numEntityDefs = state.m_edefResources.Count();
		RKIT_CHECK(resource.m_userEntityDefs.Resize(numEntityDefs));
		rkit::ProcessParallelSpans(resource.m_userEntityDefs.ToSpan(), state.m_edefResources.ToSpan(),
			[](rkit::RCPtr<AnoxEntityDefResourceBase> &outEDef, const rkit::Future<AnoxResourceRetrieveResult> &inFuture)
			{
				outEDef = inFuture.GetResult().m_resourceHandle.StaticCast<AnoxEntityDefResourceBase>();
			});

		anox::IUtilitiesDriver *anoxUtils = static_cast<anox::IUtilitiesDriver *>(rkit::GetDrivers().FindDriver(kAnoxNamespaceID, u8"Utilities"));
		const data::EntityDefsSchema &schema = anoxUtils->GetEntityDefs();

		const size_t numSpawnDefs = resource.m_chunks.m_entityTypes.Count();

		rkit::Vector<AnoxSpawnDefsResource::SpawnDef> &outSpawnDefs = resource.m_spawnDefs;
		RKIT_CHECK(outSpawnDefs.Resize(numSpawnDefs));

		rkit::Vector<size_t> spawnDefStartOffsets;
		RKIT_CHECK(spawnDefStartOffsets.Reserve(numSpawnDefs));

		size_t entityDataSize = 0;

		{
			for (const rkit::endian::LittleUInt32_t &entityType : resource.m_chunks.m_entityTypes)
			{
				const uint32_t etype = entityType.Get();

				if (etype >= schema.m_numClassDefs)
					return rkit::ResultCode::kDataError;

				const data::EntityClassDef *eclass = schema.m_classDefs[etype];

				size_t startPos = 0;
				RKIT_CHECK(rkit::SafeAlignUp(startPos, entityDataSize, eclass->m_structAlignment));
				RKIT_CHECK(spawnDefStartOffsets.Append(startPos));
				RKIT_CHECK(rkit::SafeAdd(entityDataSize, startPos, eclass->m_structSize));
			}
		}

		RKIT_CHECK(resource.m_objectDataBuffer.Resize(entityDataSize));

		uint8_t *structDataStart = resource.m_objectDataBuffer.GetBuffer();

		rkit::ProcessParallelSpans(outSpawnDefs.ToSpan(), spawnDefStartOffsets.ToSpan(),
			[structDataStart]
			(AnoxSpawnDefsResource::SpawnDef &outSpawnDef, size_t inOffset)
			{
				outSpawnDef.m_data = static_cast<void *>(structDataStart + inOffset);
			});

		rkit::ReadOnlyMemoryStream stream(resource.m_chunks.m_entityData.ToSpan());

		rkit::Result checkResult = rkit::CheckedProcessParallelSpans(outSpawnDefs.ToSpan(), resource.m_chunks.m_entityTypes.ToSpan(),
			[&schema, &stream, &resource]
			(AnoxSpawnDefsResource::SpawnDef &outSpawnDef, const rkit::endian::LittleUInt32_t &inEntityType)
			-> rkit::Result
			{
				const uint32_t etype = inEntityType.Get();

				if (etype >= schema.m_numClassDefs)
					return rkit::ResultCode::kDataError;

				const data::EntityClassDef *eclass = schema.m_classDefs[etype];
				outSpawnDef.m_eclass = eclass;

				rkit::ConstSpan<uint8_t> dataSpan;
				RKIT_CHECK(stream.ExtractSpan(dataSpan, eclass->m_dataSize));

				return ParseEntity(outSpawnDef.m_data, resource, dataSpan, eclass);
			});

		RKIT_CHECK(checkResult);

		if (stream.Tell() != stream.GetSize())
			return rkit::ResultCode::kDataError;

		if (resource.m_chunks.m_entityStringLengths.Count())
			return rkit::ResultCode::kNotYetImplemented;

		RKIT_RETURN_OK;
	}

	rkit::Result AnoxSpawnDefsLoaderInfo::ParseEntity(void *data, const AnoxSpawnDefsResource &resource, rkit::Span<const uint8_t> entityData, const data::EntityClassDef *eclass)
	{
		const data::EntityClassDef *parent = eclass->m_parent;

		if (parent)
		{
			RKIT_CHECK(ParseEntity(data, resource, entityData.SubSpan(0, parent->m_dataSize), parent));
		}

		const rkit::ConstSpan<data::EntityFieldDef> fieldDefs(eclass->m_fields, eclass->m_numFields);

		for (const data::EntityFieldDef &fieldDef : fieldDefs)
		{
			void *fieldDataPos = static_cast<void *>(static_cast<uint8_t *>(data) + fieldDef.m_structOffset);
			const void *inDataPos = entityData.Ptr() + fieldDef.m_dataOffset;

			switch (fieldDef.m_fieldType)
			{
			case data::EntityFieldType::kLabel:
				*static_cast<Label *>(fieldDataPos) = Label::FromRawValue(static_cast<const rkit::endian::LittleUInt32_t *>(inDataPos)->Get());
				break;
			case data::EntityFieldType::kBool:
			case data::EntityFieldType::kBoolOnOff:
				*static_cast<bool *>(fieldDataPos) = ((*static_cast<const uint8_t *>(inDataPos)) != 0);
				break;
			case data::EntityFieldType::kUInt:
				*static_cast<uint32_t *>(fieldDataPos) = static_cast<const rkit::endian::LittleUInt32_t *>(inDataPos)->Get();
				break;
			case data::EntityFieldType::kFloat:
				*static_cast<float *>(fieldDataPos) = rkit::sanitizers::SanitizeClampFloat(*static_cast<const rkit::endian::LittleFloat32_t *>(inDataPos), 40);
				break;
			case data::EntityFieldType::kVec2:
				ParseFloatVector<2>(fieldDataPos, inDataPos);
				break;
			case data::EntityFieldType::kVec3:
				ParseFloatVector<3>(fieldDataPos, inDataPos);
				break;
			case data::EntityFieldType::kVec4:
				ParseFloatVector<4>(fieldDataPos, inDataPos);
				break;
			case data::EntityFieldType::kString:
				{
					uint32_t stringIndex = static_cast<const rkit::endian::LittleUInt32_t *>(inDataPos)->Get();

					if (stringIndex > resource.m_chunks.m_entityStringLengths.Count())
						return rkit::ResultCode::kDataError;

					if (stringIndex == 0)
						new (fieldDataPos) rkit::Optional<uint32_t>();
					else
						new (fieldDataPos) rkit::Optional<uint32_t>(stringIndex - 1);
				}
				break;
			case data::EntityFieldType::kEntityDef:
				{
					uint32_t edefIndex = static_cast<const rkit::endian::LittleUInt32_t *>(inDataPos)->Get();
					if (edefIndex >= resource.m_userEntityDefs.Count())
						return rkit::ResultCode::kDataError;

					*static_cast<uint32_t *>(fieldDataPos) = edefIndex;
				}
				break;
			case data::EntityFieldType::kBSPModel:
				// uint32
				return rkit::ResultCode::kNotYetImplemented;
			case data::EntityFieldType::kComponent:
				{
					const data::EntityClassDef *componentClass = fieldDef.m_classDef;
					RKIT_ASSERT(componentClass != nullptr);
					RKIT_CHECK(ParseEntity(fieldDataPos, resource, entityData.SubSpan(fieldDef.m_dataOffset, componentClass->m_dataSize), componentClass));
				}
				break;
			default:
				return rkit::ResultCode::kDataError;
			};
		}

		RKIT_RETURN_OK;		
	}

	template<size_t TComponents>
	void AnoxSpawnDefsLoaderInfo::ParseFloatVector(void *outData, const void *inData)
	{
		const rkit::endian::LittleFloat32_t *inFloats = static_cast<const rkit::endian::LittleFloat32_t *>(inData);

		float floats[TComponents] = {};

		for (size_t i = 0; i < TComponents; i++)
			floats[i] = rkit::sanitizers::SanitizeClampFloat(inFloats[i], 40);

		*static_cast<rkit::math::Vec<float, TComponents> *>(outData) = rkit::math::Vec<float, TComponents>::FromArray(floats);
	}

	rkit::Result AnoxSpawnDefsResourceLoaderBase::Create(rkit::RCPtr<AnoxSpawnDefsResourceLoaderBase> &outLoader)
	{
		typedef AnoxAbstractSingleFileResourceLoader<AnoxSpawnDefsLoaderInfo> Loader_t;

		rkit::RCPtr<Loader_t> loader;
		RKIT_CHECK(rkit::New<Loader_t>(loader));

		outLoader = std::move(loader);

		RKIT_RETURN_OK;
	}
}
