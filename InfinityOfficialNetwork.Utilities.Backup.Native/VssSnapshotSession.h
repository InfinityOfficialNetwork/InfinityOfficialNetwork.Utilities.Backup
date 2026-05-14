#pragma once

#include <vector>
#include <string>
#include <stdint.h>
#include <msclr/marshal_cppstd.h>

#include "VssSnapshot.h"

namespace InfinityOfficialNetwork::Utilities::Backup::Native {
	public ref class VssSnapshotSession
	{
	internal:
		Microsoft::Extensions::Logging::ILogger^ logger;
		//std::vector<std::wstring>* volumes;
		nroot<std::vector<std::wstring>>^ volumes;

	public:
		VssSnapshotSession(Microsoft::Extensions::Logging::ILogger^ logger) : logger(logger)
		{
			volumes = gcnew nroot < std::vector < std::wstring>>();
		}

		~VssSnapshotSession()
		{
			delete volumes;
			volumes = nullptr;

			System::GC::SuppressFinalize(this);
		}

		!VssSnapshotSession() {
			this->~VssSnapshotSession();
		}

		void AddVolume(System::String^ volumeName) {
			volumes->get().push_back(msclr::interop::marshal_as<std::wstring>(volumeName));
		}

		System::Threading::Tasks::Task<VssSnapshot^>^ CreateSnapshotAsync(System::Threading::CancellationToken ct);
	};
}

