#pragma once

#include "UsnRecord.h"

namespace InfinityOfficialNetwork::Utilities::Backup::Native {

	ref class UsnJournalReader;

	public ref class UsnJournalFileAggregate {
	public:
		property System::Collections::Generic::IEnumerable<UsnRecord^>^ Records;
	};

	public ref class UsnJournalChangeAggregator {
	private:
		UsnJournalReader^ reader;

	public:
		
		
	};



}