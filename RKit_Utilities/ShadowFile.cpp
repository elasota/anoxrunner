#include "ShadowFile.h"

#include "rkit/Core/Algorithm.h"
#include "rkit/Core/BoolVector.h"
#include "rkit/Core/FourCC.h"
#include "rkit/Core/HashTable.h"
#include "rkit/Core/Mutex.h"
#include "rkit/Core/MutexLock.h"
#include "rkit/Core/NoCopy.h"
#include "rkit/Core/StaticArray.h"
#include "rkit/Core/Stream.h"
#include "rkit/Core/SystemDriver.h"
#include "rkit/Core/UniquePtr.h"

namespace rkit::utils::shadowfile
{
	class ShadowFS;
	class ShadowFSFile;

	struct FileLocator
	{
		uint32_t m_extentsSectorListSector;
		uint32_t m_extentsTableSize;
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
		uint8_t m_committedLocatorPlusOne;
	};

	struct FileTableEntry
	{
		static const uint32_t kFlagDirectory = 1;
		static const uint8_t kMaxNameLength = 255;

		uint8_t m_nameLength = 0;
		uint8_t m_name[255] = {};
		uint32_t m_flags = 0;
		uint32_t m_extentsStarts = 0;
		uint64_t m_sizes = 0;
	};

	struct ConvertToWriteInfo
	{
		typedef Result(ShadowFS:: *ConvertToWriteCallback_t)(uint32_t &newSector, const ConvertToWriteInfo &ctwi, bool isInFSLock);

		ConvertToWriteInfo() = default;
		explicit ConvertToWriteInfo(uint64_t indexInParent, uint64_t extra, ConvertToWriteCallback_t convertToWriteCallback);

		uint64_t m_indexInParent = 0;
		uint64_t m_extra = 0;
		ConvertToWriteCallback_t m_function = nullptr;
	};

	class SectorMemoryMapper
	{
	public:
		explicit SectorMemoryMapper(ShadowFS &shadowFS, ConvertToWriteInfo::ConvertToWriteCallback_t convertToWriteCallback);
		~SectorMemoryMapper();

		Result ChangeSector(uint32_t sector, uint64_t indexInParent, uint64_t extra);
		Result Close();

		Result MapRead(ConstSpan<uint8_t> &outSpan, bool isInFSLock);
		Result MapWrite(Span<uint8_t> &outSpan, bool isInFSLock);

		Result MapWriteZeroFill(Span<uint8_t> &outSpan, bool isInFSLock);

	private:
		ShadowFS &m_shadowFS;
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
		void FindAvailableSlotIndex(size_t &outSlotIndex);

		Result SetSlotState(size_t slot, bool available);

		void Reset();

		Result Duplicate(const FastIndexTable &other);

	private:
		static const size_t kTiers = 4;

		void SetSlotOccupied(size_t slot);
		Result SetSlotAvailable(size_t slot);

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
		friend class ShadowFSFile;

		ShadowFS(ISeekableReadStream &readStream, ISeekableWriteStream *writeStream);

		Result Initialize();

		Result EntryExists(const StringSliceView &str, bool &outExists) override;

		Result TryOpenFileRead(UniquePtr<ISeekableReadStream> &outStream, const StringSliceView &str) override;
		Result TryOpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, const StringSliceView &str, bool createIfNotExists, bool createDirectories) override;

		Result CopyEntry(const StringSliceView &oldName, const StringSliceView &newName) override;
		Result MoveEntry(const StringSliceView &oldName, const StringSliceView &newName) override;

		Result DeleteEntry(const StringSliceView &str) override;

		Result CommitChanges() override;

		Result LoadShadowFile() override;
		Result InitializeShadowFile() override;

		size_t GetSectorSize() const;

		Result FlushSector(uint32_t sector, const ConstSpan<uint8_t> &memory);
		Result MapReadSector(Vector<uint8_t> &memory, uint32_t sector, bool isInFSLock);

	private:
		static const uint32_t kFlagDirectory = 0;

		void ClearWriteState();

		Result ResolveFilePath(uint32_t &outDirectoryEntry, uint32_t &outDirectoryExtents, ExtentsTableEntry &outDirectoryExtentsData, ConstSpan<uint8_t> &outNameSpan, const ConstSpan<uint8_t> &pathSpan, bool createDirectories);

		Result CheckCreateRootDirectory(uint32_t &outRootDirectoryEntry, uint32_t &outRootDirectoryExtentsIndex, ExtentsTableEntry &outRootDirectoryExtentsData);
		Result ResolveRootDirectory(uint32_t &outRootDirectoryEntry, uint32_t &outRootDirectoryExtentsIndex, ExtentsTableEntry &outRootDirectoryExtentsData);

		Result ConvertExtentsSectorListToWritable(uint32_t &outNewSector, const ConvertToWriteInfo &ctw, bool isInFSLock);
		Result ConvertExtentsTableToWritable(uint32_t &outNewSector, const ConvertToWriteInfo &ctw, bool isInFSLock);
		Result ConvertFileTableToWritable(uint32_t &outNewSector, const ConvertToWriteInfo &ctw, bool isInFSLock);
		Result ConvertFileSectorToWritable(uint32_t &outNewSector, const ConvertToWriteInfo &ctw, bool isInFSLock);

