# Assessment requirements traceability

| Assessment requirement | Implemented in |
| --- | --- |
| Abstract `PatientRecord`, three patient types and `Appointable` | `include/scapms/model/PatientRecord.hpp`, `GeneralPatient.hpp`, `EmergencyPatient.hpp`, `FollowUpPatient.hpp`, `src/model/PatientRecord.cpp` |
| Runtime polymorphism | Category-specific virtual methods in patient subclasses; dashboard serializes through `PatientRecord` references in `src/server/HttpServer.cpp` |
| Appointment composition | `PatientRecord` owns `std::vector<Appointment>` |
| `ClinicDatabase` and separated controllers | `include/scapms/model/ClinicDatabase.hpp`, `ClinicManager.hpp`, `AppointmentController.hpp`, `SearchEngine.hpp` |
| Patient vector, waiting queue, undo stack and 10-record array cache | `src/model/ClinicDatabase.cpp` |
| Linear, binary and recursive search | `src/controller/SearchEngine.cpp` |
| Selection and insertion sorting by four fields | `src/model/ClinicDatabase.cpp`; controls in `web/index.html` and `web/app.js` |
| GUI panels and user events | `web/index.html`, `web/app.js`, `web/styles.css` |
| Responsive layout management | CSS Grid and Flexbox in `web/styles.css` |
| Advanced GUI features | Category-aware rows; dynamic patient notes label; import/export file selection; dashboard timer; validation; keyboard shortcuts; tooltips |
| Exceptions and validation | `include/scapms/utils/ValidationError.hpp`, `src/model/ClinicManager.cpp`, `src/controller/AppointmentController.cpp`, server catches in `src/server/HttpServer.cpp` |
| File persistence | `src/utils/FileHandler.cpp`; starter file `data/scapms.tsv` |
| Simple reports | `/api/reports` in `src/server/HttpServer.cpp`; Reports panel in `web/index.html` and `web/app.js` |
| UML / project report | `docs/SCAPMS_CLASS_DIAGRAM.mmd`, `docs/PROJECT_REPORT.md` |
