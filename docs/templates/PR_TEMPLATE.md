# PR 제목: <type>(<scope>): 요약

## 무엇을 했나
- 

## 어떻게 (설계/패턴)
- 

## 결합도 체크 (필수)
- [ ] 다른 팀원의 구체 클래스를 직접 참조하지 않았다 (인터페이스/태그/데이터로 대체)
- [ ] 새 기능은 ActorComponent 또는 GAS(GA/GE)로 구현
- [ ] 하드코딩 수치를 DataAsset/DataTable/GE로 분리

## GAS 체크
- [ ] 스탯 변경은 GE로만
- [ ] 스킬은 GA, 수치는 SetByCaller/Data
- [ ] 태그 네이밍 규칙 준수 / 연출은 GameplayCue

## 네트워크(데디 서버 대비) 체크
- [ ] 상태 변경 앞 HasAuthority() 가드
- [ ] 공유 상태 Replicated 등록
- [ ] 서버 없는 로컬 플레이어 가정에서 UI/입력 안전

## 에셋/Git
- [ ] .uasset/.umap 은 LFS 추적 (`git lfs status`)
- [ ] 자기 브랜치에서 작업, 대상 브랜치 확인
- [ ] 네이밍/폴더 규칙 준수

## 테스트
- [ ] PIE 단일 플레이 확인
- [ ] (해당 시) PIE 멀티(2P, Play As Client) 확인

## 스크린샷/영상
- 
