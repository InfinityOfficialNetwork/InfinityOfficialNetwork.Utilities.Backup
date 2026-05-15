#include "pch.h"
#include "UsnJournalReader.h"
#include "marshal.h"
#include "VssSnapshotVolume.h"
#include "VssSnapshotSession.h"

#include <windows.h>

using namespace System;
using namespace System::Runtime;
using namespace System::Runtime::InteropServices;
using namespace System::Collections::Generic;
using namespace Microsoft::Extensions::Logging;
using namespace InfinityOfficialNetwork::Utilities::Backup::Native;
using namespace InfinityOfficialNetwork::Utilities::Backup::Shared::Database::UsnJournal;

#include <winioctl.h>

void FlushToDb(List<UsnRecordCommonHeaderEntity^>^ batchHeaders,
	List<UsnRecordV2Entity^>^ batchV2,
	List<UsnRecordV3Entity^>^ batchV3,
	List<UsnRecordV4Entity^>^ batchV4,
	UsnJournalScratchDbContext^ ctx)
{
	if (batchHeaders->Count > 0) {
		if (batchHeaders->Count > 0) ctx->UsnRecords->AddRange(batchHeaders);
		if (batchV2->Count > 0) ctx->UsnRecordsV2->AddRange(batchV2);
		if (batchV3->Count > 0) ctx->UsnRecordsV3->AddRange(batchV3);
		if (batchV4->Count > 0) ctx->UsnRecordsV4->AddRange(batchV4);

		ctx->SaveChanges();
		ctx->ChangeTracker->Clear();

		batchHeaders->Clear();
		batchV2->Clear();
		batchV3->Clear();
		batchV4->Clear();
	}
}

