### \[개요\]
select 모델에서 나아가 비동기 I/O를 수행하는 네트워크 라이브러리 프로젝트이다. 비동기 I/O 요청에 대한 완료 통지 수신 방법은 여러 가지가 있고, 지능적인 스레드 풀 관리와 더불어 완료 통지를 처리하며, 추가적인 I/O 요청을 수월하게 할 수 있는 윈도우 IOCP 객체를 적극 활용한다.

네트워크가 제공하는 큰 기능의 골자는 아래와 같다.

**1. 클라이언트 세션 생명 주기 관리**

**2. 클라이언트 연결 및 패킷 수신 시 이벤트 함수 호출**

**3. 라이브러리 기능 요청 함수 (메시지 송신, 연결 강제 종료)**

이 기능들은 라이브러리 사용 측에 추상화로 제공되어, 다양한 컨텐츠 서버들(채팅/로그인/에코/게임 등)을 빠르고 쉽게 구현할 수 있도록 한다.

![](https://velog.velcdn.com/images/jinh2352/post/f57ad30d-8f5f-4b7e-843c-ee2e16b224b6/image.png)


#### \[활용 기술\]
* Windows Socket Programming
* Windows Synchronization Objects
* Windows OVERLAPPED I/O
* Windows I/O Completion Port(IOCP)
* MS Open Database Connectivity(ODBC)
  
#### \[라이브러리 API 문서\]
https://jins-software.github.io/JNetLibrary/annotated.html
  
#### \[라이브러리 활용 컨텐츠 서버 프로젝트\]
* Login Server: https://github.com/JINs-software/LoginServer_JNet
* Chatting Server: https://github.com/JINs-software/ChattingServer_JNet
* Chatting Server(with Lib's Job): https://github.com/JINs-software/ChattingServer_MultiJob 
* Echo Server: https://github.com/JINs-software/EchoGameServer_JNet
* Game Server: https://github.com/JINs-software/MarchOfWindServer

#### (cf, 'CLanLibrary': 구 버전 네트워크 라이브러리)
JNetLibrary는 기존 네트워크 코어 라이브러리인 'CLanLibrary'를 개량한 라이브러리이다. 리포지토리의 LoginServer, ChattingServer('\_JNet' 접미사가 붙지 않은)는 CLanLibrary를 상속하여 만든 프로젝트이다.
* CLanLibrary: https://github.com/JINs-software/CLanLibrary
  * Login Server: https://github.com/JINs-software/LoginServer
  * Chatting Server: https://github.com/JINs-software/ChattingServer
  * Monitoring Server: https://github.com/JINs-software/MonitoringServer

---

### \[JNetLibrary's Class]
JNetLibrary는 기본적으로 ‘세션 생명 주기 관리’ 및 ‘비동기 I/O 처리’ 기능을 제공하는 최상위 클래스(JNetCore)부터 구체적인 기능(서버 또는 클라이언트 기능, DB 커넥션 기능, 세션 그룹화)이 추가되는 하위 클래스들로 구성됩니다. 또한 LAN 구간 내 다른 서버와 연결되어 클라이언트 역할을 수행할 수 있는 클라이언트 클래스(JNetClient)도 포함됩니다.

![](https://velog.velcdn.com/images/jinh2352/post/4817d3d9-ddf3-4189-8ab2-1d0f3787cb72/image.png)

컨텐츠 기능에 맞게 특정 라이브러리 내 클래스를 상속받아 ‘TCP 연결’ 및 ‘메시지 수신’ 시 호출되는 이벤트 함수를 재정의하여 메시지를 통해 전달되는 요청을 처리하는 컨텐츠 코드를 작성합니다. 컨텐츠 코드에선 역으로 라이브러리 단 기능을 함수 호출을 통해 요청하여 메시지 송신 및 연결 강제 종료 등의 작업을 수행할 수 있습니다.

</br>

#### \[JNetCore / JNetServer 클래스]
![](https://velog.velcdn.com/images/jinh2352/post/3a65c5c1-8a99-4f83-b6dc-11ba7a96b696/image.png)

**JNetCore**
클라이언트의 TCP 세션의 생명 주기를 'JNetSession' 객체를 통해 관리하며, 다수의 작업자 IOCP 작업자 스레드 간 세션 참조 상황 (ex, 특정 클라이언트 세션에 패킷 송신)에서 thread-safe 성을 보장해줍니다.
JNetCore 하위 클래스 단 작업 중 클라이언트 접속 완료(JNetServer) 및 다른 LAN 서버와의 커넥트 성공(JNetClient) 시 연결 소켓을 전달하며 세션 객체 생성 및 IOCP에 소켓 장치 등록을 요청합니다. 이 후 해당 세션으로의 메시지 송신 및 연결 강제 종료는 thread-safe한 요청 함수를 통해 실현하며, 메시지 수신 및 연결 종료 이벤트를 받을 수 있습니다. 
* 대표 이벤트 함수: OnReceiveCompletion, OnSeissonLeave
* 대표 기능 제공 함수: SendPacket


**JNetServer**
클라이언트의 연결 요청에 대한 Accept를 수행하는 Accept 스레드 관리
클라이언트와의 연결 수립 시 세션 생성 및 연결 소켓을 IOCP에 등록
* 대표 이벤트 함수: OnClientJoin, OnRecv(with decoded packet)
* 대표 기능 제공 함수: SendPacket(encoding packet)

</br>

#### \[JNetGroupServer / JNetGroupThread 클래스]

클라이언트 세션의 패킷에 대한 직렬 처리와 독점적 세션 참조(락이 필요없는)를 보장하기 위해 JNetServer에서 기능을 확장하면서 세션 그룹화라는 추상화를 제공하는 하위 서버 및 그룹과 맵핑되는 클래스 구조입니다.

![](https://velog.velcdn.com/images/jinh2352/post/214c4832-70f7-4f9e-93e6-f26d315a8fd7/image.png)

**JNetGroupServer**
세션-그룹ID 맵핑 / 그룹ID-그룹 스레드 맵핑 자료구조를 통해 세션을 각 그룹에 소속시키고, 각 그룹을 독점적으로 처리하는 싱글 스레드에 배정시킵니다.
* 대표 기능 제공 함수: CreateGroup, EnterSessionGroup, ForwardMessage

**JNetGroupThread**
각 그룹의 패킷은 그룹과 맵핑된 싱글 그룹 스레드에게만 전달되기에 
해당 싱글 스레드는 패킷을 직렬 처리가 가능하며, 
독점적으로 세션을 점유하기에 락(lock) 기반의 동기화가 필요 없습니다. 
* 대표 이벤트 함수: OnMessage, OnEnterClient

</br>

#### \[이 외 JNetOdbcServer, JNetClient 클래스\]

**JNetClient**
* 기본적인 클라이언트 기능을 제공받을 수 있는 클래스
* ex, 모니터링 서버(JNetServer 구체 클래스) <-> 로그인/채팅/에코 서버 모니터링 클라이언트(JNetClient 구체 클래스)

**JNetOdbcServer**
* MS의 DBMS 독립적 애플리케이션 구현에 사용되는 ODBC API를 활용
* ODBC를 통한 DB 연결/쿼리 요청/응답 확인 등의 기능을 제공하는 JNetDBConn Pool 관리
* 컨텐츠 코드에 DB 연결 및 접근 추상화를 제공


---


### \[라이브러리 클래스 주요 멤버\]

### *1.  JNetCore: JNetLibrary 최상위 부모 클래스*

![JNetCore_class](https://github.com/user-attachments/assets/686c71da-e351-4630-8152-6a96debafe32)


-   **주요 멤버 변수**
    -   **m\_Session: 세션 관리 벡터:** 클라이언트 세션은 JNetSession
        클래스로 관리, JNetSession은 세션 ID와 참조 관리 변수, 네트워크
        송수신을 위한 소켓/송수신 버퍼를 가짐.
        -   **JNetSession::SessionID:** 16비트의 인덱스 부와 48비트의
            증분 부 존재. 16비트의 인덱스는 활성화된 세션에 붙여지는
            고유한 값이며, 세션 참조를 위한 벡터 자료구조의 인덱스와
            맵핑. 48비트의 증분 부(세션 생성 시 증분)와 함께 고유한 세션
            ID를 생성.
        -   **JNetSession::SessionRef:** 윈도우 Interlocked 계열 함수인
            'InterlockedCompareExchange'을 통한 원자적으로 비교 및
            변경의 대상이 되며 IOCP 작업자 스레드 및 컨텐츠 코드에서의
            접근하는 세션 객체의 참조를 관리.
    -   **m\_TlsMemPoolMgr:** TLS(Thread Local Storage)를 활용한 메모리
        풀을 사용할 수 있음. IOCP 작업자 스레드 및 기타 스레드는 이
        독립적인 메모리 풀을 활용함. 대표적으로 송신 패킷 메모리를
        할당받고, 송신 완료 시
        반환함(https://github.com/JINs-software/MemoryPool).

-   **주요 멤버 함수**
    -   **CreateNewSession, DeleteSession:** JNetCore 하위 클래스에서 세션
        생성 및 삭제 요청을 위한 함수. 예를 들어 **JNetServer** 클래스에서
        Accept 후 세션 객체를 생성 시 그리고 TCP 연결 종료 판단 시 세션
        제거를 위해 호출. JNetSession 세션 클래스의 멤버와 Interlocked
        계열의 함수를 통해 세션 삭제 요청 시 참조가 없는 세션에 대한
        삭제만을 진행
    -   **RegisterSessionToIOCP:** 세션에 대한 관리와 IOCP를 통한 비동기
        송수신이 JNetCore에서 이루어지고, TCP 연결 요청 및 수립은 하위의
        JNetServer와 JNetClient에서 수행됨. 연결 요청 및 수립 시 비동기
        송수신의 관리를 위한 등록 요청을 해당 함수를 호출함으로써 진행.
    -   **AcquireSession, ReturnSession**: 라이브러리 내 코드에서 세션에 접근 시 AcquireSession 함수를 통해
        참조가 이루어져야 한다. 윈도우 Interlocked 계열의 함수를 통해
        참조하고자 하던 세션만을 참조하거나 포기하는 'all-or-nothing' 과정을
        밟는다. 참조를 완료한 세션은 ReturnSession을 통해 반환하여 참조 관리
        변수를 갱신함.
    -   **Disconnect:** 컨텐츠 코드에서 호출될 수 있는 API임.
        PostQueuedCompletionStatus를 통해 IOCP 작업자 스레드에게 삭제의
        잡으로 전달됨. 세션의 참조 카운트가 1 이상(점유 상태)임이 보장된
        상태에서 호출되며, 라이브러리에 세션 삭제를 요청하는 함수.
    -   **SendPacket:** 클라이언트 세션 ID와 직렬화 버퍼(JBuffer, 추후
        설명)를 인수로 전달하여 패킷 전송을 요청함. SendPacket에는 해당 함수
        호출 스레드가 아닌 코어의 작업자 스레드에게 세션 송신 1회 제한
        검사와 비동기 송신 요청의 책임을 전가하는 옵션이 존재.
        -   postToWorker On: 코어의 작업자 스레드에게 비동기 송신 요청을
            등록하는 것. 비동기 송신 요청은 내부 SendPost 함수로 수행되며,
            송신 플래그(1회 송신 제한)에 대한 원자적 비교 및 변경(이하 CAS,
            Compare and Exchange)를 수행하기에 호출 스레드의 수행에 병목이
            발생할 수 있음. 이후 설명할 에코 서버 개발의 조건에 있어 에코
            수행 스레드의 초당 처리 횟수(TPS, Transaction Per Second)를
            높이기 위한 조건이 있었고, 이를 해결하기 위한 방법으로 고안된
            옵션
    -   **BufferSendPacekt, SendBufferedPacket:** 송신 1회 제한에 대한
        블로킹 없이 락-프리 큐로 구현된 송신 큐에 패킷 삽입만 진행, 그리고
        SendBufferedPacket 호출로 적재된 송신 패킷들을 일괄적으로 보냄.
        반응성이 중요한 서버가 아닐 시 한 프레임에 세션 별로 보내야 할
        메시지들을 BufferedSendPacket을 호출하고, 프레임 후반부에 한 번 씩
        호출함으로써 처리량을 높임.
    -   **AllocTlsMemPool, AllocSerialBuff, FreeSerialBuff, AddRefSerialBuff:** 라이브러리 및 컨텐츠 코드에서 스레드 분기 시
        AllocTlsMemPool를 호출하면 독립적인 메모리 풀을 할당 받을 수 있음.
        AllocSerialBuff를 통해 스레드 간 경합 없이 독립적인 메모리 풀로부터
        직렬화 버퍼 공간을 할당받고, FreeSerialBuffer를 통해 반환함.
        브로드캐스팅을 위한 송신 패킷의 경우 단일의 버퍼만 할당받도록 하고
        송신 시 이를 공유하는 방식으로 복사 오버헤드를 줄임. 여기에 사용되는
        함수가 AddRefSerialBuff로 할당 메모리 버퍼에 대한 참조 카운팅을
        수행하여 중복 반환을 막음.
    -   **On~ 접두사의 함수들**: JNetCore 단의 이벤트 가상 함수는 스레드 생성과 시작 및 중지
-   **이슈 사항(Issue)**
    -   세션 객체(JNetSession)의 생성 및 삭제에 있어서의 동기화
    -   송신 1회 제한에 대한 동기화
    -   송신 1회 제한으로 인한 오버헤드의 대응
        -   코어의 IOCP 작업자 스레드 수행 vs 컨텐츠 스레드의 수행
            (postToworker)
        -   반응성에 대한 타협 -&gt; BuffereSendPacekt,
            SendBufferedPacket
            
---

### *2.  JNetServer*
JNetCore의 자식 클래스, 클라이언트 연결 요청을 수립하며, 연결 시 세션
관리를 위한 객체 생성 및 비동기 송수신 관리를 JNetCore 상속 멤버들을
통해 수행. JNetServer 생성자 옵션에 따라 메시지 송수신 시 자체
프로토콜 인코딩, 디코딩을 수행하는 랩퍼 함수를 제공함.

![JNetServer](https://github.com/user-attachments/assets/0983d540-6e2a-4a74-a75d-cc02bf00aeee)


-   **주요 멤버 변수**
    -   **m\_ListenSocket:** JNetServer는 JNetCore에서 서버의 역할이
        구체화 된 클래스임. 클라이언트 연결 요청을 대기하고 확립하기
        위한 리슨 소켓
    -   **m\_AcceptThreadHnd:** 클라이언트 연결 요청을 대기하고 수립하기
        위해 분기되는 스레드 핸들
-   **주요 멤버 함수**
**(컨텐츠 단 호출 함수)**
    -   **SendPacket, SendPacektBlocking, BufferSendPacket:** 상위 클래스의
        Send 계열 함수를 랩핑한 함수로서 자체 프로토콜 패킷에 대한 간단한
        인코딩, 디코딩을 로직을 추가하여 패킷에 대한 탈취 시 메시지 해석에
        대한 방지와 무결성 검증을 수행함.
        (이미지)
    -   **AllocSerialSendBuff:** 상위 클래스의 직렬화 버퍼 할당 함수를
        랩핑한 함수로서 자체 프로토콜 헤더를 자동으로 부착하고, 헤더 내용을
        추가함(정적 고유 코드, 메시지 길이 삽입)
**(이벤트 함수)**
    -   **OnConnectionRequest:** 새로운 클라이언트 연결 요청에 대한 이벤트를
        처리할 수 있는 함수로서 해당 함수 재정의에서 클라이언트 수용 제한
        또는 특정 IP 주소를 차단하는 역할을 수행할 수 있음.
    -   **OnClientJoin:** OnConnectionRequest에 대한 별도의 재정의가 없거나
        true 반환 시 클라이언트 세션이 생성되며, 새로운 클라이언트의 접속을
        처리할 수 있음. 주로 컨텐츠 단에서 플레이어 생성으로 이어짐.
        전달되는 64비트 세션 ID는 고유성이 보장됨.
    -   **OnClientLeave:** 코어가 관리하는 네트워크 송수신 과정 중 TCP 연결
        종료에 대한 이벤트를 처리할 수 있음. 마찬가지로 플레이어 삭제로
        이어짐.
    -   **OnRecv**
        -   OnReve(SessionID64, JBuffer&): 자체 프로토콜 헤더가 붙은 단일
            메시지 수신을 처리할 수 있음.
        -   OnRecv(SessionID64, JSerialBuff\*): 위 버전의 OnRecv 함수는 TCP
            수신 버퍼에 단일 메시지 크기보다 큰 데이터가 수신되더라도 낱개
            씩 잘라서 이벤트 함수를 호출함. 따라서 OnRecv 함수 호출이 빈번해
            질 수 있음. 헤더 별 패킷 파싱을 하위 클래스에 전가하지만 이벤트
            함수 호출 빈도를 줄여 성능을 향상시키고자 추가된 버전의 함수
-   **이슈 사항(Issue)**
    -   OnRecv의 두 가지 버전

---

### *3.  JNetGroupServer와 JNetGroupThread*
코어 단에서 분기된 스레드를 통해 싱글 스레드 수행에 의존할 수 있음

![JNetGroup](https://github.com/user-attachments/assets/9f8070e4-78a6-4e4d-a5cb-a7ccc1653964)


-   **주요 멤버 변수 (JNetGroupServer)**
    -   **m\_SessionGroupMap:** 세션은 개별 그룹에 속하게 되고, 개별
        그룹은 싱글 스레드 그룹임. CreateGroup 함수를 통해 생성된 싱글
        스레드 그룹과 세션 간의 맵핑 자료구조
    -   **m\_SessionGroupMapSrwLock:** 그룹으로부터의 세션 삭제, 이동 등
        m\_SessionGroupMap 각각의 그룹 스레드에서 공유되는 자료구조임.
        그룹 간의 세션 이동보다는 세션이 어느 그룹에 속해 있는지
        확인하기 위한 코드가 빈번하므로 읽기 시 경합에 있어 베타성이 더
        자유로운 SRWLock을 사용함.
    -   **m\_GroupThreads:** CreateGroup을 통해 생성된 그룹 스레드 관리
-   **주요 멤버 함수 (JNetGroupServer)**
    -   **ForwardMessage:** 세션 ID와 메시지를 전달함. 세션이 속한
        그룹을 검색하여 그룹 스레드에 포워딩함. 그룹 스레드에메시지를
        전달한다 함은 'OnMessage' 이벤트 함수를 호출함을 의미.
    -   **SendGroupMessage:** 세션의 메시지와 별개로 그룹 간의 메시지를
        주고 받을 수 있는 인터페이스. 그룹 간의 메시지 전달은 크게
        그룹을 만드는 쪽에서 그룹 스레드를 상속한 클래스의 생성자 함수에
        인수로 전달하는 방법과 해당 함수를 통해 메시지를 전달하는
        방식임.
-   **주요 멤버 변수 (JNetGroupServer)**
    -   **m\_GroupThreadHnd:** 그룹 스레드에 해당하는 싱글 스레드 핸들
        객체
    -   **m\_Server:** JNetGroupServer는 JNetGroupThread 객체의 생성
        주기를 관리하고, JNetGroupThread가 그룹 서버를 참조하는 이유는
        단지 코어가 제공하는 함수를 호출하기 위함임. JNetGroupThread에는
        그룹 서버의 제공하는 그룹 생성, 삭제, 그리고 세션의 그룹 이동 뿐
        아니라 그 상위 부모의 함수인 SendPacket 계열의 함수와 Tls 메모리
        풀 할당 함수를 단순 맵핑하여 컨텐츠 코드에서 호출할 수 있도록
        함.
