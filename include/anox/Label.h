#pragma once

#include "rkit/Core/Algorithm.h"

#include <stdint.h>


namespace anox
{
	class Label final : public rkit::DefaultOperatorsMixin<Label>
	{
	public:
		typedef uint32_t LabelValue_t;

		static const uint32_t kLabelHighMultiplier = 10000;

		Label();
		Label(uint32_t high, uint32_t low);
		Label(const Label &) = default;

		static Label FromRawValue(LabelValue_t rawValue);
		LabelValue_t RawValue() const;

		static bool IsValid(uint32_t high, uint32_t low);

	private:
		explicit Label(uint32_t value);
		uint32_t m_value;
	};
}

#include "rkit/Core/RKitAssert.h"

#include <limits>

namespace anox
{
	inline Label::Label()
		: m_value(0)
	{
	}

	inline Label::Label(uint32_t high, uint32_t low)
		: m_value(low + high * kLabelHighMultiplier)
	{
		RKIT_ASSERT(IsValid(high, low));
	}

	inline Label Label::FromRawValue(LabelValue_t rawValue)
	{
		return Label(rawValue);
	}

	inline Label::LabelValue_t Label::RawValue() const
	{
		return m_value;
	}

	bool Label::IsValid(uint32_t high, uint32_t low)
	{
		if (low >= kLabelHighMultiplier)
			return false;

		const uint32_t remainingNumericSpace = std::numeric_limits<uint32_t>::max() - low;
		if (remainingNumericSpace / kLabelHighMultiplier < high)
			return false;

		return true;
	}
}
