# lab3-giam-sat-nguoi-chay

Báo cáo Lab 3 – ESP32 + BMP180 + FreeRTOS + MQTT

# BÁO CÁO LAB 3 – GIÁM SÁT VÀ CẢNH BÁO NGƯỜI CHẠY

### 1. Thông tin đề tài

Thiết kế hệ thống giám sát và cảnh báo cho người chạy bằng **ESP32**, cảm biến **BMP180**, **FreeRTOS**, **Wi-Fi** và giao thức **MQTT**.

Hệ thống đo **nhiệt độ, áp suất khí quyển và độ cao**, gửi dữ liệu lên MQTT Broker, hiển thị thông số và biểu đồ thời gian thực trên website. Đèn LED và còi cảnh báo hoạt động theo ngưỡng ở chế độ **AUTO** hoặc được điều khiển từ website ở chế độ **MANUAL**.


### 2. Báo cáo

[📄 Xem báo cáo Lab 3](https://view.officeapps.live.com/op/view.aspx?src=https%3A%2F%2Fraw.githubusercontent.com%2FZShyKa%2Flab3-de1_DangBaoPhuc%2Fmain%2FLab3_De1_DangBaoPhuc_231432316.docx)


### 3. Mô phỏng Wokwi

[*Link Wokwi*](https://wokwi.com/projects/477941224324494337)

### 4. Giao diện Website

[[Xem giao diện Website](./web.html)](https://zshyka.github.io/lab3-de1_DangBaoPhuc/web.html)

Giao diện hiển thị nhiệt độ, áp suất, độ cao, hai biểu đồ theo thời gian thực và các nút điều khiển chế độ, LED, còi cảnh báo. Website sử dụng **MQTT.js** và **Chart.js**.


### 5. Video kết quả thực nghiệm

[Xem video mô phỏng và kết quả thực nghiệm](https://github.com/ZShyKa/lab3-de1_DangBaoPhuc/blob/main/videoketqua.mp4)
