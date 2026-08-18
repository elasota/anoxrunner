#pragma once

#include "CoreDefs.h"

namespace rkit
{
#if RKIT_IS_DEBUG != 0
	class DebugString
	{
	public:
		DebugString() = default;

		const char *GetString() const;
		bool IsEmpty() const;

		static DebugString Create(const char *str);

	private:
		explicit DebugString(const char *str);

		const char *m_str = nullptr;
	};
#else
	class DebugString
	{
	public:
		static const char *GetString() const;
		static bool IsEmpty() const;
		static DebugString CreateEmpty();
	};
#endif
}


#if RKIT_IS_DEBUG != 0

namespace rkit
{
	inline const char *DebugString::GetString() const
	{
		return (m_str != nullptr) ? m_str : "";
	}

	inline bool DebugString::IsEmpty() const
	{
		return m_str == nullptr || m_str[0] == 0;
	}

	inline DebugString DebugString::Create(const char *str)
	{
		return DebugString(str);
	}

	inline DebugString::DebugString(const char *str)
		: m_str(str)
	{
	}
}

#define RKIT_DEBUG_STR(str)	(::rkit::DebugString::Create(str))

#else

namespace rkit
{
	inline const char *DebugString::GetString() const
	{
		return "";
	}

	inline bool DebugString::IsEmpty() const
	{
		return true;
	}

	inline DebugString DebugString::CreateEmpty()
	{
		return DebugString();
	}
};

#define RKIT_DEBUG_STR(str)	(::rkit::DebugString::CreateEmpty())

#endif
