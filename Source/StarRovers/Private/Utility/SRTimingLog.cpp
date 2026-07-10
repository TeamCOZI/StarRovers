#include "Utility/SRTimingLog.h"

#include "Utility/SRLog.h"
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

	bool IsSRTimingLogEnabled()
	{
		return SR_LOG_ENABLED(Timing);
	}
}

bool FSRTimingLog::IsActive()
{
	FScopeLock Lock(&SRTimingLogCriticalSection);
	return bSRTimingLogActive;
}

void FSRTimingLog::BeginSession(const FString& Title)
{
	FScopeLock Lock(&SRTimingLogCriticalSection);
	if (!IsSRTimingLogEnabled())
	{
		bSRTimingLogActive = false;
		SRTimingLogTitle.Reset();
		SRTimingLogLines.Reset();
		return;
	}

	bSRTimingLogActive = true;
	SRTimingLogTitle = Title;
	SRTimingLogLines.Reset();
}

void FSRTimingLog::AddLine(const FString& Line)
{
	if (!IsSRTimingLogEnabled())
	{
		return;
	}

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
		SR_LOG(Timing, LogTemp, Log, TEXT("[SR Timing] %s"), *Line);
		return;
	}

	SRTimingLogLines.Add(Line);
}

void FSRTimingLog::EndSessionAndLog()
{
	FScopeLock Lock(&SRTimingLogCriticalSection);
	if (!IsSRTimingLogEnabled())
	{
		bSRTimingLogActive = false;
		SRTimingLogTitle.Reset();
		SRTimingLogLines.Reset();
		return;
	}

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

	SR_LOG(Timing, LogTemp, Log, TEXT("[SR Timing]%s%s"), LINE_TERMINATOR, *Summary);

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
	if (IsSRTimingLogEnabled() && !FSRTimingLog::IsActive())
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
