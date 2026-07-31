#include "Editor/SRConfigureSpaceshipTrailCommandlet.h"

#include "Utility/SRLog.h"

#if WITH_EDITOR

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_Niagara.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "NiagaraConstants.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

namespace
{
	const FName NiagaraDynamicAddPinSubCategory(TEXT("DynamicAddPin"));
	const FName NiagaraParameterPinSubCategory(TEXT("ParameterPin"));
	const FName NiagaraParameterMapGetClassName(TEXT("NiagaraNodeParameterMapGet"));
	const FName TrailMaterialUserParameter(TEXT("User.TrailMaterial"));
	const FName TrailWidthUserParameter(TEXT("User.TrailWidth"));
	constexpr float DefaultTrailWidth = 300.0f;
	constexpr float DefaultTrailLifetime = 3.0f;
	constexpr float DefaultTrailSpawnRate = 60.0f;

	bool TryReadRequiredObjectPath(const FString& Params, const TCHAR* ParameterName, FString& OutPath)
	{
		const FString ParameterKey = FString::Printf(TEXT("%s="), ParameterName);
		if (FParse::Value(*Params, *ParameterKey, OutPath) && !OutPath.IsEmpty())
		{
			return true;
		}

		SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Missing required commandlet parameter: -%s=<ObjectPath>"), ParameterName);
		return false;
	}

