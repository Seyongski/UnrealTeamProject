# 01. Git 워크플로우

## 브랜치 전략

```
main                <- 통합/릴리스. 모든 대용량 에셋의 단일 소스(Source of Truth)
 ├─ main-1  캐릭터 A (전사 + 힐러)
 ├─ main-2  캐릭터 B (마법사 + 캐릭터 공용)
 ├─ main-3  보스
 ├─ main-4  일반몹(카오스던전) + UI
 └─ main-5  레벨
```

- `main` 에 **직접 push 금지.** 항상 자기 브랜치에서 작업 후 PR.
- 각자 자기 브랜치에서만 커밋한다. 남의 브랜치를 건드리지 않는다.
- **에셋 통합 원칙**: `.uasset`/`.umap` 대용량 에셋은 원칙적으로 `main`에서 관리한다.
  기능 브랜치에서 임시 에셋을 만들었다면, PR 시 담당자(에셋 관리자)와 통합한다.

### 브랜치 이름은 나중에 바꿀 수 있다 ✅
`main-1~5` 는 임시 이름이다. 담당이 확정되면 의미 있는 이름으로 변경한다:

```bash
# 로컬 브랜치 이름 변경
git branch -m main-1 feature/warrior-healer

# 원격에 반영 (예전 이름 삭제 + 새 이름 push)
git push origin :main-1 feature/warrior-healer
git push origin -u feature/warrior-healer
```
> 권장 규칙: 확정 시 `feature/<파트>` 형태 (예: `feature/mage`, `feature/boss`, `feature/level`).

---

## 일상 작업 루프

```bash
# 1. 최신 main 반영 (에셋/공용 코드 동기화)
git checkout main
git pull

# 2. 내 브랜치로 이동 후 main 병합(정기적으로)
git checkout main-2
git merge main            # 공용 변경 흡수 → 충돌 조기 발견

# 3. 작업 → 커밋
git add .
git commit -m "feat(mage): 파이어볼 GA 추가"

# 4. 원격 push
git push
```
- **자주(하루 1회 이상) `main`을 자기 브랜치로 병합**한다. 늦게 병합할수록 충돌이 커진다.

---

## 커밋 메시지 규칙 (Conventional Commits)

```
<type>(<scope>): <요약>

type : feat | fix | refactor | asset | docs | chore | test
scope: warrior | mage | healer | boss | mob | ui | level | gas | net | core
```
예)
- `feat(healer): 힐 오라 GameplayEffect 추가`
- `asset(boss): 레이드 보스 스켈레탈 메시 임포트`
- `fix(net): 몬스터 사망 판정 서버 권위로 이동`

---

## PR 규칙

- 대상 브랜치: 보통 `main` (기능 완료 시). 공동 작업 시 상대 브랜치.
- **PR 템플릿**([templates/PR_TEMPLATE.md](templates/PR_TEMPLATE.md)) 체크리스트를 채운다.
- 리뷰어 1명 이상 승인 후 병합.
- 병합 방식: `main` 히스토리 정리를 위해 **Squash merge** 권장.
- 코드 소유권/자동 리뷰어 지정 → 루트 `CODEOWNERS` 참고.

---

## Git LFS (대용량 에셋)

최초 1회만 각자 로컬에서:
```bash
git lfs install
```
- 추적 대상은 `.gitattributes`에 정의돼 있다(`.uasset`, `.umap`, `.fbx`, 텍스처, 사운드 등).
- 확인: `git lfs status`, `git lfs ls-files`
- ⚠️ GitHub 무료 LFS는 **1GB 저장 / 1GB 월 대역폭** 제한이 있다. 에셋이 크면 유료 데이터 팩 또는 별도 스토리지를 고려한다([00_START_HERE](00_START_HERE.md) 참고).

### 바이너리 충돌 원칙
`.uasset`/`.umap`은 병합 불가 → **같은 에셋을 두 사람이 동시에 수정하지 않는다.**
- 에셋 편집 전 팀 채널에 "이 에셋 잡습니다" 선언(잠금 관례).
- 필요 시 파일 잠금 사용: `git lfs lock <path>` / `git lfs unlock <path>`.
