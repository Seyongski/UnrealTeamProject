# Mordoom Arena Floor System

## 한 줄 요약

Mordoom 보스전 아레나 바닥은 `BP_MordoomFloorPiece` 조각들로 구성되어 있고,  
`BP_MordoomArenaManager`가 `PieceID`를 기준으로 원하는 조각을 무너뜨리거나 복구한다.

```text
Boss / GAS / DataTable
        ↓
BP_MordoomArenaManager
        ↓
PieceID 찾기
        ↓
BP_MordoomFloorPiece.BreakFloor()
```

비유하면:

```text
FloorPiece = 번호 붙은 발판
ArenaManager = 발판 관리자
GAS/보스 패턴 = 관리자에게 명령하는 쪽
```

---

## 1. 만든 Blueprint 구조

### BP_MordoomFloorPiece

아레나 바닥 조각 1개를 담당한다.  
바닥 메시와 기둥 메시가 한 Blueprint 안에 같이 들어 있다.

```text
BP_MordoomFloorPiece
├─ SharedRoot
├─ FloorMesh
├─ PillarMesh
└─ PieceID
```

주요 변수:

```text
PieceID
Type: Integer
Instance Editable: true
```

```text
StartLocation
Type: Vector
```

`StartLocation`은 BeginPlay에서 저장한다.

```text
Event BeginPlay
  ↓
Get Actor Location
  ↓
Set StartLocation
```

---

## 2. 바닥 조각 기능

### BreakFloor

바닥 조각을 무너뜨리는 이벤트다.

역할:

```text
BreakFloor()
  ↓
Collision 끄기
  ↓
Timeline으로 아래로 이동
  ↓
숨기기
```

현재 의도:

```text
FloorMesh Collision: No Collision
PillarMesh Collision: No Collision
Actor Collision: false
Actor Hidden In Game: true
```

중요:

```text
Auto Receive Input = Disabled
```

바닥 조각은 키보드 입력을 직접 받지 않는다.  
테스트할 때만 임시로 `Player 0`을 썼고, 최종 구조에서는 꺼둔다.

---

### RestoreFloor

바닥 조각을 원래 상태로 되돌리는 이벤트다.

역할:

```text
RestoreFloor()
  ↓
Actor Hidden In Game false
  ↓
Set Actor Location StartLocation
  ↓
Actor Collision true
  ↓
FloorMesh Collision Query and Physics
  ↓
PillarMesh Collision Query and Physics
```

레이드 리셋, 보스전 재시작, 테스트 복구용으로 사용한다.

---

## 3. PieceID 규칙

각 바닥 조각은 `PieceID`로 구분한다.

```text
진짜 로직 기준 = PieceID
Outliner 이름 = 사람이 보기 편한 이름
```

추천 번호 배치:

```text
          [1]
     [8]       [2]

  [7]             [3]

     [6]       [4]
          [5]
```

예시:

```text
FloorPiece_01 → PieceID 1
FloorPiece_02 → PieceID 2
FloorPiece_03 → PieceID 3
FloorPiece_04 → PieceID 4
FloorPiece_05 → PieceID 5
FloorPiece_06 → PieceID 6
FloorPiece_07 → PieceID 7
FloorPiece_08 → PieceID 8
```

---

## 4. BP_MordoomArenaManager

바닥 조각들을 관리하는 Actor Blueprint다.  
레벨에 1개만 배치한다.

```text
BP_MordoomArenaManager
├─ FloorPieces
├─ BreakPiece(TargetPieceID)
├─ BreakPieces(TargetPieceIDs)
└─ RestoreAllPieces()
```

### BeginPlay

게임 시작 시 레벨에 있는 모든 바닥 조각을 찾아 저장한다.

```text
Event BeginPlay
  ↓
Get All Actors Of Class (BP_MordoomFloorPiece)
  ↓
Set FloorPieces
```

테스트 결과:

```text
FloorPieces 개수 8개 정상 확인
```

---

## 5. Manager 함수

### BreakPiece

특정 `PieceID` 하나를 찾아 무너뜨린다.

입력:

```text
TargetPieceID: Integer
```

구조:

```text
BreakPiece(TargetPieceID)
  ↓
ForEach FloorPieces
  ↓
각 조각의 PieceID 확인
  ↓
PieceID == TargetPieceID ?
  ↓ True
  ↓
BreakFloor 호출
```

예:

```text
BreakPiece(5)
→ PieceID 5번 바닥 조각 무너짐
```

---

### BreakPieces

여러 `PieceID`를 받아서 여러 조각을 한 번에 무너뜨린다.

입력:

```text
TargetPieceIDs: Integer Array
```

구조:

