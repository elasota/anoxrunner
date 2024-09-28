#pragma once

#include "anox/AFSArchive.h"

#include "rkit/Core/NoCopy.h"
#include "rkit/Core/String.h"
#include "rkit/Core/SharedPtr.h"
#include "rkit/Core/Vector.h"

namespace rkit
{
	struct IMallocDriver;
	struct ISeekableReadStream;
	struct IMutexProtectedReadStream;

	template<class T>
	class UniquePtr;
}

namespace anox
{
	namespace afs
	{
		class Archive final : public IArchive, public rkit::NoCopy
		{
		public:
			explicit Archive(rkit::IMallocDriver *alloc);

			rkit::Result Open(rkit::UniquePtr<rkit::ISeekableReadStream> &&stream);

			FileHandle FindFile(const rkit::StringView &fileName) const override;

		private:
			struct FileInfo
			{
				FileInfo();

				rkit::StringView m_name;
				uint32_t m_filePosition;
				uint32_t m_compressedSize;
				uint32_t m_uncompressedSize;
			};

			uint32_t GetNumFiles() const override;
			FileHandle GetFileByIndex(uint32_t fileIndex) const override;
			rkit::Result OpenFileByIndex(uint32_t fileIndex, rkit::UniquePtr<rkit::IReadStream> &outStream) const override;
			uint32_t GetFileSizeByIndex(uint32_t fileIndex) const override;
			rkit::StringView GetFilePathByIndex(uint32_t fileIndex) const override;

			static rkit::Result CheckName(const rkit::StringView &name);
			static rkit::Result CheckSlice(const rkit::StringView &sliceName);

			rkit::SharedPtr<rkit::IMutexProtectedReadStream> m_stream;
			rkit::Vector<FileInfo> m_files;
			rkit::Vector<char> m_fileNameChars;

			rkit::IMallocDriver *m_alloc;
		};
	}
}
