# 11. 백헤드 데칼 — 이어서 작업할 TODO (⭐ 최우선, 다른 PC에서 그대로 따라하기)

> 이 문서는 **세션 없이 이것만 읽고 그대로 따라 하면 되도록** 자립형으로 작성됨.
> 목표: 보스 백/헤드 어택 방향 표시 데칼을 **헤드(정면)=하양 / 백(후면)=파랑** 으로,
> AOE 데칼처럼 **플레이어가 선 발판(지면) 위** 에 그려지게 한다.

---

## 0. 현재 상태 스냅샷 (2026-07-22 · PIE 표시 성공)

- **동작**: 데칼이 발판 위에 헤드=하양/백=파랑 링으로 **PIE 에서 잘 나옴**.
- **C++**: `ABossBase::UpdateBackHeadDecal()` 이 발판 Z(트레이스→`ArenaFloorZ`→발밑)를 구해
  스케일/깊이 방어까지 해서 데칼을 발판에 얹는다(§C). 진단 로그·회전 화살표는 제거됨.
  - `UBackHeadDecalComponent::UpdateRadius()` 는 `Radius` 스칼라 주입 + 투영 깊이 최소 128 클램프.
- **머티리얼**: `M_BossBackHead` 완성. 색/각도는 **머티리얼 파라미터 Default 에 baked**(하양/파랑/45°). C++ 색 주입은 안 씀.
- **남은 것**: §D 두 이슈(크기 조절 D-1 / 플레이어 색 D-2) — 나중에 수정.

---

## A. 머티리얼 `M_BossBackHead` 만들기 (여기가 핵심)

Content Browser 에서 Material 신규 생성 → 이름 `M_BossBackHead` → 더블클릭.

### 0) 머티리얼 기본 설정 (Details 패널)

| 항목 | 값 |
|---|---|
| Material Domain | **Deferred Decal** |
| Blend Mode | **Translucent** (Deferred Decal 로 바꾸면 이 드롭다운이 데칼용으로 바뀜. 별도 "Decal Blend Mode" 항목이 안 보이는 게 정상) |

> Translucent 데칼은 **Emissive Color + Opacity** 를 지원 → 최종 색을 Emissive 에, 마스크를 Opacity 에 연결한다.

### 1) Center — UV 원점화

UV(0~1) 의 중심(0.5,0.5)을 원점(0,0)으로 옮겨 극좌표 계산이 가능하게 한다.

1. `TexCoord[0]` 노드
2. `Subtract` : **A = TexCoord**, **B = Constant2Vector(0.5, 0.5)**
   - (B 입력에 아무것도 안 꽂으면 기본 스칼라가 쓰이니, 반드시 (0.5,0.5) 를 연결)
3. 결과 = **Center** (범위 −0.5 ~ +0.5, 중심 = (0,0)). 이후 여러 갈래로 재사용.

### 2) 각도 (Arctangent2Fast)

`Arctangent2Fast` 는 스칼라 X, Y 를 받으므로 Center(float2)를 성분 분리한다.

1. `ComponentMask` 2개 추가
   - 마스크1: **R 만 체크** ← Center 연결 → `Arctangent2Fast` 의 **X**
   - 마스크2: **G 만 체크** ← Center 연결 → `Arctangent2Fast` 의 **Y**
2. `Arctangent2Fast` → `RadiansToDegrees`
3. 결과 = **−180 ~ +180도** (0도 = UV +U 방향)

### 3) Abs — 정면에서 벌어진 각도

1. `Abs` ← `RadiansToDegrees`
2. 결과 = **AngleFromFront** (0~180, 정면=0 / 후면=180)
   - 좌우 대칭이라 절댓값으로 충분.

### 4) 헤드/백 존 마스크

Scalar Parameter 2개: **`HeadHalfAngle`(Default 45)**, **`BackHalfAngle`(Default 45)**

**헤드존(정면, 하양):**
1. `SmoothStep` : Min=`HeadHalfAngle`, Max=`HeadHalfAngle + 3`(Add), Value=`AngleFromFront`
2. `OneMinus` → **HeadZoneMask** (정면=1)

**백존(후면, 파랑):**
1. `Subtract` : A=`180`(Constant), B=`AngleFromFront` → **BackDist** (후면=0)
2. `SmoothStep` : Min=`BackHalfAngle`, Max=`BackHalfAngle + 3`, Value=`BackDist`
3. `OneMinus` → **BackZoneMask** (후면=1)

> `+3` = 경계 부드럽게(안티에일리어싱). 딱딱한 경계 원하면 `+1`.

### 5) 반경 (Length)

1. `Length` ← Center
2. `Multiply` : `× 2`(Constant) → **Len01** (0~1, 데칼 가장자리에서 1)

