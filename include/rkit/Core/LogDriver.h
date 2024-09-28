#pragma once

#include "StringProto.h"

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

		virtual void LogMessage(LogSeverity severity, const char *msg) = 0;
		virtual void VLogMessage(LogSeverity severity, const char *fmt, va_list arg) = 0;
	};
}

#include "Drivers.h"

namespace rkit
{
	namespace log
	{
		inline void VMessageFmt(LogSeverity severity, const char *fmt, va_list args)
		{
			if (ILogDriver *logDriver = GetDrivers().m_logDriver)
				logDriver->VLogMessage(severity, fmt, args);
		}

		inline void MessageFmt(LogSeverity severity, const char *fmt, ...)
		{
			va_list args;

			va_start(args, fmt);
			VMessageFmt(severity, fmt, args);
			va_end(args);
		}

		inline void Message(LogSeverity severity, const char *str)
		{
			if (ILogDriver *logDriver = GetDrivers().m_logDriver)
				logDriver->LogMessage(severity, str);
		}

		inline void Error(const char *str)
		{
			Message(LogSeverity::kError, str);
		}

		inline void ErrorFmt(const char *fmt, ...)
		{
			va_list args;

			va_start(args, fmt);
			VMessageFmt(LogSeverity::kError, fmt, args);
			va_end(args);
		}

		inline void Warning(const char *str)
		{
			Message(LogSeverity::kWarning, str);
		}

		inline void LogWarningFmt(const char *fmt, ...)
		{
			va_list args;

			va_start(args, fmt);
			VMessageFmt(LogSeverity::kWarning, fmt, args);
			va_end(args);
		}

		inline void LogInfo(const char *str)
		{
			Message(LogSeverity::kInfo, str);
		}

		inline void LogInfoFmt(const char *fmt, ...)
		{
			va_list args;

			va_start(args, fmt);
			VMessageFmt(LogSeverity::kInfo, fmt, args);
			va_end(args);
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

