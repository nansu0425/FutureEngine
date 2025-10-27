#pragma once

#include <wrl.h>

#include "Global/Types.h"

struct ID3D11ComputeShader;
struct ID3D11ShaderResourceView;
struct ID3D11Texture2D;
struct ID3D11UnorderedAccessView;

/**
 * @class FTextureFilter
 * @brief 텍스처에 분리 가능한(separable) 필터를 적용하는 유틸리티 클래스이다.
 *
 * @note 이 클래스는 Row/Column 컴퓨트 셰이더를 사용한 2-pass 필터링(예: 가우시안 블러)을 수행한다.
 * 내부적으로 임시 텍스처를 관리하여 필터링 패스 간 더블 버퍼링을 자동화한다.
 */
class FTextureFilter
{
public:
    /**
     * @note 셰이더 파일은 특정 전처리기 지시자(SCAN_DIRECTION_COLUMN)를 정의하여
     * Row 셰이더와 Column 셰이더를 각각 정의해야 한다.
     */
    FTextureFilter(const FString& InShaderPath);

    ~FTextureFilter();

    /**
     * @brief 텍스처의 특정 영역에 2-pass 분리형 필터를 적용한다.
     * @details
     * 1. 가로(Row) 패스: 입력 텍스처(SRV) -> 내부 임시 텍스처(UAV)
     * 2. 세로(Column) 패스: 내부 임시 텍스처(SRV) -> 출력 텍스처(UAV)
     *
     * 필터링은 (RegionStartX, RegionStartY)에서 시작하는 (RegionWidth, RegionHeight)
     * 크기의 사각 영역에 대해서만 수행된다.
     *
     * @note
     * - 입력 텍스처와 출력 텍스처의 '전체' 크기(너비/높이)는 동일해야 한다.
     * - 내부 임시 텍스처는 입력 텍스처의 '전체' 크기에 맞춰 필요에 따라 자동으로 리사이즈된다.
     *
     * @param InTexture 필터링할 원본 텍스처 아틀라스의 셰이더 리소스 뷰 (SRV).
     * @param OutTexture 필터링 결과를 저장할 텍스처 아틀라스의 순서 없는 액세스 뷰 (UAV).
     * @param ThreadGroupCountX 디스패치할 X축 스레드 그룹 수 (영역 너비 기준).
     * @param ThreadGroupCountY 디스패치할 Y축 스레드 그룹 수 (영역 높이 기준).
     * @param ThreadGroupCountZ 디스패치할 Z축 스레드 그룹 수 (보통 1).
     * @param RegionStartX 필터링할 영역의 시작 X 좌표.
     * @param RegionStartY 필터링할 영역의 시작 Y 좌표.
     * @param RegionWidth 필터링할 영역의 너비.
     * @param RegionHeight 필터링할 영역의 높이.
     */
    void FilterTexture(
        ID3D11ShaderResourceView* InTexture,
        ID3D11UnorderedAccessView* OutTexture,
        uint32 ThreadGroupCountX,
        uint32 ThreadGroupCountY,
        uint32 ThreadGroupCountZ,
        uint32 RegionStartX,
        uint32 RegionStartY,
        uint32 RegionWidth,
        uint32 RegionHeight
    );

    /**
     * @brief 텍스처의 특정 영역에 2-pass 분리형 필터를 적용한다 (Row/Column 단위 특화).
     * @details
     * 이 오버로드는 Summed Area Table (SAT) 생성과 같이 아틀라스의 특정 영역 내에서
     * Row/Column 전체를 스캔하는 방식의 필터링에 사용된다.
     *
     * 1. 첫 번째 패스 (예: Column 스캔): `Dispatch(1, ThreadGroupCount, 1)`을 호출한다.
     * 2. 두 번째 패스 (예: Row 스캔): `Dispatch(ThreadGroupCount, 1, 1)`을 호출한다.
     *
     * @note
     * - 입력 텍스처와 출력 텍스처의 '전체' 크기(너비/높이)는 동일해야 한다.
     * - 내부 임시 텍스처는 입력 텍스처의 '전체' 크기에 맞춰 필요에 따라 자동으로 리사이즈된다.
     * - 'ThreadGroupCount'는 첫 번째 패스의 Y축 그룹 수(영역 높이 기준)와
     * 두 번째 패스의 X축 그룹 수(영역 너비 기준)로 공통적으로 사용된다고 가정한다.
     *
     * @param InTexture 필터링할 원본 텍스처 아틀라스의 셰이더 리소스 뷰 (SRV).
     * @param OutTexture 필터링 결과를 저장할 텍스처 아틀라스의 순서 없는 액세스 뷰 (UAV).
     * @param ThreadGroupCount 각 패스(Column/Row)에서 디스패치할 스레드 그룹 수.
     * @param RegionStartX 필터링할 영역의 시작 X 좌표.
     * @param RegionStartY 필터링할 영역의 시작 Y 좌표.
     * @param RegionWidth 필터링할 영역의 너비.
     * @param RegionHeight 필터링할 영역의 높이.
     */
    void FilterTexture(
        ID3D11ShaderResourceView* InTexture,
        ID3D11UnorderedAccessView* OutTexture,
        uint32 ThreadGroupCount,
        uint32 RegionStartX,
        uint32 RegionStartY,
        uint32 RegionWidth,
        uint32 RegionHeight
    );

private:
    /**
     * @brief 지정된 경로의 셰이더 파일로부터 Row/Column 컴퓨트 셰이더를 생성한다.
     * @note 셰이더 파일은 특정 전처리기 지시자(SCAN_DIRECTION_COLUMN)를 정의해야한다.
     */
    void CreateShader(const FString& InShaderPath);

    /**
     * @brief 텍스쳐 필터링에 사용되는 상수 버퍼를 생성한다.
     */
    void CreateConstantBuffer();

    /**
     * @brief 2-pass 필터링에 사용할 내부 임시 텍스처를 생성한다.
     */
    void CreateTexture(uint32 InWidth, uint32 InHeight);

    /**
     * @brief 내부 임시 텍스처의 크기를 조절한다.
     * @details 현재 텍스처 크기가 새로운 크기와 다를 경우, 기존 텍스처를 해제하고 새로 생성한다.
     */
    void ResizeTexture(uint32 InWidth, uint32 InHeight);

    // 임시 텍스처 생성 시 사용되는 기본 너비
    static constexpr uint32 DEFAULT_TEXTURE_WIDTH = 1024;
    
    // 임시 텍스처 생성 시 사용되는 기본 높이
    static constexpr uint32 DEFAULT_TEXTURE_HEIGHT = 1024;

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> ComputeShaderRow;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> ComputeShaderColumn;

    struct FTextureInfo
    {
        uint32 StartX;
        uint32 StartY;
        uint32 RegionWidth;
        uint32 RegionHeight;
        uint32 TextureWidth;
        uint32 TextureHeight;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> TextureInfoConstantBuffer;

    // Double Buffering용 임시 버퍼
    // @note R32G32 format만 지원한다. (VSM의 1차 모멘트, 2차 모멘트 저장용)
    // @todo 이후에 추상화를 통해서 다양한 텍스쳐 형식을 지원해야한다.
    
    Microsoft::WRL::ComPtr<ID3D11Texture2D> TemporaryTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TemporarySRV;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> TemporaryUAV;
};