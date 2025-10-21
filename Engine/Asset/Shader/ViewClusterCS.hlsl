
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
struct FAABB
{
    float3 Min;
    float3 Max;
};

RWStructuredBuffer<FAABB> ClusterAABB : register(u0);

float GetZ(uint ZIdx)
{
    return ZNear * pow((ZFar / ZNear), (float) ZIdx / ClusterSliceNumZ);
}
float3 NDCToView(float3 NDC)
{
    float4 View = mul(float4(NDC, 1), ProjectionInv);
    return View.xyz / View.w;
}

//dir not Normalized
//eye = (0,0,0)
float3 LinearIntersectionToZPlane(float3 dir, float zDis)
{
    float t = zDis / dir.z;
    return t * dir;
}
[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    uint2 ScreenIdx = DTid.xy;
    float2 ScreenSlideNumRCP = (float) 1 / uint2(ClusterSliceNumX, ClusterSliceNumY);
    float2 ScreenMin = ScreenIdx * ScreenSlideNumRCP;
    float2 ScreenMax = (ScreenIdx + int2(1, 1)) * ScreenSlideNumRCP;
    float2 NearNDCMin = ScreenMin * 2 - float2(1, 1);
    float2 NearNDCMax = ScreenMax * 2 - float2(1, 1);
    
    //NearPlane In NDC = 0
    float3 ViewMin = NDCToView(float3(NearNDCMin, 0));
    float3 ViewMax = NDCToView(float3(NearNDCMax, 0));
    
    float MinZ = GetZ(DTid.z);
    float MaxZ = GetZ(DTid.z + 1);
    
    float3 NearViewMin = LinearIntersectionToZPlane(ViewMin, MinZ);
    float3 NearViewMax = LinearIntersectionToZPlane(ViewMax, MinZ);
    float3 FarViewMin = LinearIntersectionToZPlane(ViewMin, MaxZ);
    float3 FarViewMax = LinearIntersectionToZPlane(ViewMax, MaxZ);
    
    float3 AABBMin = min(min(NearViewMin, NearViewMax), min(FarViewMin, FarViewMax));
    float3 AABBMax = max(max(NearViewMin, NearViewMax), max(FarViewMin, FarViewMax));
    
    uint ClusterIdx = DTid.x + DTid.y * ClusterSliceNumX + DTid.z * ClusterSliceNumX * ClusterSliceNumY;
    ClusterAABB[ClusterIdx].Min = AABBMin;
    ClusterAABB[ClusterIdx].Max = AABBMax;
}