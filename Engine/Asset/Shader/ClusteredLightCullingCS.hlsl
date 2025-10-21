
struct FPointLightInfo
{
    float4 Color;
    float3 Position;
    float Intensity;
    float Range;
    float DistanceFalloffExponent;
    float2 padding;
};

//StructuredBuffer padding 없어도됨
struct FSpotLightInfo
{
	// Point Light와 공유하는 속성 (필드 순서 맞춤)
    float4 Color;
    float3 Position;
    float Intensity;
    float Range;
    float DistanceFalloffExponent;

	// SpotLight 고유 속성
    float InnerConeAngle;
    float OuterConeAngle;
    float AngleFalloffExponent;
    float3 Direction;
};

struct FAABB
{
    float3 Min;
    float3 Max;
};
cbuffer ViewClusterInfo : register(b0)
{
    row_major float4x4 ProjectionInv;
    row_major float4x4 ViewInv;
    row_major float4x4 ViewMatrix;
    float ZNear;
    float ZFar;
    float Aspect;
    float fov;
};
cbuffer ClusterSliceInfo : register(b1)
{
    uint ClusterSliceNumX;
    uint ClusterSliceNumY;
    uint ClusterSliceNumZ;
    uint LightMaxCountPerCluster;
};
cbuffer LightCountInfo : register(b2)
{
    uint PointLightCount;
    uint SpotLightCount;
    uint2 padding;
}

RWStructuredBuffer<int> LightIndices : register(u0);
StructuredBuffer<FAABB> ClusterAABB : register(t0);
StructuredBuffer<FPointLightInfo> PointLightInfos : register(t1);
StructuredBuffer<FSpotLightInfo> SpotLightInfos : register(t2);

bool IntersectAABBSphere(float3 AABBMin, float3 AABBMax, float3 SphereCenter, float SphereRadius)
{
    float3 BoxCenter = (AABBMin + AABBMax) * 0.5f;
    float3 BoxHalfSize = (AABBMax - AABBMin) * 0.5f;
    float3 ExtensionSize = BoxHalfSize + float3(SphereRadius, SphereRadius, SphereRadius);
    float3 B2L = SphereCenter - BoxCenter; //Box To  Light
    float3 AbsB2L = abs(B2L);
    if(AbsB2L.x > ExtensionSize.x || AbsB2L.y > ExtensionSize.y || AbsB2L.z > ExtensionSize.z)
    {
        return false;
    }
    
    //Over Half Size = 1
    int3 OverAxis = int3(AbsB2L.x > BoxHalfSize.x ? 1 : 0, AbsB2L.y > BoxHalfSize.y ? 1 : 0, AbsB2L.z > BoxHalfSize.z ? 1 : 0);
    int OverCount = OverAxis.x + OverAxis.y + OverAxis.z;
    if(OverCount < 2)
    {
        return true;
    }
    
    float3 SignOverAxis = float3(OverAxis.x * sign(B2L.x), OverAxis.y * sign(B2L.y), OverAxis.z * sign(B2L.z));
    float3 NearBoxPoint = SignOverAxis * BoxHalfSize + BoxCenter;
    NearBoxPoint.x = SignOverAxis.x == 0 ? SphereCenter.x : NearBoxPoint.x;
    NearBoxPoint.y = SignOverAxis.y == 0 ? SphereCenter.y : NearBoxPoint.y;
    NearBoxPoint.z = SignOverAxis.z == 0 ? SphereCenter.z : NearBoxPoint.z;
    
    float3 P2L = SphereCenter - NearBoxPoint; //NearBoxPoint To Light
    float DisSquare = dot(P2L, P2L);
    return DisSquare < SphereRadius * SphereRadius;
}

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint ClusterIdx = DTid.x + DTid.y * ClusterSliceNumX + DTid.z * ClusterSliceNumX * ClusterSliceNumY;
    FAABB CurAABB = ClusterAABB[ClusterIdx];
    
    int LightIndicesOffset = LightMaxCountPerCluster * ClusterIdx;
    uint IncludeLightCount = 0;
    for (int i = 0; (i < PointLightCount)  && (IncludeLightCount < LightMaxCountPerCluster); i++)
    {
        FPointLightInfo PointLightInfo = PointLightInfos[i];
        float4 LightViewPos = mul(float4(PointLightInfo.Position, 1), ViewMatrix);
        if (IntersectAABBSphere(CurAABB.Min, CurAABB.Max, LightViewPos.xyz, PointLightInfo.Range))
        {
            LightIndices[LightIndicesOffset + IncludeLightCount] = i;
            IncludeLightCount++;
        }
    }    
    
    for (uint i = IncludeLightCount; i < LightMaxCountPerCluster;i++)
    {
        LightIndices[LightIndicesOffset + i] = -1;

    }
    
    
    //if (ClusterIdx == 0)
    //{
    //    LightIndices[0] = LightIndicesOffset;
    //    LightIndices[1] = IncludeLightCount;
    //    LightIndices[2] = CurAABB.Min.x;
    //    LightIndices[3] = CurAABB.Min.y;
    //    LightIndices[4] = CurAABB.Min.z;
    //    LightIndices[5] = CurAABB.Max.x;
    //    LightIndices[6] = CurAABB.Max.y;
    //    LightIndices[7] = CurAABB.Max.z;
    //    if (PointLightCount > 0)
    //    {
    //        FPointLightInfo PointLightInfo = PointLightInfos[0];
    //        float4 LightViewPos = mul(float4(PointLightInfo.Position, 1), ViewMatrix);
    //        LightIndices[8] = LightViewPos.x;
    //        LightIndices[9] = LightViewPos.y;
    //        LightIndices[10] = LightViewPos.z;
    //        LightIndices[11] = LightViewPos.w;
    //    }

    //}
}