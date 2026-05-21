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


IList<UsnRecord^>^ UsnJournalReader::ExtractAllRecords()
{
    auto logger = volume->session->logger;
    IDisposable^ scope = LoggerExtensions::BeginScope(logger, "USN MFT Enumeration (In-Memory)");

    uint64_t recordsProcessed = 0;
    void* buffer = nullptr;

    // The list that will hold all records
    auto allRecords = gcnew List<UsnRecord^>(0);

    try {
        LoggerExtensions::LogInformation(logger, "Querying USN journal for MFT metadata...", nullptr);

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

        const DWORD bufferSize = 0x40000; // 256KB
        buffer = malloc(bufferSize);
        if (!buffer) throw gcnew OutOfMemoryException("Buffer allocation failed");

        std::chrono::high_resolution_clock clk;
        auto lastLogTime = clk.now();
        auto startTime = lastLogTime;

        // Note: Building a massive list in memory might exceed 200MB. 
        // If the journal is huge, NoGCRegion might fail or trigger OOM.
        bool noGc = GC::TryStartNoGCRegion(500 * 1024 * 1024, false);

        try {
            while (true) {
                // Periodic logging
                if (std::chrono::duration_cast<std::chrono::milliseconds>(clk.now() - lastLogTime).count() > 2000) {
                    double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clk.now() - startTime).count() / 1000.0;
                    LoggerExtensions::LogInformation(logger, "Enumerated {0} records so far... ({1}s)", recordsProcessed, elapsed);
                    lastLogTime = clk.now();
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

                    UsnRecord^ record = nullptr;

                    if (header->MajorVersion == 2) {
                        USN_RECORD_V2* v2 = (USN_RECORD_V2*)pRawRecord;
                        auto v2Rec = gcnew UsnRecordV2();
                        v2Rec->Usn = v2->Usn;
                        v2Rec->FileReferenceNumber = v2->FileReferenceNumber;
                        v2Rec->ParentFileReferenceNumber = v2->ParentFileReferenceNumber;
                        v2Rec->Timestamp = DateTime::FromFileTimeUtc(v2->TimeStamp.QuadPart);
                        v2Rec->Reason = (UsnReason)v2->Reason;
                        v2Rec->SourceInfo = v2->SourceInfo;
                        if (v2->FileNameLength > 0) {
                            v2Rec->FileName = gcnew String((wchar_t*)(pRawRecord + v2->FileNameOffset), 0, v2->FileNameLength / sizeof(wchar_t));
                        }
                        record = v2Rec;
                    }
                    else if (header->MajorVersion == 3) {
                        USN_RECORD_V3* v3 = (USN_RECORD_V3*)pRawRecord;
                        auto v3Rec = gcnew UsnRecordV3();
                        v3Rec->Usn = v3->Usn;
                        uint64_t* pFid = (uint64_t*)v3->FileReferenceNumber.Identifier;
                        v3Rec->FileReferenceNumber = System::UInt128(pFid[1], pFid[0]);
                        uint64_t* pPid = (uint64_t*)v3->ParentFileReferenceNumber.Identifier;
                        v3Rec->ParentFileReferenceNumber = System::UInt128(pPid[1], pPid[0]);
                        v3Rec->Timestamp = DateTime::FromFileTimeUtc(v3->TimeStamp.QuadPart);
                        v3Rec->Reason = (UsnReason)v3->Reason;
                        v3Rec->SourceInfo = v3->SourceInfo;
                        if (v3->FileNameLength > 0) {
                            v3Rec->FileName = gcnew String((wchar_t*)(pRawRecord + v3->FileNameOffset), 0, v3->FileNameLength / sizeof(wchar_t));
                        }
                        record = v3Rec;
                    }
                    else if (header->MajorVersion == 4) {
                        USN_RECORD_V4* v4 = (USN_RECORD_V4*)pRawRecord;
                        auto v4Rec = gcnew UsnRecordV4();
                        v4Rec->Usn = v4->Usn;
                        uint64_t* pFid = (uint64_t*)v4->FileReferenceNumber.Identifier;
                        v4Rec->FileReferenceNumber = System::UInt128(pFid[1], pFid[0]);
                        uint64_t* pPid = (uint64_t*)v4->ParentFileReferenceNumber.Identifier;
                        v4Rec->ParentFileReferenceNumber = System::UInt128(pPid[1], pPid[0]);
                        v4Rec->Reason = (UsnReason)v4->Reason;
                        v4Rec->SourceInfo = v4->SourceInfo;

                        if (v4->NumberOfExtents > 0) {
                            v4Rec->Extents = gcnew array<UsnExtent>(v4->NumberOfExtents);
                            BYTE* pExtent = (BYTE*)v4->Extents;
                            for (WORD i = 0; i < v4->NumberOfExtents; i++) {
                                USN_RECORD_EXTENT* ext = (USN_RECORD_EXTENT*)pExtent;
                                v4Rec->Extents[i].Offset = (uint64_t)ext->Offset;
                                v4Rec->Extents[i].Length = (uint64_t)ext->Length;
                                pExtent += v4->ExtentSize;
                            }
                        }
                        record = v4Rec;
                    }

                    if (record != nullptr) {
                        record->RecordLength = header->RecordLength;
                        record->MajorVersion = header->MajorVersion;
                        record->MinorVersion = header->MinorVersion;
                        allRecords->Add(record);
                    }

                    pRawRecord += header->RecordLength;
                    recordsProcessed++;
                }
                enumData.StartFileReferenceNumber = nextFileRef;
            }
        }
        finally {
            if (noGc && GCSettings::LatencyMode == GCLatencyMode::NoGCRegion)
                GC::EndNoGCRegion();
        }

    }
    finally {
        if (buffer) free(buffer);
        if (scope) delete scope;
    }

    LoggerExtensions::LogInformation(logger, "Enumeration complete. Collected {0} records.", allRecords->Count);
    return allRecords;
}
