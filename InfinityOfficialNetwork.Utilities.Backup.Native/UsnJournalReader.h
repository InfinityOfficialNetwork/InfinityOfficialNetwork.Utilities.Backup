#pragma once

#include "UsnRecord.h"

namespace InfinityOfficialNetwork::Utilities::Backup::Native {

	ref class VssSnapshotVolume;
	ref class VssSnapshotSession;

	public ref class UsnJournalReader
	{
		VssSnapshotVolume^ volume;
		VssSnapshotSession^ session;

	internal:
		UsnJournalReader(VssSnapshotVolume^ volume, VssSnapshotSession^ session)
			: volume(volume), session(session)
		{
		}

	public:
		System::Collections::Generic::IList<UsnRecord^>^ ExtractAllRecords();

		~UsnJournalReader() {

			System::GC::SuppressFinalize(this);
		}

		!UsnJournalReader() {
			this->~UsnJournalReader();
		}
	};

}