// Copyright 2016-2018 Chris Conway (Koderz). All Rights Reserved.

#include "RuntimeMeshComponentPlugin.h"
#include "RuntimeMeshBuilder.h"


template<typename TYPE>
FORCEINLINE static TYPE& GetStreamAccess(TArray<uint8>* Data, int32 Index, int32 Stride, int32 Offset)
{
	int32 StartPosition = (Index * Stride + Offset);
	return *((TYPE*)&(*Data)[StartPosition]);
}


template<typename TYPE>
struct FRuntimeMeshEightUV
{
	TStaticArray<TYPE, 8> UVs;
};

static_assert(sizeof(FRuntimeMeshEightUV<FVector2D>) == (8 * sizeof(FVector2D)), "Incorrect size for 8 UV struct");
static_assert(sizeof(FRuntimeMeshEightUV<FVector2DHalf>) == (8 * sizeof(FVector2DHalf)), "Incorrect size for 8 UV struct");


//////////////////////////////////////////////////////////////////////////
//	FRuntimeMeshVerticesAccessor

FRuntimeMeshVerticesAccessor::FRuntimeMeshVerticesAccessor(TArray<uint8>* PositionStreamData, TArray<uint8>* TangentStreamData, TArray<uint8>* UVStreamData, TArray<uint8>* ColorStreamData)
	: bIsInitialized(false)
	, PositionStream(PositionStreamData)
	, TangentStream(TangentStreamData), bTangentHighPrecision(false), TangentSize(0), TangentStride(0)
	, UVStream(UVStreamData), bUVHighPrecision(false), UVSize(0), UVStride(0), UVChannelCount(0)
	, ColorStream(ColorStreamData)
{
}

FRuntimeMeshVerticesAccessor::FRuntimeMeshVerticesAccessor(bool bInTangentsHighPrecision, bool bInUVsHighPrecision, int32 bInUVCount,
	TArray<uint8>* PositionStreamData, TArray<uint8>* TangentStreamData, TArray<uint8>* UVStreamData, TArray<uint8>* ColorStreamData)
	: bIsInitialized(false)
	, PositionStream(PositionStreamData)
	, TangentStream(TangentStreamData), bTangentHighPrecision(false), TangentSize(0), TangentStride(0)
	, UVStream(UVStreamData), bUVHighPrecision(false), UVSize(0), UVStride(0), UVChannelCount(0)
	, ColorStream(ColorStreamData)
{
	Initialize(bInTangentsHighPrecision, bInUVsHighPrecision, bInUVCount);
}

FRuntimeMeshVerticesAccessor::~FRuntimeMeshVerticesAccessor()
{
}

void FRuntimeMeshVerticesAccessor::Initialize(bool bInTangentsHighPrecision, bool bInUVsHighPrecision, int32 bInUVCount)
{
	bIsInitialized = true;
	const_cast<bool&>(bTangentHighPrecision) = bInTangentsHighPrecision;
	const_cast<int32&>(TangentSize) = (bTangentHighPrecision ? sizeof(FPackedRGBA16N) : sizeof(FPackedNormal));
	const_cast<int32&>(TangentStride) = TangentSize * 2;

	const_cast<bool&>(bUVHighPrecision) = bInUVsHighPrecision;
	const_cast<int32&>(UVChannelCount) = bInUVCount;
	const_cast<int32&>(UVSize) = (bUVHighPrecision ? sizeof(FVector2D) : sizeof(FVector2DHalf));
	const_cast<int32&>(UVStride) = UVSize * UVChannelCount;
}

int32 FRuntimeMeshVerticesAccessor::NumVertices() const
{
	check(bIsInitialized);
	int32 Count = PositionStream->Num() / PositionStride;
	return Count;
}

int32 FRuntimeMeshVerticesAccessor::NumUVChannels() const
{
	check(bIsInitialized);
	return UVChannelCount;
}

