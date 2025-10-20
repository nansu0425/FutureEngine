struct FPointLightInfo
{
    float4 Color;
    float3 Position;
    float Intensity;
    float Range;
    float DistanceFalloffExponent;
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
cbuffer FViewClusterInfo : register(b0)
{
    float4x4 ProjectionInv;
    float4x4 ViewInv;
    float4x4 ViewMatrix;
    float ZNear;
    float ZFar;
    uint2 ScreenSlideNum;
    uint ZSlideNum;
    uint LightMaxCountPerCluster;
    float2 padding;
};
cbuffer FLightCountInfo : register(b1)
{
    uint PointLightCount;
    uint SpotLightCount;
    float2 Padding;
};

struct FGizmoVertex
{
    float3 Pos;
    float4 Color;
};
struct FAABB
{
    float3 Min;
    float3 Max;
};

RWStructuredBuffer<FGizmoVertex> ClusterGizmoVertex : register(u0);
StructuredBuffer<FAABB> ClusterAABB : register(t0);
StructuredBuffer<FPointLightInfo> PointLightInfos : register(t1);
StructuredBuffer<FSpotLightInfo> SpotLightInfos : register(t2);
StructuredBuffer<int> LightIndices : register(t3);


[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint ClusterIdx = DTid.x + DTid.y * ScreenSlideNum.x + DTid.z * ScreenSlideNum.x * ScreenSlideNum.y;
    float4 Color = float4(0, 0, 0, 0);
    for (int i = 0; i < LightMaxCountPerCluster;i++)
    {
        uint LightIdx = LightIndices[i];
        if (LightIdx > 0)
        {
            Color += PointLightInfos[LightIdx].Color;
        }
    }
    
    FAABB CurAABB = ClusterAABB[ClusterIdx];
    float4 WorldMin = mul(float4(CurAABB.Min, 1), ViewInv);
    float4 WorldMax = mul(float4(CurAABB.Max, 1), ViewInv);
    
    uint VertexOffset = 24 * ClusterIdx;
    float3 VertexWorld[8];
    VertexWorld[0] = float3(WorldMin.x, WorldMin.y, WorldMin.z);
    VertexWorld[1] = float3(WorldMin.x, WorldMax.y, WorldMin.z);
    VertexWorld[2] = float3(WorldMax.x, WorldMax.y, WorldMin.z);
    VertexWorld[3] = float3(WorldMax.x, WorldMin.y, WorldMin.z);
    VertexWorld[4] = float3(WorldMin.x, WorldMin.y, WorldMax.z);
    VertexWorld[5] = float3(WorldMin.x, WorldMax.y, WorldMax.z);
    VertexWorld[6] = float3(WorldMax.x, WorldMax.y, WorldMax.z);
    VertexWorld[7] = float3(WorldMax.x, WorldMin.y, WorldMax.z);
    
    ClusterGizmoVertex[0].Pos = VertexWorld[0];
    ClusterGizmoVertex[1].Pos = VertexWorld[1];
    ClusterGizmoVertex[2].Pos = VertexWorld[1];
    ClusterGizmoVertex[3].Pos = VertexWorld[2];
    ClusterGizmoVertex[4].Pos = VertexWorld[2];
    ClusterGizmoVertex[5].Pos = VertexWorld[3];
    ClusterGizmoVertex[6].Pos = VertexWorld[3];
    ClusterGizmoVertex[7].Pos = VertexWorld[0];
    ClusterGizmoVertex[8].Pos = VertexWorld[0];
    ClusterGizmoVertex[9].Pos = VertexWorld[4];
    ClusterGizmoVertex[10].Pos = VertexWorld[1];
    ClusterGizmoVertex[11].Pos = VertexWorld[5];
    ClusterGizmoVertex[12].Pos = VertexWorld[2];
    ClusterGizmoVertex[13].Pos = VertexWorld[6];
    ClusterGizmoVertex[14].Pos = VertexWorld[3];
    ClusterGizmoVertex[15].Pos = VertexWorld[7];
    ClusterGizmoVertex[16].Pos = VertexWorld[4];
    ClusterGizmoVertex[17].Pos = VertexWorld[5];
    ClusterGizmoVertex[18].Pos = VertexWorld[5];
    ClusterGizmoVertex[19].Pos = VertexWorld[6];
    ClusterGizmoVertex[20].Pos = VertexWorld[6];
    ClusterGizmoVertex[21].Pos = VertexWorld[7];
    ClusterGizmoVertex[22].Pos = VertexWorld[7];
    ClusterGizmoVertex[23].Pos = VertexWorld[4];
    Color.w = 1;
    for (int i = 0; i < 24;i++)
    {
        ClusterGizmoVertex[i].Color = Color;
    }

}