```text
BreakPieces([4, 5])
  ↓
ForEach TargetPieceIDs
  ↓
BreakPiece(Array Element)
```

예:

```text
BreakPieces([2, 4, 6])
→ 2번, 4번, 6번 바닥 조각 무너짐
```

테스트 결과:

```text
BreakPieces([4, 5]) 정상 작동 확인
```

---

### RestoreAllPieces

모든 바닥 조각을 복구한다.

구조:

```text
RestoreAllPieces()
  ↓
ForEach FloorPieces
  ↓
RestoreFloor 호출
```

테스트 결과:

```text
무너진 조각 복구 정상 확인
```

---

## 6. 테스트한 내용

완료된 테스트:

```text
1. BP_MordoomFloorPiece 단일 조각 BreakFloor 테스트
2. PieceID 출력 테스트
3. 8개 조각 PieceID 부여
4. BP_MordoomArenaManager가 조각 8개 수집하는지 확인
5. BreakPiece(특정 ID) 테스트
6. BreakPieces([4, 5]) 테스트
7. RestoreAllPieces() 테스트
```

최종 정리:

```text
모든 FloorPiece Auto Receive Input = Disabled
Manager Event Graph는 BeginPlay 수집만 유지
테스트용 Delay / Print / Keyboard Input은 제거
```

---

## 7. GAS / 보스 패턴에서 호출하는 방법

GAS나 보스 패턴 쪽에서는 바닥 조각을 직접 만지지 말고,  
`BP_MordoomArenaManager`를 통해 호출하면 된다.

권장 흐름:

```text
GA_Boss_FloorBreakPattern
  ↓
Get BP_MordoomArenaManager
  ↓
BreakPieces(TargetPieceIDs)
```

예시 패턴:

```text
Phase 1
BreakPieces([3])

Phase 2
BreakPieces([3, 5])

Enrage
BreakPieces([2, 4, 6, 8])
```

DataTable에서 관리한다면:

```text
PatternName: Cross_01
TargetPieceIDs: [2, 4, 6, 8]

PatternName: Half_01
TargetPieceIDs: [1, 2, 3, 4]

PatternName: RandomPair_01
TargetPieceIDs: [4, 5]
```

GAS 쪽 의사 흐름:

```text
Ability Activate
  ↓
DataTable에서 TargetPieceIDs 읽기
  ↓
ArenaManager 찾기
  ↓
ArenaManager.BreakPieces(TargetPieceIDs)
```

복구가 필요할 때:

```text
ArenaManager.RestoreAllPieces()
```

---

## 8. 전달사항

### 바닥 쪽에서 보장하는 것

```text
BreakFloor 호출 시 해당 PieceID 바닥 조각이 무너짐
RestoreFloor 호출 시 해당 조각이 복구됨
RestoreAllPieces 호출 시 전체 복구됨
```

### 캐릭터 낙하 관련 이슈

바닥 충돌은 꺼지지만, 캐릭터 Movement 쪽에 절벽 낙하 방지 설정이 있으면  
일부 방향에서 바로 떨어지지 않을 수 있다.

확인 필요 항목:

```text
Character Movement
Can Walk Off Ledges
Max Step Height
Walkable Floor Angle
커스텀 절벽/낙하 방지 로직
```

팀 공유 문장:

```text
FloorPiece 쪽 Collision Off와 Break/Restore는 정상 작동 확인됨.
다만 캐릭터가 절벽에서 떨어지지 않게 막는 Movement 로직이 있으면
레이드 낙하 기믹 구간에서는 예외 처리가 필요할 수 있음.
```

---

## 9. Manager 테스트 연결 방법

`BP_MordoomArenaManager`에서 테스트할 때는 `Event Graph`의 `Set FloorPieces` 뒤에 임시 노드를 이어 붙이면 된다.

기본 상태:

```text
Event BeginPlay
  ↓
Get All Actors Of Class (BP_MordoomFloorPiece)
  ↓
Set FloorPieces
```

### 여러 조각 무너뜨리기 테스트

`Set FloorPieces` 뒤에 이렇게 연결한다.

```text
Set FloorPieces
  ↓
Delay 1.0
  ↓
BreakPieces
```

`BreakPieces`의 `TargetPieceIDs`에는 `Make Array`를 연결한다.

예:

```text
Make Array
├─ 4
└─ 5

BreakPieces(TargetPieceIDs = [4, 5])
```

전체 그림:

```text
Event BeginPlay
  ↓
Get All Actors Of Class
  ↓
Set FloorPieces
  ↓
Delay 1.0
  ↓
BreakPieces([4, 5])
```

결과:

```text
게임 시작
  ↓
1초 뒤
  ↓
4번, 5번 바닥 조각 무너짐
```

### 복구까지 테스트

