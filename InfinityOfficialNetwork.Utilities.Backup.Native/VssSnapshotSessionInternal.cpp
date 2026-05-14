#include "pch.h"
#include "VssSnapshotSessionInternal.h"
#include "VssSnapshotProperties.Marshal.h"

#include <chrono>
#include <thread>
#include <functional>
#include <vcclr.h>
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <windows.h>

#include "VssException.h"
#include "VssSnapshot.h"
#include "VssSnapshotSession.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "vssapi.lib")

using namespace InfinityOfficialNetwork::Utilities::Backup::Native;
using namespace System::Threading::Tasks;
using namespace System::Threading;
using namespace System;
using namespace Microsoft::Extensions::Logging;
using namespace System::Collections::Generic;


namespace {
	void WaitAndCheckAsync(IVssAsync*& pAsync, VssOperation op, ILogger^ logger) {
		if (!pAsync) return;

		LoggerExtensions::LogInformation(logger, "VSS Async operation began");

		auto begin = std::chrono::steady_clock::now();
		auto lastLog = begin;

		HRESULT hrStatus = VSS_S_ASYNC_PENDING;
		HRESULT hrQuery = S_OK;

		// Poll until the operation is no longer pending
		while (true) {
			// Check if the operation is finished
			hrQuery = pAsync->QueryStatus(&hrStatus, nullptr);

			if (FAILED(hrQuery)) break;             // The Query call itself failed
			if (hrStatus != VSS_S_ASYNC_PENDING) break; // The operation finished (S_OK or error)

			// Wait a bit before checking again (100ms is a good balance)
			::Sleep(100);

			auto now = std::chrono::steady_clock::now();
			auto secondsSinceLastLog = std::chrono::duration_cast<std::chrono::seconds>(now - lastLog).count();

			if (secondsSinceLastLog >= 5) {
				auto totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - begin).count();
				LoggerExtensions::LogInformation(logger, "Waited for {0} seconds", (double)totalSeconds);
				lastLog = now;
			}
		}

		auto end = std::chrono::steady_clock::now();
		double totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() / 1000.0;

		// Cleanup: The operation is done or QueryStatus failed
		if (FAILED(hrQuery) || FAILED(hrStatus)) {
			HRESULT errorToThrow = FAILED(hrQuery) ? hrQuery : hrStatus;

			pAsync->Release();
			pAsync = nullptr;

			LoggerExtensions::LogCritical(logger, "VSS Async operation failed after {0}s with HR: 0x{1:X}", totalDuration, (unsigned int)errorToThrow);
			throw gcnew VssException(op, errorToThrow);
		}

		// Success
		LoggerExtensions::LogInformation(logger, "VSS Async operation completed after {0}s", totalDuration);

		pAsync->Release();
		pAsync = nullptr;
	}

	void VerifyWriterStatus(IVssBackupComponents* pBackup, VssOperation op, ILogger^ logger) {
		IVssAsync* pAsync = nullptr;
		HRESULT hr = pBackup->GatherWriterStatus(&pAsync);
		if (FAILED(hr)) throw gcnew VssException(VssOperation::GatherWriterStatus, hr);
		WaitAndCheckAsync(pAsync, VssOperation::GatherWriterStatus, logger);

		UINT nWriters = 0;
		pBackup->GetWriterStatusCount(&nWriters);
		LoggerExtensions::LogInformation(logger, "Found {0} writers to verify", nWriters);

		for (UINT i = 0; i < nWriters; i++) {
			VSS_ID id, instance;
			VSS_WRITER_STATE state;
			BSTR name;
			HRESULT hrWriterFailure;

			if (SUCCEEDED(pBackup->GetWriterStatus(i, &instance, &id, &name, &state, &hrWriterFailure))) {
				if (state != VSS_WS_STABLE && state != VSS_WS_WAITING_FOR_BACKUP_COMPLETE) {
					String^ writerName = msclr::interop::marshal_as<String^>(name);
					SysFreeString(name);
					LoggerExtensions::LogError(logger, "Writer {0} reported bad state {1}", writerName, (int)state);
					throw gcnew VssException(op, hrWriterFailure);
				}
				else {
					String^ writerName = msclr::interop::marshal_as<String^>(name);
					SysFreeString(name);
					LoggerExtensions::LogInformation(logger, "Writer {0} reported valid state {1}", writerName, (int)state == state == VSS_WS_STABLE ? "VSS_WS_STABLE" : "VSS_WS_WAITING_FOR_BACKUP_COMPLETE");
				}
			}
		}
	}
}

#pragma managed(push,off)
static inline thread_local bool threadCoInitd = false;
static bool processCoInitd = false;

inline static bool& ThreadCoInitd() {
	return threadCoInitd;
}

inline static bool& ProcessCoInitd() {
	return processCoInitd;
}
#pragma managed(pop)

