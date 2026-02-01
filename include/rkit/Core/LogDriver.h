#pragma once

#include "StringProto.h"
#include "FormatProtos.h"

#include <cstddef>
#include <stdarg.h>

namespace rkit
{
	enum class LogSeverity
	{
		kInfo,
		kWarning,
		kError,
	};

	struct ILogDriver
	{
		virtual ~ILogDriver() {}

		virtual void LogMessage(LogSeverity severity, const rkit::StringSliceView &msg) = 0;
		virtual void VLogMessage(LogSeverity severity, const rkit::StringSliceView &msg, const FormatParameterList<Utf8Char_t> &args) = 0;
	};
}

#include "Drivers.h"
#include "Format.h"

namespace rkit
{
	namespace log
	{
		inline void VMessageFmt(LogSeverity severity, const rkit::StringSliceView &fmt, const FormatParameterList<Utf8Char_t> &args)
		{
			if (ILogDriver *logDriver = GetDrivers().m_logDriver)
				logDriver->VLogMessage(severity, fmt, args);
		}

		template<class... TArgs>
		inline void MessageFmt(LogSeverity severity, const rkit::StringSliceView &fmt, const TArgs &... args)
		{
			VMessageFmt(severity, fmt, CreateFormatParameterList<Utf8Char_t>(args...));
		}

		inline void Message(LogSeverity severity, const rkit::StringSliceView &msg)
		{
			if (ILogDriver *logDriver = GetDrivers().m_logDriver)
				logDriver->LogMessage(severity, msg);
		}

		inline void Error(const rkit::StringSliceView &msg)
		{
			Message(LogSeverity::kError, msg);
		}

		template<class... TArgs>
		inline void ErrorFmt(const rkit::StringSliceView &fmt, const TArgs &...args)
		{
			VMessageFmt(LogSeverity::kError, fmt, CreateFormatParameterList<Utf8Char_t>(args...));
		}

		inline void Warning(const rkit::StringSliceView &msg)
		{
			Message(LogSeverity::kWarning, msg);
		}

		template<class... TArgs>
		inline void LogWarningFmt(const rkit::StringSliceView &fmt, const TArgs &...args)
		{
			VMessageFmt(LogSeverity::kWarning, fmt, CreateFormatParameterList<Utf8Char_t>(args...));
		}

		inline void LogInfo(const rkit::StringSliceView &msg)
		{
			Message(LogSeverity::kInfo, msg);
		}

		template<class... TArgs>
		inline void LogInfoFmt(const rkit::StringSliceView &fmt, const TArgs &...args)
		{
			VMessageFmt(LogSeverity::kInfo, fmt, CreateFormatParameterList<Utf8Char_t>(args...));
		}
	}
}

#if RKIT_IS_DEBUG

#define RKIT_DEBUG_LOG_FMT(msg, ...)	\
	(::rkit::log::VMessageFmt(::rkit::LogSeverity::kInfo, msg, __VA_ARGS))

#define RKIT_DEBUG_LOG(msg, ...)	\
	(::rkit::log::Message(::rkit::LogSeverity::kInfo, msg))

#define RKIT_DEBUG_ERROR_FMT(msg, ...)	\
	(::rkit::log::VMessageFmt(::rkit::LogSeverity::kError, msg, __VA_ARGS))

#define RKIT_DEBUG_ERROR(msg, ...)	\
	(::rkit::log::Message(::rkit::LogSeverity::kError, msg))

#define RKIT_DEBUG_WARN_FMT(msg, ...)	\
	(::rkit::log::VMessageFmt(::rkit::LogSeverity::kWarn, msg, __VA_ARGS))

#define RKIT_DEBUG_WARN(msg, ...)	\
	(::rkit::log::Message(::rkit::LogSeverity::kWarn, msg))

#else

#define RKIT_DEBUG_LOG_FMT(msg, ...)	((void)0)
#define RKIT_DEBUG_LOG(msg, ...)		((void)0)
#define RKIT_DEBUG_ERROR_FMT(msg, ...)	((void)0)
#define RKIT_DEBUG_ERROR(msg, ...)		((void)0)
#define RKIT_DEBUG_WARN_FMT(msg, ...)	((void)0)
#define RKIT_DEBUG_WARN(msg, ...)		((void)0)

#endif

