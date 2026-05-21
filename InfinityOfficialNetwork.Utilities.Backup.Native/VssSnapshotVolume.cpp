#include "pch.h"
#include "VssSnapshotVolume.h"
#include "UsnJournalReader.h"
#include "FileHandle.h"

using namespace System;
using namespace System::IO;
using namespace System::Runtime::InteropServices;
using namespace Microsoft::Win32::SafeHandles;
using namespace System::ComponentModel;
using namespace InfinityOfficialNetwork::Utilities::Backup::Native;

UsnJournalReader^ VssSnapshotVolume::OpenUsnJournalReader()
{
    return gcnew UsnJournalReader(this, session);
}

FileHandle^ InfinityOfficialNetwork::Utilities::Backup::Native::VssSnapshotVolume::OpenFileById(System::ValueTuple<System::UInt128, bool> fileId) {
    return gcnew FileHandle(this, fileId);
}

