# ESP-C3-Jammer (Standalone)

Firmware **jammer SubGHz doc lap** cho ESP32-C3.

- ❌ Khong WiFi
- ❌ Khong man hinh OLED
- ❌ Khong nut bam
- ✅ **Cam nguon vao la tu dong gay nhieu toan bo dai tan**

## Nguyen ly hoat dong
CC1101 duoc dat o che do phat truc tiep (async serial data), liên tục chuyển đổi
ngẫu nhiên mức tín hiệu (OOK noise) và **quét vòng qua toàn bộ danh sách tần số**
Sub-GHz (300MHz → 928MHz, gồm cả 315 / 433.92 / 868 / 915 MHz).

## Cam day (giong sơ đồ cũ ESP-GRABER)
| CC1101 | ESP32-C3 |
|--------|----------|
| SCK  | GPIO 4 |
| MISO | GPIO 6 |
| MOSI | GPIO 7 |
| CS   | GPIO 5 |
| GDO0 | GPIO 10 |

## Nap code
### PlatformIO
`pio run --target upload`

### Firmware co san
Tu dong build bang GitHub Actions (xem Releases), nap bang esptool:
```bash
esptool.py --chip esp32c3 --port COMx write_flash 0x0 firmware.bin
```

## ⚠️ CANH BAO PHAP LY
Gay nhieu song vo tuyen la **bat hop phap** o hau het cac quoc gia (bao gom Viet Nam -
Nghi dinh 02/2023/ND-CP). Chi su dung cho muc dich hoc tap, thu nghiem trong moi truong
kin, tren thiet bi cua ban va co su cho phep. Moi hau qua phap ly thuoc ve nguoi su dung.
# ESP-GRABER-Web
# ESP32-C3-CC1101-JM
# ESP32-C3-CC1101-JM
