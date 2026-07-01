# UnrealTeamProject

Unreal Engine **5.3** 기반 멀티플레이 액션 RPG 팀 프로젝트.

- **직업 3종**: 전사 / 마법사 / 힐러
- **레벨 흐름**: 튜토리얼 → 카오스 던전 → 레이드
- **핵심 기술**: GAS(Gameplay Ability System) 필수, 컴포넌트 기반 설계
- **네트워크 목표**: 데디케이트 서버(파티장 이탈에도 서버 유지)
- **기간**: 2026-07-01 ~ 2026-07-26

---

## 📌 시작하기 전에 반드시 읽을 것

> **모든 팀원은 첫 작업 전에 [docs/00_START_HERE.md](docs/00_START_HERE.md) 를 읽는다.**
> 이 문서(하네스)의 규칙을 지키지 않은 커밋/PR은 병합하지 않는다.

## 📚 문서 목차

| 문서 | 내용 |
|------|------|
| [00_START_HERE](docs/00_START_HERE.md) | **하네스** — 전체 규칙 요약 / 필독 |
| [01_GIT_WORKFLOW](docs/01_GIT_WORKFLOW.md) | 브랜치 전략, 커밋/PR 규칙, LFS |
| [02_ARCHITECTURE](docs/02_ARCHITECTURE.md) | 컴포넌트 기반 설계, 디자인 패턴, 결합도 최소화 |
| [03_GAS_GUIDE](docs/03_GAS_GUIDE.md) | GAS 사용 규칙(Ability/Effect/Attribute/Tag) |
| [04_CLASSES](docs/04_CLASSES.md) | 전사/마법사/힐러 설계 규격 |
| [05_LEVELS](docs/05_LEVELS.md) | 튜토리얼/카오스던전/레이드 구성 |
| [06_NETWORKING](docs/06_NETWORKING.md) | 데디케이트 서버, 리플리케이션 규칙 |
| [07_SCHEDULE](docs/07_SCHEDULE.md) | 4주 일정 / 마일스톤 |
| [08_CODING_CONVENTIONS](docs/08_CODING_CONVENTIONS.md) | 네이밍/폴더/코드 스타일 |

## 👥 브랜치 · 담당

| 브랜치 | 담당 | 범위 |
|--------|------|------|
| `main`   | 전원(에셋 관리자) | 모든 대용량 에셋 통합 관리, 릴리스 |
| `main-1` | 캐릭터 A | 전사 + 힐러 (공용 CharacterBase는 main 선반영) |
| `main-2` | 캐릭터 B | 마법사 + 캐릭터 공용 시스템 보조 |
| `main-3` | 보스 | 레이드 보스, 보스 AI, 페이즈 |
| `main-4` | 일반몹 + UI | 카오스 던전 몬스터, HUD/UI |
| `main-5` | 레벨 | 튜토리얼/카오스던전/레이드 맵, 스테이지 흐름 |

> 브랜치 `main-1~5` 이름은 임시다. 담당이 확정되면
> `git branch -m main-1 feature/warrior` 처럼 언제든 변경 가능
> ([01_GIT_WORKFLOW](docs/01_GIT_WORKFLOW.md) 참고).
> 작업을 먼저 끝낸 사람은 다른 파트를 돕는다 — 그래서 **결합도 최소화**가 최우선 원칙이다.