void FRuntimeMeshVerticesAccessor::EmptyVertices(int32 Slack /*= 0*/)
{
	check(bIsInitialized);
	PositionStream->Empty(Slack * PositionStride);
	TangentStream->Empty(Slack * TangentStride);
	UVStream->Empty(Slack * UVSize);
	ColorStream->Empty(Slack * ColorStride);
}

void FRuntimeMeshVerticesAccessor::SetNumVertices(int32 NewNum)
{
	check(bIsInitialized);
	PositionStream->SetNumZeroed(NewNum * PositionStride);
	TangentStream->SetNumZeroed(NewNum * TangentStride);
	UVStream->SetNumZeroed(NewNum * UVStride);
	ColorStream->SetNumZeroed(NewNum * ColorStride);
}

int32 FRuntimeMeshVerticesAccessor::AddVertex(FVector InPosition)
{
	check(bIsInitialized);
	int32 NewIndex = AddSingleVertex();

	SetPosition(NewIndex, InPosition);

	return NewIndex;
}

FVector FRuntimeMeshVerticesAccessor::GetPosition(int32 Index) const
{
	check(bIsInitialized);
	return GetStreamAccess<FVector>(PositionStream, Index, PositionStride, 0);
}

FVector4 FRuntimeMeshVerticesAccessor::GetNormal(int32 Index) const
{
	check(bIsInitialized);
	if (bTangentHighPrecision)
	{
		return GetStreamAccess<FRuntimeMeshTangentsHighPrecision>(TangentStream, Index, TangentStride, 0).Normal;
	}
	else
	{
		return GetStreamAccess<FRuntimeMeshTangents>(TangentStream, Index, TangentStride, 0).Normal;
	}
}

FVector FRuntimeMeshVerticesAccessor::GetTangent(int32 Index) const
{
	check(bIsInitialized);
	if (bTangentHighPrecision)
	{
		return GetStreamAccess<FRuntimeMeshTangentsHighPrecision>(TangentStream, Index, TangentStride, 0).Tangent;
	}
	else
	{
		return GetStreamAccess<FRuntimeMeshTangents>(TangentStream, Index, TangentStride, 0).Tangent;
	}
}

FColor FRuntimeMeshVerticesAccessor::GetColor(int32 Index) const
{
	check(bIsInitialized);
	return GetStreamAccess<FColor>(ColorStream, Index, ColorStride, 0);
}

FVector2D FRuntimeMeshVerticesAccessor::GetUV(int32 Index, int32 Channel) const
{
	check(bIsInitialized);
	check(Channel >= 0 && Channel < UVChannelCount);
	if (bUVHighPrecision)
	{
		return GetStreamAccess<FRuntimeMeshEightUV<FVector2D>>(UVStream, Index, UVStride, 0).UVs[Channel];
	}
	else
	{
		return GetStreamAccess<FRuntimeMeshEightUV<FVector2DHalf>>(UVStream, Index, UVStride, 0).UVs[Channel];
	}
}

void FRuntimeMeshVerticesAccessor::SetPosition(int32 Index, FVector Value)
{
	check(bIsInitialized);
	GetStreamAccess<FVector>(PositionStream, Index, PositionStride, 0) = Value;
}

void FRuntimeMeshVerticesAccessor::SetNormal(int32 Index, const FVector4& Value)
{
	check(bIsInitialized);
	if (bTangentHighPrecision)
	{
		GetStreamAccess<FRuntimeMeshTangentsHighPrecision>(TangentStream, Index, TangentStride, 0).Normal = Value;
	}
	else
	{
		GetStreamAccess<FRuntimeMeshTangents>(TangentStream, Index, TangentStride, 0).Normal = Value;
	}
}

void FRuntimeMeshVerticesAccessor::SetTangent(int32 Index, FVector Value)
{
	check(bIsInitialized);
	if (bTangentHighPrecision)
	{
		GetStreamAccess<FRuntimeMeshTangentsHighPrecision>(TangentStream, Index, TangentStride, 0).Tangent = Value;
	}
	else
	{
		GetStreamAccess<FRuntimeMeshTangents>(TangentStream, Index, TangentStride, 0).Tangent = Value;
	}
}

