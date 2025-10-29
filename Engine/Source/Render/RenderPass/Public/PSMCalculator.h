#pragma once

#include "PSMBounding.h"
#include "Global/Vector.h"
#include "Global/Matrix.h"
#include "Global/Types.h"
#include <vector>

class UCamera;
class UStaticMeshComponent;

/**
 * @brief Shadow mapping projection types
 */
enum class EShadowProjectionMode : uint8
{
	Uniform = 0,        // Standard orthographic shadow map
	PSM = 1,            // Perspective Shadow Map
	LSPSM = 2,          // Light-Space Perspective Shadow Map
	TSM = 3             // Trapezoidal Shadow Map
};

/**
 * @brief PSM algorithm parameters
 */
struct FPSMParameters
{
	// Virtual camera parameters
	float ZNear = 0.1f;
	float ZFar = 1000.0f;
	float SlideBack = 0.0f;

	// PSM-specific
	float MinInfinityZ = 1.5f;         // Minimum distance to infinity plane
	bool bUnitCubeClip = true;         // Optimize shadow map to actual receivers
	bool bSlideBackEnabled = true;     // Enable slide-back optimization

	// LSPSM-specific
	float LSPSMNoptWeight = 1.0f;      // Weight for Nopt calculation (0-1)

	// TSM-specific
	float TSMDelta = 0.52f;            // Trapezoidal focus parameter (0-1)

	// Computed values (output)
	float PPNear = 0.0f;               // Post-projective near plane
	float PPFar = 1.0f;                // Post-projective far plane
	float CosGamma = 0.0f;             // Angle between light and view direction
	bool bShadowTestInverted = false;  // Light is behind camera
};

/**
 * @brief PSM projection matrix calculator
 *
 * Implements perspective shadow mapping algorithms from
 * PracticalPSM (NVIDIA sample) with FutureEngine coordinate system adaptation.
 */
class FPSMCalculator
{
public:
	/**
	 * @brief Calculate light view-projection matrix using specified algorithm
	 *
	 * @param Mode Shadow projection mode (Uniform, PSM, LSPSM, TSM)
	 * @param OutViewMatrix Output light view matrix
	 * @param OutProjectionMatrix Output light projection matrix
	 * @param LightDirection Directional light direction (world space)
	 * @param Camera Scene camera
	 * @param Meshes Scene meshes for AABB calculation
	 * @param InOutParams PSM parameters (input/output)
	 */
	static void CalculateShadowProjection(
		EShadowProjectionMode Mode,
		FMatrix& OutViewMatrix,
		FMatrix& OutProjectionMatrix,
		const FVector& LightDirection,
		UCamera* Camera,
		const TArray<UStaticMeshComponent*>& Meshes,
		FPSMParameters& InOutParams
	);

private:
	/**
	 * @brief Compute shadow caster/receiver classification
	 * Determines which meshes cast shadows and which receive them
	 */
	static void ComputeVirtualCameraParameters(
		const FVector& LightDirection,
		UCamera* Camera,
		const TArray<UStaticMeshComponent*>& Meshes,
		std::vector<FPSMBoundingBox>& OutShadowCasters,
		std::vector<FPSMBoundingBox>& OutShadowReceivers,
		FPSMParameters& InOutParams
	);

	/**
	 * @brief Build Uniform Shadow Map (Orthographic Projection)
	 */
	static void BuildUniformShadowMap(
		FMatrix& OutView,
		FMatrix& OutProj,
		const FVector& LightDirection,
		UCamera* Camera,
		const std::vector<FPSMBoundingBox>& ShadowCasters,
		const std::vector<FPSMBoundingBox>& ShadowReceivers,
		FPSMParameters& Params
	);

	/**
	 * @brief Build Perspective Shadow Map (PSM)
	 */
	static void BuildPSMProjection(
		FMatrix& OutView,
		FMatrix& OutProj,
		const FVector& LightDirection,
		UCamera* Camera,
		const std::vector<FPSMBoundingBox>& ShadowCasters,
		const std::vector<FPSMBoundingBox>& ShadowReceivers,
		FPSMParameters& Params
	);

	/**
	 * @brief Build Light-Space Perspective Shadow Map (LSPSM)
	 */
	static void BuildLSPSMProjection(
		FMatrix& OutView,
		FMatrix& OutProj,
		const FVector& LightDirection,
		UCamera* Camera,
		const std::vector<FPSMBoundingBox>& ShadowCasters,
		const std::vector<FPSMBoundingBox>& ShadowReceivers,
		FPSMParameters& Params
	);

	/**
	 * @brief Build Trapezoidal Shadow Map (TSM)
	 */
	static void BuildTSMProjection(
		FMatrix& OutView,
		FMatrix& OutProj,
		const FVector& LightDirection,
		UCamera* Camera,
		const std::vector<FPSMBoundingBox>& ShadowCasters,
		const std::vector<FPSMBoundingBox>& ShadowReceivers,
		FPSMParameters& Params
	);
};
