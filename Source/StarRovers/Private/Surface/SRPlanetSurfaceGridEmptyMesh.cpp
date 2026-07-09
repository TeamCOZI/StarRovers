#include "SRPlanetSurfaceGridEmptyMesh.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

void StarRovers::SurfaceGridEmptyMesh::ApplyEmptyPrimaryColorMesh(UDynamicMeshComponent& GridComponent)
{
	UE::Geometry::FDynamicMesh3 EmptyGridMesh;
	EmptyGridMesh.EnableAttributes();
	EmptyGridMesh.Attributes()->EnablePrimaryColors();
	GridComponent.SetMesh(MoveTemp(EmptyGridMesh));
}
