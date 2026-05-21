#include "DataStreamExtractor.h"
#include "../UsnJournalChangeAggregator.h"
#include "../UsnJournalReader.h"

#include <Windows.h>

using namespace InfinityOfficialNetwork::Utilities::Backup::Native::Streams;
using namespace InfinityOfficialNetwork::Utilities::Backup::Native;
using namespace System;
using namespace System::IO;
using namespace System::Collections::Generic;
using namespace System::Linq;

ref class DataStreamUsnRecordVisitor : IUsnRecordVisitor {
public:
	virtual void Visit(UsnRecordV2^ record)
	{
	}
	virtual void Visit(UsnRecordV3^ record)
	{
	}
	virtual void Visit(UsnRecordV4^ record)
	{
	}
};

IEnumerable<Stream^>^ DataStreamExtractor::ExtractAllStreams(VssSnapshot^ snapshot, UsnJournalFileAggregate^ fileAggregate)
{
	FILE_ID_DESCRIPTOR desc {};
	desc.dwSize = sizeof(FILE_ID_DESCRIPTOR);

	fileAggregate->Records;
	throw 1;
}
