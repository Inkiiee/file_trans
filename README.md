# file_trans

TCP 기반 파일 전송 서버 프로그램입니다. UDP 브로드캐스트를 통한 서버 디스커버리 기능을 포함하고 있습니다.

## 주요 기능

- **파일 다운로드/업로드** — 클라이언트의 요청에 따라 파일 전송
- **파일 관리** — 파일 목록 조회, 정보 조회, 삭제, 이름 변경, 이동, 복사
- **디렉토리 관리** — 디렉토리 생성, 현재 디렉토리 조회/변경
- **시스템 명령 실행** — 원격 시스템 명령 실행
- **UDP 브로드캐스트 디스커버리** — 클라이언트가 네트워크 내 서버를 자동으로 검색
- **Heartbeat** — 연결 상태 유지 확인

## 기술 스택

- **C++20** (coroutines, `std::jthread`, `std::filesystem`)
- **Asio 1.38.0** (standalone, header-only) — 비동기 네트워크 I/O
- **CMake 3.16+** — 빌드 시스템
- **Yocto SDK 크로스 컴파일** 지원

## 빌드 방법

### 네이티브 빌드

```bash
mkdir build && cd build
cmake ..
make
```

### Yocto SDK 크로스 컴파일

```bash
source /opt/poky/<version>/environment-setup-<target>
mkdir build && cd build
cmake ..
make
```

`SDKTARGETSYSROOT` 환경변수가 설정되면 자동으로 크로스 컴파일 경로가 적용됩니다.

## 실행

```bash
./file_trans <address> <port> <id>
```

| 인자 | 설명 |
|------|------|
| `address` | 서버 바인딩 주소 (예: `0.0.0.0`) |
| `port` | TCP 리스닝 포트 |
| `id` | UDP 브로드캐스트 디스커버리에서 사용할 서버 식별자 |

## 프로토콜

커스텀 바이너리 패킷 프로토콜을 사용합니다.

### 패킷 타입

| 코드 | 타입 | 설명 |
|------|------|------|
| 0x0001 | HEARTBEAT | 연결 유지 확인 |
| 0x0002 | REQUEST_FILE_TRANSFER | 파일 다운로드 요청 |
| 0x0003 | FILE_TRANSFER_DATA | 파일 데이터 전송 |
| 0x0004 | FILE_TRANSFER_COMPLETE | 전송 완료 |
| 0x0007 | REQUEST_FILE_LIST | 파일 목록 요청 |
| 0x0008 | REQUEST_FILE_INFO | 파일 정보 요청 |
| 0x000E | REQUEST_FILE_DELETE | 파일 삭제 요청 |
| 0x0011 | REQUEST_FILE_RENAME | 파일 이름 변경 요청 |
| 0x0014 | REQUEST_DIRECTORY_CREATE | 디렉토리 생성 요청 |
| 0x0017 | REQUEST_FILE_MOVE | 파일 이동 요청 |
| 0x001A | REQUEST_FILE_COPY | 파일 복사 요청 |
| 0x001D | REQUEST_FILE_UPLOAD | 파일 업로드 요청 |
| 0x0020 | REQUEST_SYSTEM_COMMAND | 시스템 명령 실행 요청 |
| 0x0023 | REQUEST_CURRENT_DIRECTORY | 현재 디렉토리 조회 |
| 0x0025 | REQUEST_CHANGE_DIRECTORY | 디렉토리 변경 요청 |

### UDP 브로드캐스트

- 브로드캐스트 주소: `255.255.255.255:30001`
- 클라이언트가 `REQUEST_SERVER_INFO`를 보내면 서버 주소/포트/ID로 응답

## 프로젝트 구조

```
file_trans/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── file_trans_server.h      # TCP 서버 (acceptor)
│   ├── trans_session.h          # 클라이언트 세션 처리
│   └── udp_broadcast_server.h   # UDP 브로드캐스트 디스커버리
├── src/
│   ├── main.cpp
│   ├── file_trans_server.cpp
│   ├── trans_session.cpp
│   └── udp_broadcast_server.cpp
└── third_party/
    └── asio_1.38.0/             # Asio standalone 라이브러리
```

## 라이선스
"https://think-async.com/Asio/"의 Asio Standalone 라이브러리를 사용하기에 해당 라이선스를 따릅니다.
