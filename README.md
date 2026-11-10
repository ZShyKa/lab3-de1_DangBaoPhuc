# lab3-giam-sat-nguoi-chay

Báo cáo Lab 3 – ESP32 + BMP180 + FreeRTOS + MQTT

# BÁO CÁO LAB 3 – GIÁM SÁT VÀ CẢNH BÁO NGƯỜI CHẠY

### 1. Thông tin đề tài

Thiết kế hệ thống giám sát và cảnh báo cho người chạy bằng **ESP32**, cảm biến **BMP180**, **FreeRTOS**, **Wi-Fi** và giao thức **MQTT**.

Hệ thống đo **nhiệt độ, áp suất khí quyển và độ cao**, gửi dữ liệu lên MQTT Broker, hiển thị thông số và biểu đồ thời gian thực trên website. Đèn LED và còi cảnh báo hoạt động theo ngưỡng ở chế độ **AUTO** hoặc được điều khiển từ website ở chế độ **MANUAL**.

### 2. Báo cáo

[Download báo cáo Lab 3](./Lab3_De1_DangBaoPhuc_231432316.docx)

### 3. Mô phỏng Wokwi

[*Link Wokwi*](https://wokwi.com/projects/477941224324494337)

### 4. Giao diện Website

[Xem mã nguồn giao diện Website](./web.html)

Giao diện hiển thị nhiệt độ, áp suất, độ cao, hai biểu đồ theo thời gian thực và các nút điều khiển chế độ, LED, còi cảnh báo. Website sử dụng **MQTT.js** và **Chart.js**.
