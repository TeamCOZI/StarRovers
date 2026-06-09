#pragma once

#include "CoreMinimal.h"

class FSRTimingLog
{
public:
	static bool IsActive();
	static void BeginSession(const FString& Title);
	static void AddLine(const FString& Line);
	static void EndSessionAndLog();
	static void PushSuppressLines();
	static void PopSuppressLines();
};

class FSRTimingLogSession
{
public:
	explicit FSRTimingLogSession(const FString& Title);
	~FSRTimingLogSession();

	FSRTimingLogSession(const FSRTimingLogSession&) = delete;
	FSRTimingLogSession& operator=(const FSRTimingLogSession&) = delete;

private:
	bool bOwnsSession = false;
};

class FSRTimingLogScopedSuppress
{
public:
	FSRTimingLogScopedSuppress();
	~FSRTimingLogScopedSuppress();

	FSRTimingLogScopedSuppress(const FSRTimingLogScopedSuppress&) = delete;
	FSRTimingLogScopedSuppress& operator=(const FSRTimingLogScopedSuppress&) = delete;
};

class FSRTimingLogScopedCapture
{
public:
	explicit FSRTimingLogScopedCapture(TArray<FString>& OutCapturedLines);
	~FSRTimingLogScopedCapture();

	FSRTimingLogScopedCapture(const FSRTimingLogScopedCapture&) = delete;
	FSRTimingLogScopedCapture& operator=(const FSRTimingLogScopedCapture&) = delete;

private:
	TArray<FString>* CapturedLines = nullptr;
};
