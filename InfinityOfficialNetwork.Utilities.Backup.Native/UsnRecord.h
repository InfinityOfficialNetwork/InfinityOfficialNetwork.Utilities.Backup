#pragma once

#include <cstdint>

namespace InfinityOfficialNetwork::Utilities::Backup::Native {

	ref class UsnRecordV2;
	ref class UsnRecordV3;
	ref class UsnRecordV4;

	public interface class IUsnRecordVisitor {
		void Visit(UsnRecordV2^ record);
		void Visit(UsnRecordV3^ record);
		void Visit(UsnRecordV4^ record);
	};

	// --- Supporting Types ---

	[System::Flags]
		public enum class UsnReason : uint32_t {
		DataOverwrite = (0x00000001),
		DataExtend = (0x00000002),
		DataTruncation = (0x00000004),
		NamedDataOverwrite = (0x00000010),
		NamedDataExtend = (0x00000020),
		NamedDataTruncation = (0x00000040),
		FileCreate = (0x00000100),
		FileDelete = (0x00000200),
		EaChange = (0x00000400),
		SecurityChange = (0x00000800),
		RenameOldName = (0x00001000),
		RenameNewName = (0x00002000),
		IndexableChange = (0x00004000),
		BasicInfoChange = (0x00008000),
		HardLinkChange = (0x00010000),
		CompressionChange = (0x00020000),
		EncryptionChange = (0x00040000),
		ObjectIdChange = (0x00080000),
		ReparsePointChange = (0x00100000),
		StreamChange = (0x00200000),
		TransactedChange = (0x00400000),
		IntegrityChange = (0x00800000),
		Close = (0x80000000)
	};

	public value struct UsnExtent {
		uint64_t Offset;
		uint64_t Length;
	};

	// --- The Hierarchy ---

	public ref class UsnRecord abstract {
	public:
		property uint32_t RecordLength;
		property uint16_t MajorVersion;
		property uint16_t MinorVersion;
		property uint64_t Usn;

		virtual void Accept(IUsnRecordVisitor^ visitor) abstract;
	};

	public ref class UsnRecordV2 : UsnRecord {
	public:
		property uint64_t FileReferenceNumber;
		property uint64_t ParentFileReferenceNumber;
		property System::DateTime Timestamp;
		property UsnReason Reason;
		property uint32_t SourceInfo;
		property System::String^ FileName;

		virtual void Accept(IUsnRecordVisitor^ visitor) override {
			visitor->Visit(this);
		}
	};

	public ref class UsnRecordV3 : UsnRecord {
	public:
		property System::UInt128 FileReferenceNumber;
		property System::UInt128 ParentFileReferenceNumber;
		property System::DateTime Timestamp;
		property UsnReason Reason;
		property uint32_t SourceInfo;
		property System::String^ FileName;

		virtual void Accept(IUsnRecordVisitor^ visitor) override {
			visitor->Visit(this);
		}
	};

	public ref class UsnRecordV4 : UsnRecord {
	public:
		property System::UInt128 FileReferenceNumber;
		property System::UInt128 ParentFileReferenceNumber;
		property UsnReason Reason;
		property uint32_t SourceInfo;
		property array<UsnExtent>^ Extents;

		virtual void Accept(IUsnRecordVisitor^ visitor) override {
			visitor->Visit(this);
		}
	};
}