### 6) 링 마스크 (바닥 덜 가리게 테두리만)

Scalar Parameter: **`RingThickness`(Default 0.15)**

1. 안쪽: `SmoothStep(Min = 1 − RingThickness, Max = (1 − RingThickness) + 0.03, Value = Len01)`
   - `1 − RingThickness` 는 `Subtract(1, RingThickness)`
2. 바깥: `SmoothStep(Min = 0.97, Max = 1.0, Value = Len01)` → `OneMinus` (사각 모서리 컷)
3. 1×2 `Multiply` → **RingMask**

> 꽉 찬 부채꼴을 원하면 §6 생략하고 RingMask 자리에 `Constant 1` 사용(바닥 많이 가림).

### 7) 색 합성 (하양/파랑 baked 지점)

Vector Parameter: **`HeadColor` Default (1,1,1) 하양**, **`BackColor` Default (0.05, 0.35, 1) 파랑**
Scalar Parameter: **`Opacity` Default 0.6**

1. `HeadFinal = HeadZoneMask × RingMask` (Multiply)
2. `BackFinal = BackZoneMask × RingMask` (Multiply)
3. 색: `HeadColor × HeadFinal`, `BackColor × BackFinal` → **Add** → **FinalColor**
4. 알파: `HeadFinal + BackFinal` → `Saturate` → `× Opacity` → **FinalOpacity**

### 8) 출력

- **Emissive Color** ← FinalColor
- **Opacity** ← FinalOpacity

`Apply` → `Save`.

### 파라미터 Default 요약표

| 이름 | 타입 | Default | 의미 |
|---|---|---|---|
| `HeadHalfAngle` | Scalar | 45 | 정면 하양 존 반각(도) |
| `BackHalfAngle` | Scalar | 45 | 후면 파랑 존 반각(도) |
| `RingThickness` | Scalar | 0.15 | 링 두께(0=선, 1=꽉 참) |
| `HeadColor` | Vector | (1,1,1) | 헤드=하양 |
| `BackColor` | Vector | (0.05,0.35,1) | 백=파랑 |
| `Opacity` | Scalar | 0.6 | 전체 불투명도 |

> **각도 일치 규칙**: `HeadHalfAngle/BackHalfAngle` 은 판정값 `ABossBase::HeadZoneHalfAngle/BackZoneHalfAngle`
> (기본 45°)과 **반드시 같게**. 어긋나면 "표시는 백인데 판정은 측면"이 됨.

---

## B. 보스 BP 연결 & PIE 정렬 확인

1. 보스 BP → `BackHeadDecal` 컴포넌트 → **Decal Material** 슬롯에 `M_BossBackHead` 지정.
2. PIE 실행 후 확인:
   - **하양(헤드)이 보스 정면과 안 맞고 90°/180° 틀어짐** →
     `Arctangent2Fast` 의 **X↔Y 입력을 바꾸거나**, ComponentMask 하나 뒤에 `× −1`(Multiply) 추가.
     (`ABossBase::bDrawFacingDebug = true` 로 정면 화살표 띄워 놓고 맞추면 정확)
   - **아예 안 보임** → 마지막이 Emissive 에 연결됐는지, `Opacity` Default 가 0 아닌지,
     데칼이 실제 바닥에 닿는지(§C) 확인.

---

## C. 데칼을 발판 위에 올리기 — C++ (✅ 적용됨, 리빌드 필요)

### 왜 "에디터는 되는데 PIE 는 안 보이나"

`UpdateBackHeadDecal()` 은 **BeginPlay(런타임)에서만** 실행된다.
- **에디터 뷰포트**: 이 함수가 안 돌아 데칼이 놔둔 자리에 그려짐 → 바닥에 잘 보임.
- **PIE**: BeginPlay 에서 데칼을 **캡슐 발밑(`-HalfHeight`)** 으로 내리는데, 이 보스는 **캡슐째 공중 부양**
  → 발밑도 공중이라 얕은 투영 박스(±ProjectionDepth)가 저 아래 바닥에 **안 닿아 안 보임**.

### 수정 내용 (적용 완료 — PIE 확인됨)

`ABossBase::UpdateBackHeadDecal()` 이 발판 Z 를 **AoE(`ResolveGroundZ`)와 동일 규칙**으로 구해
그 위에 데칼을 얹는다. 데칼은 캡슐에 부착돼 **XY/Yaw(앞=헤드/뒤=백)는 자동 정렬**, **Z 만** 지면에 맞춘다.

1. **발밑 아래로 라인트레이스** — `bTraceComplex=true` + `LineTraceSingleByObjectType`(`WorldStatic`/`WorldDynamic`).
   병합 바닥(`SM_MERGED_*`)이 복합 콜리전만 있고 Visibility Block 안 해도 잡힌다.
