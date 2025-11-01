# Pet-Tracker
This repository is a submission for XIAO Showdown. We have implemented a pet tracker using the xiao mg24 sense board with lora module. We wanted to build a prototype to track our indoor/outdoor pets and to know about their whereabouts.

# Components
1. Xiao MG24 sense board
2. Lora SX1278 Ra-02
3. GPS NEO-7M module

# Circuit diagram
Lora - Xiao connections:
- NSS/CS → D2
- DIO0   → D3
- RESET  → D4
- SCK    → D8
- MISO   → D9
- MOSI   → D10

GPS - Xiao connections:
- GPS TX → D7 (RX1)
- GPS RX → D6 (TX1)
- VCC → 3.3V 
- GND → GND

**Receiver diagram:**

<img width="2740" height="1833" alt="circuit_image" src="https://github.com/user-attachments/assets/8041f40e-eda3-4f84-839b-9d544217628f" />

**Transmitter diagram:**

<img width="3000" height="1741" alt="circuit_image (1)" src="https://github.com/user-attachments/assets/2529dc81-3885-4ca0-b9b0-5888f9ab2082" />