void FRuntimeMeshVerticesAccessor::SetTangent(int32 Index, FRuntimeMeshTangent Value)
{
	check(bIsInitialized);
	if (bTangentHighPrecision)
	{
		FRuntimeMeshTangentsHighPrecision& Tangents = GetStreamAccess<FRuntimeMeshTangentsHighPrecision>(TangentStream, Index, TangentStride, 0);
		FVector4 NewNormal = Tangents.Normal;
		NewNormal.W = Value.bFlipTangentY ? -1.0f : 1.0f;
		Tangents.Normal = NewNormal;
		Tangents.Tangent = Value.TangentX;
	}
	else
	{
		FRuntimeMeshTangents& Tangents = GetStreamAccess<FRuntimeMeshTangents>(TangentStream, Index, TangentStride, 0);
		FVector4 NewNormal = Tangents.Normal;
		NewNormal.W = Value.bFlipTangentY ? -1.0f : 1.0f;
		Tangents.Normal = NewNormal;
		Tangents.Tangent = Value.TangentX;
	}
}

void FRuntimeMeshVerticesAccessor::SetColor(int32 Index, FColor Value)
{
	check(bIsInitialized);
	GetStreamAccess<FColor>(ColorStream, Index, ColorStride, 0) = Value;
}

void FRuntimeMeshVerticesAccessor::SetUV(int32 Index, FVector2D Value)
{
	check(bIsInitialized);
	check(UVChannelCount > 0);
	if (bUVHighPrecision)
	{
		GetStreamAccess<FRuntimeMeshEightUV<FVector2D>>(UVStream, Index, UVStride, 0).UVs[0] = Value;
	}
	else
	{
		GetStreamAccess<FRuntimeMeshEightUV<FVector2DHalf>>(UVStream, Index, UVStride, 0).UVs[0] = Value;
	}
}

void FRuntimeMeshVerticesAccessor::SetUV(int32 Index, int32 Channel, FVector2D Value)
{
	check(bIsInitialized);
	check(Channel >= 0 && Channel < UVChannelCount);
	if (bUVHighPrecision)
	{
		GetStreamAccess<FRuntimeMeshEightUV<FVector2D>>(UVStream, Index, UVStride, 0).UVs[Channel] = Value;
	}
	else
	{
		GetStreamAccess<FRuntimeMeshEightUV<FVector2DHalf>>(UVStream, Index, UVStride, 0).UVs[Channel] = Value;
	}
}

void FRuntimeMeshVerticesAccessor::SetNormalTangent(int32 Index, FVector Normal, FRuntimeMeshTangent Tangent)
{
	check(bIsInitialized);
	if (bTangentHighPrecision)
	{
		FRuntimeMeshTangentsHighPrecision& Tangents = GetStreamAccess<FRuntimeMeshTangentsHighPrecision>(TangentStream, Index, TangentStride, 0);
		Tangents.Normal = FVector4(Normal, Tangent.bFlipTangentY ? -1 : 1);
		Tangents.Tangent = Tangent.TangentX;
	}
	else
	{
		FRuntimeMeshTangents& Tangents = GetStreamAccess<FRuntimeMeshTangents>(TangentStream, Index, TangentStride, 0);
		Tangents.Normal = FVector4(Normal, Tangent.bFlipTangentY ? -1 : 1);
		Tangents.Tangent = Tangent.TangentX;
	}
}

void FRuntimeMeshVerticesAccessor::SetTangents(int32 Index, FVector TangentX, FVector TangentY, FVector TangentZ)
{
	check(bIsInitialized);
	if (bTangentHighPrecision)
	{
		FRuntimeMeshTangentsHighPrecision& Tangents = GetStreamAccess<FRuntimeMeshTangentsHighPrecision>(TangentStream, Index, TangentStride, 0);
		Tangents.Normal = FVector4(TangentZ, GetBasisDeterminantSign(TangentX, TangentY, TangentZ));
		Tangents.Tangent = TangentX;
	}
	else
	{
		FRuntimeMeshTangents& Tangents = GetStreamAccess<FRuntimeMeshTangents>(TangentStream, Index, TangentStride, 0);
		Tangents.Normal = FVector4(TangentZ, GetBasisDeterminantSign(TangentX, TangentY, TangentZ));
		Tangents.Tangent = TangentX;
	}
}





