# SCAPMS — Smart Clinic Appointment & Patient Management System

SCAPMS is a C++17 clinic-management application built for the COS202 mid-semester lab assessment. It has a responsive browser dashboard and a C++ backend that runs locally on the clinic computer. Patient records, appointment rules, the waiting queue, persistence, reports, searching and sorting are handled in C++.

## Build and run

Requirements: a C++17 compiler, GNU Make, and Linux or another POSIX environment. No third-party C++ libraries are required.

```bash
make
./build/scapms
```

Open **http://127.0.0.1:8080** in a browser. Keep the terminal running while using the dashboard. To use a different port:

```bash
./build/scapms 8081
```

The Makefile compiles all files under `src/` into `build/scapms`. A CMake project file is also included. Use `make clean` to remove the compiled binary.

## What the system includes

- Patient registration, editing, deletion and category-specific records for General, Emergency and Follow-Up patients.
- Appointment scheduling, completion, cancellation, history and undo support.
- A first-in, first-out waiting queue.
- Patient search by ID, name or category; binary search is exact by name, and recursive name search accepts partial text.
- Selectable Selection Sort and Insertion Sort by name, ID, age or earliest appointment date.
- An Undo Last Action stack for patient and appointment administration.
- A ten-slot recent-patient cache backed by a fixed-size `std::array`.
- Reports for category counts, most completed visits, pending appointments and daily appointment totals.
- TSV data loading, automatic saving, manual save/load, and import/export controls.
- Input validation, meaningful error messages, tooltips, keyboard shortcuts and periodic dashboard refresh.

## Data file

The starter records in `data/scapms.tsv` are fictional examples. The application loads this file when it starts and saves changes there automatically. Use **File → Export data file** to make a separate backup before importing another dataset.

## Layout

```text
include/scapms/       C++ interfaces and declarations
src/model/            Patient hierarchy, appointment model, database and manager
src/controller/       Search and appointment workflows
src/server/           Local HTTP server and API routes
src/utils/             ID generation, validation errors and TSV persistence
web/                   Responsive dashboard and event handlers
data/                  Starter clinic data
docs/                  Assessment report and class diagram
```

## Algorithms and data structures

| Requirement | Implementation |
| --- | --- |
| Patient storage | `std::vector<std::unique_ptr<PatientRecord>>` |
| Waiting line | `std::queue<std::string>` of patient IDs |
| Undo | `std::stack<UndoAction>` of prior patient snapshots |
| Recent-record cache | Fixed `std::array<PatientRecord*, 10>` |
| Linear search | Manually checks records for ID, name or category |
| Binary search | Manually sorts a temporary name index, then performs binary search for an exact name |
| Recursive search | Recursively checks each patient name for partial text |
| Selection Sort | Manually selects the next smallest record by the chosen field |
| Insertion Sort | Manually inserts each record into its sorted position |

Binary search is only offered for names because it requires sorted data. Its temporary index is ordered before the search, so it does not depend on the visible directory already being sorted.

## Controls

- **Ctrl+S** saves the clinic data.
- **Ctrl+F** opens Search & Sort.
- **Ctrl+Q** shows the browser-tab exit hint.
- The File menu supports save, load, import and export.

The local server binds to `127.0.0.1` only; it does not listen on the network.
