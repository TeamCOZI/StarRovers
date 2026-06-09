#pragma once

#include "CoreMinimal.h"

class UClass;
class UWorld;

struct STARROVERS_API FSRMemoryDiagnosticsTrackedClass
{
	FName Key;
	FString Label;
	TWeakObjectPtr<UClass> Class;
	bool bWorldOnly = true;
};

class STARROVERS_API FSRMemoryDiagnostics
{
public:
	static void RegisterTrackedClass(FName Key, UClass* Class, const FString& Label = FString(), bool bWorldOnly = true);
	static void UnregisterTrackedClass(FName Key);
	static void ResetTrackedClasses();

	static void LogSnapshot(const UWorld* World, const FString& Label, const TArray<FString>& ExtraLines = TArray<FString>());
	static void LogSnapshotNextTick(UWorld* World, const FString& Label, const TArray<FString>& ExtraLines = TArray<FString>());
	static void RequestGarbageCollectionAndLogNextTick(UWorld* World, const FString& LabelPrefix, const TArray<FString>& ExtraLines = TArray<FString>());
	static FString BuildSnapshot(const UWorld* World, const FString& Label, const TArray<FString>& ExtraLines = TArray<FString>());
	static int32 CountObjectsOfClass(UClass* Class, const UWorld* World, bool bWorldOnly);

private:
	static void AppendMemoryStats(TArray<FString>& OutLines);
	static void AppendTrackedClassCounts(const UWorld* World, TArray<FString>& OutLines);
};