FRuntimeMeshAccessorVertex FRuntimeMeshVerticesAccessor::GetVertex(int32 Index) const
{
	check(bIsInitialized);
	FRuntimeMeshAccessorVertex Vertex;

	Vertex.Position = GetStreamAccess<FVector>(PositionStream, Index, PositionStride, 0);

	if (bTangentHighPrecision)
	{
		FRuntimeMeshTangentsHighPrecision& Tangents = GetStreamAccess<FRuntimeMeshTangentsHighPrecision>(TangentStream, Index, TangentStride, 0);
		Vertex.Normal = Tangents.Normal;
		Vertex.Tangent = Tangents.Tangent;
	}
	else
	{
		FRuntimeMeshTangents& Tangents = GetStreamAccess<FRuntimeMeshTangents>(TangentStream, Index, TangentStride, 0);
		Vertex.Normal = Tangents.Normal;
		Vertex.Tangent = Tangents.Tangent;
	}

	Vertex.Color = GetStreamAccess<FColor>(ColorStream, Index, ColorStride, 0);
			
	Vertex.UVs.SetNum(NumUVChannels());
	if (bUVHighPrecision)
	{
		FRuntimeMeshEightUV<FVector2D>& UVs = GetStreamAccess<FRuntimeMeshEightUV<FVector2D>>(UVStream, Index, UVStride, 0);
		for (int32 UVIndex = 0; UVIndex < Vertex.UVs.Num(); UVIndex++)
		{
			Vertex.UVs[UVIndex] = UVs.UVs[UVIndex];
		}
	}
	else
	{
		FRuntimeMeshEightUV<FVector2DHalf>& UVs = GetStreamAccess<FRuntimeMeshEightUV<FVector2DHalf>>(UVStream, Index, UVStride, 0);
		for (int32 UVIndex = 0; UVIndex < Vertex.UVs.Num(); UVIndex++)
		{
			Vertex.UVs[UVIndex] = UVs.UVs[UVIndex];
		}
	}

	return Vertex;
}

void FRuntimeMeshVerticesAccessor::SetVertex(int32 Index, const FRuntimeMeshAccessorVertex& Vertex)
{
	check(bIsInitialized);

	GetStreamAccess<FVector>(PositionStream, Index, PositionStride, 0) = Vertex.Position;

	if (bTangentHighPrecision)
	{
		FRuntimeMeshTangentsHighPrecision& Tangents = GetStreamAccess<FRuntimeMeshTangentsHighPrecision>(TangentStream, Index, TangentStride, 0);
		Tangents.Normal = Vertex.Normal;
		Tangents.Tangent = Vertex.Tangent;
	}
	else
	{
		FRuntimeMeshTangents& Tangents = GetStreamAccess<FRuntimeMeshTangents>(TangentStream, Index, TangentStride, 0);
		Tangents.Normal = Vertex.Normal;
		Tangents.Tangent = Vertex.Tangent;
	}

	GetStreamAccess<FColor>(ColorStream, Index, ColorStride, 0) = Vertex.Color;

	if (bUVHighPrecision)
	{
		FRuntimeMeshEightUV<FVector2D>& UVs = GetStreamAccess<FRuntimeMeshEightUV<FVector2D>>(UVStream, Index, UVStride, 0);
		for (int32 UVIndex = 0; UVIndex < Vertex.UVs.Num(); UVIndex++)
		{
			UVs.UVs[UVIndex] = Vertex.UVs[UVIndex];
		}
	}
	else
	{
		FRuntimeMeshEightUV<FVector2DHalf>& UVs = GetStreamAccess<FRuntimeMeshEightUV<FVector2DHalf>>(UVStream, Index, UVStride, 0);
		for (int32 UVIndex = 0; UVIndex < Vertex.UVs.Num(); UVIndex++)
		{
			UVs.UVs[UVIndex] = Vertex.UVs[UVIndex];
		}
	}
}