무너진 뒤 다시 복구하려면 `BreakPieces` 뒤에 `Delay`와 `RestoreAllPieces`를 붙인다.

```text
Event BeginPlay
  ↓
Get All Actors Of Class
  ↓
Set FloorPieces
  ↓
Delay 1.0
  ↓
BreakPieces([4, 5])
  ↓
Delay 3.0
  ↓
RestoreAllPieces
```

결과:

```text
게임 시작
  ↓
1초 뒤 4번, 5번 무너짐
  ↓
3초 뒤 전체 바닥 복구
```

### 테스트 후 정리

테스트가 끝나면 Manager의 Event Graph는 다시 아래 상태만 남긴다.

```text
Event BeginPlay
  ↓
Get All Actors Of Class
  ↓
Set FloorPieces
```

삭제할 테스트 노드:

```text
Delay
Make Array
BreakPieces 테스트 호출
RestoreAllPieces 테스트 호출
Print String
```

### BP_MordoomFloorPiece의 1번 키 테스트 주의

현재 `BP_MordoomFloorPiece` 안에 `1번 키 → BreakFloor` 테스트가 남아 있다면, 테스트 중 헷갈릴 수 있다.

특히 여러 바닥 조각의 `Auto Receive Input`이 `Player 0`이면:

```text
1번 키
  ↓
여러 FloorPiece가 동시에 입력을 받음
  ↓
여러 조각이 같이 무너짐
```

그래서 Manager 테스트를 할 때는 권장 상태가 이렇다.

```text
모든 BP_MordoomFloorPiece
Auto Receive Input = Disabled
```

그리고 최종 제출 전에는 `BP_MordoomFloorPiece` 안의 테스트용 키 입력 노드를 제거한다.

최종 `BP_MordoomFloorPiece`에는 이것만 남기는 것을 권장한다.

```text
BeginPlay
BreakFloor
RestoreFloor
PieceID
StartLocation
```

---

## 10. 최종 구조 요약

```text
BP_MordoomFloorPiece
├─ PieceID
├─ StartLocation
├─ BreakFloor()
└─ RestoreFloor()

BP_MordoomArenaManager
├─ BeginPlay: FloorPieces 수집
├─ BreakPiece(ID)
├─ BreakPieces(ID Array)
└─ RestoreAllPieces()

Boss / GAS / DataTable
└─ ArenaManager.BreakPieces([ID 목록]) 호출
```

한 줄 결론:

```text
맵 담당자는 번호 붙은 바닥 조각과 Manager 호출 함수를 준비했다.
보스/GAS 담당자는 원하는 타이밍에 Manager의 BreakPieces 또는 RestoreAllPieces를 호출하면 된다.
```

---

## 11. 보스 파괴 파이프라인과 연동 (실제 연결)

> 이 절은 위에서 만든 바닥 시스템을, 이미 C++ 로 완성돼 있는 **보스 지형파괴 파이프라인**에
> 실제로 물리는 방법이다. 핵심: 보스/GAS 는 `BreakPieces` 를 직접 부르지 않는다.
> 보스는 이미 노티파이로 "슬라이스 파괴"를 방송하고 있고, 바닥은 그 방송을 듣고 스스로 무너진다.

### 이미 되어 있는 것 (C++, 손댈 필요 없음)

```text
보스 몽타주 노티파이
  AnimNotify_BossGimmickDestroySlice   (Delay 후 파괴 예약)
        ↓
  UBossTerrainGimmickComponent.RequestDestroyGimmickSlice
        ↓
  ABossRaidGameMode.DestroySlice(SliceIndex)
        ↓
  ABossRaidGameState.MarkSliceDestroyed
        → DestroyedSliceMask 비트 세팅(복제) + OnArenaSlicesChanged 방송(서버+클라)
```

- 파괴 대상은 **PieceID 가 아니라 SliceIndex(0~7)** 로 표현된다.
- 슬라이스 규약: **+X 축 방향 = 0, 반시계 방향, 한 조각 = 360 / SliceCount(=8) 도.**
- 방송(`OnArenaSlicesChanged`)은 서버·클라 모두에서 발동한다 → 각 머신이 자기 바닥을 로컬로 처리.

### 새로 추가한 다리 (C++, 이미 커밋됨)

`UArenaFloorBridgeComponent`
( `LostArk/Source/LostArk/Monster/ArenaFloorBridgeComponent.h/.cpp` )

- `OnArenaSlicesChanged` 를 대신 듣고, **새로 파괴된 SliceIndex 를 한 번씩** BP 로 넘긴다.
- 노출 멤버:
  - `OnSliceDestroyed(int32 SliceIndex)` — BlueprintImplementableEvent. 파괴될 때 슬라이스 번호 전달.
  - `GetSliceIndexForLocation(Vector) → int32` — 바닥 조각 위치로 슬라이스 번호를 역산(매핑표 불필요).
  - `IsSliceDestroyed(int32) → bool` — 직접 조회용.
