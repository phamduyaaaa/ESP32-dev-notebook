# 📓 Notebook ESP32: Điều Khiển 2 Driver

> _Ghi chú quá trình thực hiện điều khiển động cơ qua giao thức Modbus RTU sử dụng ESP32 và FreeRTOS._

---

## 📌 Mục tiêu

- Tìm hiểu sâu về phần cứng ESP32
- Tìm hiểu cách giao tiếp với driver động cơ qua Modbus RTU (chuẩn RS485)
- Ứng dụng **FreeRTOS** để phân chia tác vụ điều khiển
- Làm quen với cách sử dụng **ESP32 lõi kép**
- Sử dụng **OTA** để upload code thông qua Wifi
- Ghi chú lại toàn bộ quá trình học như một _notebook_

---

## 📌 Ghi chú
### 0. **ESP32**
![image](https://github.com/user-attachments/assets/407a370b-c19e-4456-8d66-9fe44d57f56e)

  [Hình ảnh: nguồn internet]  
Giải thích từ trái sang phải: 
| **Thành phần**                 | **Chức năng**                                                                 |
|-------------------------------|------------------------------------------------------------------------------|
| **Reset (RST)**               | Dùng để khởi động lại ESP32                                                 |
| **USB**                       | Dùng để nạp code từ máy tính vào ESP32                                      |
| **Boot (BOOT)**               | Dùng để chuyển ESP32 vào chế độ nạp code (nạp thủ công)                     |
| **External Voltage Source**   | Cấp nguồn cho ESP32, thường sử dụng điện áp 5V                              |
| **3.3V Voltage Regulator**    | Hạ áp từ 5V xuống 3.3V để phù hợp với mức điện áp làm việc của ESP32        |
| **Status LED**                | Đèn báo trạng thái hoạt động của ESP32                                      |
| **USB to UART Bridge**        | Chuyển đổi tín hiệu USB ↔ UART giúp máy tính giao tiếp với ESP32           |
| **ESP-WROOM-32 Chip**         | Vi xử lý chính gồm: 2 nhân Xtensa LX6, Wi-Fi, Bluetooth, RAM, ROM, Flash,...|
| **2.4 GHz Antenna**           | Ăng-ten dùng để thu/phát tín hiệu Wi-Fi và Bluetooth                        |
 
 _Flash là bộ nhớ của ESP32, bao gồm:_  
- **Firmware App**: chứa chương trình
- **OTA Partition**: Update không dây (Over the air)
- **SPIFFS/LittleFS/FATFS**: chứa file tĩnh
- **NVS**: EEPROM (chip nhớ bán dẫn)
- **Bootloader** + **Partition Table**: Khởi động hệ thống

_ESP-WROOM-32 Chip gồm 2 nhân với tốc độ 240MHz:_  
- **CORE0**: Tên là PRO_CPU, mặc định sử dụng cho Wifi và Bluetooth
- **CORE1**: Tên là APP_CPU, mặc định sử dụng cho void loop()

 _Khác_:
- **RainMaker**: Đây là một nền tảng IoT của Espressif giúp bạn tạo ứng dụng điều khiển thiết bị từ xa qua đám mây.
Khi bạn sử dụng RainMaker SDK, chương trình và các cấu hình của nó sẽ được lưu vào flash.
_NOTE_: Khi bạn sử dụng Arduino IDE (thời điểm tôi sử dụng là phiên bản 2.3.5), tại thẻ Tools bạn sẽ thấy
có nhiều tùy chỉnh khác nhau, có những tùy chỉnh quan trọng sau:
- **CPU Frequency**: Tốc độ CPU, cái này tùy vào mainboard của bạn. Nhưng nếu không biết thì thử từ cao nhất, nếu lỗi
thì hạ dần xuống
- **Events Run On**: Lựa chọn Core xử lý các Event như Wifi, Bluetooth, ..., nên chọn Core0
- **Flash Frequency**: Tốc độ Flash, cái này cũng theo mainboard.
- **Flash Mode**: QIO hoặc DIO.Về cơ bản QIO sẽ 'xịn' hơn và nhanh hơn. Bạn thử QIO trước nếu không được thì sử dụng DIO
- **Flash Size**: Dung lượng Flash của bạn. Cái này bạn xem ở thông số mainbroad bạn sử dụng
- **Arduino Run On**: Lựa chọn Core xử lý setup() + loop(), nên chọn Core1
- **Partition Scheme**: Cấu hình Flash, tùy vào mục đích của bạn thì cách cấu hình sẽ khác nhau,
nhưng thường tác vụ đon giản nếu bạn upload code và chạy ok thì không cần quá quan tâm
- **Upload Speed**: Cái này là tốc độ nạp code xuống ESP32. Về lý thuyết thì càng cao càng tốt nhưng
có thể broad của bạn bị giới hạn thông số này. Dễ nhất thì bạn cứ thử từ cao nhất, nếu không upload code được
thì hạ dần xuống.

### 1. **Cách hoạt động của Modbus RTU**
- Giao tiếp dạng master-slave, mỗi driver là một thiết bị slave còn ESP32 là master
- Gửi/nhận dữ liệu qua UART bằng chuẩn RS485
- Sử dụng thư viện `ModbusMaster` trong Arduino
- Truyền tuần tự, không song song: Vì RS485 là half-duplex, chỉ 1 thiết bị được truyền tại một thời điểm.

- Master phải chờ nhận xong phản hồi của Slave trước khi chuyển sang Slave khác.

- Không có "broadcast trả lời": Modbus có lệnh broadcast (địa chỉ 0) → tất cả slave thực hiện nhưng không phản hồi.

- Ví dụ: 00 06 00 01 00 64 CRC → yêu cầu ghi giá trị 100 vào thanh ghi 1 ở tất cả các slave.

- Master phải quản lý thời gian timeout: Nếu gửi cho Slave 1 nhưng không có phản hồi trong thời gian quy định (VD: 1 giây), thì Master cần bỏ qua và chuyển sang Slave 2.

### 2. **FreeRTOS trên ESP32**
- Giúp tổ chức chương trình theo luồng riêng biệt (tasks)
- Cho phép xử lý song song 2 driver một cách mượt mà

### 3. **Sử dụng Wifi trên ESP32**
-
-
---

## 📌 Quá trình thử nghiệm
### Phần cứng
| Thiết bị         | Mô tả                                 |
|------------------|----------------------------------------|
| ESP-WROOM-32     | Vi điều khiển lõi kép, hỗ trợ Wi-Fi    |
| ZLAC8015D        | Driver hỗ trợ Modbus, số lượng 2 |
| RS485 TTL Module | Bộ chuyển đổi UART ↔ RS485 (MAX485) |
| Nguồn            | 24V cho driver, 5V cho ESP32           |
| Nút nhấn         | Dùng cho chức năng dừng khẩn cấp       |
### Sơ đồ dây
###  Tạo task với FreeRTOS
```cpp
xTaskCreatePinnedToCore(Task1, "Task1", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xTask1Handle, PRO_CPU);
xTaskCreatePinnedToCore(Task2, "Task2", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xTask2Handle, APP_CPU);

