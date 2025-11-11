# Network Programming Exam

**Task**: Implement a registry server/client system in any language.

## Overview

Build a DNS-like registry service that maps service names to IP:Port pairs.

**Example**:
- `web-server` → `192.168.1.10:8080`, `192.168.1.11:8080`
- `db-server` → `192.168.1.20:5432`

---

## Server Operations

The server must support two operations:

### 1. GET Operation

**Purpose**: Query service name → returns IP:Port pairs

**Protocol**: 
- **UDP** for small responses (≤ 3 IP addresses)
- **TCP** fallback for large responses (> 3 IP addresses)

**Behavior**:
- Client sends service name
- Server responds with one or more IP:Port pairs
- If more than 3 results → server responds "USE_TCP" via UDP
- Client must reconnect via TCP to get full list

**Example**:
```
Client: GET web-server
Server: 192.168.1.10:8080
        192.168.1.11:8080
```

---

### 2. SET Operation

**Purpose**: Register/add IP:Port pair for a service name

**Protocol**: 
- **TCP only** (requires reliability and authentication)

**Authentication**:
- Client must provide a password
- Server validates password before updating
- Reject requests with invalid passwords

**Behavior**:
- Client sends: service name, IP, port, password
- Server validates password
- If valid → adds IP:Port to service's list
- Multiple IP:Port pairs can be registered for same service

**Example**:
```
Client: SET web-server 192.168.1.12:8080 secret123
Server: OK (or ERROR if password wrong)
```

---

## Additional Requirements

### 1. Multiple Clients Support
- Server must handle multiple clients simultaneously
- Use **threads** or **select()** for concurrency

### 2. Thread Safety
- Protect registry updates with **locks**
- Prevent reading a record while it's being updated
- No race conditions allowed

### 3. UDP to TCP Fallback
- Count results before sending
- If ≤ 3 results → send via UDP
- If > 3 results → tell client to use TCP

---

## Implementation Details

### Data Structure
```
Registry:
  "web-server"   → [(192.168.1.10, 8080), (192.168.1.11, 8080)]
  "db-server"    → [(192.168.1.20, 5432)]
  "api-gateway"  → [(192.168.1.30, 3000), (192.168.1.31, 3000)]
```

### Concurrency Model
- **Option 1**: Thread per client (recommended)
- **Option 2**: select() for I/O multiplexing
- **Must**: Use locks for shared data

### Protocol
- **Text-based** (simplest)
- Clear message format for GET/SET
- Handle errors gracefully

---

## Grading Criteria

| Category | Points | Requirements |
|----------|--------|--------------|
| **Basic Functionality** (PASS) | 50% | Server starts, GET works (UDP), SET works (TCP), Password validation |
| **Protocol** | 15% | Correct UDP/TCP handling, proper message format |
| **Concurrency** | 20% | Multiple clients work, thread-safe updates |
| **UDP→TCP Fallback** | 10% | Correctly switches when > 3 results |
| **Code Quality** | 5% | Clean code, error handling |

**Note**: Basic Functionality (50%) is required to pass the exam.

---

## Testing Checklist

- [ ] Server starts without errors
- [ ] GET returns correct results (1-3 addresses via UDP)
- [ ] GET switches to TCP when > 3 addresses
- [ ] SET adds new entries (TCP only)
- [ ] SET validates password correctly
- [ ] Wrong password is rejected
- [ ] Multiple clients can connect simultaneously
- [ ] No race conditions (concurrent SET operations)
- [ ] Client disconnect handled gracefully

---

## Language Choice

You can use **any language**. Common choices:
- Python
- C
- C++

---

