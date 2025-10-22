#define DEGREE_TO_RADIAN (3.141592f / 180.0f)

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
    uint SpotLightIntersectOption;
    uint3 ClusterSliceInfo_padding;
};
cbuffer LightCountInfo : register(b2)
{
    uint PointLightCount;
    uint SpotLightCount;
    uint2 padding;
}