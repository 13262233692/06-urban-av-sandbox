#include "MapMapping/SplineMapperComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/SplineMeshComponent.h"

USplineMapperComponent::USplineMapperComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USplineMapperComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USplineMapperComponent::CreateSplineForNode(const FLaneGraphNode& Node, int32 NodeIdx)
{
	if (Node.CenterlinePoints.Num() < 2) return;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	FString SplineName = FString::Printf(TEXT("LaneSpline_R%d_S%d_L%d_N%d"),
		Node.RoadId, Node.LaneSectionIndex, Node.LaneId, NodeIdx);

	USplineComponent* Spline = NewObject<USplineComponent>(Owner, FName(*SplineName));
	if (!Spline) return;

	Spline->SetupAttachment(Owner->GetRootComponent());
	Spline->RegisterComponent();
	Spline->SetClosedLoop(false);

	int32 NumPoints = Node.CenterlinePoints.Num();
	Spline->ClearSplinePoints();

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const FVector& Pt = Node.CenterlinePoints[i];
		FVector NextPt = (i + 1 < NumPoints) ? Node.CenterlinePoints[i + 1] : Pt;
		FVector PrevPt = (i > 0) ? Node.CenterlinePoints[i - 1] : Pt;
		FVector Tangent = (NextPt - PrevPt) * 0.5f;

		Spline->AddSplinePoint(Pt, ESplineCoordinateSpace::World, false);
		Spline->SetTangentAtSplinePoint(i, Tangent, ESplineCoordinateSpace::World, false);
	}

	Spline->UpdateSpline();

	if (RoadMesh)
	{
		int32 NumSegments = Spline->GetNumberOfSplinePoints() - 1;
		for (int32 SegIdx = 0; SegIdx < NumSegments; ++SegIdx)
		{
			FString MeshName = FString::Printf(TEXT("RoadMesh_%d_%d"), NodeIdx, SegIdx);
			USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(Owner, FName(*MeshName));

			if (SplineMesh)
			{
				SplineMesh->SetupAttachment(Spline);
				SplineMesh->SetStaticMesh(RoadMesh);
				SplineMesh->SetMobility(EComponentMobility::Movable);
				SplineMesh->SetForwardAxis(ESplineMeshAxis::X);

				FVector StartPos, StartTan, EndPos, EndTan;
				Spline->GetLocationAndTangentAtSplinePoint(SegIdx, StartPos, StartTan, ESplineCoordinateSpace::Local);
				Spline->GetLocationAndTangentAtSplinePoint(SegIdx + 1, EndPos, EndTan, ESplineCoordinateSpace::Local);

				SplineMesh->SetStartAndEnd(StartPos, StartTan, EndPos, EndTan, true);

				float HalfWidth = MeshWidth * 0.5f;
				SplineMesh->SetStartScale(FVector2D(HalfWidth, 1.0f));
				SplineMesh->SetEndScale(FVector2D(HalfWidth, 1.0f));

				FLinearColor Color = Node.bIsJunction ? JunctionLaneColor : DrivingLaneColor;
				SplineMesh->SetVectorParameterValueOnMaterials(FName(TEXT("LaneColor")),
					FVector(Color.R, Color.G, Color.B));

				SplineMesh->RegisterComponent();
			}
		}
	}

	if (bDrawDebug)
	{
		for (int32 i = 0; i < NumPoints; ++i)
		{
			FVector Pt = Node.CenterlinePoints[i];
			DrawDebugSphere(GetWorld(), Pt, 20.0f, 8,
				Node.bIsJunction ? FColor::Yellow : FColor::Green, true, -1.0f, 0, 2.0f);
		}
	}

	LaneSplines.Add(Spline);
	NodeIdToSpline.Add(NodeIdx, Spline);
}

bool USplineMapperComponent::BuildSplinesFromLaneGraph(const FLaneGraph& Graph)
{
	ClearSplines();

	for (int32 i = 0; i < Graph.Nodes.Num(); ++i)
	{
		CreateSplineForNode(Graph.Nodes[i], i);
	}

	UE_LOG(LogTemp, Log, TEXT("[SplineMapper] Created %d splines from lane graph"), LaneSplines.Num());
	return LaneSplines.Num() > 0;
}

void USplineMapperComponent::ClearSplines()
{
	for (USplineComponent* Spline : LaneSplines)
	{
		if (Spline)
		{
			Spline->DestroyComponent();
		}
	}

	LaneSplines.Empty();
	NodeIdToSpline.Empty();
}

USplineComponent* USplineMapperComponent::GetSplineForLaneNode(int32 NodeId) const
{
	const USplineComponent* const* Found = NodeIdToSpline.Find(NodeId);
	return Found ? *Found : nullptr;
}

FTransform USplineMapperComponent::GetTransformAtDistanceAlongSpline(int32 NodeId, float Distance) const
{
	USplineComponent* Spline = GetSplineForLaneNode(NodeId);
	if (!Spline) return FTransform::Identity;

	return Spline->GetTransformAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
}

float USplineMapperComponent::GetTotalSplineLength(int32 NodeId) const
{
	USplineComponent* Spline = GetSplineForLaneNode(NodeId);
	if (!Spline) return 0.0f;

	return Spline->GetSplineLength();
}
