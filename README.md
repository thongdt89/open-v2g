# V2G — Bộ bàn giao cho Codex

Ngày xuất: 2026-09-08. Ngôn ngữ: tiếng Việt, giữ nguyên technical identifiers.

Đây là repository ngữ cảnh, tài liệu thiết kế và bộ khung G1 host/SIL cho EVCC/AUTOSAR. Bộ khung C11 hiện tại phục vụ phát triển logic độc lập target và kiểm thử kiến trúc; đây không phải production AUTOSAR implementation hay release đã freeze.

## Bắt đầu

1. Giải nén ZIP và mở thư mục `V2G_Codex_Handoff` bằng Codex; hoặc chép nội dung vào repository dự án.
2. Đọc [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md), [DECISIONS.md](DECISIONS.md), rồi [docs/INDEX.md](docs/INDEX.md).
3. `AGENTS.md` chứa hướng dẫn làm việc và thứ tự đọc tài liệu. Nếu repository đã có `AGENTS.md`, hợp nhất nội dung phù hợp; không ghi đè mất chỉ dẫn đang có.
4. Giao nhiệm vụ tiếp theo với tài liệu trong thư mục này làm ngữ cảnh. Không cần dán lại từng cuộc trao đổi.

Ví dụ nhiệm vụ khởi động:

```text
Đọc AGENTS.md, PROJECT_CONTEXT.md, DECISIONS.md và docs/INDEX.md.
Tiếp tục dự án EVCC AUTOSAR theo bản atomic interface Revision 3.
Trước tiên đối chiếu interface/type/state-machine consistency và lập danh sách
open points cần đóng để đạt R0/WP0. Giữ nguyên requirement/interface IDs.
Phân biệt thiết kế đã được ghi nhận, đề xuất mới và bằng chứng còn thiếu.
Không tự coi các imported AUTOSAR types hoặc safety timing là đã freeze.
```

## Thành phần

| Đường dẫn | Mục đích |
|---|---|
| [AGENTS.md](AGENTS.md) | Chỉ dẫn Codex cho thư mục/repository |
| [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md) | Ngữ cảnh nhanh, mục tiêu, kiến trúc và tiến độ |
| [DECISIONS.md](DECISIONS.md) | Quyết định được ghi nhận, open points và các chỗ cần đối chiếu |
| [NEXT_STEPS.md](NEXT_STEPS.md) | Danh sách công việc đề xuất để tiếp tục |
| [docs/INDEX.md](docs/INDEX.md) | Chỉ mục đọc theo chủ đề và mức ưu tiên nguồn |
| `docs/*.md` | Specification, architecture, composition, project plan và scope đầu vào |
| `docs/assets/` | 19 hình trích nguyên bản từ tài liệu kiến trúc |
| `references/originals/` | Ba file Word gốc, giữ nguyên byte |
| `references/images/` | Hình tham chiếu đã có trong Project |
| [SOURCES.md](SOURCES.md) | Nguồn, revision và giới hạn của gói xuất |
| [VALIDATION.md](VALIDATION.md) | Kiểm tra đóng gói và chuyển đổi đã thực hiện |
| `include/openv2g/` | Public logical contracts cho host/SIL; không phải supplier ABI |
| `src/` | Skeleton cho ChargingCoordinator, ISO15118Chrg và supporting domains |
| `integration/` | Điểm tích hợp bị chặn cho AUTOSAR/supplier và protocol-generated artifacts |
| `tests/` | Unit/integration tests cho các hard invariant ở G1 |
| `examples/host_demo/` | Ví dụ hiển thị feature baseline provisional |

## Bộ khung G1 host/SIL

Active scope tạm thời là DIN SPEC 70121 DC EIM và ISO 15118-2 DC EIM. PnC, ISO 15118-20 và BPT mặc định bị tắt. Xem [mô tả skeleton](docs/architecture/HOST_SIL_SKELETON.md) và [Provisional Baseline Proposal](docs/PROVISIONAL_BASELINE_PROPOSAL_VI.md).

Build dự kiến bằng CMake và một C11 compiler:

```powershell
cmake -S . -B build-host
cmake --build build-host --config Debug
Push-Location build-host
ctest -C Debug --output-on-failure
Pop-Location
```

Các lệnh trên chỉ tạo host/SIL artifacts. Chúng không generate RTE/ARXML và không chứng minh target, HIL, timing, safety hoặc protocol conformance.

## Trạng thái bàn giao

- Đã có tài liệu thiết kế chi tiết hai composition chính, atomic interfaces và project datatype catalogue.
- Đã có G1 host/SIL skeleton và test source; kết quả chạy cụ thể phụ thuộc toolchain được ghi trong lần kiểm tra tương ứng.
- Chưa có bằng chứng cho target build, RTE generation, HIL/conformance, timing/WCET hoặc phê duyệt safety.
- Checkbox hoàn tất trong tài liệu nguồn là trạng thái do tài liệu đó khai báo; đợt xuất này không tái chứng nhận tính đầy đủ kỹ thuật.
- Snapshot tài liệu không tự cập nhật khi các cuộc chat hoặc repository thay đổi. Khi có quyết định mới, cập nhật context, decision log và tài liệu liên quan cùng nhau.
