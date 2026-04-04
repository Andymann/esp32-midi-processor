# ESP32 MIDI Processor — Signal Flow Diagrams

## 1. Top-Level Signal Flow

```mermaid
flowchart LR
    IN1["MIDI In 1\n(Serial1 / UART)"]
    IN2["MIDI In 2\n(Serial2 / UART)"]
    IN3["USB MIDI In\n(MAX3421E Host)"]

    R1["Routing In1\n(None/1/2/3/1+2/1+3/2+3/All)"]
    R2["Routing In2\n(None/1/2/3/1+2/1+3/2+3/All)"]
    R3["Routing In3\n(None/1/2/1+2)"]

    P1["Output 1\nProcessing Pipeline"]
    P2["Output 2\nProcessing Pipeline"]
    P3["USB Output\nProcessing Pipeline"]

    OUT1["MIDI Out 1\n(Serial1 / UART)"]
    OUT2["MIDI Out 2\n(Serial2 / UART)"]
    OUT3["USB MIDI Out\n(MAX3421E Host)"]

    IN1 --> R1
    IN2 --> R2
    IN3 --> R3

    R1 -->|"to Out1"| P1
    R1 -->|"to Out2"| P2
    R1 -->|"to USB"| P3

    R2 -->|"to Out1"| P1
    R2 -->|"to Out2"| P2
    R2 -->|"to USB"| P3

    R3 -->|"to Out1"| P1
    R3 -->|"to Out2"| P2

    P1 --> OUT1
    P2 --> OUT2
    P3 --> OUT3
```

> **Note:** USB In (In3) cannot be routed back to USB Out — only to Out1, Out2, or both.

---

## 2. Per-Output Processing Pipeline

Each of the three outputs has an independent processing pipeline applied to every MIDI message routed to it.

```mermaid
flowchart TD
    A["Incoming MIDI packet\n(from routing)"]
    B["Velocity Modifier\n(passthru / fix 63,100,127 / random / random-100)\nNote On/Off only"]
    C["Note Channel Remap\n(passthru or Ch 1–16)\nNote On/Off only"]
    D["CC Channel Remap\n(passthru or Ch 1–16)\nCC messages only"]
    E["Scale Filter\n(passthru / Major / Minor /\nPentatonic Maj / Pentatonic Min)\nNote On/Off only — drop if out of scale"]
    F["Clock Filter\n(pass all / block clock / clock only)"]
    G{"drop flag set?"}
    H["Send to Output\n(Serial1 / Serial2 / USB)"]
    X["Discard"]

    A --> B --> C --> D --> E --> F --> G
    G -->|"no"| H
    G -->|"yes"| X
```

---

## 3. Routing Options Matrix

Each input has an independent routing setting. The table below lists all values.

```mermaid
flowchart LR
    subgraph "Routing values"
        direction TB
        NONE["None — dropped"]
        TO1["→ Out 1 only"]
        TO2["→ Out 2 only"]
        TO3["→ USB only"]
        TO12["→ Out 1 + Out 2"]
        TO13["→ Out 1 + USB"]
        TO23["→ Out 2 + USB"]
        TO123["→ Out 1 + Out 2 + USB"]
    end

    subgraph "Input 1  (full set)"
        I1(In 1) --> NONE & TO1 & TO2 & TO3 & TO12 & TO13 & TO23 & TO123
    end

    subgraph "Input 2  (full set)"
        I2(In 2) --> NONE2["None"] & TO1b["→ Out 1"] & TO2b["→ Out 2"] & TO3b["→ USB"] & TO12b["→ Out 1+2"] & TO13b["→ Out 1+USB"] & TO23b["→ Out 2+USB"] & TO123b["→ All"]
    end

    subgraph "Input 3 / USB  (limited)"
        I3(USB In) --> NONE3["None"] & TO1c["→ Out 1"] & TO2c["→ Out 2"] & TO12c["→ Out 1+2"]
    end
```

---

## 4. Per-Output Modifier Details

```mermaid
flowchart TD
    subgraph "Velocity  (Note On only)"
        V0[Passthru]
        V1[Fix 63]
        V2[Fix 100]
        V3[Fix 127]
        V4[Random 0–127]
        V5[Random 91–110]
    end

    subgraph "Scale Filter  (Note On/Off only)"
        S0[Passthru — all notes pass]
        S1[Major scale]
        S2[Natural Minor scale]
        S3[Pentatonic Major]
        S4[Pentatonic Minor]
        SN[Root Note: C C# D D# E F F# G G# A A# B]
    end

    subgraph "Channel Remap"
        C0[Note Ch: passthru or 1–16]
        C1[CC Ch: passthru or 1–16]
    end

    subgraph "Clock Filter"
        F0[Pass All — default]
        F1[Block Clock — drop F8/FA/FC]
        F2[Clock Only — pass only F8/FA/FC]
    end
```

---

## 5. MIDI Queue / Processing Order

```mermaid
sequenceDiagram
    participant HW1 as Serial1 (In 1)
    participant HW2 as Serial2 (In 2)
    participant USB as USB MIDI (In 3)
    participant Q  as MIDI Queue (FIFO, 10 slots)
    participant SP as sendPacket()
    participant O1 as Out 1
    participant O2 as Out 2
    participant O3 as USB Out

    loop Every loop()
        HW1->>Q: push (source=1)
        HW2->>Q: push (source=2)
        USB->>Q: push (source=3)
        Q->>SP: pop & process in order
        SP->>SP: apply routing
        SP->>SP: velocity / channel / scale / clock filter (per output)
        SP->>O1: write if routed
        SP->>O2: write if routed
        SP->>O3: send if routed
    end
```
