#include "pch.h"
#include "UsnJournalReader.h"
#include "marshal.h"
#include "VssSnapshotVolume.h"
#include "VssSnapshotSession.h"

#include <windows.h>

using namespace System;
using namespace System::Runtime::InteropServices;
using namespace System::Collections::Generic;
using namespace Microsoft::Extensions::Logging;
using namespace InfinityOfficialNetwork::Utilities::Backup::Native;
using namespace InfinityOfficialNetwork::Utilities::Backup::Shared::Database::UsnJournal;

#include <winioctl.h>

void UsnJournalReader::ExtractAllRecords(InfinityOfficialNetwork::Utilities::Backup::Shared::Database::UsnJournal::UsnJournalScratchDbContext^ ctx)
{
    auto logger = volume->session->logger;
    IDisposable^ scope = LoggerExtensions::BeginScope(logger, "USN MFT Enumeration (All Files)");

    uint64_t recordsProcessed = 0;

    void* buffer = nullptr;

    try {
        LoggerExtensions::LogInformation(logger, "Querying USN journal for MFT metadata", nullptr);

        HANDLE hVol = (HANDLE)volume->SnapshotVolumeHandle->DangerousGetHandle();

        // 1. Get the current Journal State
        USN_JOURNAL_DATA_V1 journalData;
        DWORD bytesReturned;
        if (!DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &journalData, sizeof(journalData), &bytesReturned, NULL)) {
            throw gcnew System::IO::IOException("Failed to query USN journal", Marshal::GetLastWin32Error());
        }

        // 2. Setup Enumeration Parameters
        MFT_ENUM_DATA_V1 enumData;
        enumData.StartFileReferenceNumber = 0;
        enumData.LowUsn = 0;
        enumData.HighUsn = journalData.NextUsn; // Limit to records existing at this moment
        enumData.MaxMajorVersion = 4;
        enumData.MinMajorVersion = 2;

        const DWORD bufferSize = 0x10000; // 64KB
        buffer = malloc(bufferSize);
        if (!buffer) throw gcnew OutOfMemoryException("Buffer allocation failed");

        std::chrono::high_resolution_clock clk;
        auto begin = clk.now();

        int batchCount = 0;

        // 3. The Enumeration Loop
        while (true) {
            int64_t timeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clk.now() - begin).count();
            if (timeElapsed > 1000) {
                LoggerExtensions::LogInformation(logger, "Usn journal query enumerated {0} records so far, time elapsed {1}s", recordsProcessed, timeElapsed / 1000.0);
                begin = clk.now();
            }

            // FSCTL_ENUM_USN_DATA is used for full volume listing
            if (!DeviceIoControl(hVol, FSCTL_ENUM_USN_DATA, &enumData, sizeof(enumData), buffer, bufferSize, &bytesReturned, NULL)) {
                DWORD error = Marshal::GetLastWin32Error();
                if (error == ERROR_HANDLE_EOF) break; // Finished MFT scan
                throw gcnew System::IO::IOException("MFT enumeration failed", error);
            }

            if (bytesReturned <= sizeof(DWORDLONG)) break;

            // The first 8 bytes of the buffer is the NEXT FileReferenceNumber to query
            DWORDLONG nextFileRef = *(DWORDLONG*)buffer;

            // Records start after the 8-byte reference number
            BYTE* pRawRecord = (BYTE*)buffer + sizeof(DWORDLONG);
            BYTE* pEnd = (BYTE*)buffer + bytesReturned;

            while (pRawRecord < pEnd) {
                USN_RECORD_COMMON_HEADER* header = (USN_RECORD_COMMON_HEADER*)pRawRecord;
                if (header->RecordLength == 0) break;

                // Create the common header (Wait to assign USN until we know the version)
                auto headerEntity = gcnew UsnRecordCommonHeaderEntity();
                headerEntity->MajorVersion = header->MajorVersion;
                headerEntity->MinorVersion = header->MinorVersion;

                if (header->MajorVersion == 2) {
                    USN_RECORD_V2* v2 = (USN_RECORD_V2*)pRawRecord;
                    headerEntity->Usn = v2->Usn;
                    ctx->UsnRecords->Add(headerEntity);

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

                    ctx->UsnRecordsV2->Add(v2Entity);
                    batchCount++;
                }
                else if (header->MajorVersion == 3) {
                    USN_RECORD_V3* v3 = (USN_RECORD_V3*)pRawRecord;
                    headerEntity->Usn = v3->Usn;
                    ctx->UsnRecords->Add(headerEntity);

                    auto v3Entity = gcnew UsnRecordV3Entity();
                    v3Entity->Usn = v3->Usn;

                    // FILE_ID_128 is a 16-byte array. We read it as two 64-bit integers.
                    // System::UInt128 constructor takes (upper, lower). Little-endian means index 1 is upper.
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

                    ctx->UsnRecordsV3->Add(v3Entity);
                    batchCount++;
                }
                else if (header->MajorVersion == 4) {
                    USN_RECORD_V4* v4 = (USN_RECORD_V4*)pRawRecord;
                    headerEntity->Usn = v4->Usn;
                    ctx->UsnRecords->Add(headerEntity);

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
                            UsnExtent^ ue = gcnew UsnExtent();
                            ue->Offset = (uint64_t)ext->Offset;
                            ue->Length = (uint64_t)ext->Length;
                            v4Entity->Extents->Add(ue);
                            pExtent += v4->ExtentSize;
                        }
                    }

                    ctx->UsnRecordsV4->Add(v4Entity);
                    batchCount++;
                }

                // Batch Save & Purge Tracker
                if (batchCount >= 5000) {
                    ctx->SaveChanges();
                    ctx->ChangeTracker->Clear();

                    batchCount = 0;
                }

                pRawRecord += header->RecordLength;
                recordsProcessed++;
            }

            // Set the start for the next batch to the ID returned at the start of the buffer
            enumData.StartFileReferenceNumber = nextFileRef;
        }

        // Final commit for any trailing records that didn't hit the 5000 threshold
        if (batchCount > 0) {
            ctx->SaveChanges();
            ctx->ChangeTracker->Clear();
        }

    }
    finally {
        if (buffer) free(buffer);
        if (scope) delete scope;
    }

    LoggerExtensions::LogInformation(logger, "Enumeration complete, successfully persisted {0} records to DB", recordsProcessed);
}