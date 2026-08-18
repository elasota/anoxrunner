#pragma once

#include "AnoxResourceManager.h"

#include "rkit/Core/Opaque.h"

#include "anox/Data/MaterialData.h"
#include "anox/Data/MaterialResourceType.h"

namespace rkit
{
	template<class T>
	class Span;
}

namespace anox
{
	class AnoxMaterialResourceImpl;
	class AnoxMaterialResourceLoader;
	class AnoxTextureResourceBase;

	class AnoxMaterialResource final : public AnoxResourceBase, public rkit::Opaque<AnoxMaterialResourceImpl>
	{
	public:
		friend class AnoxMaterialResourceLoader;

		struct FrameDef
		{
			uint32_t m_bitmapIndex = 0;
			uint32_t m_next = 0;
			uint32_t m_waitMSec = 0;
		};

		struct InterformHalf
		{
			data::MaterialInterformMovement m_movement = data::MaterialInterformMovement::kNone;
			uint32_t m_bitmap = 0;
			float m_vx = 0.f;
			float m_vy = 0.f;
			float m_rate = 0.f;
			float m_strength = 0.f;
			float m_speed = 0.f;
		};

		struct InterformData
		{
			InterformHalf m_halves[2];
			uint32_t m_paletteBitmap = 0;
		};

		rkit::Span<const FrameDef> GetFrameDefs() const;
		const InterformData &GetInterformData() const;
		rkit::Span<const rkit::RCPtr<AnoxTextureResourceBase>> GetBitmaps() const;
	};

	class AnoxMaterialResourceLoaderBase : public AnoxContentIDKeyedResourceLoader<AnoxMaterialResource>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxMaterialResourceLoaderBase> &outLoader, data::MaterialResourceType resType);
	};
}
