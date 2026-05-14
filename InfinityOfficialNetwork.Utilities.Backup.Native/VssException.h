#pragma once

public enum class VssOperation {
	CoInitialize,
	CoInitializeSecurity,
	CreateVssBackupComponents,
	InitializeForBackup,
	SetContext,
	SetBackupState,
	GatherWriterMetadata,
	StartSnapshotSet,
	AddToSnapshotSet,
	PrepareForBackup,
	GatherWriterStatus,
	DoSnapshotSet,
	GetSnapshotProperties,
	OpenSnapshotHandle,
	DeleteSnapshots
};

public ref class VssException : public System::Runtime::InteropServices::COMException {
public:
	property VssOperation Operation;
	property int HResultValue;

	VssException(VssOperation op, int hr);
};