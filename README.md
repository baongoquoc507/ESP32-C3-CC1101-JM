# ESP-C3-DeauthDetector

Phat hien tan cong **Deauth WiFi** bang ESP32-C3.

- Phat WiFi AP: `Quốc Bảo` / `12345678` → vao **http://192.168.4.1**
- Nghe promiscuous tren kenh 2.4GHz da chon
- Hien thi cac mang WiFi xung quanh (SSID, MAC, kenh, RSSI)
- Dem va log cac frame DEAUTH (0xC0) - bao dong tren web + LED GPIO8
- Doi kenh thu tu web (1-13)

## Su dung
1. Nap firmware (PlatformIO: `pio run --target upload`, hoac flash file
   `firmware.bin` trong Releases bang esptool)
2. Noi WiFi `Quốc Bảo` / `12345678`, mo trinh duyet vao 192.168.4.1
3. Chon kenh WiFi can giam sat -> Set Channel
4. Khi co deauth attack, web hien canh bao do + LED nhap nhay

## Luu y
Che do promiscuous chay dong thoi voi AP nen chi nghe duoc tren 1 kenh tai
mot thoi diem. Chon dung kenh cua mang can bao ve.