- 늦은 접속/리로드/멀티플레이 안전(바인드 재시도 + 슬라이스당 1회 발동)까지 C++ 안에서 처리.

### 맵 담당자가 할 일 (BP, 여기만 하면 끝)

1. **엔진 재빌드** — 새 C++ 파일이 추가됐으니 에디터에서 C++ 컴파일(또는 IDE 빌드) 1회.

2. `BP_MordoomArenaManager` 에 **Add Component → Arena Floor Bridge** 추가.

3. 컴포넌트의 **`OnSliceDestroyed`** 이벤트를 구현. 매핑표 없이 위치로 대응:

   ```text
   Event OnSliceDestroyed (SliceIndex)
     ↓
   ForEach FloorPieces
     ↓
   Target = GetSliceIndexForLocation(  조각의 월드 위치  )   // 브리지 컴포넌트 함수
     ↓
   Target == SliceIndex ?
     ↓ True
   조각.BreakFloor()
   ```

   - "조각의 월드 위치" 는 조각 피벗이 아레나 중심이 아니라 **조각 자기 위치(FloorMesh 월드 위치)** 여야 한다.
     (BP_MordoomFloorPiece 피벗이 링 위 자기 자리에 있으면 GetActorLocation 그대로 사용 가능.)
   - 하드매핑을 선호하면 대신 SliceIndex→PieceID 표를 만들어 `BreakPiece(PieceID)` 를 불러도 된다.
     단, **아래 정렬(4번)** 이 맞아야 한다.

4. **정렬 맞추기 (가장 중요).** 보스가 부르는 SliceIndex 와 바닥 위치가 어긋나면 엉뚱한 조각이 무너진다.
   - `SliceCount` 기본값이 **4** 로 바뀌었다 (십자 사분면 = 바닥 조각 8개의 절반 단위).
     `BossRaidGameState`(GameState BP) 에서 별도로 8을 세팅해뒀다면 4로 되돌릴 것.
     `ArenaFloorZ = 실제 바닥 Z` 도 함께 확인.
   - `ArenaCenter` 는 GameMode 가 조우 시작 시 자동 세팅한다. 단, **`AArenaSliceActor` 를 레벨에 안 뒀다면**
     중심이 보스 위치로 폴백되니, 아레나 정중앙에 `AArenaSliceActor` 하나(혹은 중심 기준점)를 두는 것을 권장.

5. **파괴 대상은 이제 몽타주가 직접 지정한다 (자동 순번 DestructionOrder 제거됨).**
   라운드마다 `AN_SpawnTower` 노티파이의 **`TargetSlice`(0~SliceCount-1)** 로 이번에 부술 슬라이스를 박는다.
   이 한 값이 바라보기(FaceSlice)·파괴(DestroySlice)·타워 제외를 모두 결정한다.
   - 보스 BP 의 `BossTerrainGimmickComponent.TowerSpawnPoints` 는 **인덱스 = 슬라이스 인덱스**.
     `TowerSpawnPoints[N]` = 슬라이스 N 위에 타워가 설 위치. 슬라이스 개수(SliceCount)만큼 채운다.
   - 타워는 [미파괴 && 이번 TargetSlice 아님] 슬라이스에 랜덤 스폰. 마지막 라운드(남은 지형이
     파괴대상 하나뿐)엔 그 파괴대상 위에 스폰(폴백).

6. **테스트:** 보스 BP 의 `bAutoStartOnBeginPlay` 를 켜고(또는 트리거로 StartEncounter),
   `AN_SpawnTower(TargetSlice=N)` + `AN_DestroySlice` 가 든 몽타주를 재생 → 슬라이스 N 의 조각 2개가 무너지는지 확인.

### 정리: 누가 무엇을 부르나 (변경점)

```text
[옛 계획]  보스/GAS  →  ArenaManager.BreakPieces([ID])      // 직접 호출 (미연결이었음)

[새 연결]  보스 노티파이 → (C++ 파이프라인) → OnArenaSlicesChanged
                          → ArenaFloorBridge.OnSliceDestroyed(SliceIndex)
                          → 대응 FloorPiece.BreakFloor()      // 방송 구독 (자동)
```

`BreakPieces` / `RestoreAllPieces` 는 그대로 유지 — **테스트·레이드 리셋용 수동 API** 로 계속 쓴다.
(복구는 파괴 마스크를 되돌리는 방송이 없으므로 브리지가 자동 구동하지 않는다. 리셋은 레벨 리로드 기준.)
