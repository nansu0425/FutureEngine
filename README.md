# Week5+ Team1(국동희, 박영빈, 최진혁, 홍신화) 기술문서

## 1. 개요
본 문서는 Week5+의 주요 주제인 Component Pattern 아키텍처와 Play In Editor(PIE) 시스템의 구현 원리를 설명합니다.

---

## 2. 핵심 클래스 구조

### 2.1 UObject - 기본 오브젝트 시스템

**특징:**
- 모든 언리얼 오브젝트의 기본 클래스
- Duplicate 함수를 통해 객체 복제 지원
- 일반 오브젝트 참조는 포인터만 복사 (얕은 복사)
- SubObject는 소유 관계에 있으므로 깊은 복사 수행

### 2.2 EWorldType - 월드 타입 구분

언리얼 엔진은 다음 월드 타입을 구분합니다:
- **Editor**: 에디터 전용 월드
- **PIE**: Play In Editor 월드

### 2.3 UWorld - 게임 월드 관리

**역할:**
- 레벨과 액터들의 최상위 컨테이너
- WorldType에 따라 다른 동작 수행
- 모든 액터의 Tick 관리
- Level 오브젝트를 소유하며 게임 루프 제어

### 2.4 AActor - 게임 오브젝트

**역할:**
- Component들의 컨테이너
- Component Pattern의 핵심 구현체
- OwnedComponents 집합으로 컴포넌트 소유
- RootComponent를 통한 계층 구조 관리
- bTickInEditor 플래그로 에디터 모드 동작 제어
- 자신이 소유한 모든 Component의 생명주기 관리

---

## 3. Component Pattern 구조

### 3.1 설계 원칙

**Actor = 컨테이너 + Component 조합**

Actor는 직접 기능을 구현하지 않고, Component를 조합하여 기능을 구성합니다:
- USceneComponent: Transform 정보 제공 (RootComponent)
- UStaticMeshComponent: 3D 메시 렌더링
- UTextRenderComponent: 텍스트 렌더링
- UBillboardComponent: 에디터 시각화

### 3.2 장점

- **재사용성**: Component를 다른 Actor에서도 사용 가능
- **확장성**: 새로운 기능을 Component로 추가
- **유지보수**: 기능별로 분리되어 관리 용이
- **모듈화**: 독립적인 기능 단위로 개발 가능

---

## 4. Object Duplication 메커니즘

### 4.1 복사 방식 비교

| 구분 | 얕은 복사 (Shallow Copy) | 깊은 복사 (Deep Copy) |
|------|------------------------|---------------------|
| 대상 | 일반 오브젝트 참조 | SubObject (소유 관계) |
| 방식 | 포인터만 복사 | 새 인스턴스 생성 |
| 특징 | 원본과 데이터 공유 | 완전히 독립적인 복사본 |
| 메모리 | 추가 메모리 불필요 | 새 메모리 할당 |

### 4.2 구현 흐름

복제 프로세스는 다음 단계로 진행됩니다:

1. **새 객체 생성**
2. **DuplicateSubObjects 호출**: SubObject 식별 및 복제
3. **재귀적 깊은 복사**: SubObject들의 Duplicate를 호출해 재귀적 깊은 복사
4. **포인터 갱신**: 복제된 SubObject를 새 포인터로 연결

**핵심 원칙:** 
- 공유 가능한 데이터는 얕은 복사
- 소유 관계의 SubObject는 깊은 복사
- 가상 함수를 통해 하위 클래스에서 커스터마이징 가능

---

## 5. Play In Editor (PIE) 시스템

### 5.1 UEditorEngine의 Tick 처리

UEditorEngine은 여러 WorldContext를 관리하며, 각 월드의 타입에 따라 다르게 처리합니다:

**Editor 모드:**
- bTickInEditor가 true인 액터만 Tick 실행
- 에디터에서 미리보기가 필요한 특수 액터만 동작
- 일반 게임플레이 로직은 실행되지 않음

**PIE 모드:**
- 모든 활성화된 액터가 Tick 실행
- 실제 게임과 동일한 동작
- BeginPlay, EndPlay 등 생명주기 이벤트 호출

### 5.2 PIE 시작 프로세스

PIE는 다음 단계로 시작됩니다:

1. **에디터 월드 참조**: 현재 편집 중인 EditorWorld 가져오기
2. **PIE 월드 복제**: DuplicateWorldForPIE를 통해 완전한 복사본 생성
3. **글로벌 컨텍스트 전환**: GWorld 포인터를 PIEWorld로 변경
4. **액터 초기화**: InitializeActorsForPlay 호출로 모든 액터의 BeginPlay 실행

**핵심 메커니즘:**
- EditorWorld는 원본으로 유지되며 변경되지 않음
- PIEWorld는 완전히 독립적인 복제본
- 두 월드는 메모리상에서 동시에 존재
- PIE 중 발생하는 모든 변경사항은 PIEWorld에만 영향

### 5.3 PIE 종료 프로세스

PIE 종료 시 다음과 같이 처리됩니다:

1. **PIE 월드 정리**: CleanupWorld로 모든 액터의 EndPlay 호출
2. **리소스 해제**: PIEWorld와 관련된 모든 오브젝트 삭제
3. **에디터 복귀**: GWorld를 다시 EditorWorld로 전환
4. **메모리 정리**: PIE 전용 오브젝트 메모리 해제

**동작 원리:**
- PIE 중 생성된 모든 액터와 Component는 자동 정리
- 에디터 월드는 PIE 시작 전 상태 그대로 유지
- 원본 데이터 손상 없이 안전한 테스트 환경 제공

---

## 6. 전체 구조 요약

```
UEditorEngine
    ├── EditorWorld (WorldType::Editor)
    │       └── Level
    │           └── Actors (bTickInEditor=true만 Tick)
    │
    └── PIEWorld (WorldType::PIE)
            └── Level (EditorWorld의 복제본)
                └── Actors (모두 Tick)
```

### 6.1 핵심 원리

1. **Component Pattern**: Actor는 기능의 조합체로 구성
2. **Object Duplication**: SubObject는 깊은 복사로 독립성 유지
3. **PIE 격리**: EditorWorld와 PIEWorld는 완전 분리
4. **생명주기 관리**: BeginPlay → Tick → EndPlay 순서로 실행

### 6.2 데이터 흐름

**에디터 모드:**
- 사용자 편집 → EditorWorld 수정 → 저장 → 디스크 반영

**PIE 모드:**
- EditorWorld 복제 → PIEWorld 생성 → 게임 실행 → 종료 시 PIEWorld 삭제