		Result FindFileInDirectory(uint32_t &outFileEntry, uint32_t &outFileExtents, ExtentsTableEntry &outFileExtentsData, uint32_t &outFileFlags, bool &outExists, uint32_t directoryEntry, uint32_t directoryExtentsIndex, const ExtentsTableEntry &directoryExtentsData, const ConstSpan<uint8_t> &nameSpan);
		Result CreateFileInDirectory(uint32_t &outFileEntry, uint32_t &outFileExtents, ExtentsTableEntry &outFileExtentsData, uint32_t directoryEntry, uint32_t directoryExtents, const ExtentsTableEntry &directoryExtentsData, const ConstSpan<uint8_t> &nameSpan, uint32_t flags);

		Result CreateNewExtents(uint32_t &outFirstSector, uint64_t &outTableIndex, uint64_t &outExtra);

		Result AllocateSector(uint32_t &outSector);
		Result AllocateExtents(uint32_t &outExtentsIndex, ExtentsTableEntry &outExtentsData);

		Result ResolveExtents(ExtentsTableEntry &outExtentsData, uint32_t extentsIndex);

		Result CheckInitializeWriteAccess();
		Result InitializeWriteAccess();

		BoolVector m_oldUsedSectors;

		// Table of sectors used by the new state
		FastIndexTable m_newUsedSectors;

		// Table of sectors used by the old state
		FastIndexTable m_anyUsedSectors;

		// Table of extents slot availabilities
		FastIndexTable m_extentsTableUsed;

		// Table of used file table entries
		FastIndexTable m_fileTableUsed;

		SectorMemoryMapper m_extentsSectorListMMS;
		SectorMemoryMapper m_extentsTableMMS;
		SectorMemoryMapper m_fileTableMMS;
		SectorMemoryMapper m_directoryFileMMS;

		uint64_t m_fileTableExtra;
		uint64_t m_directoryFileExtra;

		ISeekableReadStream &m_readStream;
		ISeekableWriteStream *m_writeStream;

		static const size_t kSectorSizeBitsMin = 9;
		static const size_t kSectorSizeBitsMax = 24;
		static const size_t kSectorSizeBitsDefault = 12;

		static const uint32_t kFileTableExtentsIndex = 0;
		static const uint32_t kRootDirectoryExtentsIndex = 1;

		bool m_isWriteAccessInitialized;
		bool m_haveFSWriteFaults;
		bool m_haveSectorWriteFaults;

		uint32_t m_maxExtentsTableSize = 0;

		ShadowFSHeader m_sfHeader;
		const FileLocator *m_writeLocator = nullptr;

		bool m_mayBecomeWritable;
		UniquePtr<IMutex> m_fsStateMutex;
		UniquePtr<IMutex> m_sectorFlushMutex;

		size_t m_fileInitialDataSize = 0;

