#pragma once

#include "rkit/BuildSystem/PackageBuilder.h"

namespace rkit::data
{
	struct IRenderDataHandler;
}

namespace rkit::buildsystem
{
	struct IPackageObjectWriter;

	class PackageBuilderBase : public IPackageBuilder
	{
	public:
		static Result Create(data::IRenderDataHandler *dataHandler, IPackageObjectWriter *objWriter, bool allowTempStrings, UniquePtr<IPackageBuilder> &outPackageBuilder);
	};

	class PackageObjectWriterBase : public IPackageObjectWriter
	{
	public:
		static Result Create(UniquePtr<IPackageObjectWriter> &outPackageObjectWriter);
	};
}