int32 FRuntimeMeshVerticesAccessor::AddVertex(const FRuntimeMeshAccessorVertex& Vertex)
{
	check(bIsInitialized);
	int32 NewIndex = AddSingleVertex();
	SetVertex(NewIndex, Vertex);
	return NewIndex;
}



int32 FRuntimeMeshVerticesAccessor::AddSingleVertex()
{
	int32 NewIndex = NumVertices();

	PositionStream->AddZeroed(PositionStride);
	TangentStream->AddZeroed(TangentStride);
	UVStream->AddZeroed(UVStride);
	ColorStream->AddZeroed(ColorStride);

	return NewIndex;
}





//////////////////////////////////////////////////////////////////////////
//	FRuntimeMeshIndicesAccessor

FRuntimeMeshIndicesAccessor::FRuntimeMeshIndicesAccessor(TArray<uint8>* IndexStreamData)
	: bIsInitialized(false)
	, IndexStream(IndexStreamData), b32BitIndices(false)
{
}

FRuntimeMeshIndicesAccessor::FRuntimeMeshIndicesAccessor(bool bIn32BitIndices, TArray<uint8>* IndexStreamData)
	: bIsInitialized(false)
	, IndexStream(IndexStreamData), b32BitIndices(false)
{
	Initialize(bIn32BitIndices);
}

FRuntimeMeshIndicesAccessor::~FRuntimeMeshIndicesAccessor()
{
}

void FRuntimeMeshIndicesAccessor::Initialize(bool bIn32BitIndices)
{
	bIsInitialized = true;
	b32BitIndices = bIn32BitIndices;

	check((IndexStream->Num() % (b32BitIndices ? 4 : 2)) == 0);
}

int32 FRuntimeMeshIndicesAccessor::NumIndices() const
{
	check(bIsInitialized);
	return IndexStream->Num() / GetIndexStride();
}

void FRuntimeMeshIndicesAccessor::EmptyIndices(int32 Slack)
{
	check(bIsInitialized);
	IndexStream->Empty(Slack * GetIndexStride());
}

void FRuntimeMeshIndicesAccessor::SetNumIndices(int32 NewNum)
{
	check(bIsInitialized);
	IndexStream->SetNumZeroed(NewNum * GetIndexStride());
}

int32 FRuntimeMeshIndicesAccessor::AddIndex(int32 NewIndex)
{
	check(bIsInitialized);
	int32 NewPosition = NumIndices();
	IndexStream->AddZeroed(GetIndexStride());
	SetIndex(NewPosition, NewIndex);
	return NewPosition;
}

int32 FRuntimeMeshIndicesAccessor::AddTriangle(int32 Index0, int32 Index1, int32 Index2)
{
	check(bIsInitialized);
	int32 NewPosition = NumIndices();
	IndexStream->AddZeroed(GetIndexStride() * 3);
	SetIndex(NewPosition + 0, Index0);
	SetIndex(NewPosition + 1, Index1);
	SetIndex(NewPosition + 2, Index2);
	return NewPosition;
}

int32 FRuntimeMeshIndicesAccessor::GetIndex(int32 Index) const
{
	check(bIsInitialized);
	if (b32BitIndices)
	{
		return GetStreamAccess<int32>(IndexStream, Index, sizeof(int32), 0);
	}
	else
	{
		return GetStreamAccess<uint16>(IndexStream, Index, sizeof(uint16), 0);
	}
}

void FRuntimeMeshIndicesAccessor::SetIndex(int32 Index, int32 Value)
{
	check(bIsInitialized);
	if (b32BitIndices)
	{
		GetStreamAccess<int32>(IndexStream, Index, sizeof(int32), 0) = Value;
	}
	else
	{
		GetStreamAccess<uint16>(IndexStream, Index, sizeof(uint16), 0) = Value;
	}
}




