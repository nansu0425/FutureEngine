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
    row_major float4x4 ProjectionInv;
    row_major float4x4 ViewInv;
    row_major float4x4 ViewMatrix;
    float ZNear;
    float ZFar;
    float Aspect;
    float fov;
    uint2 ScreenSlideNum;
    uint ZSlideNum;
    uint LightMaxCountPerCluster;
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
    uint LightIndicesOffset = ClusterIdx * LightMaxCountPerCluster;
    for (int i = 0; i < LightMaxCountPerCluster;i++)
    {
        uint LightIdx = LightIndices[LightIndicesOffset + i];
        if (LightIdx >= 0)
        {
            Color += PointLightInfos[LightIdx].Color;
        }
    }
    
    FAABB CurAABB = ClusterAABB[ClusterIdx];
    float4 WorldMin = mul(float4(CurAABB.Min, 1), ViewInv);
    float4 WorldMax = mul(float4(CurAABB.Max, 1), ViewInv);
    
    Color.w = 1;
    uint VertexOffset = 8 * ClusterIdx + 8;
    ClusterGizmoVertex[0 + VertexOffset].Pos = float3(WorldMin.x, WorldMin.y, WorldMin.z);
    ClusterGizmoVertex[1 + VertexOffset].Pos = float3(WorldMin.x, WorldMax.y, WorldMin.z);
    ClusterGizmoVertex[2 + VertexOffset].Pos = float3(WorldMax.x, WorldMax.y, WorldMin.z);
    ClusterGizmoVertex[3 + VertexOffset].Pos = float3(WorldMax.x, WorldMin.y, WorldMin.z);
    ClusterGizmoVertex[4 + VertexOffset].Pos = float3(WorldMin.x, WorldMin.y, WorldMax.z);
    ClusterGizmoVertex[5 + VertexOffset].Pos = float3(WorldMin.x, WorldMax.y, WorldMax.z);
    ClusterGizmoVertex[6 + VertexOffset].Pos = float3(WorldMax.x, WorldMax.y, WorldMax.z);
    ClusterGizmoVertex[7 + VertexOffset].Pos = float3(WorldMax.x, WorldMin.y, WorldMax.z);
    ClusterGizmoVertex[0 + VertexOffset].Color = Color;
    ClusterGizmoVertex[1 + VertexOffset].Color = Color;
    ClusterGizmoVertex[2 + VertexOffset].Color = Color;
    ClusterGizmoVertex[3 + VertexOffset].Color = Color;
    ClusterGizmoVertex[4 + VertexOffset].Color = Color;
    ClusterGizmoVertex[5 + VertexOffset].Color = Color;
    ClusterGizmoVertex[6 + VertexOffset].Color = Color;
    ClusterGizmoVertex[7 + VertexOffset].Color = Color;
    if(ClusterIdx == 0)
    {
        float tanhalffov = tan(fov * 0.5f * 3.141592f / 180.0f);
        float3 ViewFrustumPos[8];
        ViewFrustumPos[0] = float3(-tanhalffov * ZNear * Aspect, -tanhalffov * ZNear, ZNear);
        ViewFrustumPos[1] = float3(tanhalffov * ZNear * Aspect, -tanhalffov * ZNear, ZNear);
        ViewFrustumPos[2] = float3(tanhalffov * ZNear * Aspect, tanhalffov * ZNear, ZNear);
        ViewFrustumPos[3] = float3(-tanhalffov * ZNear * Aspect, tanhalffov * ZNear, ZNear);
        ViewFrustumPos[4] = float3(-tanhalffov * ZFar * Aspect, -tanhalffov * ZFar, ZFar);
        ViewFrustumPos[5] = float3(tanhalffov * ZFar * Aspect, -tanhalffov * ZFar, ZFar);
        ViewFrustumPos[6] = float3(tanhalffov * ZFar * Aspect, tanhalffov * ZFar, ZFar);
        ViewFrustumPos[7] = float3(-tanhalffov * ZFar * Aspect, tanhalffov * ZFar, ZFar);
        for (int i = 0; i < 8;i++)
        {
            ClusterGizmoVertex[i].Pos = mul(float4(ViewFrustumPos[i], 1), ViewInv);
            ClusterGizmoVertex[i].Color = float4(1, 1, 1, 1);
        }
      
    }

}