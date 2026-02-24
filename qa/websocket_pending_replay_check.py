from dataclasses import dataclass


@dataclass
class MockConnection:
    pendingMessages: list[bytes]
    handlers: list

    def __init__(self):
        self.pendingMessages = []
        self.handlers = []

    def messageReceived(self, payload: bytes):
        stablePayload = bytes(payload)
        if not self.handlers:
            self.pendingMessages.append(stablePayload)
            return
        for handler in self.handlers:
            handler(stablePayload)

    def onMessageConnect(self, handler):
        self.handlers.append(handler)
        for payload in self.pendingMessages:
            stablePayload = bytes(payload)
            handler(stablePayload)
        self.pendingMessages = []


def runPendingReplayCheck():
    conn = MockConnection()
    queued = [b"alpha", b"beta\x00gamma", b"\x00\xff\x10z"]
    for payload in queued:
        conn.messageReceived(payload)

    received = []
    conn.onMessageConnect(lambda payload: received.append(payload))

    assert received == queued, f"pending replay mismatch: {received!r} != {queued!r}"
    assert conn.pendingMessages == [], "pending queue not flushed"


def runImmediateMessageCheck():
    conn = MockConnection()
    received = []
    conn.onMessageConnect(lambda payload: received.append(payload))
    livePayloads = [b"live-1", b"live\x00two"]
    for payload in livePayloads:
        conn.messageReceived(payload)

    assert received == livePayloads, f"live dispatch mismatch: {received!r} != {livePayloads!r}"


if __name__ == "__main__":
    runPendingReplayCheck()
    runImmediateMessageCheck()
    print("websocket pending replay checks passed")
