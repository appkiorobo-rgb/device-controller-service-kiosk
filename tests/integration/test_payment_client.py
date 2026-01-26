#!/usr/bin/env python3
"""
Payment Terminal Test Client
Flutter 없이 결제 단말기 기능을 테스트하는 Python 클라이언트

사용법:
    python test_payment_client.py status          # 상태 확인
    python test_payment_client.py start 10000     # 10,000원 결제 시작
    python test_payment_client.py cancel           # 결제 취소
    python test_payment_client.py reset           # 단말기 리셋
    python test_payment_client.py check            # 장치 체크
    python test_payment_client.py listen            # 이벤트 수신 (대기)
"""

import sys
import json
import uuid
import time
import struct
from typing import Optional, Dict, Any

try:
    import win32pipe
    import win32file
    import pywintypes
except ImportError:
    print("ERROR: pywin32가 설치되지 않았습니다.")
    print("설치 방법: pip install pywin32")
    sys.exit(1)


PIPE_NAME = r"\\.\pipe\DeviceControllerService"
PROTOCOL_VERSION = "1.0"


class DeviceServiceClient:
    def __init__(self, pipe_name: str = PIPE_NAME):
        self.pipe_name = pipe_name
        self.handle = None

    def connect(self) -> bool:
        """Named Pipe에 연결"""
        try:
            self.handle = win32file.CreateFile(
                self.pipe_name,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0,
                None,
                win32file.OPEN_EXISTING,
                0,
                None
            )
            print(f"✓ Connected to {self.pipe_name}")
            return True
        except pywintypes.error as e:
            print(f"✗ Failed to connect: {e}")
            return False

    def disconnect(self):
        """연결 종료"""
        if self.handle:
            win32file.CloseHandle(self.handle)
            self.handle = None

    def send_command(self, command_type: str, payload: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """명령어 전송 및 응답 수신"""
        if not self.handle:
            print("✗ Not connected")
            return None

        if payload is None:
            payload = {}

        command = {
            "protocolVersion": PROTOCOL_VERSION,
            "kind": "command",
            "commandId": str(uuid.uuid4()),
            "type": command_type,
            "timestampMs": int(time.time() * 1000),
            "payload": payload
        }

        try:
            # JSON 메시지 전송
            message = json.dumps(command).encode('utf-8')
            win32file.WriteFile(self.handle, message)

            # 응답 수신
            result, data = win32file.ReadFile(self.handle, 4096)
            response = json.loads(data.decode('utf-8'))
            return response
        except Exception as e:
            print(f"✗ Error sending command: {e}")
            return None

    def listen_events(self, timeout_seconds: int = 30):
        """이벤트 수신 (타임아웃 설정)"""
        if not self.handle:
            print("✗ Not connected")
            return

        print(f"Listening for events (timeout: {timeout_seconds}s)...")
        print("Press Ctrl+C to stop\n")

        start_time = time.time()
        try:
            while True:
                elapsed = time.time() - start_time
                if elapsed > timeout_seconds:
                    print(f"\nTimeout after {timeout_seconds} seconds")
                    break

                try:
                    # 논블로킹 읽기 시도
                    result, data = win32file.ReadFile(self.handle, 4096)
                    event = json.loads(data.decode('utf-8'))

                    if event.get('kind') == 'event':
                        self._print_event(event)
                except pywintypes.error as e:
                    if e.winerror == 109:  # ERROR_BROKEN_PIPE
                        print("Connection closed")
                        break
                    time.sleep(0.1)  # 짧은 대기
        except KeyboardInterrupt:
            print("\nStopped by user")

    def _print_event(self, event: Dict[str, Any]):
        """이벤트 출력"""
        event_type = event.get('eventType', 'unknown')
        device_type = event.get('deviceType', 'unknown')
        data = event.get('data', {})

        print(f"\n[{time.strftime('%H:%M:%S')}] Event: {event_type}")
        print(f"  Device: {device_type}")
        print(f"  Data: {json.dumps(data, indent=4, ensure_ascii=False)}")


def print_response(response: Dict[str, Any]):
    """응답 출력"""
    if not response:
        print("✗ No response received")
        return

    status = response.get('status', 'UNKNOWN')
    command_id = response.get('commandId', 'N/A')
    result = response.get('result', {})
    error = response.get('error')

    print(f"\nResponse:")
    print(f"  Status: {status}")
    print(f"  Command ID: {command_id}")

    if error:
        print(f"  Error Code: {error.get('code', 'N/A')}")
        print(f"  Error Message: {error.get('message', 'N/A')}")
    else:
        print(f"  Result: {json.dumps(result, indent=4, ensure_ascii=False)}")


def print_state_name(state: int) -> str:
    """상태 코드를 이름으로 변환"""
    states = {
        0: "DISCONNECTED",
        1: "CONNECTING",
        2: "READY",
        3: "PROCESSING",
        4: "ERROR",
        5: "HUNG"
    }
    return states.get(state, f"UNKNOWN({state})")


def cmd_status(client: DeviceServiceClient):
    """상태 확인"""
    print("Checking payment terminal status...")
    response = client.send_command("payment_status_check")
    print_response(response)

    if response and response.get('status') == 'OK':
        result = response.get('result', {})
        state = result.get('state')
        if state is not None:
            print(f"\nState: {print_state_name(state)}")


def cmd_start(client: DeviceServiceClient, amount: int):
    """결제 시작"""
    print(f"Starting payment: {amount:,}원")
    response = client.send_command("payment_start", {"amount": amount})
    print_response(response)

    if response and response.get('status') == 'OK':
        result = response.get('result', {})
        state = result.get('state')
        print(f"\nState: {print_state_name(state)}")
        print("\n⚠️  결제 결과는 이벤트로 수신됩니다. 'listen' 명령으로 확인하세요.")


def cmd_cancel(client: DeviceServiceClient):
    """결제 취소"""
    print("Cancelling payment...")
    response = client.send_command("payment_cancel")
    print_response(response)


def cmd_reset(client: DeviceServiceClient):
    """단말기 리셋"""
    print("Resetting payment terminal...")
    response = client.send_command("payment_reset")
    print_response(response)


def cmd_check(client: DeviceServiceClient):
    """장치 체크"""
    print("Checking payment device...")
    response = client.send_command("payment_device_check")
    print_response(response)


def cmd_listen(client: DeviceServiceClient, timeout: int = 30):
    """이벤트 수신"""
    client.listen_events(timeout)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    command = sys.argv[1].lower()
    client = DeviceServiceClient()

    if not client.connect():
        sys.exit(1)

    try:
        if command == "status":
            cmd_status(client)
        elif command == "start":
            if len(sys.argv) < 3:
                print("Usage: python test_payment_client.py start <amount>")
                sys.exit(1)
            amount = int(sys.argv[2])
            cmd_start(client, amount)
        elif command == "cancel":
            cmd_cancel(client)
        elif command == "reset":
            cmd_reset(client)
        elif command == "check":
            cmd_check(client)
        elif command == "listen":
            timeout = int(sys.argv[2]) if len(sys.argv) > 2 else 30
            cmd_listen(client, timeout)
        else:
            print(f"Unknown command: {command}")
            print(__doc__)
            sys.exit(1)
    finally:
        client.disconnect()


if __name__ == "__main__":
    main()
