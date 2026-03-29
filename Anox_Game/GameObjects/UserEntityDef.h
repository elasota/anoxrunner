#pragma once

#include "anox/Label.h"
#include "anox/Data/EntityDef.h"

#include "rkit/Core/EnumMask.h"
#include "rkit/Core/String.h"
#include "rkit/Math/BBox.h"
#include "rkit/Math/Vec.h"

namespace anox::game
{
	struct UserEntityDef
	{
		uint32_t m_modelCodeFourCC = 0;
		rkit::math::Vec3 m_scale;
		data::UserEntityShadowType m_shadowType = data::UserEntityShadowType::kCount;
		rkit::math::BBox3 m_bbox;
		rkit::EnumMask<data::UserEntityFlags> m_userEntityFlags;
		float m_walkSpeed = 0.f;
		float m_runSpeed = 0.f;
		float m_speed = 0.f;
		Label m_targetSequence;
		Label m_startSequence;
		uint32_t m_miscValue = 0;
		rkit::ByteString m_description;
	};
}

