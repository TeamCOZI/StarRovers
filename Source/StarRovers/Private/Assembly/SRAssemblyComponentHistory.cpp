#include "Assembly/SRAssemblyComponent.h"

#include "Assembly/SRAssemblyPlacementHistory.h"

bool USRAssemblyComponent::TryUndoAssemblyPlacement()
{
	return PlacementHistory.TryUndo(*this);
}

bool USRAssemblyComponent::TryRedoAssemblyPlacement()
{
	return PlacementHistory.TryRedo(*this);
}