2. **트레이스 실패 시 `GameState.ArenaFloorZ`** — 보스가 선 자리엔 바닥 콜리전이 안 잡히는 맵이 있어 필요.
   `ABossRaidGameState::ArenaFloorZ`(아레나 바닥 단일 진실, GameState BP 에 지정)를 쓴다. AoE 도 동일 폴백.
3. 그래도 없으면 발밑.

추가로 **두 함정**을 처리:
- **보스 스케일 대응**: 보스가 x5 등으로 크게 스케일돼 있으면 `SetRelativeLocation` 의 상대 Z 가 부모 스케일만큼
  뻥튀기돼 데칼이 땅속으로 꺼진다 → **`SetWorldScale3D(1)` + `SetWorldLocation`(월드 좌표 직접 지정)** 으로 회피.
- **투영 깊이 0 방어**: `UBackHeadDecalComponent::UpdateRadius` 에서 `DecalSize.X` 를 **최소 128 클램프**
  (실수로 `ProjectionDepth=0` 세팅 시 데칼이 아예 안 그려지던 것 방지).
- **스폰 타이밍**: 보스 BeginPlay 가 발판 스폰보다 먼저 도는 맵 대응 — 바닥값을 잡을 때까지 0.25초 재시도 후 정지(`DecalGroundTimer`).

수정 파일: `LostArk/Source/LostArk/Boss/BossBase.cpp`(+`.h` 타이머), `BackHeadDecalComponent.cpp`(깊이 클램프).
**C++ 변경이라 에디터 리빌드 필요.** (진단 로그/디버그 박스/회전 화살표는 제거 완료.)

---

## D. 알려진 이슈 / 나중에 수정 (TODO)

### D-1. 데칼 크기를 `RadiusPadding` 으로 못 줄임

- **증상**: `BackHeadDecal` 의 `RadiusPadding` 을 낮춰도 데칼 링이 작아지지 않음.
- **원인 추정**: 크기(`DecalSize.Y/Z = GetScaledCapsuleRadius + RadiusPadding`)가 **캡슐 반경에 종속** →
  스케일 큰 보스(x5)는 반경만 ~510 이라 패딩을 줄여도 여전히 큼. `RadiusPadding` 은 더하는 값이라 **음수**로 줄이거나,
  크기를 캡슐 반경이 아니라 **별도 `EditAnywhere` 반경 파라미터**로 직접 지정하도록 바꿔야 함.
- **수정 방향**: `UpdateRadius` 를 "캡슐 반경 연동" 대신 "고정 반경 프로퍼티(`DecalRadius`)" 로 바꾸거나,
  `RadiusPadding` 클램프(음수 허용)로 조정 가능하게.

### D-2. 데칼이 플레이어 위에도 투영돼 플레이어 색이 바뀜

- **원함**: 데칼은 지면에만 그려지고 **플레이어 메시 색은 안 바뀌게**.
- **원인**: Deferred Decal 이 데칼 박스 안의 **모든 Receives Decals 서프에이스**(=플레이어 메시 포함)에 투영됨.
- **수정 방향 (택1)**:
  1. **플레이어 메시** `SetReceivesDecals(false)` — 캐릭터 스켈레탈메시에서 데칼 수신 끄기(가장 간단).
     (보스 메시는 이미 `SetReceivesDecals(false)` 처리돼 있음 — 같은 방식)
  2. 데칼 머티리얼을 특정 **Decal Response/스텐실**로 제한해 바닥만 받게.
  3. `ProjectionDepth` 를 얇게(발판만 덮게) 줄여 서 있는 플레이어 몸에 덜 닿게 — 부분 완화용.

---

## 완료 체크리스트

- [x] `M_BossBackHead` 생성 + Deferred Decal / Translucent 설정
- [x] §A 1~8 노드 그래프 연결 (Center → 각도/반경 → 존/링 마스크 → 색 합성 → 출력)
- [x] 파라미터 Default 채우기 (하양/파랑/45/45/0.6/0.15)
- [x] 보스 BP `BackHeadDecal` 슬롯에 머티리얼 지정
- [x] PIE 에서 발판 위 표시 확인 (§C — ArenaFloorZ 폴백 + 스케일/깊이 방어)
- [ ] (D-1) 데칼 크기 조절 가능하게 (`RadiusPadding`/전용 반경 프로퍼티)
- [ ] (D-2) 플레이어 메시 `SetReceivesDecals(false)` 로 플레이어 색 안 바뀌게
- [ ] (선택) PIE 정렬 미세조정 (하양=정면) — atan2 입력/AngleOffset 보정