//////////////////////////////////////////////////////////////////////////
//	FRuntimeMeshAccessor

FRuntimeMeshAccessor::FRuntimeMeshAccessor(bool bInTangentsHighPrecision, bool bInUVsHighPrecision, int32 bInUVCount, bool bIn32BitIndices, TArray<uint8>* PositionStreamData,
	TArray<uint8>* TangentStreamData, TArray<uint8>* UVStreamData, TArray<uint8>* ColorStreamData, TArray<uint8>* IndexStreamData)
	: FRuntimeMeshVerticesAccessor(bInTangentsHighPrecision, bInUVsHighPrecision, bInUVCount, PositionStreamData, TangentStreamData, UVStreamData, ColorStreamData)
	, FRuntimeMeshIndicesAccessor(bIn32BitIndices, IndexStreamData)
{

}

FRuntimeMeshAccessor::~FRuntimeMeshAccessor()
{
}

FRuntimeMeshAccessor::FRuntimeMeshAccessor(TArray<uint8>* PositionStreamData, TArray<uint8>* TangentStreamData, TArray<uint8>* UVStreamData, TArray<uint8>* ColorStreamData, TArray<uint8>* IndexStreamData)
	: FRuntimeMeshVerticesAccessor(PositionStreamData, TangentStreamData, UVStreamData, ColorStreamData)
	, FRuntimeMeshIndicesAccessor(IndexStreamData)
{

}

void FRuntimeMeshAccessor::Initialize(bool bInTangentsHighPrecision, bool bInUVsHighPrecision, int32 bInUVCount, bool bIn32BitIndices)
{
	FRuntimeMeshVerticesAccessor::Initialize(bInTangentsHighPrecision, bInUVsHighPrecision, bInUVCount);
	FRuntimeMeshIndicesAccessor::Initialize(bIn32BitIndices);
}




void FRuntimeMeshAccessor::CopyTo(const TSharedPtr<FRuntimeMeshAccessor>& Other, bool bClearDestination) const
{
	if (bClearDestination)
	{
		Other->EmptyVertices(NumVertices());
		Other->EmptyIndices(NumIndices());
	}

	// TODO: Make this faster using short paths when the structures are the same.

	int32 StartVertex = Other->NumVertices();
	int32 NumVerts = NumVertices();
	int32 NumUVs = FMath::Min(NumUVChannels(), Other->NumUVChannels());

	for (int32 Index = 0; Index < NumVerts; Index++)
	{
		int32 NewIndex = Other->AddVertex(GetPosition(Index));
		Other->SetNormal(NewIndex, GetNormal(Index));
		Other->SetTangent(NewIndex, GetTangent(Index));
		Other->SetColor(NewIndex, GetColor(Index));
		for (int32 UVIndex = 0; UVIndex < NumUVs; UVIndex++)
		{
			Other->SetUV(NewIndex, UVIndex, GetUV(Index, UVIndex));
		}
	}

	int32 NumInds = NumIndices();
	for (int32 Index = 0; Index < NumInds; Index++)
	{
		Other->AddIndex(GetIndex(Index) + StartVertex);
	}
}

//////////////////////////////////////////////////////////////////////////
//	FRuntimeMeshBuilder

FRuntimeMeshBuilder::FRuntimeMeshBuilder(bool bInTangentsHighPrecision, bool bInUVsHighPrecision, int32 bInUVCount, bool bIn32BitIndices)
	: FRuntimeMeshAccessor(&PositionStream, &TangentStream, &UVStream, &ColorStream, &IndexStream)
{
	FRuntimeMeshAccessor::Initialize(bInTangentsHighPrecision, bInUVsHighPrecision, bInUVCount, bIn32BitIndices);
}

FRuntimeMeshBuilder::~FRuntimeMeshBuilder()
{

}


