

//inputlayout 만들어 놨으나 안쓴다. 주의 GizmoInputLayout

struct FGizmoVertex
{
    float3 Pos;
    float4 Color;
};
cbuffer PerFrame : register(b1)
{
    row_major float4x4 View; // View Matrix Calculation of MVP Matrix
    row_major float4x4 Projection; // Projection Matrix Calculation of MVP Matrix
};


StructuredBuffer<FGizmoVertex> ClusterGizmoVertex : register(t0);


struct PS_INPUT
{
    float4 Position : SV_POSITION; // Transformed position to pass to the pixel shader
    float4 Color : COLOR; // Transformed position to pass to the pixel shader
};

PS_INPUT mainVS(uint id : SV_VertexID)
{
    FGizmoVertex Vertex = ClusterGizmoVertex[id];
    PS_INPUT output;
    float4 tmp = float4(Vertex.Pos, 1);
    tmp = mul(tmp, View);
    tmp = mul(tmp, Projection);
	
    output.Position = tmp;
    output.Color = Vertex.Color;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    return input.Color;
}
