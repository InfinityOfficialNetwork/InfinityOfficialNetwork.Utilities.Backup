#pragma once

namespace InfinityOfficialNetwork::Utilities::Backup::Native {


public ref class Core
{
private:
	static Microsoft::Extensions::Logging::ILoggerFactory^ disposeLoggerFactory = gcnew Microsoft::Extensions::Logging::Abstractions::NullLoggerFactory();
public:

	static property Microsoft::Extensions::Logging::ILoggerFactory^ DisposeLoggerFactory {
		Microsoft::Extensions::Logging::ILoggerFactory^ get() { return disposeLoggerFactory; }
		void set(Microsoft::Extensions::Logging::ILoggerFactory^ value) { disposeLoggerFactory = value; }
	}
};

}
