#include "ShadowFile.h"

#include "rkit/Core/BoolVector.h"
#include "rkit/Core/FourCC.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/UniquePtr.h"

namespace rkit::utils::shadowfile
{
	class ShadowFS;

	struct FileLocator
	{
		uint32_t m_extentsSectorListSector;
		uint32_t m_extentsTableSize;
		uint32_t m_fileTableExtentsStart;
		uint32_t m_numSectors;
	};

	struct ShadowFSHeader
	{
		static const uint32_t kExpectedIdentifier = RKIT_FOURCC('R', 'S', 'F', 'S');
		static const uint16_t kExpectedVersion = 1;

		uint32_t m_identifier;
		uint16_t m_version;
		uint8_t m_sectorSizeBits;
		uint8_t m_padding;
		FileLocator m_locators[2];
		uint8_t m_activeLocatorPlusOne;
	};

	struct FileTableEntry
	{
		static const uint32_t kFlagDirectory = 1;

		char m_name[256] = {};
		uint32_t m_flags = 0;
		uint32_t m_extentsStarts = 0;
		uint64_t m_sizes = 0;
	};

	struct ConvertToWriteInfo
	{
		uint64_t m_indexInParent = 0;
		uint64_t m_extra = 0;
		Result (ShadowFS::*m_function)(const ConvertToWriteInfo &);
	};

	class MemMappedSector
	{
	public:
		explicit MemMappedSector(ShadowFS &shadowFS);
		~MemMappedSector();

		void SetSector(uint32_t sector);
		Result Close();

		const uint8_t *MapRead();
		uint8_t *MapWrite();

	private:
		ConvertToWriteInfo m_convertToWrite;
		Vector<uint8_t> m_memory;
		bool m_isWritable;
		bool m_isOpen;
		uint32_t m_sector;
	};

	struct ExtentsTableEntry
	{
		uint32_t m_sector;		// 0 = unused
		uint32_t m_nextEntry;
	};

	class FastIndexTable
	{
	public:
		void Reset();

	private:
		static const size_t kTiers = 4;

		StaticArray<BoolVector, kTiers> m_tables;
	};

	class SectorCacheEntry
	{
	public:
		SectorCacheEntry(SectorCacheEntry &&other);

	private:
		uint32_t m_sector;
		Vector<uint8_t> m_memory;
		SectorCacheEntry() = delete;
	};

	class ShadowFS final : public ShadowFileBase, public NoCopy
	{
	public:
		ShadowFS(ISeekableReadStream &readStream, ISeekableWriteStream *writeStream);

		Result EntryExists(const StringSliceView &str, bool &outExists) override;

		Result TryOpenFileRead(UniquePtr<ISeekableReadStream> &outStream, const StringSliceView &str) override;
		Result TryOpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, const StringSliceView &str, bool createIfNotExists) override;

		Result CopyEntry(const StringSliceView &oldName, const StringSliceView &newName) override;
		Result MoveEntry(const StringSliceView &oldName, const StringSliceView &newName) override;

		Result DeleteEntry(const StringSliceView &str) override;

		Result CommitChanges() override;

		Result LoadShadowFile() override;
		Result InitializeShadowFile() override;

		size_t GetSectorSize() const;

	private:
		void ClearWriteState();

		BoolVector m_oldUsedSectors;

		// Table of sectors used by the new state
		FastIndexTable m_newUsedSectors;

		// Table of sectors used by the old state
		FastIndexTable m_anyUsedSectors;

		// Table of extents slot availabilities
		FastIndexTable m_extentsTableUsed;

		// Table of used file table entries
		FastIndexTable m_fileTableUsed;

		ISeekableReadStream &m_readStream;
		ISeekableWriteStream *m_writeStream;

		static const size_t kSectorSizeBitsMin = 9;
		static const size_t kSectorSizeBitsMax = 24;
		static const size_t kSectorSizeBitsDefault = 12;

		bool m_isWriteAccessInitialized;
		bool m_haveWriteFaults;

