# 06. 네트워킹 — 데디케이트 서버 준비

## 목표
- **데디케이트 서버(Dedicated Server)** 채택. 리슨 서버(Listen Server) 아님.
- 이유: 리슨 서버는 호스트(파티장) = 서버라서, **파티장이 나가면 세션이 붕괴**한다.
  데디케이트 서버는 서버가 독립 프로세스이므로 **파티장이 튕겨도 서버·세션 유지**된다.

> 지금 당장 서버를 구축하지 않아도, **아래 규칙을 처음부터 지키면**
> 나중에 데디 서버를 붙일 때 재작업이 거의 없다. (남는 시간에 서버 구축)

---

## 반드시 지킬 규칙 (day 1부터)

### 1. 서버 권위(Server-Authoritative)
- 모든 **게임플레이 판정은 서버에서** 한다: 데미지, 사망, 스폰, 스테이지 클리어, 드랍.
- 클라이언트는 **입력을 보내고 결과를 받는다.** 클라가 스탯/사망을 스스로 확정하지 않는다.
```cpp
if (HasAuthority()) { /* 실제 상태 변경은 여기서만 */ }
```

### 2. 리플리케이션
- 공유돼야 할 상태는 `UPROPERTY(Replicated)` / `ReplicatedUsing=OnRep_*`.
- `GetLifetimeReplicatedProps` 등록 필수.
- 액터: `bReplicates = true`, 움직이는 액터는 `SetReplicateMovement(true)`.
- GAS의 어트리뷰트/태그/GE는 **GAS가 자동 리플리케이트** → 스탯을 수동 RPC로 동기화 금지.

### 3. RPC 규칙
- `Server` RPC: 클라 → 서버 요청(입력/스킬 시전 요청). `WithValidation` 붙여 검증.
- `Client` RPC: 서버 → 특정 클라(개인 UI/알림).
- `NetMulticast`: 서버 → 전원(짧은 연출). 남발 금지 → 연출은 **GameplayCue** 우선.

### 4. 역할별 로직 분리
- `HasAuthority()`(서버)와 `IsLocallyControlled()`(내 클라)를 구분해 코드 경로를 나눈다.
- 데디케이트 서버엔 로컬 플레이어가 없다 → **UI/입력/사운드/카메라 코드가 서버에서 크래시하지 않게** 가드.

### 5. GAS 세팅(멀티 대비)
- 플레이어 **ASC는 PlayerState**에 (리스폰/재접속에도 유지).
- ASC Replication Mode: 플레이어 `Mixed`, AI/몹 `Minimal`.
- 어빌리티 예측이 필요하면 `LocalPredicted`.

---

## 세션/서버 구조 (남는 시간에 구축)

```
[Dedicated Server 프로세스]  ← 독립 실행, 게임 상태의 유일한 권위
      ▲          ▲          ▲
   Client1    Client2    Client3(파티장)
   파티장이 나가도 서버는 계속 → 남은 인원 게임 지속
```
- 매치메이킹/파티: `OnlineSubsystem`(초기엔 NULL/LAN, 확장 시 Steam 등).
- 서버 빌드: UE `Development Server` / `Shipping Server` 타깃 구성.
- 접속: `open <serverIP>` 또는 세션 검색 → travel.
- 재접속/승계: 서버가 권위를 갖고 있으므로 클라 이탈은 세션에 영향 없음. (호스트 마이그레이션 불필요)

---

## "미리 준비" 체크리스트 (지금 코드에 반영)
- [ ] 상태 변경 앞에 `HasAuthority()` 가드가 있는가
- [ ] 공유 상태에 `Replicated` 지정 + `GetLifetimeReplicatedProps` 등록했는가
- [ ] 스킬 시전은 Server RPC / GA 로 서버를 거치는가
- [ ] UI/입력/사운드 코드가 서버(로컬 플레이어 없음)에서 안전한가
- [ ] 연출을 NetMulticast 남발 대신 GameplayCue로 했는가
- [ ] 플레이어 ASC를 PlayerState에 두었는가
- [ ] `PlayInEditor > Net Mode = Play As Client`, `Number of Players ≥ 2`로 수시 테스트했는가
