#include "pch.h"
#include "VssException.h"

using namespace System;
using namespace System::Collections;
using namespace System::Collections::Generic; 

VssException::VssException(VssOperation op, int hr) :
	COMException(String::Format("VSS Failure during {0} (HRESULT: 0x{1:X8})", op, (unsigned int)hr), hr)
{
	Operation = op;
	HResultValue = hr;
	this->Data->Add("VssOperation", op);
	this->Data->Add("HResult", hr);
}