	UNiagaraScript* LoadNiagaraModuleScript(const FString& ModulePath)
	{
		UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModulePath);
		if (!ModuleScript)
		{
			SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Could not load Niagara module script: %s"), *ModulePath);
		}
		return ModuleScript;
	}

	UNiagaraNodeOutput* FindOutputNodeByUsage(UNiagaraGraph* Graph, ENiagaraScriptUsage Usage)
	{
		if (!Graph)
		{
			return nullptr;
		}

		TArray<UNiagaraNodeOutput*> OutputNodes;
		Graph->GetNodesOfClass(OutputNodes);
		for (UNiagaraNodeOutput* OutputNode : OutputNodes)
		{
			if (OutputNode && UNiagaraScript::IsEquivalentUsage(OutputNode->GetUsage(), Usage))
			{
				return OutputNode;
			}
		}

		return nullptr;
	}

	void CollectUpstreamNiagaraNodes(UEdGraphNode* Node, TSet<UEdGraphNode*>& VisitedNodes, TArray<UNiagaraNode*>& OutNodes)
	{
		if (!Node || VisitedNodes.Contains(Node))
		{
			return;
		}

		VisitedNodes.Add(Node);
		if (UNiagaraNode* NiagaraNode = Cast<UNiagaraNode>(Node))
		{
			OutNodes.Add(NiagaraNode);
		}

		for (UEdGraphPin* Pin : Node->GetAllPins())
		{
			if (!Pin || Pin->Direction != EGPD_Input)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
			{
				CollectUpstreamNiagaraNodes(LinkedPin ? LinkedPin->GetOwningNode() : nullptr, VisitedNodes, OutNodes);
			}
		}
	}

	void GetTraversalNodes(UNiagaraNodeOutput& OutputNode, TArray<UNiagaraNode*>& OutNodes)
	{
		OutNodes.Reset();
		TSet<UEdGraphNode*> VisitedNodes;
		CollectUpstreamNiagaraNodes(&OutputNode, VisitedNodes, OutNodes);
	}

	UNiagaraNodeFunctionCall* FindModuleNode(UNiagaraNodeOutput& OutputNode, const UNiagaraScript* ModuleScript)
	{
		if (!ModuleScript)
		{
			return nullptr;
		}

		TArray<UNiagaraNode*> Nodes;
		GetTraversalNodes(OutputNode, Nodes);
		for (UNiagaraNode* Node : Nodes)
		{
			UNiagaraNodeFunctionCall* FunctionCall = Cast<UNiagaraNodeFunctionCall>(Node);
			if (FunctionCall && FunctionCall->FunctionScript == ModuleScript)
			{
				return FunctionCall;
			}
		}

		return nullptr;
	}

	UNiagaraNodeFunctionCall* EnsureModule(
		UNiagaraNodeOutput& OutputNode,
		const FString& ModulePath,
		int32 TargetIndex,
		bool& bChanged,
		int32& AddedModuleCount)
	{
		UNiagaraScript* ModuleScript = LoadNiagaraModuleScript(ModulePath);
		if (!ModuleScript)
		{
			return nullptr;
		}

		if (UNiagaraNodeFunctionCall* ExistingNode = FindModuleNode(OutputNode, ModuleScript))
		{
			return ExistingNode;
		}

		UNiagaraNodeFunctionCall* AddedNode = FNiagaraStackGraphUtilities::AddScriptModuleToStack(
			ModuleScript,
			OutputNode,
			TargetIndex,
			FString());
		if (AddedNode)
		{
			bChanged = true;
			++AddedModuleCount;
		}
		return AddedNode;
	}

	UNiagaraNodeAssignment* FindAssignmentForTarget(UNiagaraNodeOutput& OutputNode, const FNiagaraVariable& TargetVariable, int32& OutTargetIndex)
	{
		OutTargetIndex = INDEX_NONE;

		TArray<UNiagaraNode*> Nodes;
		GetTraversalNodes(OutputNode, Nodes);
		for (UNiagaraNode* Node : Nodes)
		{
			UNiagaraNodeAssignment* AssignmentNode = Cast<UNiagaraNodeAssignment>(Node);
			if (!AssignmentNode)
			{
				continue;
			}

			const int32 TargetIndex = AssignmentNode->FindAssignmentTarget(TargetVariable.GetName(), TargetVariable.GetType());
			if (TargetIndex != INDEX_NONE)
			{
				OutTargetIndex = TargetIndex;
				return AssignmentNode;
			}
		}

		return nullptr;
	}

	void EnsureAssignment(
		UNiagaraNodeOutput& OutputNode,
		const FNiagaraVariable& TargetVariable,
		const FString& DefaultValue,
		bool& bChanged,
		int32& AddedAssignmentCount,
		int32& UpdatedAssignmentCount)
	{
		int32 ExistingTargetIndex = INDEX_NONE;
		UNiagaraNodeAssignment* ExistingAssignment = FindAssignmentForTarget(OutputNode, TargetVariable, ExistingTargetIndex);
		if (ExistingAssignment)
		{
			const TArray<FString>& ExistingDefaults = ExistingAssignment->GetAssignmentDefaults();
			const FString* ExistingDefaultValue = ExistingDefaults.IsValidIndex(ExistingTargetIndex)
				? &ExistingDefaults[ExistingTargetIndex]
				: nullptr;
			if (!ExistingDefaultValue || *ExistingDefaultValue != DefaultValue)
			{
				ExistingAssignment->Modify();
				if (ExistingAssignment->SetAssignmentTarget(ExistingTargetIndex, TargetVariable, &DefaultValue))
				{
					ExistingAssignment->RefreshFromExternalChanges();
					bChanged = true;
					++UpdatedAssignmentCount;
				}
			}
			return;
		}

		TArray<FNiagaraVariable> TargetVariables;
		TargetVariables.Add(TargetVariable);

		TArray<FString> DefaultValues;
		DefaultValues.Add(DefaultValue);

		UNiagaraNodeAssignment* AddedAssignment = FNiagaraStackGraphUtilities::AddParameterModuleToStack(
			TargetVariables,
			OutputNode,
			INDEX_NONE,
			DefaultValues);
		if (AddedAssignment)
		{
			AddedAssignment->RefreshFromExternalChanges();
			bChanged = true;
			++AddedAssignmentCount;
		}
	}

	bool IsParameterMapGetNode(const UNiagaraNode* Node)
	{
		return Node && Node->GetClass()->GetFName() == NiagaraParameterMapGetClassName;
	}

	UEdGraphPin* FindOutputPinByName(UNiagaraNode& GetNode, const FName& PinName)
	{
		TArray<UEdGraphPin*> OutputPins;
		GetNode.GetOutputPins(OutputPins);
		for (UEdGraphPin* OutputPin : OutputPins)
		{
			const bool bIsDynamicAddPin =
				OutputPin &&
				OutputPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc &&
				OutputPin->PinType.PinSubCategory == NiagaraDynamicAddPinSubCategory;
			if (OutputPin && !bIsDynamicAddPin && OutputPin->PinName == PinName)
			{
				return OutputPin;
			}
		}

		return nullptr;
	}

	UEdGraphPin* FindDefaultInputPinForType(UNiagaraNode& GetNode, const FNiagaraTypeDefinition& ExpectedType)
	{
		TArray<UEdGraphPin*> InputPins;
		GetNode.GetInputPins(InputPins);
		for (UEdGraphPin* InputPin : InputPins)
		{
			if (!InputPin || InputPin->PinName != NAME_None)
			{
				continue;
			}

			if (UEdGraphSchema_Niagara::PinToTypeDefinition(InputPin) == ExpectedType)
			{
				return InputPin;
			}
		}

		for (UEdGraphPin* InputPin : InputPins)
		{
			if (InputPin && UEdGraphSchema_Niagara::PinToTypeDefinition(InputPin) == ExpectedType)
			{
				return InputPin;
			}
		}

		return nullptr;
	}

	UEdGraphPin* CreateParameterMapGetOutputPin(UNiagaraNode& GetNode, const FNiagaraVariable& Variable)
	{
		GetNode.Modify();
		FEdGraphPinType PinType = UEdGraphSchema_Niagara::TypeDefinitionToPinType(Variable.GetType());
		UEdGraphPin* NewPin = GetNode.CreatePin(EGPD_Output, PinType, Variable.GetName());
		if (NewPin)
		{
			NewPin->PinType.PinSubCategory = NiagaraParameterPinSubCategory;
			NewPin->PinFriendlyName = FText::AsCultureInvariant(NewPin->PinName.ToString());
			if (!NewPin->PersistentGuid.IsValid())
			{
				NewPin->PersistentGuid = FGuid::NewGuid();
			}
		}

		return NewPin;
	}

	bool IsBeginDefaultsGetNode(const UNiagaraNode& GetNode)
	{
		const UEdGraphPin* InputPin = GetNode.GetInputPin(0);
		if (!InputPin)
		{
			return false;
		}

		for (const UEdGraphPin* LinkedPin : InputPin->LinkedTo)
		{
			const UNiagaraNodeInput* InputNode = LinkedPin ? Cast<UNiagaraNodeInput>(LinkedPin->GetOwningNode()) : nullptr;
			if (InputNode && InputNode->Usage == ENiagaraInputNodeUsage::TranslatorConstant && InputNode->Input == TRANSLATOR_PARAM_BEGIN_DEFAULTS)
			{
				return true;
			}
		}

		return false;
	}

	UNiagaraNode* FindBeginDefaultsGetNode(UNiagaraGraph& Graph, UNiagaraNode* DirectValueGetNode)
	{
		TArray<UNiagaraNode*> GetNodes;
		Graph.GetNodesOfClass(GetNodes);
		for (UNiagaraNode* GetNode : GetNodes)
		{
			if (IsParameterMapGetNode(GetNode) && GetNode != DirectValueGetNode && IsBeginDefaultsGetNode(*GetNode))
			{
				return GetNode;
			}
		}

		for (UNiagaraNode* GetNode : GetNodes)
		{
			if (IsParameterMapGetNode(GetNode) && GetNode != DirectValueGetNode)
			{
				return GetNode;
			}
		}

		return nullptr;
	}

	bool EnsureAssignmentDefaultReadsUserParameter(
		UNiagaraNodeAssignment& AssignmentNode,
		const FNiagaraVariable& TargetVariable,
		const FNiagaraVariable& UserParameterVariable)
	{
		UNiagaraScriptSource* FunctionScriptSource = AssignmentNode.GetFunctionScriptSource();
		UNiagaraGraph* AssignmentGraph = FunctionScriptSource ? FunctionScriptSource->NodeGraph : nullptr;
		if (!AssignmentGraph)
		{
			return false;
		}

		const FNiagaraParameterHandle TargetHandle(TargetVariable.GetName());
		const FName ModuleTargetName(*FString::Printf(TEXT("Module.%s"), *TargetHandle.GetParameterHandleString().ToString()));
		UNiagaraNode* DirectValueGetNode = nullptr;
		UEdGraphPin* DirectValueOutputPin = nullptr;

		TArray<UNiagaraNode*> GetNodes;
		AssignmentGraph->GetNodesOfClass(GetNodes);
		for (UNiagaraNode* GetNode : GetNodes)
		{
			UEdGraphPin* OutputPin = IsParameterMapGetNode(GetNode) ? FindOutputPinByName(*GetNode, ModuleTargetName) : nullptr;
			if (OutputPin)
			{
				DirectValueGetNode = GetNode;
				DirectValueOutputPin = OutputPin;
				break;
			}
		}

		if (!DirectValueGetNode || !DirectValueOutputPin)
		{
			return false;
		}

		UEdGraphPin* TargetDefaultPin = FindDefaultInputPinForType(*DirectValueGetNode, TargetVariable.GetType());
		UNiagaraNode* BeginDefaultsGetNode = FindBeginDefaultsGetNode(*AssignmentGraph, DirectValueGetNode);
		if (!TargetDefaultPin || !BeginDefaultsGetNode)
		{
			return false;
		}

		bool bChanged = false;
		UEdGraphPin* UserParameterOutputPin = FindOutputPinByName(*BeginDefaultsGetNode, UserParameterVariable.GetName());
		if (!UserParameterOutputPin)
		{
			UserParameterOutputPin = CreateParameterMapGetOutputPin(*BeginDefaultsGetNode, UserParameterVariable);
			bChanged = UserParameterOutputPin != nullptr;
		}

		if (!UserParameterOutputPin)
		{
			return bChanged;
		}

		const bool bAlreadyLinked =
			TargetDefaultPin->LinkedTo.Num() == 1 &&
			TargetDefaultPin->LinkedTo[0] == UserParameterOutputPin;
		if (!bAlreadyLinked)
		{
			TargetDefaultPin->Modify();
			UserParameterOutputPin->Modify();
			TargetDefaultPin->BreakAllPinLinks(false);
			UserParameterOutputPin->MakeLinkTo(TargetDefaultPin);
			TargetDefaultPin->bDefaultValueIsIgnored = true;
			bChanged = true;
		}

		if (bChanged)
		{
			AssignmentGraph->NotifyGraphChanged();
		}

		return bChanged;
	}

	FNiagaraVariable CreateRapidIterationParameter(
		const FString& UniqueEmitterName,
		ENiagaraScriptUsage ScriptUsage,
		const FName& AliasedInputName,
		const FNiagaraTypeDefinition& InputType)
	{
		FNiagaraVariable InputVariable(InputType, AliasedInputName);
		if (ScriptUsage == ENiagaraScriptUsage::SystemSpawnScript || ScriptUsage == ENiagaraScriptUsage::SystemUpdateScript)
		{
			return FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(InputVariable, nullptr, ScriptUsage);
		}

		return FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(InputVariable, *UniqueEmitterName, ScriptUsage);
	}

	template<typename ValueType>
	void SetRapidIterationParameter(
		const FString& UniqueEmitterName,
		UNiagaraScript& TargetScript,
		UNiagaraNodeFunctionCall& TargetModuleNode,
		FName InputName,
		const FNiagaraTypeDefinition& InputType,
		ValueType Value,
		bool& bChanged)
	{
		FNiagaraParameterHandle InputHandle = FNiagaraParameterHandle::CreateModuleParameterHandle(InputName);
		FNiagaraParameterHandle AliasedInputHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(InputHandle, &TargetModuleNode);
		FNiagaraVariable RapidIterationParameter = CreateRapidIterationParameter(
			UniqueEmitterName,
			TargetScript.GetUsage(),
			AliasedInputHandle.GetParameterHandleString(),
			InputType);

		RapidIterationParameter.SetValue(Value);

		const uint8* ExistingData = TargetScript.RapidIterationParameters.GetParameterData(RapidIterationParameter);
		if (!ExistingData || FMemory::Memcmp(ExistingData, RapidIterationParameter.GetData(), RapidIterationParameter.GetSizeInBytes()) != 0)
		{
			TargetScript.Modify();
			TargetScript.RapidIterationParameters.SetParameterData(RapidIterationParameter.GetData(), RapidIterationParameter, true);
			bChanged = true;
		}
	}

	FString FloatDefaultString(float Value)
	{
		return FString::SanitizeFloat(Value);
	}
}

