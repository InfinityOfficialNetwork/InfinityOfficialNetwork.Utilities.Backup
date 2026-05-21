#include "pch.h"
#include "FileHandle.h"
#include <windows.h>
#include <winternl.h>
#include <aclapi.h>

typedef struct _FILE_STREAM_INFORMATION {
	ULONG         NextEntryOffset;
	ULONG         StreamNameLength;
	LARGE_INTEGER StreamSize;
	LARGE_INTEGER StreamAllocationSize;
	WCHAR         StreamName[1];
} FILE_STREAM_INFORMATION, * PFILE_STREAM_INFORMATION;


#include "VssSnapshotVolume.h"

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "advapi32.lib")

using namespace System;
using namespace System::IO;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;
using namespace Microsoft::Win32::SafeHandles;
using namespace System::ComponentModel;
using namespace InfinityOfficialNetwork::Utilities::Backup::Native;

extern "C" {


	// If you need NtQueryEaFile as well:
	NTSYSAPI NTSTATUS NTAPI NtQueryEaFile(
		IN  HANDLE FileHandle,
		OUT PIO_STATUS_BLOCK IoStatusBlock,
		OUT PVOID Buffer,
		IN  ULONG Length,
		IN  BOOLEAN ReturnSingleEntry,
		IN  PVOID EaList OPTIONAL,
		IN  ULONG EaListLength,
		IN  PULONG EaIndex OPTIONAL,
		IN  BOOLEAN RestartScan
	);
}

SafeFileHandle^ OpenFileHandle(SafeFileHandle^ volume, ValueTuple<UInt128, bool> fileId, DWORD access) {
	FILE_ID_DESCRIPTOR desc = { 0 };

	if (fileId.Item2) {
		desc.dwSize = sizeof(FILE_ID_DESCRIPTOR);
		desc.Type = FileIdType; // Use 64-bit type
		desc.FileId.QuadPart = (LONGLONG)fileId.Item1;
	}
	else {
		desc.dwSize = sizeof(FILE_ID_DESCRIPTOR);
		desc.Type = ExtendedFileIdType; // Use 128-bit type

		// Copy bytes into the native 128-bit structure
		memcpy(&desc.ExtendedFileId, &fileId.Item1, 16);
	}

	HANDLE hFile = OpenFileById(
		(HANDLE)volume->DangerousGetHandle(),
		&desc,
		access,          // Desired Access
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,                  // Security Attributes
		FILE_FLAG_BACKUP_SEMANTICS                      // Flags
	);

	if (hFile == INVALID_HANDLE_VALUE) {
		int error = Marshal::GetLastWin32Error();
		throw gcnew Win32Exception(error);
	}

	return gcnew SafeFileHandle(IntPtr(hFile), true);
}

Stream^ FileHandle::OpenDataStream()
{
	return gcnew FileStream(OpenFileHandle(volume->SnapshotVolumeHandle, fileId, GENERIC_READ), FileAccess::Read, 65536, true);
}

Stream^ FileHandle::OpenAlternateDataStream(String^ name)
{
	auto hFile = OpenFileHandle(volume->SnapshotVolumeHandle, fileId, GENERIC_READ);

	std::wstring streamName = msclr::interop::marshal_as<std::wstring>(name);
	if (streamName[0] != ':')
		streamName = L":" + streamName;

	UNICODE_STRING uStreamName;
	uStreamName.Buffer = const_cast<wchar_t*>(streamName.c_str());
	uStreamName.Length = streamName.size() * sizeof(wchar_t);
	uStreamName.MaximumLength = uStreamName.Length;

	OBJECT_ATTRIBUTES oa;
	InitializeObjectAttributes(
		&oa,
		&uStreamName,               // The relative name (:stream)
		OBJ_CASE_INSENSITIVE,
		(HANDLE)hFile->DangerousGetHandle(), // The base file handle
		NULL
	);

	HANDLE hStream = NULL;
	IO_STATUS_BLOCK ioStatus = { 0 };

	NTSTATUS status = NtOpenFile(
		&hStream,
		GENERIC_READ | SYNCHRONIZE,
		&oa,
		&ioStatus,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		FILE_NON_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT
	);

	if (!NT_SUCCESS(status) && status != STATUS_BREAKPOINT) {
		int win32Err = RtlNtStatusToDosError(status);
		throw gcnew Win32Exception(win32Err);
	}

	SafeFileHandle^ shStream = gcnew SafeFileHandle(IntPtr(hStream), true);
	return gcnew FileStream(shStream, FileAccess::Read, 65536, true);
}

