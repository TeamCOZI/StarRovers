#include "Utility/SRTimingLog.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace
{
	bool bSRTimingLogActive = false;
	FString SRTimingLogTitle;
	TArray<FString> SRTimingLogLines;
	FCriticalSection SRTimingLogCriticalSection;

	thread_local int32 SRTimingLogSuppressDepth = 0;
	thread_local TArray<TArray<FString>*> SRTimingLogCaptureStack;
}

bool FSRTimingLog::IsActive()
{
	FScopeLock Lock(&SRTimingLogCriticalSection);
	return bSRTimingLogActive;
}

void FSRTimingLog::BeginSession(const FString& Title)
{
	FScopeLock Lock(&SRTimingLogCriticalSection);
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

	FScopeLock Lock(&SRTimingLogCriticalSection);
	if (!bSRTimingLogActive)
	{
		UE_LOG(LogTemp, Log, TEXT("[SR Timing] %s"), *Line);
		return;
	}

	SRTimingLogLines.Add(Line);
}

void FSRTimingLog::EndSessionAndLog()
{
	FScopeLock Lock(&SRTimingLogCriticalSection);
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