void UsnJournalReader::ExtractAllRecords(InfinityOfficialNetwork::Utilities::Backup::Shared::Database::UsnJournal::UsnJournalScratchDbContext^ ctx)
{
	auto logger = volume->session->logger;
	IDisposable^ scope = LoggerExtensions::BeginScope(logger, "USN MFT Enumeration (All Files)");

	uint64_t recordsProcessed = 0;
	void* buffer = nullptr;

	// 1. Optimization: Disable automatic change detection
	ctx->ChangeTracker->AutoDetectChangesEnabled = false;

	constexpr int batchSize = 5000;

	// 2. Initialize Batch Lists
	auto batchHeaders = gcnew List<UsnRecordCommonHeaderEntity^>(batchSize);
	auto batchV2 = gcnew List<UsnRecordV2Entity^>(batchSize);
	auto batchV3 = gcnew List<UsnRecordV3Entity^>(batchSize);
	auto batchV4 = gcnew List<UsnRecordV4Entity^>(batchSize);

	try {
		LoggerExtensions::LogInformation(logger, "Querying USN journal for MFT metadata", nullptr);

		HANDLE hVol = (HANDLE)volume->SnapshotVolumeHandle->DangerousGetHandle();

		USN_JOURNAL_DATA_V1 journalData;
		DWORD bytesReturned;
		if (!DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &journalData, sizeof(journalData), &bytesReturned, NULL)) {
			throw gcnew System::IO::IOException("Failed to query USN journal", Marshal::GetLastWin32Error());
		}

		MFT_ENUM_DATA_V1 enumData;
		enumData.StartFileReferenceNumber = 0;
		enumData.LowUsn = 0;
		enumData.HighUsn = journalData.NextUsn;
		enumData.MaxMajorVersion = 4;
		enumData.MinMajorVersion = 2;

		const DWORD bufferSize = 0x10000; // 64KB
		buffer = malloc(bufferSize);
		if (!buffer) throw gcnew OutOfMemoryException("Buffer allocation failed");

		std::chrono::high_resolution_clock clk;
		auto begin = clk.now();

		auto firstBegin = begin;

		bool noGc = GC::TryStartNoGCRegion(200 * 1024 * 1024, false);

		try {

			while (true) {
				if (std::chrono::duration_cast<std::chrono::milliseconds>(clk.now() - begin).count() > 1000) {
					LoggerExtensions::LogInformation(logger, "Usn journal query enumerated {0} records so far, time elapsed {1}s", recordsProcessed, std::chrono::duration_cast<std::chrono::milliseconds>(clk.now() - firstBegin).count() / 1000.0);
					begin = clk.now();
				}

				if (!DeviceIoControl(hVol, FSCTL_ENUM_USN_DATA, &enumData, sizeof(enumData), buffer, bufferSize, &bytesReturned, NULL)) {
					DWORD error = Marshal::GetLastWin32Error();
					if (error == ERROR_HANDLE_EOF) break;
					throw gcnew System::IO::IOException("MFT enumeration failed", error);
				}


				if (bytesReturned <= sizeof(DWORDLONG)) break;

				DWORDLONG nextFileRef = *(DWORDLONG*)buffer;
				BYTE* pRawRecord = (BYTE*)buffer + sizeof(DWORDLONG);
				BYTE* pEnd = (BYTE*)buffer + bytesReturned;

				while (pRawRecord < pEnd) {
					USN_RECORD_COMMON_HEADER* header = (USN_RECORD_COMMON_HEADER*)pRawRecord;
					if (header->RecordLength == 0) break;

					auto headerEntity = gcnew UsnRecordCommonHeaderEntity();
					headerEntity->MajorVersion = header->MajorVersion;
					headerEntity->MinorVersion = header->MinorVersion;

					if (header->MajorVersion == 2) {
						USN_RECORD_V2* v2 = (USN_RECORD_V2*)pRawRecord;
						headerEntity->Usn = v2->Usn;

						auto v2Entity = gcnew UsnRecordV2Entity();
						v2Entity->Usn = v2->Usn;
						v2Entity->FileReferenceNumber = v2->FileReferenceNumber;
						v2Entity->ParentFileReferenceNumber = v2->ParentFileReferenceNumber;
						v2Entity->Timestamp = DateTime::FromFileTimeUtc(v2->TimeStamp.QuadPart);
						v2Entity->Reason = v2->Reason;
						v2Entity->SourceInfo = v2->SourceInfo;
						if (v2->FileNameLength > 0) {
							v2Entity->FileName = gcnew String((wchar_t*)(pRawRecord + v2->FileNameOffset), 0, v2->FileNameLength / sizeof(wchar_t));
						}

						batchHeaders->Add(headerEntity);
						batchV2->Add(v2Entity);
					}
					else if (header->MajorVersion == 3) {
						USN_RECORD_V3* v3 = (USN_RECORD_V3*)pRawRecord;
						headerEntity->Usn = v3->Usn;

						auto v3Entity = gcnew UsnRecordV3Entity();
						v3Entity->Usn = v3->Usn;
						uint64_t* pFid = (uint64_t*)v3->FileReferenceNumber.Identifier;
						v3Entity->FileReferenceNumber = System::UInt128(pFid[1], pFid[0]);
						uint64_t* pPid = (uint64_t*)v3->ParentFileReferenceNumber.Identifier;
						v3Entity->ParentFileReferenceNumber = System::UInt128(pPid[1], pPid[0]);
						v3Entity->Timestamp = DateTime::FromFileTimeUtc(v3->TimeStamp.QuadPart);
						v3Entity->Reason = v3->Reason;
						v3Entity->SourceInfo = v3->SourceInfo;
						if (v3->FileNameLength > 0) {
							v3Entity->FileName = gcnew String((wchar_t*)(pRawRecord + v3->FileNameOffset), 0, v3->FileNameLength / sizeof(wchar_t));
						}

						batchHeaders->Add(headerEntity);
						batchV3->Add(v3Entity);
					}
					else if (header->MajorVersion == 4) {
						USN_RECORD_V4* v4 = (USN_RECORD_V4*)pRawRecord;
						headerEntity->Usn = v4->Usn;

						auto v4Entity = gcnew UsnRecordV4Entity();
						v4Entity->Usn = v4->Usn;
						uint64_t* pFid = (uint64_t*)v4->FileReferenceNumber.Identifier;
						v4Entity->FileReferenceNumber = System::UInt128(pFid[1], pFid[0]);
						uint64_t* pPid = (uint64_t*)v4->ParentFileReferenceNumber.Identifier;
						v4Entity->ParentFileReferenceNumber = System::UInt128(pPid[1], pPid[0]);
						v4Entity->Reason = v4->Reason;
						v4Entity->SourceInfo = v4->SourceInfo;

						if (v4->NumberOfExtents > 0) {
							BYTE* pExtent = (BYTE*)v4->Extents;
							for (WORD i = 0; i < v4->NumberOfExtents; i++) {
								USN_RECORD_EXTENT* ext = (USN_RECORD_EXTENT*)pExtent;
								// Optimization Tip: If UsnExtent is changed to a struct, 
								// you avoid 'gcnew' here and just do: v4Entity->Extents->Add(UsnExtent(o, l));
								UsnExtent^ ue = gcnew UsnExtent();
								ue->Offset = (uint64_t)ext->Offset;
								ue->Length = (uint64_t)ext->Length;
								v4Entity->Extents->Add(ue);
								pExtent += v4->ExtentSize;
							}
						}

						batchHeaders->Add(headerEntity);
						batchV4->Add(v4Entity);
					}

					// Batch Save every 5000 header records
					if (batchHeaders->Count >= batchSize) {
						FlushToDb(batchHeaders, batchV2, batchV3, batchV4, ctx);

						if (noGc && GCSettings::LatencyMode == GCLatencyMode::NoGCRegion)
							GC::EndNoGCRegion(), GC::Collect(0);
					}

					pRawRecord += header->RecordLength;
					recordsProcessed++;
				}
				enumData.StartFileReferenceNumber = nextFileRef;

			}

			// Final commit
			FlushToDb(batchHeaders, batchV2, batchV3, batchV4, ctx);
		}
		finally {
			if (noGc && GCSettings::LatencyMode == GCLatencyMode::NoGCRegion)
				GC::EndNoGCRegion(), GC::Collect(2, GCCollectionMode::Aggressive, true, true);
		}

	}
	finally {
		if (buffer) free(buffer);
		if (scope) delete scope;
	}

	LoggerExtensions::LogInformation(logger, "Enumeration complete, successfully persisted {0} records to DB", recordsProcessed);
}
