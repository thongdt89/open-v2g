# Nguồn và provenance

Ngày xuất: 2026-09-08. Bộ này dùng các tài liệu hiện có trong Project V2G và ngữ cảnh trao đổi liên quan. Không bổ sung nghiên cứu kỹ thuật mới hoặc khẳng định lại tình trạng upstream repositories trong đợt xuất.

## Tài liệu nguồn đã đưa vào gói

| Nguồn | Revision ghi trong nội dung | Bản trong ZIP | Xử lý |
|---|---|---|---|
| EVCC_AUTOSAR_Two_Main_Compositions_Atomic_Interface_Spec_VI.md | Rev.3 — 08/09/2026 | docs/ cùng tên | Sao chép nguyên byte; đây là bản mới nhất tìm thấy cho atomic interfaces/datatype catalogue |
| EVCC_AUTOSAR_Two_Main_Compositions_Detailed_Description_VI.docx | Rev.1 — 08/09/2026 | docs/ bản .md và references/originals/ bản .docx | Chuyển nội dung sang GFM; giữ Word gốc |
| EVCC_AUTOSAR_Project_Overview_VI.docx | Rev.1 — 07/09/2026 | docs/ bản .md và references/originals/ bản .docx | Chuyển nội dung sang GFM; giữ Word gốc |
| EVCC_AUTOSAR_Charging_Architecture.docx | Rev.4 — 07/09/2026 | docs/ bản .md, docs/assets/ và references/originals/ | Chuyển nội dung sang GFM; trích 19 hình; giữ Word gốc |
| Pasted markdown.md | Không ghi revision | docs/Scope_Input_Original.md | Đổi tên dễ tra cứu; nội dung UTF-8 nguyên văn, 11.997 byte |
| 646a7975-ce59-4b75-a6d9-e126515d5216.png | Không ghi revision | references/images/ cùng tên | Giữ hình tham chiếu gốc; không diễn giải lại hoặc xem là specification |

Revision ở bảng này là revision bên trong tài liệu, không phải số phiên bản lưu trữ. Ví dụ Architecture ghi Revision 4 trong nội dung; Atomic specification ghi Revision 3.

## File mới do đợt bàn giao tạo

`README.md`, `AGENTS.md`, `PROJECT_CONTEXT.md`, `DECISIONS.md`, `NEXT_STEPS.md`, `docs/INDEX.md`, `SOURCES.md`, `VALIDATION.md` và `MANIFEST.sha256` là artifacts bàn giao mới. Chúng hướng dẫn đọc và ghi trạng thái nguồn; không phải supplier specification hoặc formal approval record.

Chronology trong PROJECT_CONTEXT.md dựa trên ngữ cảnh dự án truy xuất được. Phát biểu kỹ thuật và open points ưu tiên nội dung các tài liệu nguồn. Không có bản transcript đầy đủ của mọi cuộc chat trong ZIP.

## Tài liệu/repository được nhắc tới nhưng không được đóng gói

- EVerest: `https://github.com/EVerest/EVerest` — nguồn khảo sát ban đầu trong trao đổi dự án.
- SwitchEV iso15118: `https://github.com/SwitchEV/iso15118` — reference implementation/test oracle trong project plan.
- SwitchEV pyslac: `https://github.com/SwitchEV/pyslac` — reference SLAC; limitations trong project plan là nhận định của baseline lúc lập tài liệu.
- AUTOSAR Classic, DIN SPEC 70121, ISO 15118-2/-3/-20 và licensed schemas được các tài liệu nguồn dẫn chiếu.

Các link chỉ định danh nguồn đã được nhắc tới; đợt xuất không tải repo, pin commit hoặc kiểm chứng lại support/features. Không có giấy phép tiêu chuẩn/schema, supplier ARXML/header, target source code, credential/private key hoặc test result được thêm vào gói.

## Giới hạn chuyển đổi

- Word → Markdown giữ nội dung và cấu trúc đọc; không bảo toàn page breaks, font, kích thước bảng hoặc header/footer trình bày.
- Hình được giữ nguyên và liên kết tương đối từ Markdown. Các caption/alt text có thể hiện khác với bố cục Word.
- Khi cần đối chiếu hình thức hoặc một chi tiết chuyển đổi, dùng Word gốc. Khi cần exact normative behavior, dùng selected licensed standard/supplier artifact ngoài gói này.
- Bản source scope có bất nhất SDP/SLAC và các claim protocol cần kiểm tra; xem DECISIONS.md. Việc giữ nguyên không đồng nghĩa xác nhận đúng chuẩn.