Stream^ FileHandle::OpenSecurityInformation()
{
	// 1. Open the file handle by ID.
	// READ_CONTROL is for DACL/Owner.
	// ACCESS_SYSTEM_SECURITY is REQUIRED for SACL (Auditing).
	// FILE_FLAG_BACKUP_SEMANTICS is required for OpenFileById on files/folders.
	HANDLE hVolume = (HANDLE)volume->SnapshotVolumeHandle->DangerousGetHandle();

	FILE_ID_DESCRIPTOR desc = { 0 };
	desc.dwSize = sizeof(FILE_ID_DESCRIPTOR);
	if (fileId.Item2) {
		desc.Type = FileIdType;
		desc.FileId.QuadPart = (LONGLONG)(unsigned __int64)fileId.Item1;
	}
	else {
		desc.Type = ExtendedFileIdType;
		pin_ptr<UInt128> pId = &fileId.Item1;
		memcpy(&desc.ExtendedFileId, pId, 16);
	}

	HANDLE hNative = OpenFileById(hVolume, &desc,
		READ_CONTROL | ACCESS_SYSTEM_SECURITY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		FILE_FLAG_BACKUP_SEMANTICS);

	if (hNative == INVALID_HANDLE_VALUE) {
		// Fallback: If SACL fails, try reading without SACL (Auditing info will be lost)
		hNative = OpenFileById(hVolume, &desc, READ_CONTROL,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, FILE_FLAG_BACKUP_SEMANTICS);

		if (hNative == INVALID_HANDLE_VALUE)
			throw gcnew Win32Exception(Marshal::GetLastWin32Error());
	}

	PSECURITY_DESCRIPTOR pSD = NULL;

	// 2. Query the security information
	// We include SACL_SECURITY_INFORMATION for a "Full Backup"
	DWORD result = GetSecurityInfo(
		hNative,
		SE_FILE_OBJECT,
		OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
		DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION,
		NULL, NULL, NULL, NULL,
		&pSD
	);

	// Close handle immediately after query
	CloseHandle(hNative);

	if (result != ERROR_SUCCESS) {
		throw gcnew Win32Exception(result);
	}

	try {
		DWORD length = GetSecurityDescriptorLength(pSD);
		array<Byte>^ managedBuffer = gcnew array<Byte>(length);
		Marshal::Copy(IntPtr(pSD), managedBuffer, 0, length);

		// This stream now contains the binary Self-Relative Security Descriptor
		return gcnew MemoryStream(managedBuffer);
	}
	finally {
		if (pSD != NULL) LocalFree(pSD);
	}
}

