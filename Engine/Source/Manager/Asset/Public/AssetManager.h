#pragma once

#include "ObjImporter.h"
#include "Component/Mesh/Public/StaticMesh.h"

struct FAABB;

/**
 * @brief 전역의 On-Memory Asset을 관리하는 매니저 클래스
 */
UCLASS()
class UAssetManager
	: public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UAssetManager, UObject)

public:
	void Initialize();
	void Release();

	// Vertex 관련 함수들
	TArray<FNormalVertex>* GetVertexData(EPrimitiveType InType);
	ID3D11Buffer* GetVertexbuffer(EPrimitiveType InType);
	uint32 GetNumVertices(EPrimitiveType InType);

	// Index 관련 함수들
	TArray<uint32>* GetIndexData(EPrimitiveType InType);
	ID3D11Buffer* GetIndexbuffer(EPrimitiveType InType);
	uint32 GetNumIndices(EPrimitiveType InType);

	// Shader 관련 함수들
	ID3D11VertexShader* GetVertexShader(EShaderType Type);
	ID3D11PixelShader* GetPixelShader(EShaderType Type);
	ID3D11InputLayout* GetIputLayout(EShaderType Type);

	// Texture 관련 함수들
	ComPtr<ID3D11ShaderResourceView> LoadTexture(const FName& InFilePath, const FName& InName = FName::GetNone());
	UTexture* CreateTexture(const FName& InFilePath, const FName& InName = FName::GetNone());
	ComPtr<ID3D11ShaderResourceView> GetTexture(const FName& InFilePath);
	void ReleaseTexture(const FName& InFilePath);
	bool HasTexture(const FName& InFilePath) const;

	// StaticMesh 관련 함수
	void LoadAllObjStaticMesh();
	ID3D11Buffer* GetVertexBuffer(FName InObjPath);
	ID3D11Buffer* GetIndexBuffer(FName InObjPath);

	// StaticMesh Cache Accessors
	UStaticMesh* GetStaticMeshFromCache(const FName& InObjPath);
	void AddStaticMeshToCache(const FName& InObjPath, UStaticMesh* InStaticMesh);

	// Create Texture
	static ID3D11ShaderResourceView* CreateTextureFromFile(const path& InFilePath);
	static ID3D11ShaderResourceView* CreateTextureFromMemory(const void* InData, size_t InDataSize);

	// Bounding Box
	FAABB& GetAABB(EPrimitiveType InType);
	FAABB& GetStaticMeshAABB(FName InName);

	const TMap<FName, ID3D11ShaderResourceView*>& GetTextureCache() const;

private:
	// Vertex Resource
	TMap<EPrimitiveType, ID3D11Buffer*> VertexBuffers;
	TMap<EPrimitiveType, uint32> NumVertices;
	TMap<EPrimitiveType, TArray<FNormalVertex>*> VertexDatas;

	// 인덱스 리소스
	TMap<EPrimitiveType, ID3D11Buffer*> IndexBuffers;
	TMap<EPrimitiveType, uint32> NumIndices;
	TMap<EPrimitiveType, TArray<uint32>*> IndexDatas;

	// Shaser Resources
	TMap<EShaderType, ID3D11VertexShader*> VertexShaders;
	TMap<EShaderType, ID3D11InputLayout*> InputLayouts;
	TMap<EShaderType, ID3D11PixelShader*> PixelShaders;

	// Texture Resource
	TMap<FName, ID3D11ShaderResourceView*> TextureCache;

	// StaticMesh Resource
	TMap<FName, std::unique_ptr<UStaticMesh>> StaticMeshCache;
	TMap<FName, ID3D11Buffer*> StaticMeshVertexBuffers;
	TMap<FName, ID3D11Buffer*> StaticMeshIndexBuffers;

	// Release Functions
	void ReleaseAllTextures();

	// Helper Functions
	ID3D11Buffer* CreateVertexBuffer(TArray<FNormalVertex> InVertices);
	ID3D11Buffer* CreateIndexBuffer(TArray<uint32> InIndices);
	FAABB CalculateAABB(const TArray<FNormalVertex>& Vertices);

	// AABB Resource
	TMap<EPrimitiveType, FAABB> AABBs;		// 각 타입별 AABB 저장
	TMap<FName, FAABB> StaticMeshAABBs;	// 스태틱 메시용 AABB 저장
};
