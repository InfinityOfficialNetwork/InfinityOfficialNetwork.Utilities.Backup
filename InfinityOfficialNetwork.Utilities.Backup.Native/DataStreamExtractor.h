#pragma once

#include "UsnJournalReader.h"

namespace InfinityOfficialNetwork::Utilities::Backup::Native {



	ref class DataStreamExtractor
	{

	};


	public ref class DataStream abstract
	{
	public:
		property System::IO::Stream^ Stream;
		property System::Collections::Generic::IDictionary<System::String^, System::String^>^ Metadata;
	};

	ref class DataStreamGroup
	{
	public:
		property System::Collections::Generic::List<DataStream^>^ Streams;
	};

	public ref class DataStreamProvider abstract {
	public:

	};
}