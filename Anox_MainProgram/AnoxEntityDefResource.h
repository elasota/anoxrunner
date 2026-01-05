#pragma once

#include "rkit/Core/EnumMask.h"
#include "rkit/Core/String.h"
#include "rkit/Math/Vec.h"

#include "anox/Data/EntityDef.h"
#include "anox/Label.h"

#include "AnoxDataReader.h"
#include "AnoxResourceManager.h"

namespace anox { namespace data {
	struct UserEntityDef;
} }

namespace anox
{
	class AnoxEntityDefResourceLoader;

	class AnoxEntityDefResourceBase : public AnoxResourceBase
	{
	public:
		struct Values
		{
			uint32_t m_modelCodeFourCC = 0;
			rkit::math::Vec3 m_scale;
			data::UserEntityShadowType m_shadowType = data::UserEntityShadowType::kCount;
			rkit::math::Vec3 m_bboxMin;
			rkit::math::Vec3 m_bboxMax;
			rkit::EnumMask<data::UserEntityFlags> m_userEntityFlags;
			float m_walkSpeed = 0.f;
			float m_runSpeed = 0.f;
			float m_speed = 0.f;
			Label m_targetSequence;
			Label m_startSequence;
			uint32_t m_miscValue = 0;

			rkit::String m_description;
		};

		virtual const Values &GetValues() const = 0;
	};

	class AnoxEntityDefResourceLoaderBase : public AnoxContentIDKeyedResourceLoader<AnoxEntityDefResourceBase>
	{
	public:
		static rkit::Result Create(rkit::RCPtr<AnoxEntityDefResourceLoaderBase> &outLoader);
	};
}
