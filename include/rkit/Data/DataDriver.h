#pragma once

#include "rkit/Core/Drivers.h"

namespace rkit { namespace data
{
	struct IRenderDataHandler;

	struct IDataDriver : public ICustomDriver
	{
		virtual ~IDataDriver() {}

		virtual IRenderDataHandler *GetRenderDataHandler() const = 0;
	};
} } // rkit::data
