#include "Utility/SRMemoryDiagnostics.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformMemory.h"
#include "Misc/ScopeLock.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
	FCriticalSection GSRMemoryDiagnosticsCriticalSection;
	TMap<FName, FSRMemoryDiagnosticsTrackedClass> GSRMemoryDiagnosticsTrackedClasses;

	FString FormatBytes(uint64 Bytes)
	{
		const double MiB = static_cast<double>(Bytes) / (1024.0 * 1024.0);
		return FString::Printf(TEXT("%.2f MiB"), MiB);
	}

	bool ObjectBelongsToWorld(const UObject* Object, const UWorld* World)
	{
		if (!Object || !World)
		{
			return false;
		}

		if (Object == World || Object->GetWorld() == World)
		{
			return true;
		}

		for (const UObject* Outer = Object->GetOuter(); Outer; Outer = Outer->GetOuter())
		{
			if (Outer == World || Outer->GetWorld() == World)
			{
				return true;
			}
		}

		return false;
	}
}

void FSRMemoryDiagnostics::RegisterTrackedClass(FName Key, UClass* Class, const FString& Label, bool bWorldOnly)
{
	if (Key.IsNone() || !IsValid(Class))
	{
		return;
	}

	FSRMemoryDiagnosticsTrackedClass TrackedClass;
	TrackedClass.Key = Key;
	TrackedClass.Label = Label.IsEmpty() ? Class->GetName() : Label;
	TrackedClass.Class = Class;
	TrackedClass.bWorldOnly = bWorldOnly;

	FScopeLock Lock(&GSRMemoryDiagnosticsCriticalSection);
	GSRMemoryDiagnosticsTrackedClasses.FindOrAdd(Key) = TrackedClass;
}

void FSRMemoryDiagnostics::UnregisterTrackedClass(FName Key)
{
	FScopeLock Lock(&GSRMemoryDiagnosticsCriticalSection);
	GSRMemoryDiagnosticsTrackedClasses.Remove(Key);
}

void FSRMemoryDiagnostics::ResetTrackedClasses()
{
	FScopeLock Lock(&GSRMemoryDiagnosticsCriticalSection);
	GSRMemoryDiagnosticsTrackedClasses.Reset();
}

void FSRMemoryDiagnostics::LogSnapshot(const UWorld* World, const FString& Label, const TArray<FString>& ExtraLines)
{
	UE_LOG(LogTemp, Log, TEXT("[SR Memory]%s%s"), LINE_TERMINATOR, *BuildSnapshot(World, Label, ExtraLines));
}

void FSRMemoryDiagnostics::LogSnapshotNextTick(UWorld* World, const FString& Label, const TArray<FString>& ExtraLines)
{
	if (!IsValid(World))
	{
		return;
	}

	const TWeakObjectPtr<UWorld> WeakWorld = World;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakWorld, Label, ExtraLines]()
	{
		if (const UWorld* SnapshotWorld = WeakWorld.Get())
		{
			FSRMemoryDiagnostics::LogSnapshot(SnapshotWorld, Label, ExtraLines);
		}
	}));
}

void FSRMemoryDiagnostics::RequestGarbageCollectionAndLogNextTick(UWorld* World, const FString& LabelPrefix, const TArray<FString>& ExtraLines)
{
	if (!IsValid(World) || !World->IsGameWorld() || !GEngine)
	{
		return;
	}

	GEngine->ForceGarbageCollection(true);
	LogSnapshot(World, FString::Printf(TEXT("%s.AfterGCRequest"), *LabelPrefix), ExtraLines);
	LogSnapshotNextTick(World, FString::Printf(TEXT("%s.AfterGCTick"), *LabelPrefix), ExtraLines);
}

FString FSRMemoryDiagnostics::BuildSnapshot(const UWorld* World, const FString& Label, const TArray<FString>& ExtraLines)
{
	TArray<FString> Lines;
	Lines.Add(Label.IsEmpty() ? TEXT("Snapshot") : Label);
	Lines.Add(FString::Printf(TEXT("World=%s"), *GetNameSafe(World)));
	AppendMemoryStats(Lines);
	AppendTrackedClassCounts(World, Lines);
	for (const FString& ExtraLine : ExtraLines)
	{
		Lines.Add(ExtraLine);
	}

	FString Summary;
	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		if (LineIndex > 0)
		{
			Summary += LINE_TERMINATOR;
			Summary += TEXT("  ");
		}
		Summary += Lines[LineIndex];
	}
	return Summary;
}

int32 FSRMemoryDiagnostics::CountObjectsOfClass(UClass* Class, const UWorld* World, bool bWorldOnly)
{
	if (!IsValid(Class))
	{
		return 0;
	}

	int32 Count = 0;
	for (TObjectIterator<UObject> ObjectIt; ObjectIt; ++ObjectIt)
	{
		UObject* Object = *ObjectIt;
		if (!IsValid(Object)
			|| Object->HasAnyFlags(RF_ClassDefaultObject | RF_BeginDestroyed | RF_FinishDestroyed)
			|| !Object->IsA(Class))
		{
			continue;
		}

		if (bWorldOnly && !ObjectBelongsToWorld(Object, World))
		{
			continue;
		}

		++Count;
	}
	return Count;
}

void FSRMemoryDiagnostics::AppendMemoryStats(TArray<FString>& OutLines)
{
	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	OutLines.Add(FString::Printf(
		TEXT("Memory UsedPhysical=%s PeakUsedPhysical=%s UsedVirtual=%s PeakUsedVirtual=%s AvailablePhysical=%s AvailableVirtual=%s"),
		*FormatBytes(Stats.UsedPhysical),
		*FormatBytes(Stats.PeakUsedPhysical),
		*FormatBytes(Stats.UsedVirtual),
		*FormatBytes(Stats.PeakUsedVirtual),
		*FormatBytes(Stats.AvailablePhysical),
		*FormatBytes(Stats.AvailableVirtual)));
}

void FSRMemoryDiagnostics::AppendTrackedClassCounts(const UWorld* World, TArray<FString>& OutLines)
{
	TArray<FSRMemoryDiagnosticsTrackedClass> TrackedClasses;
	{
		FScopeLock Lock(&GSRMemoryDiagnosticsCriticalSection);
		GSRMemoryDiagnosticsTrackedClasses.GenerateValueArray(TrackedClasses);
	}

	TrackedClasses.Sort([](const FSRMemoryDiagnosticsTrackedClass& Left, const FSRMemoryDiagnosticsTrackedClass& Right)
	{
		return Left.Label < Right.Label;
	});

	for (const FSRMemoryDiagnosticsTrackedClass& TrackedClass : TrackedClasses)
	{
		UClass* Class = TrackedClass.Class.Get();
		if (!IsValid(Class))
		{
			continue;
		}

		OutLines.Add(FString::Printf(
			TEXT("ClassCount %s=%d Scope=%s"),
			*TrackedClass.Label,
			CountObjectsOfClass(Class, World, TrackedClass.bWorldOnly),
			TrackedClass.bWorldOnly ? TEXT("World") : TEXT("Global")));
	}
}