		HashMap<uint32_t, size_t> m_readHandleCount;
	};

	class ShadowFSFile final : public ISeekableReadWriteStream
	{
	public:
		ShadowFSFile(ShadowFS &shadowFS, uint32_t fileTableIndex, uint32_t fileExtentsStart, const ExtentsTableEntry &extentsStart, bool isWritable);
		~ShadowFSFile();

		Result CreateInitialData(const ConstSpan<uint8_t> &fileName);
		Result LoadInitialData();

		Result ReadPartial(void *data, size_t count, size_t &outCountRead) override;
		Result WritePartial(const void *data, size_t count, size_t &outCountWritten) override;

		Result Flush() override;

		Result SeekStart(FilePos_t pos) override;
		Result SeekCurrent(FileOffset_t pos) override;
		Result SeekEnd(FileOffset_t pos) override;

		FilePos_t Tell() const override;
		FilePos_t GetSize() const override;

		Result Truncate(FilePos_t size) override;

	private:
		Result PrivFlush();
		Result PrivTruncate(FilePos_t size);

		FilePos_t m_size;
		FilePos_t m_pos;

		ShadowFS &m_shadowFS;

		uint32_t m_fileTableIndex;
		uint32_t m_initialFileTableEntrySector;
		ExtentsTableEntry m_currentExtents;
		bool m_isWritable;
		bool m_isMapped;
	};

	void FastIndexTable::FindAvailableSlotIndex(size_t &outSlotIndex)
	{
		int tier = kTiers - 1;

		ConstSpan<BoolVector::Chunk_t> bottomTierChunks = m_tables[kTiers - 1].GetChunks();

		for (size_t chunkIndex = 0; chunkIndex < bottomTierChunks.Count(); chunkIndex++)
		{
			if (bottomTierChunks[chunkIndex] != 0)
			{
				size_t slotIndex = chunkIndex;

				for (int tier = kTiers - 1; tier >= 0; tier--)
				{
					ConstSpan<BoolVector::Chunk_t> tierChunks = m_tables[kTiers - 1].GetChunks();

					int firstSetBit = FindLowestSetBit(bottomTierChunks[slotIndex]);
					RKIT_ASSERT(firstSetBit > 0);
					slotIndex = static_cast<size_t>(firstSetBit) + slotIndex * BoolVector::kBitsPerChunk;
				}

				outSlotIndex = slotIndex;
				break;
			}
		}

		outSlotIndex = bottomTierChunks.Count();
	}

	Result FastIndexTable::SetSlotState(size_t slot, bool occupied)
	{
		if (occupied)
		{
			if (slot == m_tables[0].Count())
			{
				RKIT_CHECK(SetSlotAvailable(slot));
			}

			SetSlotOccupied(slot);
			return ResultCode::kOK;
		}
		else
			return SetSlotAvailable(slot);
	}

	void FastIndexTable::SetSlotOccupied(size_t slot)
	{
		RKIT_ASSERT(slot < m_tables[0].Count());
		if (slot >= m_tables[0].Count())
			return;

		m_tables[0].Set(slot, false);

		size_t chunkIndexInLowerTier = slot / BoolVector::kBitsPerChunk;
		BoolVector::Chunk_t lowerChunk = m_tables[0].GetChunks()[chunkIndexInLowerTier];

		for (size_t upperTier = 1; upperTier < kTiers; upperTier++)
		{
			if (lowerChunk != 0)
				break;

			m_tables[upperTier].Set(chunkIndexInLowerTier, false);

			size_t chunkIndexInUpperTier = chunkIndexInLowerTier / BoolVector::kBitsPerChunk;
			BoolVector::Chunk_t upperChunk = m_tables[upperTier].GetChunks()[chunkIndexInUpperTier];

			chunkIndexInLowerTier = chunkIndexInUpperTier;
			lowerChunk = upperChunk;
		}
	}

	Result FastIndexTable::SetSlotAvailable(size_t slot)
	{
		bool isExisting = (slot < m_tables[0].Count());

		for (size_t tier = 0; tier < kTiers; tier++)
		{
			size_t slotInTier = slot >> (BoolVector::kLog2BitsPerChunk * tier);
			if (slotInTier == m_tables[tier].Count())
			{
				RKIT_CHECK(m_tables[tier].Append(false));
			}
		}

		size_t chunkIndexInLowerTier = slot / BoolVector::kBitsPerChunk;
		BoolVector::Chunk_t lowerChunk = m_tables[0].GetChunks()[chunkIndexInLowerTier];

		for (size_t upperTier = 1; upperTier < kTiers; upperTier++)
		{
			if (lowerChunk != 0)
				break;

			size_t chunkIndexInUpperTier = chunkIndexInLowerTier / BoolVector::kBitsPerChunk;
			BoolVector::Chunk_t upperChunk = m_tables[upperTier].GetChunks()[chunkIndexInUpperTier];

			m_tables[upperTier].Set(chunkIndexInLowerTier, true);

			chunkIndexInLowerTier = chunkIndexInUpperTier;
			lowerChunk = upperChunk;
		}

		m_tables[0].Set(slot, true);
		return ResultCode::kOK;
	}

	void FastIndexTable::Reset()
	{
		for (BoolVector &bv : m_tables)
			bv.Reset();
	}

	Result FastIndexTable::Duplicate(const FastIndexTable &other)
	{
		for (size_t i = 0; i < kTiers; i++)
		{
			RKIT_CHECK(m_tables[i].Duplicate(other.m_tables[i]));
		}

		return ResultCode::kOK;
	}


	ConvertToWriteInfo::ConvertToWriteInfo(uint64_t indexInParent, uint64_t extra, ConvertToWriteCallback_t convertToWriteCallback)
		: m_indexInParent(indexInParent)
		, m_extra(extra)
		, m_function(convertToWriteCallback)
	{
	}


	SectorMemoryMapper::SectorMemoryMapper(ShadowFS &shadowFS, ConvertToWriteInfo::ConvertToWriteCallback_t convertToWriteCallback)
		: m_shadowFS(shadowFS)
		, m_convertToWrite(0, 0, convertToWriteCallback)
		, m_isWritable(false)
		, m_isOpen(false)
		, m_sector(0)
	{
	}

	SectorMemoryMapper::~SectorMemoryMapper()
	{
		(void)Close();
	}

	Result SectorMemoryMapper::ChangeSector(uint32_t sector, uint64_t indexInParent, uint64_t extra)
	{
		if (sector != m_sector)
		{
			RKIT_CHECK(Close());
		}

		m_convertToWrite.m_indexInParent = indexInParent;
		m_convertToWrite.m_extra = extra;

		return ResultCode::kOK;
	}

	Result SectorMemoryMapper::Close()
	{
		if (m_isOpen)
		{
			if (m_isWritable)
			{
				RKIT_CHECK(m_shadowFS.FlushSector(m_sector, m_memory.ToSpan()));
			}

			m_isOpen = false;
			m_isWritable = false;
		}

		return ResultCode::kOK;
	}

	Result SectorMemoryMapper::MapRead(ConstSpan<uint8_t> &outSpan, bool isInFSLock)
	{
		if (!m_isOpen)
		{
			RKIT_CHECK(m_shadowFS.MapReadSector(m_memory, m_sector, isInFSLock));
			m_isOpen = true;
		}

		outSpan = m_memory.ToSpan();
		return ResultCode::kOK;
	}

	Result SectorMemoryMapper::MapWrite(Span<uint8_t> &outSpan, bool isInFSLock)
	{
		if (!m_isOpen)
		{
			ConstSpan<uint8_t> tempSpan;
			RKIT_CHECK(MapRead(tempSpan, isInFSLock));
		}

		if (!m_isWritable)
		{
			ConvertToWriteInfo::ConvertToWriteCallback_t callback = m_convertToWrite.m_function;
			RKIT_CHECK((m_shadowFS.*callback)(m_sector, m_convertToWrite, isInFSLock));
			m_isWritable = true;
		}

		outSpan = m_memory.ToSpan();
		return ResultCode::kOK;
	}

	Result SectorMemoryMapper::MapWriteZeroFill(Span<uint8_t> &outSpan, bool isInFSLock)
	{
		if (!m_isOpen)
		{
			RKIT_CHECK(m_memory.Resize(m_shadowFS.GetSectorSize()));

			memset(m_memory.GetBuffer(), 0, m_memory.Count());

			m_isOpen = true;
		}

		if (!m_isWritable)
		{
			ConvertToWriteInfo::ConvertToWriteCallback_t callback = m_convertToWrite.m_function;
			RKIT_CHECK((m_shadowFS.*callback)(m_sector, m_convertToWrite, isInFSLock));
			m_isWritable = true;
		}

		outSpan = m_memory.ToSpan();
		return ResultCode::kOK;
	}

	ShadowFS::ShadowFS(ISeekableReadStream &readStream, ISeekableWriteStream *writeStream)
		: m_sfHeader{}
		, m_readStream(readStream)
		, m_writeStream(writeStream)
		, m_isWriteAccessInitialized(false)
		, m_haveFSWriteFaults(false)
		, m_haveSectorWriteFaults(false)
		, m_mayBecomeWritable(writeStream != nullptr)
		, m_extentsSectorListMMS(*this, &ShadowFS::ConvertExtentsSectorListToWritable)
		, m_extentsTableMMS(*this, &ShadowFS::ConvertExtentsTableToWritable)
		, m_fileTableMMS(*this, &ShadowFS::ConvertFileTableToWritable)
		, m_directoryFileMMS(*this, &ShadowFS::ConvertFileSectorToWritable)
	{
		static_assert((static_cast<size_t>(1) << kSectorSizeBitsMin) >= sizeof(FileTableEntry));
		static_assert((static_cast<size_t>(1) << kSectorSizeBitsMin) >= sizeof(ShadowFSHeader));
	}

	Result ShadowFS::Initialize()
	{
		if (m_mayBecomeWritable)
		{
			ISystemDriver *sysDriver = GetDrivers().m_systemDriver;
			RKIT_CHECK(sysDriver->CreateMutex(m_fsStateMutex));
			RKIT_CHECK(sysDriver->CreateMutex(m_sectorFlushMutex));
		}

		return ResultCode::kOK;
	}

	Result ShadowFS::EntryExists(const StringSliceView &str, bool &outExists)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::TryOpenFileRead(UniquePtr<ISeekableReadStream> &outStream, const StringSliceView &str)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::TryOpenFileReadWrite(UniquePtr<ISeekableReadWriteStream> &outStream, const StringSliceView &str, bool createIfNotExists, bool createDirectories)
	{
		bool isCreating = false;

		outStream.Reset();

		if (!m_writeStream)
			return ResultCode::kOK;

		MutexLock lock(*m_fsStateMutex);
		if (m_haveFSWriteFaults)
			return ResultCode::kOperationFailed;

		ConstSpan<uint8_t> pathSpan = ConstSpan<uint8_t>(reinterpret_cast<const uint8_t *>(str.GetChars()), str.Length());

		ConstSpan<uint8_t> nameSpan;
		uint32_t directoryEntry = 0;
		uint32_t directoryExtentsIndex = 0;
		ExtentsTableEntry directoryExtentsData = {};
		RKIT_CHECK(ResolveFilePath(directoryEntry, directoryExtentsIndex, directoryExtentsData, nameSpan, pathSpan, createIfNotExists && createDirectories));

		uint32_t fileEntry = 0;
		uint32_t fileExtentsIndex = 0;
		uint32_t fileFlags = 0;
		ExtentsTableEntry fileExtentsData = {};
		bool exists = false;
		RKIT_CHECK(FindFileInDirectory(fileEntry, fileExtentsIndex, fileExtentsData, fileFlags, exists, directoryEntry, directoryExtentsIndex, directoryExtentsData, nameSpan));

		if (!exists)
		{
			if (!createIfNotExists)
				return ResultCode::kOK;

			RKIT_CHECK(CreateFileInDirectory(fileEntry, fileExtentsIndex, fileExtentsData, directoryEntry, directoryExtentsIndex, directoryExtentsData, nameSpan, 0));
		}

		UniquePtr<ShadowFSFile> file;
		RKIT_CHECK(New<ShadowFSFile>(file, *this, fileEntry, fileExtentsIndex, fileExtentsData, true));

		RKIT_CHECK(file->LoadInitialData());

		HashMap<uint32_t, size_t>::Iterator_t readCountEntryIt = m_readHandleCount.Find(fileEntry);
		if (readCountEntryIt == m_readHandleCount.end())
		{
			RKIT_CHECK(m_readHandleCount.Set(fileEntry, 0));
		}
		else
		{
			// Already opened
			return ResultCode::kFileOpenError;
		}

		outStream = std::move(file);

		return ResultCode::kOK;
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

		RKIT_CHECK(m_extentsSectorListMMS.Close());
		RKIT_CHECK(m_extentsTableMMS.Close());
		RKIT_CHECK(m_fileTableMMS.Close());
		RKIT_CHECK(m_directoryFileMMS.Close());

		if (m_haveFSWriteFaults || m_haveSectorWriteFaults)
			return ResultCode::kIOWriteError;

		bool needFullHeader = false;

		uint8_t newLocatorPlusOne = 0;

		ShadowFSHeader newHeader = m_sfHeader;

		if (newHeader.m_committedLocatorPlusOne == 1)
			newHeader.m_committedLocatorPlusOne = 2;
		else if (newHeader.m_committedLocatorPlusOne == 2)
			newHeader.m_committedLocatorPlusOne = 1;
		else
		{
			newHeader.m_committedLocatorPlusOne = 1;
			needFullHeader = true;
		}

		newHeader.m_identifier = ShadowFSHeader::kExpectedIdentifier;
		newHeader.m_version = ShadowFSHeader::kExpectedVersion;

		RKIT_CHECK(m_writeStream->Flush());

		if (m_writeStream->GetSize() < sizeof(ShadowFSHeader))
		{
			RKIT_CHECK(m_writeStream->WriteAll(&newHeader, sizeof(ShadowFSHeader)));
		}
		else
		{
			if (needFullHeader)
			{
				RKIT_CHECK(m_writeStream->SeekStart(0));

				RKIT_CHECK(m_writeStream->WriteAll(&newHeader, offsetof(ShadowFSHeader, m_locators)));
				RKIT_CHECK(m_writeStream->Flush());
			}

			uint8_t committedLocator = newHeader.m_committedLocatorPlusOne - 1;

			RKIT_CHECK(m_writeStream->SeekStart(offsetof(ShadowFSHeader, m_locators) + sizeof(FileLocator) * committedLocator));
			RKIT_CHECK(m_writeStream->WriteAll(&newHeader.m_locators[committedLocator], sizeof(FileLocator)));
			RKIT_CHECK(m_writeStream->SeekStart(offsetof(ShadowFSHeader, m_committedLocatorPlusOne)));
			RKIT_CHECK(m_writeStream->WriteAll(&newHeader.m_committedLocatorPlusOne, sizeof(newHeader.m_committedLocatorPlusOne)));
			RKIT_CHECK(m_writeStream->Flush());
		}

		m_sfHeader = newHeader;

		return ResultCode::kOK;
	}

	void ShadowFS::ClearWriteState()
	{
		m_isWriteAccessInitialized = false;
		m_writeLocator = nullptr;

		m_newUsedSectors.Reset();
		m_anyUsedSectors.Reset();
		m_extentsTableUsed.Reset();
		m_fileTableUsed.Reset();
	}

	Result ShadowFS::ResolveFilePath(uint32_t &outDirectoryEntry, uint32_t &outDirectoryExtents, ExtentsTableEntry &outDirectoryExtentsData, ConstSpan<uint8_t> &outNameSpan, const ConstSpan<uint8_t> &pathSpan, bool createDirectories)
	{
		uint32_t directoryEntry = 0;
		uint32_t directoryExtentsIndex = 0;
		ExtentsTableEntry directoryExtentsData = {};

		size_t componentStart = 0;
		for (size_t i = 0; i < pathSpan.Count(); i++)
		{
			if (i == '/')
			{
				ConstSpan<uint8_t> slice = pathSpan.SubSpan(componentStart, i - componentStart);

				if (slice.Count() == 0 || slice.Count() > FileTableEntry::kMaxNameLength)
					return ResultCode::kFileOpenError;

				componentStart = i;
			}
		}

		if (componentStart == pathSpan.Count() || (pathSpan.Count() - componentStart) > FileTableEntry::kMaxNameLength)
		{
			return ResultCode::kFileOpenError;
		}

		if (createDirectories)
		{
			RKIT_CHECK(CheckCreateRootDirectory(directoryEntry, directoryExtentsIndex, directoryExtentsData));
		}
		else
		{
			RKIT_CHECK(ResolveRootDirectory(directoryEntry, directoryExtentsIndex, directoryExtentsData));

		}

		componentStart = 0;
		for (size_t i = 0; i < pathSpan.Count(); i++)
		{
			if (i == '/')
			{
				ConstSpan<uint8_t> slice = pathSpan.SubSpan(componentStart, i - componentStart);

				if (slice.Count() == 0 || slice.Count() > FileTableEntry::kMaxNameLength)
					return ResultCode::kFileOpenError;

				uint32_t subfileEntry = 0;
				uint32_t subfileExtentsIndex = 0;
				uint32_t subfileFlags = 0;
				ExtentsTableEntry subfileExtentsData = {};
				bool exists = false;

				RKIT_CHECK(FindFileInDirectory(subfileEntry, subfileExtentsIndex, subfileExtentsData, subfileFlags, exists, directoryEntry, directoryExtentsIndex, directoryExtentsData, slice));

				if (!exists)
				{
					if (createDirectories)
					{
						RKIT_CHECK(CreateFileInDirectory(subfileEntry, subfileExtentsIndex, subfileExtentsData, directoryEntry, directoryExtentsIndex, directoryExtentsData, slice, kFlagDirectory));
					}
					else
						return ResultCode::kFileOpenError;
				}
				else
				{
					if ((subfileFlags & kFlagDirectory) == 0)
						return ResultCode::kFileOpenError;
				}

				directoryEntry = subfileEntry;
				directoryExtentsIndex = subfileExtentsIndex;
				directoryExtentsData = subfileExtentsData;

				componentStart = i;
			}
		}

		outNameSpan = ConstSpan<uint8_t>(pathSpan.SubSpan(componentStart, pathSpan.Count()));

		outDirectoryEntry = directoryEntry;
		outDirectoryExtents = directoryExtentsIndex;
		outDirectoryExtentsData = directoryExtentsData;

		return ResultCode::kOK;
	}

	Result ShadowFS::CheckCreateRootDirectory(uint32_t &outRootDirectoryEntry, uint32_t &outRootDirectoryExtentsIndex, ExtentsTableEntry &outRootDirectoryExtentsData)
	{
		FileLocator &locator = m_sfHeader.m_locators[m_sfHeader.m_committedLocatorPlusOne - 1];

		RKIT_CHECK(CheckInitializeWriteAccess());

		if (locator.m_extentsSectorListSector == 0)
		{
			RKIT_CHECK(AllocateSector(locator.m_extentsSectorListSector));
			locator.m_extentsTableSize = 0;
		}

		if (locator.m_extentsTableSize == 0)
		{
			RKIT_CHECK(AllocateExtents(outRootDirectoryExtentsIndex, outRootDirectoryExtentsData));
			RKIT_ASSERT(outRootDirectoryExtentsIndex == 0);
		}
		else
		{
			outRootDirectoryExtentsIndex = 0;
			RKIT_CHECK(ResolveExtents(outRootDirectoryExtentsData, 0));
		}

		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::ResolveRootDirectory(uint32_t &outRootDirectoryEntry, uint32_t &outRootDirectoryExtentsIndex, ExtentsTableEntry &outRootDirectoryExtentsData)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::ConvertExtentsSectorListToWritable(uint32_t &outNewSector, const ConvertToWriteInfo &ctw, bool isInFSLock)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::ConvertExtentsTableToWritable(uint32_t &outNewSector, const ConvertToWriteInfo &ctw, bool isInFSLock)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::ConvertFileTableToWritable(uint32_t &outNewSector, const ConvertToWriteInfo &ctw, bool isInFSLock)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::ConvertFileSectorToWritable(uint32_t &outNewSector, const ConvertToWriteInfo &ctw, bool isInFSLock)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::FindFileInDirectory(uint32_t &outFileEntry, uint32_t &outFileExtents, ExtentsTableEntry &outFileExtentsData, uint32_t &outFileFlags, bool &outExists, uint32_t directoryEntry, uint32_t directoryExtentsIndex, const ExtentsTableEntry &directoryExtentsData, const ConstSpan<uint8_t> &nameSpan)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::CreateFileInDirectory(uint32_t &outFileEntry, uint32_t &outFileExtents, ExtentsTableEntry &outFileExtentsData, uint32_t directoryEntry, uint32_t directoryExtents, const ExtentsTableEntry &directoryExtentsData, const ConstSpan<uint8_t> &nameSpan, uint32_t flags)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::CreateNewExtents(uint32_t &outFirstSector, uint64_t &outTableIndex, uint64_t &outExtra)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::AllocateSector(uint32_t &outSector)
	{
		FileLocator &locator = m_sfHeader.m_locators[m_sfHeader.m_committedLocatorPlusOne - 1];

		RKIT_CHECK(CheckInitializeWriteAccess());

		size_t slotIndex = 0;
		m_anyUsedSectors.FindAvailableSlotIndex(slotIndex);

		if (slotIndex >= 0x80000000u)
		{
			m_haveFSWriteFaults = true;
			return ResultCode::kOperationFailed;
		}

		RKIT_CHECK(m_anyUsedSectors.SetSlotState(slotIndex, true));

		if (slotIndex == locator.m_numSectors)
			locator.m_numSectors++;

		outSector = static_cast<uint32_t>(slotIndex);
		
		return ResultCode::kOK;
	}

	Result ShadowFS::AllocateExtents(uint32_t &outExtentsIndex, ExtentsTableEntry &outExtentsData)
	{
		FileLocator &locator = m_sfHeader.m_locators[m_sfHeader.m_committedLocatorPlusOne - 1];

		uint32_t sector = 0;
		RKIT_CHECK(AllocateSector(sector));

		RKIT_CHECK(CheckInitializeWriteAccess());

		size_t slotIndex = 0;
		m_extentsTableUsed.FindAvailableSlotIndex(slotIndex);

		RKIT_CHECK(m_extentsTableUsed.SetSlotState(slotIndex, true));

		if (slotIndex == m_maxExtentsTableSize)
		{
			m_haveFSWriteFaults = true;
			return ResultCode::kOperationFailed;
		}

		size_t extentsEntriesPerSector = GetSectorSize() / sizeof(ExtentsTableEntry);

		uint32_t extentsTableEntrySector = 0;
		if (slotIndex == locator.m_extentsTableSize)
		{
			locator.m_extentsTableSize++;

			if (slotIndex % extentsEntriesPerSector == 0)
			{
				// Requires a new sector
				RKIT_CHECK(AllocateSector(extentsTableEntrySector));

				RKIT_CHECK(m_extentsTableMMS.ChangeSector(extentsTableEntrySector, slotIndex, 0));

				Span<uint8_t> extentsTableMap;
				RKIT_CHECK(m_extentsTableMMS.MapWriteZeroFill(extentsTableMap, true));

				ExtentsTableEntry &newEntry = *reinterpret_cast<ExtentsTableEntry *>(extentsTableMap.Ptr());
				newEntry.m_sector = sector;
				newEntry.m_nextEntry = static_cast<uint32_t>(slotIndex);

				outExtentsData = newEntry;
				outExtentsIndex = static_cast<uint32_t>(slotIndex);

				return ResultCode::kOK;
			}
		}

		// Maps to existing sector
		const size_t extentsTableSectorOffset = slotIndex / extentsEntriesPerSector;
		const size_t slotIndexInExtentsTableSector = slotIndex % extentsEntriesPerSector;

		RKIT_CHECK(m_extentsSectorListMMS.ChangeSector(locator.m_extentsSectorListSector, 0, 0));

		Span<uint8_t> extentsSectorListMap;
		RKIT_CHECK(m_extentsSectorListMMS.MapWrite(extentsSectorListMap, true));

		uint32_t extentsTableSector = reinterpret_cast<uint32_t *>(extentsSectorListMap.Ptr())[slotIndexInExtentsTableSector];

		RKIT_CHECK(m_extentsTableMMS.ChangeSector(extentsTableSector, slotIndex, 0));

		Span<uint8_t> extentsTableMap;
		RKIT_CHECK(m_extentsTableMMS.MapWrite(extentsTableMap, true));

		ExtentsTableEntry &newEntry = reinterpret_cast<ExtentsTableEntry *>(extentsTableMap.Ptr())[slotIndexInExtentsTableSector];
		newEntry.m_sector = sector;
		newEntry.m_nextEntry = static_cast<uint32_t>(slotIndex);

		outExtentsData = newEntry;
		outExtentsIndex = static_cast<uint32_t>(slotIndex);

		return ResultCode::kOK;
	}

	Result ShadowFS::ResolveExtents(ExtentsTableEntry &outExtentsData, uint32_t extentsIndex)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::CheckInitializeWriteAccess()
	{
		if (m_haveFSWriteFaults)
			return ResultCode::kOperationFailed;

		if (m_isWriteAccessInitialized)
			return ResultCode::kOK;

		ClearWriteState();

		Result initializeWriteAccessResult = InitializeWriteAccess();

		if (initializeWriteAccessResult.IsOK())
			m_isWriteAccessInitialized = true;
		else
			m_haveFSWriteFaults = true;

		return initializeWriteAccessResult;
	}

	Result ShadowFS::InitializeWriteAccess()
	{
		if (m_sfHeader.m_committedLocatorPlusOne != 1 && m_sfHeader.m_committedLocatorPlusOne != 2)
			return ResultCode::kMalformedFile;

		uint8_t oldLocatorIndex = m_sfHeader.m_committedLocatorPlusOne - 1;
		uint8_t newLocatorIndex = 1 - oldLocatorIndex;

		m_sfHeader.m_locators[newLocatorIndex] = m_sfHeader.m_locators[oldLocatorIndex];

		m_sfHeader.m_committedLocatorPlusOne = newLocatorIndex + 1;

		const FileLocator &activeLocator = m_sfHeader.m_locators[newLocatorIndex];

		for (size_t i = 0; i < activeLocator.m_numSectors; i++)
		{
			RKIT_CHECK(m_anyUsedSectors.SetSlotState(i, false));
		}

		RKIT_CHECK(m_anyUsedSectors.SetSlotState(0, true));

		if (activeLocator.m_extentsTableSize > 0)
		{
			RKIT_CHECK(m_extentsSectorListMMS.ChangeSector(activeLocator.m_extentsSectorListSector, 0, 0));

			for (size_t i = 0; i < activeLocator.m_extentsTableSize; i++)
			{
				RKIT_CHECK(m_extentsTableUsed.SetSlotState(i, false));

				return ResultCode::kNotYetImplemented;
			}

			return ResultCode::kNotYetImplemented;
		}

		RKIT_CHECK(m_newUsedSectors.Duplicate(m_anyUsedSectors));

		return ResultCode::kOK;
	}

	ShadowFSFile::ShadowFSFile(ShadowFS &shadowFS, uint32_t fileTableIndex, uint32_t fileExtentsStart, const ExtentsTableEntry &initialExtentsData, bool isWritable)
		: m_shadowFS(shadowFS)
		, m_size(0)
		, m_pos(0)
		, m_fileTableIndex(fileTableIndex)
		, m_isWritable(isWritable)
		, m_isMapped(false)
		, m_currentExtents(initialExtentsData)
	{
	}

	ShadowFSFile::~ShadowFSFile()
	{
		(void)PrivFlush();
	}

	Result ShadowFSFile::CreateInitialData(const ConstSpan<uint8_t> &fileName)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFSFile::LoadInitialData()
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFSFile::ReadPartial(void *data, size_t count, size_t &outCountRead)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFSFile::WritePartial(const void *data, size_t count, size_t &outCountWritten)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFSFile::Flush()
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFSFile::SeekStart(FilePos_t pos)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFSFile::SeekCurrent(FileOffset_t pos)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFSFile::SeekEnd(FileOffset_t pos)
	{
		return ResultCode::kNotYetImplemented;
	}

	FilePos_t ShadowFSFile::Tell() const
	{
		return m_pos;
	}

	FilePos_t ShadowFSFile::GetSize() const
	{
		return m_size;
	}

	Result ShadowFSFile::Truncate(FilePos_t size)
	{
		if (size == m_size)
			return ResultCode::kOK;

		if (size > m_size)
			return ResultCode::kOperationFailed;

		RKIT_CHECK(PrivTruncate(size));

		if (m_pos > size)
			m_pos = size;

		m_size = size;

		return ResultCode::kOK;
	}

	Result ShadowFSFile::PrivFlush()
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFSFile::PrivTruncate(FilePos_t size)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::LoadShadowFile()
	{
		ClearWriteState();

		m_sfHeader = {};

		RKIT_CHECK(m_readStream.SeekStart(0));

		RKIT_CHECK(m_readStream.ReadAll(&m_sfHeader, sizeof(m_sfHeader)));

		if (m_sfHeader.m_version != m_sfHeader.kExpectedVersion || (m_sfHeader.m_committedLocatorPlusOne != 1 && m_sfHeader.m_committedLocatorPlusOne != 2))
			return Result::SoftFault(ResultCode::kOperationFailed);

		if (m_sfHeader.m_identifier != ShadowFSHeader::kExpectedIdentifier)
			return ResultCode::kOperationFailed;

		const FileLocator &locator = m_sfHeader.m_locators[m_sfHeader.m_committedLocatorPlusOne - 1];
		if (locator.m_extentsTableSize != 0 && (locator.m_extentsSectorListSector == 0 || locator.m_extentsSectorListSector >= locator.m_numSectors))
			return ResultCode::kOperationFailed;

		if (m_sfHeader.m_sectorSizeBits < kSectorSizeBitsMin || m_sfHeader.m_sectorSizeBits > kSectorSizeBitsMax)
			return ResultCode::kOperationFailed;

		const size_t numExtentsEntriesPerSector = GetSectorSize() / sizeof(ExtentsTableEntry);
		const size_t numSectorIndexesPerSector = GetSectorSize() / 4;

		uint64_t maxExtentsTableSize = static_cast<uint64_t>(numExtentsEntriesPerSector) * static_cast<uint64_t>(numSectorIndexesPerSector);
		if (maxExtentsTableSize > std::numeric_limits<uint32_t>::max())
			maxExtentsTableSize = std::numeric_limits<uint32_t>::max();

		m_maxExtentsTableSize = static_cast<uint32_t>(maxExtentsTableSize);

		if (locator.m_extentsTableSize > m_maxExtentsTableSize)
			return ResultCode::kOperationFailed;

		return ResultCode::kOK;
	}

	Result ShadowFS::InitializeShadowFile()
	{
		if (!m_writeStream)
			return ResultCode::kOperationFailed;

		m_sfHeader = {};
		m_sfHeader.m_sectorSizeBits = kSectorSizeBitsDefault;

		RKIT_CHECK(CommitChanges());
		
		return ResultCode::kOK;
	}

	size_t ShadowFS::GetSectorSize() const
	{
		return static_cast<size_t>(1) << m_sfHeader.m_sectorSizeBits;
	}

	Result ShadowFS::FlushSector(uint32_t sector, const ConstSpan<uint8_t> &memory)
	{
		return ResultCode::kNotYetImplemented;
	}

	Result ShadowFS::MapReadSector(Vector<uint8_t> &memory, uint32_t sector, bool isInFSLock)
	{
		return ResultCode::kNotYetImplemented;
	}
}

namespace rkit::utils
{
	Result ShadowFileBase::Create(UniquePtr<ShadowFileBase> &outShadowFile, ISeekableReadStream &readStream, ISeekableWriteStream *writeStream)
	{
		UniquePtr<shadowfile::ShadowFS> shadowFile;

		RKIT_CHECK(New<shadowfile::ShadowFS>(shadowFile, readStream, writeStream));
		RKIT_CHECK(shadowFile->Initialize());

		outShadowFile = std::move(shadowFile);

		return ResultCode::kOK;
	}
}
