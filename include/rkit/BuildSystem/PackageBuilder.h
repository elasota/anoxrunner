#pragma once

#include "rkit/Core/StringProto.h"
#include "rkit/Core/NoCopy.h"

#include "rkit/Core/UniquePtr.h"

#include "rkit/Data/RenderDataHandler.h"

#include <cstdint>

namespace rkit
{
	struct IWriteStream;

	template<class T>
	class UniquePtr;

	template<class T>
	class Span;

	namespace data
	{
		struct RenderRTTITypeBase;
		struct IRenderDataHandler;
	}
}

namespace rkit
{
	namespace buildsystem
	{
		struct IStringResolver;
		struct IPackageObjectWriter;

		class BinaryBlobRef;

		struct IBinaryBlob
		{
			virtual ~IBinaryBlob() {}

			virtual Span<const uint8_t> GetBytes() const = 0;

			virtual Result Append(const void *data, size_t size) = 0;
		
			virtual bool operator==(const IBinaryBlob &other) const = 0;
			virtual bool operator!=(const IBinaryBlob &other) const = 0;
		};

		class BinaryBlobRef : public NoCopy
		{
		public:
			BinaryBlobRef(BinaryBlobRef &&other) noexcept;

			explicit BinaryBlobRef(UniquePtr<IBinaryBlob> &&blob);

			Span<const uint8_t> GetBytes() const;
			const IBinaryBlob *GetBlob() const;

			bool operator==(const BinaryBlobRef &other) const;
			bool operator!=(const BinaryBlobRef &other) const;

		private:
			BinaryBlobRef() = delete;

			UniquePtr<IBinaryBlob> m_blob;
		};

		struct IPackageBuilder
		{
			virtual ~IPackageBuilder() {}

			virtual const data::IRenderDataHandler *GetDataHandler() const = 0;
			virtual bool IsWritingTempStrings() const = 0;
			virtual const IStringResolver *GetStringResolver() const = 0;

			virtual Result IndexObjectList(data::RenderRTTIIndexableStructType indexableType, BinaryBlobRef &&blob, size_t &outIndex) = 0;
			virtual Result IndexObject(const void *obj, const data::RenderRTTIStructType *rtti, bool cached, size_t &outIndex) = 0;
			virtual Result IndexString(const StringSliceView &str, size_t &outIndex) = 0;
			virtual Result IndexConfigKey(size_t globalStringIndex, data::RenderRTTIMainType mainType, size_t &outIndex) = 0;
			virtual Result IndexBinaryContent(BinaryBlobRef &&blob, size_t &outIndex) = 0;

			virtual void BeginSource(const IStringResolver *resolver) = 0;

			virtual const IPackageObjectWriter *GetWriter() const = 0;

			virtual Result WritePackage(ISeekableWriteStream &stream) const = 0;
		};

		struct IPackageObjectWriter
		{
			virtual ~IPackageObjectWriter() {}

			virtual Result WriteObject(IPackageBuilder &pkgBuilder, const void *obj, const data::RenderRTTITypeBase *rtti, IWriteStream &stream) const = 0;
		};

		struct IStringResolver
		{
			virtual StringSliceView ResolveGlobalString(size_t index) const = 0;
			virtual StringSliceView ResolveConfigKey(size_t index) const = 0;
			virtual StringSliceView ResolveTempString(size_t index) const = 0;

			virtual Span<const uint8_t> ResolveBinaryContent(size_t index) const = 0;
		};
	}
}

#include "rkit/Core/Hasher.h"

namespace rkit
{
	template<>
	struct Hasher<rkit::buildsystem::BinaryBlobRef>
		: public DefaultSpanHasher<rkit::buildsystem::BinaryBlobRef>
	{
		static HashValue_t ComputeHash(HashValue_t baseHash, const rkit::buildsystem::BinaryBlobRef &blob)
		{
			return Hasher<uint8_t>::ComputeHash(baseHash, blob.GetBytes());
		}
	};
}

#include <utility>

namespace rkit
{
	namespace buildsystem
	{
		inline BinaryBlobRef::BinaryBlobRef(BinaryBlobRef &&other) noexcept
			: m_blob(std::move(other.m_blob))
		{
		}

		inline BinaryBlobRef::BinaryBlobRef(UniquePtr<IBinaryBlob> &&blob)
			: m_blob(std::move(blob))
		{
		}

		inline Span<const uint8_t> BinaryBlobRef::GetBytes() const
		{
			const IBinaryBlob *blob = m_blob.Get();
			if (!blob)
				return Span<const uint8_t>();

			return blob->GetBytes();
		}

		inline const IBinaryBlob *BinaryBlobRef::GetBlob() const
		{
			return m_blob.Get();
		}

		inline bool BinaryBlobRef::operator==(const BinaryBlobRef &other) const
		{
			if (!m_blob.IsValid())
			{
				if (!other.m_blob.IsValid())
					return true;

				if (other.m_blob->GetBytes().Count() == 0)
					return true;

				return false;
			}

			return (*m_blob) == *other.m_blob;
		}

		inline bool BinaryBlobRef::operator!=(const BinaryBlobRef &other) const
		{
			return !((*this) == other);
		}
	}
}
