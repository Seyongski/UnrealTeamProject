# 11. 백헤드 데칼 — 이어서 작업할 TODO (⭐ 최우선, 다른 PC에서 그대로 따라하기)

> 이 문서는 **세션 없이 이것만 읽고 그대로 따라 하면 되도록** 자립형으로 작성됨.
> 목표: 보스 백/헤드 어택 방향 표시 데칼을 **헤드(정면)=하양 / 백(후면)=파랑** 으로,
> AOE 데칼처럼 **플레이어가 선 발판(지면) 위** 에 그려지게 한다.

---

## 0. 현재 상태 스냅샷 (2026-07-22)

- **C++**: 원본 상태(색 주입/지면 트레이스 코드는 되돌려짐).
  - `UBackHeadDecalComponent`(`LostArk/Source/LostArk/Boss/BackHeadDecalComponent.*`)
    는 `UpdateRadius()` 에서 **`Radius` 스칼라 파라미터만** MID 에 주입한다.
  - 데칼 위치는 `ABossBase::UpdateBackHeadDecal()` 에서 **보스 발밑(`-HalfHeight`) 고정**.
    → 공중에 뜬 보스는 지면이 아니라 발밑 허공에 투영됨(= "데칼이 저 아래 뜨는" 문제, **아직 미해결** → §C).
- **머티리얼**: `M_BossBackHead` 제작 중.
  - 색/각도는 **머티리얼 파라미터의 Default 값에 직접 baked**(하양/파랑/45°). C++ 색 주입은 **안 씀**.
  - 파라미터로 만들어 두면 나중에 Material Instance 로 튜닝 가능(이름 자유, C++ 의존 없음).
- **남은 핵심 작업**: ① 머티리얼 노드 그래프 완성 → ② 보스 BP 데칼 슬롯에 지정 → ③ PIE 정렬 확인.

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

## C. (선택/미해결) 데칼을 발판 위에 올리기 — C++ 개선안

현재 `ABossBase::UpdateBackHeadDecal()` 은 데칼을 **보스 발밑(`-HalfHeight`) 고정**이라,
공중에 뜬 보스는 지면이 아니라 허공에 투영된다("저 아래 뜨는" 문제).

**해결 방향**: 발밑에서 **아래로 라인트레이스** 해 실제 발판 Z 를 찾아 그 위에 데칼을 얹는다(AOE 데칼처럼).
- 데칼은 캡슐에 부착돼 있어 **XY/Yaw(앞=헤드/뒤=백)는 자동 정렬** → **Z 만** 지면에 맞추면 됨.
- 트레이스는 프로젝트에 이미 검증된 규칙을 그대로 따를 것:
  참고: `ABossPatternActorBase::TraceGroundZ`
  (`LostArk/Source/LostArk/Boss/Damage/BossPatternActorBase.cpp`)
  - `bTraceComplex = true` — 병합 바닥 메시(`SM_MERGED_*`)는 복합 콜리전만 있어 이게 없으면 못 맞춤
  - `LineTraceSingleByObjectType` + `ECC_WorldStatic` / `ECC_WorldDynamic` — 바닥이 Visibility Block 안 해도 잡힘
- 보스 이동/지형파괴로 발판 Z 가 바뀔 수 있으니, 0.1초 주기 타이머로 재투영하면 안전.

> 이전 세션에서 위 방식으로 C++ 를 수정했다가 되돌린 상태. 필요하면 재적용.

---

## 완료 체크리스트

- [ ] `M_BossBackHead` 생성 + Deferred Decal / Translucent 설정
- [ ] §A 1~8 노드 그래프 연결 (Center → 각도/반경 → 존/링 마스크 → 색 합성 → 출력)
- [ ] 파라미터 Default 채우기 (하양/파랑/45/45/0.6/0.15)
- [ ] 보스 BP `BackHeadDecal` 슬롯에 머티리얼 지정
- [ ] PIE 정렬 확인 (하양=정면) + 필요 시 atan2 입력 보정
- [ ] (선택) §C 지면 트레이스로 발판 위 투영 적용
