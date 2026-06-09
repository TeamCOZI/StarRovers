#include "Utility/SRTimingLog.h"

namespace
{
	bool bSRTimingLogActive = false;
	int32 SRTimingLogSuppressDepth = 0;
	FString SRTimingLogTitle;
	TArray<FString> SRTimingLogLines;
	TArray<TArray<FString>*> SRTimingLogCaptureStack;
}

bool FSRTimingLog::IsActive()
{
	return bSRTimingLogActive;
}

void FSRTimingLog::BeginSession(const FString& Title)
{
	bSRTimingLogActive = true;
	SRTimingLogTitle = Title;
	SRTimingLogLines.Reset();
}

void FSRTimingLog::AddLine(const FString& Line)
{
	if (SRTimingLogSuppressDepth > 0)
	{
		if (!SRTimingLogCaptureStack.IsEmpty() && SRTimingLogCaptureStack.Last())
		{
			SRTimingLogCaptureStack.Last()->Add(Line);
		}
		return;
	}

	if (!bSRTimingLogActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[SR Timing] %s"), *Line);
		return;
	}

	SRTimingLogLines.Add(Line);
}

void FSRTimingLog::EndSessionAndLog()
{
	if (!bSRTimingLogActive)
	{
		return;
	}

	FString Summary = SRTimingLogTitle;
	for (const FString& Line : SRTimingLogLines)
	{
		Summary += LINE_TERMINATOR;
		Summary += TEXT("  ");
		Summary += Line;
	}

	UE_LOG(LogTemp, Log, TEXT("[SR Timing]%s%s"), LINE_TERMINATOR, *Summary);

	bSRTimingLogActive = false;
	SRTimingLogTitle.Reset();
	SRTimingLogLines.Reset();
}

void FSRTimingLog::PushSuppressLines()
{
	++SRTimingLogSuppressDepth;
}

void FSRTimingLog::PopSuppressLines()
{
	SRTimingLogSuppressDepth = FMath::Max(0, SRTimingLogSuppressDepth - 1);
}

FSRTimingLogSession::FSRTimingLogSession(const FString& Title)
{
	if (!FSRTimingLog::IsActive())
	{
		FSRTimingLog::BeginSession(Title);
		bOwnsSession = true;
	}
}

FSRTimingLogSession::~FSRTimingLogSession()
{
	if (bOwnsSession)
	{
		FSRTimingLog::EndSessionAndLog();
	}
}

FSRTimingLogScopedSuppress::FSRTimingLogScopedSuppress()
{
	FSRTimingLog::PushSuppressLines();
}

FSRTimingLogScopedSuppress::~FSRTimingLogScopedSuppress()
{
	FSRTimingLog::PopSuppressLines();
}

FSRTimingLogScopedCapture::FSRTimingLogScopedCapture(TArray<FString>& OutCapturedLines)
	: CapturedLines(&OutCapturedLines)
{
	OutCapturedLines.Reset();
	SRTimingLogCaptureStack.Add(CapturedLines);
	FSRTimingLog::PushSuppressLines();
}

FSRTimingLogScopedCapture::~FSRTimingLogScopedCapture()
{
	FSRTimingLog::PopSuppressLines();
	SRTimingLogCaptureStack.RemoveSingleSwap(CapturedLines, EAllowShrinking::No);
	CapturedLines = nullptr;
}