void VssSnapshotSessionInternal::ThreadInit(ILogger^ logger) {
	ThreadCoInitd() = true;

	if (!ProcessCoInitd()) {
		//LoggerExtensions::LogInformation(logger, "Initializing COM Security", nullptr);
		HRESULT hr = CoInitializeSecurity(
			NULL,                           // Allow all users/processes
			-1,                             // COM chooses authentication services
			NULL,                           // Reserved
			NULL,                           // Reserved
			RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  // Recommended for VSS
			RPC_C_IMP_LEVEL_IDENTIFY,       // Required for VSS
			NULL,                           // Reserved
			EOAC_NONE,                      // Additional capabilities
			NULL                            // Reserved
		);
		if (FAILED(hr)) throw gcnew VssException(VssOperation::CoInitializeSecurity, hr);
		ProcessCoInitd() = true;
	}

	//LoggerExtensions::LogInformation(logger, "Initializing COM VSS Api", nullptr);
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr)) throw gcnew VssException(VssOperation::CoInitialize, hr);
	LoggerExtensions::LogInformation(logger, "Initialized COM VSS Api", nullptr);
}

void VssSnapshotSessionInternal::ThreadDeinit(ILogger^ logger) {
	if (ThreadCoInitd()) {
		LoggerExtensions::LogInformation(logger, "Deinitializing COM VSS Api", nullptr);
		CoUninitialize();
		ThreadCoInitd() = false;
	}
}

void VssSnapshotSessionInternal::CreateSnapshotWorker(
	VssSnapshotSession^ session,
	TaskCompletionSource<VssSnapshot^>^ tcs, 
	CancellationToken ct, 
	std::vector<std::wstring> volumes, 
	ILogger^ logger)
{
	IDisposable^ scope = LoggerExtensions::BeginScope(logger, "VSS Operation");

	IVssBackupComponents* pBackup = nullptr;
	IVssAsync* pAsync = nullptr;
	VSS_ID snapshotSetId = GUID_NULL;
	VSS_ID snapshotId = GUID_NULL;
	VSS_SNAPSHOT_PROP prop = { 0 };
	HRESULT hr = S_OK;

	try {
		ThreadInit(logger);

		LoggerExtensions::LogInformation(logger, "Creating VSS Backup Components", nullptr);
		hr = CreateVssBackupComponents(&pBackup);
		if (FAILED(hr)) throw gcnew VssException(VssOperation::CreateVssBackupComponents, hr);

		LoggerExtensions::LogInformation(logger, "Initializing for Backup", nullptr);
		hr = pBackup->InitializeForBackup();
		if (FAILED(hr)) throw gcnew VssException(VssOperation::InitializeForBackup, hr);

		LoggerExtensions::LogInformation(logger, "Setting Context", nullptr);
		hr = pBackup->SetContext(VSS_CTX_BACKUP);
		if (FAILED(hr)) throw gcnew VssException(VssOperation::SetContext, hr);

		LoggerExtensions::LogInformation(logger, "Setting Backup State", nullptr);
		hr = pBackup->SetBackupState(true, true, VSS_BT_FULL, false);
		if (FAILED(hr)) throw gcnew VssException(VssOperation::SetBackupState, hr);

		LoggerExtensions::LogInformation(logger, "Gathering Writer Metadata", nullptr);
		hr = pBackup->GatherWriterMetadata(&pAsync);
		if (FAILED(hr)) throw gcnew VssException(VssOperation::GatherWriterMetadata, hr);
		WaitAndCheckAsync(pAsync, VssOperation::GatherWriterMetadata, logger);

		LoggerExtensions::LogInformation(logger, "Starting Snapshot Set", nullptr);
		hr = pBackup->StartSnapshotSet(&snapshotSetId);
		if (FAILED(hr)) throw gcnew VssException(VssOperation::StartSnapshotSet, hr);

		for (auto& vol : volumes) {
			LoggerExtensions::LogInformation(logger, "Adding Volume \"{0}\" to Set", msclr::interop::marshal_as<String^>(vol));
			hr = pBackup->AddToSnapshotSet(vol.data(), GUID_NULL, &snapshotId);
			if (FAILED(hr)) throw gcnew VssException(VssOperation::AddToSnapshotSet, hr);
		}

		IDisposable^ prepare_scope = LoggerExtensions::BeginScope(logger, "Prepare for backup");
		try {
			LoggerExtensions::LogInformation(logger, "Preparing for Backup (PrepareForBackup)", nullptr);
			hr = pBackup->PrepareForBackup(&pAsync);
			if (FAILED(hr)) throw gcnew VssException(VssOperation::PrepareForBackup, hr);
			WaitAndCheckAsync(pAsync, VssOperation::PrepareForBackup, logger);

			VerifyWriterStatus(pBackup, VssOperation::PrepareForBackup, logger);
		}
		finally {
			delete prepare_scope;
		}

		IDisposable^ committ_scope = LoggerExtensions::BeginScope(logger, "Committing snapshot");
		try {
			LoggerExtensions::LogInformation(logger, "Committing Snapshot (DoSnapshotSet)", nullptr);
			hr = pBackup->DoSnapshotSet(&pAsync);
			if (FAILED(hr)) throw gcnew VssException(VssOperation::DoSnapshotSet, hr);
			WaitAndCheckAsync(pAsync, VssOperation::DoSnapshotSet, logger);

			VerifyWriterStatus(pBackup, VssOperation::DoSnapshotSet, logger);
		}
		finally {
			delete committ_scope;
		}

		List<VssSnapshotProperties^>^ snapshotList = gcnew List<VssSnapshotProperties^>();
		IVssEnumObject* pEnum = nullptr;

		// Query for all snapshots. 
		// Note: You can filter by SnapshotSetId if you have multiple sets, 
		// but usually Query(GUID_NULL, VSS_OBJECT_NONE, VSS_OBJECT_SNAPSHOT, &pEnum) is used.
		hr = pBackup->Query(GUID_NULL, VSS_OBJECT_NONE, VSS_OBJECT_SNAPSHOT, &pEnum);

		if (SUCCEEDED(hr)) {
			VSS_OBJECT_PROP objProp;
			ULONG fetched = 0;

			// Iterate through all snapshots found
			while (SUCCEEDED(pEnum->Next(1, &objProp, &fetched)) && fetched > 0) {
				// VSS_OBJECT_PROP is a union; since we queried for snapshots, we use the "Snap" member
				VSS_SNAPSHOT_PROP& snap = objProp.Obj.Snap;

				snapshotList->Add(msclr::interop::marshal_as<VssSnapshotProperties^>(snap));

				// VERY IMPORTANT: Free the strings inside the struct before the next iteration
				VssFreeSnapshotProperties(&snap);
			}
			pEnum->Release();
		}

		auto snap = gcnew VssSnapshot(snapshotList, msclr::interop::marshal_as<System::Guid>(snapshotSetId), session);

		tcs->SetResult(snap);
	}
	catch (Exception^ ex) {
		LoggerExtensions::LogError(logger, ex, "Critical VSS failure", nullptr);
		if (pBackup) pBackup->AbortBackup();
		if (pBackup) pBackup->Release();

		tcs->SetException(ex);
	}
	finally {
		if (scope != nullptr) delete scope;

		ThreadDeinit(logger);
	}
}