USRConfigureSpaceshipTrailCommandlet::USRConfigureSpaceshipTrailCommandlet()
{
	IsEditor = true;
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
}

int32 USRConfigureSpaceshipTrailCommandlet::Main(const FString& Params)
{
	FString SpaceshipTrailSystemPath;
	FString SpaceshipTrailMaterialPath;
	FString EmitterStateModulePath;
	FString SpawnRateModulePath;
	FString SystemLocationModulePath;
	FString UpdateAgeModulePath;
	if (!TryReadRequiredObjectPath(Params, TEXT("TrailSystem"), SpaceshipTrailSystemPath)
		|| !TryReadRequiredObjectPath(Params, TEXT("TrailMaterial"), SpaceshipTrailMaterialPath)
		|| !TryReadRequiredObjectPath(Params, TEXT("EmitterStateModule"), EmitterStateModulePath)
		|| !TryReadRequiredObjectPath(Params, TEXT("SpawnRateModule"), SpawnRateModulePath)
		|| !TryReadRequiredObjectPath(Params, TEXT("SystemLocationModule"), SystemLocationModulePath)
		|| !TryReadRequiredObjectPath(Params, TEXT("UpdateAgeModule"), UpdateAgeModulePath))
	{
		return 1;
	}

	UNiagaraSystem* TrailSystem = LoadObject<UNiagaraSystem>(nullptr, *SpaceshipTrailSystemPath);
	if (!TrailSystem)
	{
		SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Could not load spaceship trail Niagara system: %s"), *SpaceshipTrailSystemPath);
		return 1;
	}

	UMaterialInterface* TrailMaterial = LoadObject<UMaterialInterface>(nullptr, *SpaceshipTrailMaterialPath);
	if (!TrailMaterial)
	{
		SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Could not load spaceship trail material: %s"), *SpaceshipTrailMaterialPath);
		return 1;
	}

	bool bChanged = false;
	int32 RibbonRendererCount = 0;
	int32 AddedModuleCount = 0;
	int32 AddedAssignmentCount = 0;
	int32 UpdatedAssignmentCount = 0;
	int32 LinkedUserParameterCount = 0;
	const FNiagaraVariable TrailMaterialVariable(FNiagaraTypeDefinition::GetUMaterialDef(), TrailMaterialUserParameter);
	const FNiagaraVariable TrailWidthVariable(FNiagaraTypeDefinition::GetFloatDef(), TrailWidthUserParameter);

	TrailSystem->Modify();
	TrailSystem->GetExposedParameters().AddParameter(TrailMaterialVariable, false, true);
	const TOptional<float> ExistingTrailWidth = TrailSystem->GetExposedParameters().GetParameterOptionalValue<float>(TrailWidthVariable);
	if (!ExistingTrailWidth.IsSet() || !FMath::IsNearlyEqual(ExistingTrailWidth.GetValue(), DefaultTrailWidth))
	{
		TrailSystem->GetExposedParameters().SetParameterValue(DefaultTrailWidth, TrailWidthVariable, true);
		bChanged = true;
	}

	for (FNiagaraEmitterHandle& EmitterHandle : TrailSystem->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
		if (!EmitterData)
		{
			continue;
		}

		if (EmitterHandle.GetInstance().Emitter)
		{
			EmitterHandle.GetInstance().Emitter->Modify();
		}
		if (EmitterData->SimTarget != ENiagaraSimTarget::CPUSim)
		{
			EmitterData->SimTarget = ENiagaraSimTarget::CPUSim;
			bChanged = true;
		}
		if (EmitterData->bLocalSpace)
		{
			EmitterData->bLocalSpace = false;
			bChanged = true;
		}
		if (!EmitterData->bRequiresPersistentIDs)
		{
			EmitterData->bRequiresPersistentIDs = true;
			bChanged = true;
		}
		if (EmitterData->InterpolatedSpawnMode != ENiagaraInterpolatedSpawnMode::Interpolation)
		{
			EmitterData->InterpolatedSpawnMode = ENiagaraInterpolatedSpawnMode::Interpolation;
			bChanged = true;
		}

		UNiagaraScriptSource* GraphSource = Cast<UNiagaraScriptSource>(EmitterData->GraphSource);
		UNiagaraGraph* Graph = GraphSource ? GraphSource->NodeGraph : nullptr;
		if (!Graph)
		{
			SR_LOG(EditorCommandlet, LogTemp, Warning, TEXT("Emitter %s has no editable Niagara graph source"), *EmitterHandle.GetUniqueInstanceName());
		}
		else
		{
			UNiagaraNodeOutput* EmitterUpdateOutputNode = FindOutputNodeByUsage(Graph, ENiagaraScriptUsage::EmitterUpdateScript);
			UNiagaraNodeOutput* ParticleSpawnOutputNode = FindOutputNodeByUsage(Graph, ENiagaraScriptUsage::ParticleSpawnScript);
			UNiagaraNodeOutput* ParticleUpdateOutputNode = FindOutputNodeByUsage(Graph, ENiagaraScriptUsage::ParticleUpdateScript);

			if (EmitterUpdateOutputNode)
			{
				EnsureModule(*EmitterUpdateOutputNode, EmitterStateModulePath, 0, bChanged, AddedModuleCount);
				UNiagaraNodeFunctionCall* SpawnRateNode = EnsureModule(*EmitterUpdateOutputNode, SpawnRateModulePath, INDEX_NONE, bChanged, AddedModuleCount);
				if (SpawnRateNode && EmitterData->EmitterUpdateScriptProps.Script)
				{
					SetRapidIterationParameter(
						EmitterHandle.GetUniqueInstanceName(),
						*EmitterData->EmitterUpdateScriptProps.Script,
						*SpawnRateNode,
						TEXT("SpawnRate"),
						FNiagaraTypeDefinition::GetFloatDef(),
						DefaultTrailSpawnRate,
						bChanged);
				}
			}
			else
			{
				SR_LOG(EditorCommandlet, LogTemp, Warning, TEXT("No Emitter Update output found in %s"), *EmitterHandle.GetUniqueInstanceName());
			}

			if (ParticleSpawnOutputNode)
			{
				EnsureModule(*ParticleSpawnOutputNode, SystemLocationModulePath, 0, bChanged, AddedModuleCount);
				EnsureAssignment(
					*ParticleSpawnOutputNode,
					SYS_PARAM_PARTICLES_RIBBONWIDTH,
					FString(),
					bChanged,
					AddedAssignmentCount,
					UpdatedAssignmentCount);
				{
					int32 WidthTargetIndex = INDEX_NONE;
					UNiagaraNodeAssignment* WidthAssignment = FindAssignmentForTarget(
						*ParticleSpawnOutputNode,
						SYS_PARAM_PARTICLES_RIBBONWIDTH,
						WidthTargetIndex);
					if (WidthAssignment && EnsureAssignmentDefaultReadsUserParameter(*WidthAssignment, SYS_PARAM_PARTICLES_RIBBONWIDTH, TrailWidthVariable))
					{
						bChanged = true;
						++LinkedUserParameterCount;
					}
				}
				EnsureAssignment(
					*ParticleSpawnOutputNode,
					SYS_PARAM_PARTICLES_LIFETIME,
					FloatDefaultString(DefaultTrailLifetime),
					bChanged,
					AddedAssignmentCount,
					UpdatedAssignmentCount);
				EnsureAssignment(
					*ParticleSpawnOutputNode,
					SYS_PARAM_PARTICLES_COLOR,
					TEXT("(R=0.100000,G=0.950000,B=1.000000,A=1.000000)"),
					bChanged,
					AddedAssignmentCount,
					UpdatedAssignmentCount);
			}
			else
			{
				SR_LOG(EditorCommandlet, LogTemp, Warning, TEXT("No Particle Spawn output found in %s"), *EmitterHandle.GetUniqueInstanceName());
			}

			if (ParticleUpdateOutputNode)
			{
				EnsureModule(*ParticleUpdateOutputNode, UpdateAgeModulePath, INDEX_NONE, bChanged, AddedModuleCount);
			}
			else
			{
				SR_LOG(EditorCommandlet, LogTemp, Warning, TEXT("No Particle Update output found in %s"), *EmitterHandle.GetUniqueInstanceName());
			}

			Graph->NotifyGraphChanged();
		}

		for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
		{
			UNiagaraRibbonRendererProperties* RibbonRenderer = Cast<UNiagaraRibbonRendererProperties>(Renderer);
			if (!RibbonRenderer)
			{
				continue;
			}

			++RibbonRendererCount;
			RibbonRenderer->Modify();
			if (RibbonRenderer->Material != TrailMaterial)
			{
				RibbonRenderer->Material = TrailMaterial;
				bChanged = true;
			}
			if (RibbonRenderer->MaterialUserParamBinding.Parameter != TrailMaterialVariable)
			{
				RibbonRenderer->MaterialUserParamBinding.Parameter = TrailMaterialVariable;
				bChanged = true;
			}
			if (RibbonRenderer->FacingMode != ENiagaraRibbonFacingMode::Screen)
			{
				RibbonRenderer->FacingMode = ENiagaraRibbonFacingMode::Screen;
				bChanged = true;
			}
			if (!RibbonRenderer->bLinkOrderUseUniqueID)
			{
				RibbonRenderer->bLinkOrderUseUniqueID = true;
				bChanged = true;
			}
			if (RibbonRenderer->MaxNumRibbons != 1)
			{
				RibbonRenderer->MaxNumRibbons = 1;
				bChanged = true;
			}
			if (RibbonRenderer->bCastShadows)
			{
				RibbonRenderer->bCastShadows = false;
				bChanged = true;
			}
		}
	}

	if (RibbonRendererCount == 0)
	{
		SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("No Ribbon Renderer found in %s"), *SpaceshipTrailSystemPath);
		return 1;
	}

	if (bChanged)
	{
		TrailSystem->InvalidateCachedData();
		TrailSystem->RequestCompile(true);
		TrailSystem->MarkPackageDirty();
	}

	UPackage* Package = TrailSystem->GetOutermost();
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;

	const bool bSaved = UPackage::SavePackage(Package, TrailSystem, *PackageFilename, SaveArgs);
	if (!bSaved)
	{
		SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("Failed to save %s"), *PackageFilename);
		return 1;
	}

	SR_LOG(EditorCommandlet, LogTemp, Display, TEXT("Configured %d Ribbon Renderer(s) in %s to use %s through %s"),
		RibbonRendererCount,
		*SpaceshipTrailSystemPath,
		*SpaceshipTrailMaterialPath,
		*TrailMaterialUserParameter.ToString());
	SR_LOG(EditorCommandlet, LogTemp, Display, TEXT("Trail stack changes: added modules=%d, added assignments=%d, updated assignments=%d, linked user params=%d, width=%s default %.1f, lifetime=%.1f, spawnRate=%.1f"),
		AddedModuleCount,
		AddedAssignmentCount,
		UpdatedAssignmentCount,
		LinkedUserParameterCount,
		*TrailWidthUserParameter.ToString(),
		DefaultTrailWidth,
		DefaultTrailLifetime,
		DefaultTrailSpawnRate);

	return 0;
}

#else

USRConfigureSpaceshipTrailCommandlet::USRConfigureSpaceshipTrailCommandlet()
{
	IsEditor = true;
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	UseCommandletResultAsExitCode = true;
}

int32 USRConfigureSpaceshipTrailCommandlet::Main(const FString& Params)
{
	SR_LOG(EditorCommandlet, LogTemp, Error, TEXT("SRConfigureSpaceshipTrailCommandlet is only available in editor builds."));
	return 1;
}

#endif
