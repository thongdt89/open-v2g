# Kiểm tra bộ bàn giao

Ngày kiểm tra: 2026-09-08.

## Chuyển đổi Word sang Markdown

So sánh AST từ Pandoc cho Word nguồn và Markdown đầu ra: số tiêu đề, số bảng và tập từ có đếm số lần xuất hiện. Kiểm tra từ không thay thế review ngữ nghĩa hay xác nhận normative correctness. Alt text/caption của hình có thể được biểu diễn thêm trong Markdown.

| Tài liệu | Tiêu đề nguồn/MD | Bảng nguồn/MD | Kiểm tra từ |
|---|---|---|---|
| EVCC_AUTOSAR_Charging_Architecture | 128/128 | 96/96 | Không thiếu từ nguồn trong kiểm tra token |
| EVCC_AUTOSAR_Project_Overview_VI | 28/28 | 15/15 | Không thiếu từ nguồn trong kiểm tra token |
| EVCC_AUTOSAR_Two_Main_Compositions_Detailed_Description_VI | 81/81 | 47/47 | Không thiếu từ nguồn trong kiểm tra token |

Số bảng và tiêu đề của cả ba tài liệu khớp bản nguồn. Hình chuyển sang cú pháp Markdown và một HTML table đã chuyển thành pipe table để đọc thuận tiện.

## Kiểm tra đóng gói

- Atomic specification: bản sao nguyên byte của file đã lấy được.
- Ba Word gốc: so sánh nguyên byte thành công.
- Scope đầu vào: đọc đủ 25 dòng, không còn phần tiếp theo; khớp kích thước UTF-8 11.997 byte.
- 19 hình embedded từ architecture đã trích xuất; các đường dẫn ảnh/tài liệu nội bộ được kiểm tra tồn tại.
- Các Markdown trong gói đọc được bằng UTF-8.
- Manifest SHA-256 được tạo cho toàn bộ file payload, trừ chính manifest.
- Sau khi tạo ZIP: kiểm tra CRC, danh sách member và byte nội dung từng file khớp thư mục nguồn.

## Ngoài phạm vi kiểm tra lần xuất

Không chạy build, unit test, HIL, WCET, safety validation hoặc protocol conformance. Không sửa thiết kế nguồn để đóng các open points. Không chứng nhận tất cả datatype/signature/standard claims đã đúng hoặc frozen. Bản Word nguyên gốc chỉ được sao chép, không sửa và không tái kiểm tra bố cục in trong đợt bàn giao Markdown/ZIP này.