void VssSnapshotSessionInternal::DeleteSnapshotWorker(TaskCompletionSource^ tcs, Guid snapshotSetId, ILogger^ logger, CancellationToken ct) {
	if (ct.IsCancellationRequested) return;

	IDisposable^ scope = LoggerExtensions::BeginScope(logger, "VSS Delete Snapshot");

	HRESULT hr = S_OK;
	IVssBackupComponents* pBackup = nullptr;

	try {
		// 1. Initialize COM for this thread
		ThreadInit(logger);

		// 2. Create the VSS interface
		hr = CreateVssBackupComponents(&pBackup);
		if (FAILED(hr)) throw gcnew VssException(VssOperation::CreateVssBackupComponents, hr);

		// 3. Initialize for the deletion operation
		hr = pBackup->InitializeForBackup();
		if (FAILED(hr)) throw gcnew VssException(VssOperation::InitializeForBackup, hr);

		// 4. Set context to match the creation context (VSS_CTX_BACKUP)
		// If the contexts don't match, VSS might not find the ID.
		hr = pBackup->SetContext(VSS_CTX_BACKUP);
		if (FAILED(hr)) throw gcnew VssException(VssOperation::SetContext, hr);

		// 5. Convert managed Guid to native VSS_ID (using our marshal_as specialization)
		VSS_ID setID = msclr::interop::marshal_as<VSS_ID>(snapshotSetId);

		LONG deletedCount = 0;
		VSS_ID nonDeletedId = GUID_NULL;

		LoggerExtensions::LogInformation(logger, "Starting deletion of VSS Snapshot Set: {0}", snapshotSetId);

		// 6. Execute Deletion
		hr = pBackup->DeleteSnapshots(
			setID,
			VSS_OBJECT_SNAPSHOT_SET, // Delete the whole set
			TRUE,                    // Force delete
			&deletedCount,
			&nonDeletedId
		);

		if (FAILED(hr)) {
			if (hr == VSS_E_OBJECT_NOT_FOUND) {
				LoggerExtensions::LogWarning(logger, "Snapshot set {0} was not found (it may have already been deleted).", snapshotSetId);
			}
			else {
				LoggerExtensions::LogError(logger, "Failed to delete snapshot set. HR: 0x{0:X}", (unsigned int)hr);
				throw gcnew VssException(VssOperation::DeleteSnapshots, hr);
			}
		}
		else {
			LoggerExtensions::LogInformation(logger, "Successfully deleted {0} snapshot(s) from system.", deletedCount);
		}

		tcs->SetResult();
	}
	catch (Exception^ ex) {
		LoggerExtensions::LogError(logger, "An error occurred while deleting the snapshot: {0}", ex->Message);
		tcs->SetException(ex);
	}
	finally {
		// 7. Cleanup resources
		if (pBackup) {
			pBackup->Release();
			pBackup = nullptr;
		}

		ThreadDeinit(logger);

		if (scope != nullptr) delete scope;
	}
}
