#pragma once

#include "rkit/Core/EnumMask.h"

#include "anox/Data/EntityDef.h"
#include "anox/Label.h"

namespace anox::game
{
	struct UserEntityDefValues
	{
		uint32_t m_modelCodeFourCC = 0;
		rkit::StaticArray<float, 3> m_scale;
		data::UserEntityShadowType m_shadowType = data::UserEntityShadowType::kCount;
		rkit::StaticArray<float, 3> m_bboxMin;
		rkit::StaticArray<float, 3> m_bboxMax;
		rkit::EnumMask<data::UserEntityFlags> m_userEntityFlags;
		float m_walkSpeed = 0.f;
		float m_runSpeed = 0.f;
		float m_speed = 0.f;
		Label m_targetSequence;
		Label m_startSequence;
		uint32_t m_miscValue = 0;
		uint32_t m_descLength = 0;
	};
}