Stream^ FileHandle::OpenExtendedAttributes()
{
	// 1. Open the file handle by ID with FILE_READ_EA access
	SafeFileHandle^ hFile = OpenFileHandle(volume->SnapshotVolumeHandle, fileId, FILE_READ_EA);

	HANDLE hNative = (HANDLE)hFile->DangerousGetHandle();

	// 2. Allocate the maximum possible size for NTFS EAs (64KB)
	// We use a native buffer to satisfy NT API alignment requirements
	const ULONG maxEaSize = 65536;
	void* buffer = malloc(maxEaSize);
	if (!buffer) throw gcnew OutOfMemoryException();

	try {
		IO_STATUS_BLOCK ioStatus = { 0 };

		// 3. Query all EAs
		// RestartScan = TRUE ensures we start from the first EA
		NTSTATUS status = NtQueryEaFile(
			hNative,
			&ioStatus,
			buffer,
			maxEaSize,
			FALSE,  // ReturnSingleEntry = FALSE (get all of them)
			NULL, 0, NULL,
			TRUE    // RestartScan
		);

		// 4. Handle results
		// STATUS_NO_EAS or STATUS_SUCCESS with 0 bytes means no EAs exist.
		if (status == 0xC0000052) { // STATUS_NO_EAS
			return gcnew MemoryStream(0);
		}

		if (!NT_SUCCESS(status)) {
			throw gcnew Win32Exception(RtlNtStatusToDosError(status));
		}

		// ioStatus.Information contains the actual number of bytes written to the buffer
		DWORD actualSize = (DWORD)ioStatus.Information;
		if (actualSize == 0) {
			return gcnew MemoryStream(0);
		}

		// 5. Wrap the binary blob into a managed stream
		array<Byte>^ managedBuffer = gcnew array<Byte>(actualSize);
		Marshal::Copy(IntPtr(buffer), managedBuffer, 0, actualSize);

		return gcnew MemoryStream(managedBuffer);
	}
	finally {
		free(buffer);
		delete hFile;
	}
}

IEnumerable<String^>^ FileHandle::GetAlternateDataStreams()
{
	auto list = gcnew List<String^>(0);

	// 1. Get the base handle to the file
	auto hFile = OpenFileHandle(volume->SnapshotVolumeHandle, fileId, GENERIC_READ);
	if (hFile->IsInvalid) return list;
	HANDLE hNative = (HANDLE)hFile->DangerousGetHandle();

	// 2. Prepare a dynamic buffer
	DWORD bufferSize = 4096; // Start with 4KB
	std::vector<BYTE> buffer(bufferSize);

	// 3. Query the stream information with a retry loop
	while (!GetFileInformationByHandleEx(hNative, FileStreamInfo, buffer.data(), (DWORD)buffer.size())) {
		DWORD error = GetLastError();

		if (error == ERROR_MORE_DATA || error == ERROR_INSUFFICIENT_BUFFER) {
			// Buffer was too small. Double the size and try again.
			bufferSize *= 2;
			buffer.resize(bufferSize);
			continue;
		}

		// ERROR_HANDLE_EOF or ERROR_NO_MORE_FILES means no streams exist
		if (error == ERROR_HANDLE_EOF || error == ERROR_NO_MORE_FILES) {
			return list;
		}

		// A real error occurred
		throw gcnew Win32Exception(error);
	}

	// 4. Iterate through the buffer
	PFILE_STREAM_INFORMATION pInfo = reinterpret_cast<PFILE_STREAM_INFORMATION>(buffer.data());

	while (true) {
		// StreamNameLength is in bytes
		int nameCharCount = pInfo->StreamNameLength / sizeof(wchar_t);
		String^ fullStreamName = gcnew String(pInfo->StreamName, 0, nameCharCount);

		// Filter out the default data stream (::$DATA)
		if (!String::Equals(fullStreamName, "::$DATA", StringComparison::OrdinalIgnoreCase)) {

			String^ cleanName = fullStreamName;

			// Remove leading colon: ":StreamName:$DATA" -> "StreamName:$DATA"
			if (cleanName->StartsWith(L":")) {
				cleanName = cleanName->Substring(1);
			}

			// Remove trailing type: "StreamName:$DATA" -> "StreamName"
			if (cleanName->EndsWith(L":$DATA", StringComparison::OrdinalIgnoreCase)) {
				cleanName = cleanName->Substring(0, cleanName->Length - 6);
			}

			list->Add(cleanName);
		}

		if (pInfo->NextEntryOffset == 0) break;
		pInfo = reinterpret_cast<PFILE_STREAM_INFORMATION>(reinterpret_cast<BYTE*>(pInfo) + pInfo->NextEntryOffset);
	}

	// hFile will be automatically closed when the SafeFileHandle is garbage collected, 
	// but we can be explicit:
	delete hFile;

	return list;
}