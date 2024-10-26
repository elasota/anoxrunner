#pragma once

#include "rkit/Core/Drivers.h"

namespace rkit::data
{
	struct IRenderDataHandler;

	struct IDataDriver : public ICustomDriver
	{
		virtual ~IDataDriver() {}

		virtual IRenderDataHandler *GetRenderDataHandler() const = 0;
	};
}