		ShadowFSHeader m_sfHeader;
		const FileLocator *m_writeLocator = nullptr;
	};

	void FastIndexTable::Reset()
	{
		for (BoolVector &bv : m_tables)
			bv.Reset();
	}

	ShadowFS::ShadowFS(ISeekableReadStream &readStream, ISeekableWriteStream *writeStream)
		: m_sfHeader{}
		, m_readStream(readStream)
		, m_writeStream(writeStream)
		, m_isWriteAccessInitialized(false)
		, m_haveWriteFaults(false)
	{
		static_assert((static_cast<size_t>(1) << kSectorSizeBitsMin) >= sizeof(FileTableEntry));
		static_assert((static_cast<size_t>(1) << kSectorSizeBitsMin) >= sizeof(ShadowFSHeader));
	}

	Result ShadowFS::EntryExists(const StringSliceView &str, bool &outExists)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::TryOpenFileRead(UniquePtr<ISeekableReadStream> &outStream, const StringSliceView &str)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::TryOpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, const StringSliceView &str, bool createIfNotExists)
	{
		if (!m_writeStream)
		{
			outStream.Reset();
			return ResultCode::kOK;
		}

		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::CopyEntry(const StringSliceView &oldName, const StringSliceView &newName)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::MoveEntry(const StringSliceView &oldName, const StringSliceView &newName)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::DeleteEntry(const StringSliceView &str)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::CommitChanges()
	{
		if (!m_writeStream)
			return ResultCode::kOperationFailed;

		if (m_haveWriteFaults)
			return ResultCode::kIOWriteError;

		bool needFullHeader = false;

		uint8_t newLocatorPlusOne = 0;

		ShadowFSHeader newHeader = m_sfHeader;

		if (newHeader.m_activeLocatorPlusOne == 1)
			newHeader.m_activeLocatorPlusOne = 2;
		else if (newHeader.m_activeLocatorPlusOne == 2)
			newHeader.m_activeLocatorPlusOne = 1;
		else
		{
			newHeader.m_activeLocatorPlusOne = 1;
			needFullHeader = true;
		}

		RKIT_CHECK(m_writeStream->Flush());

		if (m_writeStream->GetSize() < sizeof(ShadowFSHeader))
		{
			RKIT_CHECK(m_writeStream->WriteAll(&newHeader, sizeof(ShadowFSHeader)));
		}
		else
		{
			if (needFullHeader)
			{
				newHeader.m_identifier = ShadowFSHeader::kExpectedIdentifier;
				newHeader.m_version = ShadowFSHeader::kExpectedVersion;

				RKIT_CHECK(m_writeStream->SeekStart(0));

				RKIT_CHECK(m_writeStream->WriteAll(&newHeader, offsetof(ShadowFSHeader, m_locators)));
				RKIT_CHECK(m_writeStream->Flush());
			}

			uint8_t activeLocator = newHeader.m_activeLocatorPlusOne - 1;

			RKIT_CHECK(m_writeStream->SeekStart(offsetof(ShadowFSHeader, m_locators) + sizeof(FileLocator) * activeLocator));
			RKIT_CHECK(m_writeStream->WriteAll(&newHeader.m_locators[activeLocator], sizeof(FileLocator)));
			RKIT_CHECK(m_writeStream->SeekStart(offsetof(ShadowFSHeader, m_activeLocatorPlusOne)));
			RKIT_CHECK(m_writeStream->WriteAll(&newHeader.m_activeLocatorPlusOne, sizeof(newHeader.m_activeLocatorPlusOne)));
			RKIT_CHECK(m_writeStream->Flush());
		}

		m_sfHeader = newHeader;

		return ResultCode::kOK;
	}

	void ShadowFS::ClearWriteState()
	{
		m_isWriteAccessInitialized = false;
		m_haveWriteFaults = false;
		m_writeLocator = nullptr;

		m_newUsedSectors.Reset();
		m_anyUsedSectors.Reset();
		m_extentsTableUsed.Reset();
		m_fileTableUsed.Reset();
	}

	Result ShadowFS::LoadShadowFile()
	{
		ClearWriteState();

		m_sfHeader = {};

		RKIT_CHECK(m_readStream.SeekStart(0));

		RKIT_CHECK(m_readStream.ReadAll(&m_sfHeader, sizeof(m_sfHeader)));

		if (m_sfHeader.m_version != m_sfHeader.kExpectedVersion || (m_sfHeader.m_activeLocatorPlusOne != 1 && m_sfHeader.m_activeLocatorPlusOne != 2))
			return Result::SoftFault(ResultCode::kOperationFailed);

		if (m_sfHeader.m_identifier != ShadowFSHeader::kExpectedIdentifier)
			return ResultCode::kOperationFailed;

		const FileLocator &locator = m_sfHeader.m_locators[m_sfHeader.m_activeLocatorPlusOne - 1];
		if (locator.m_extentsTableSize != 0 && (locator.m_extentsSectorListSector == 0 || locator.m_extentsSectorListSector >= locator.m_numSectors))
			return ResultCode::kOperationFailed;

		return ResultCode::kOK;
	}

	Result ShadowFS::InitializeShadowFile()
	{
		if (!m_writeStream)
			return ResultCode::kOperationFailed;

		m_sfHeader = {};

		RKIT_CHECK(CommitChanges());
		
		return ResultCode::kOK;
	}
}

namespace rkit::utils
{
	Result ShadowFileBase::Create(UniquePtr<ShadowFileBase> &outShadowFile, ISeekableReadStream &readStream, ISeekableWriteStream *writeStream)
	{
		UniquePtr<shadowfile::ShadowFS> shadowFile;

		RKIT_CHECK(New<shadowfile::ShadowFS>(shadowFile, readStream, writeStream));

		outShadowFile = std::move(shadowFile);

		return ResultCode::kOK;
	}
}
