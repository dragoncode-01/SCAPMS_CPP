# Smart Clinic Appointment & Patient Management System (SCAPMS)

## Project description

SCAPMS is a clinic administration prototype for registering patients, scheduling appointments, organizing a first-in, first-out waiting queue and producing basic activity reports. The application separates its C++ model and business rules from its local web dashboard. The C++ program serves the interface on `127.0.0.1`; the browser sends user actions to C++ API routes. This keeps clinic records and algorithms in the C++ application while giving staff a responsive graphical interface.

## Features and class hierarchy

`PatientRecord` is an abstract base class holding the shared patient ID, name, age, contact details, notes and appointment history. It implements the `Appointable` interface for scheduling, cancelling and removing appointments. `GeneralPatient`, `EmergencyPatient` and `FollowUpPatient` implement category-specific priority labels and care instructions. The dashboard renders each patient through a `PatientRecord` reference, demonstrating runtime polymorphism.

`Appointment` records a patient ID, date and time, reason and status. Each patient owns their appointment history. `ClinicDatabase` owns patient records and the waiting queue, provides a fixed-size frequently accessed record cache, and keeps previous administrative snapshots on an undo stack. `ClinicManager` validates and coordinates patient operations. `AppointmentController` handles appointment workflows, and `SearchEngine` contains the manual search algorithms.

## Data structures

Registered patients are stored in `std::vector<std::unique_ptr<PatientRecord>>`. The waiting line is a `std::queue<std::string>`, which preserves first-in, first-out service. Administrative undo uses `std::stack<UndoAction>` and restores the most recent patient snapshot. A `std::array<PatientRecord*, 10>` keeps the ten most recently accessed records. Appointment history is composed into each patient record.

## Searching, sorting and recursion

Linear search manually scans patient records and can match IDs, names or categories. Binary search is implemented for exact patient-name lookup: the algorithm manually insertion-sorts a temporary list of record pointers by lowercase name, then performs the binary-search interval updates. Recursive search checks each record name for partial text and makes a recursive call for the next record.

Selection Sort and Insertion Sort are both implemented manually. Users choose either algorithm and sort by name, patient ID, age or earliest non-cancelled appointment date. Selection Sort is straightforward to follow for a small clinic list; Insertion Sort performs well when a directory is already nearly sorted. Both take quadratic time in the worst case, which is appropriate for the assessment-sized dataset and makes the required algorithm steps visible.

## GUI and event handling

The dashboard includes Overview, Patients, Appointments, Waiting Queue, Search & Sort and Reports panels. Browser events handle buttons, forms, keyboard shortcuts and file selection. CSS Grid and Flexbox manage the page, card and form layouts so the dashboard adapts to smaller screens. The patient form changes its notes label and care hint when the category changes. Tables use distinct category and status badges. File controls allow importing and exporting the tab-separated data file, and a one-minute timer refreshes the dashboard date and summary counts.

## Validation, exceptions and persistence

Patient names and contact details are required; ages must be between 0 and 120; categories must be recognized; duplicate IDs from imported data are rejected. Appointment dates, times and reasons are checked before a record is created. Invalid actions return a clear message to the dashboard and do not terminate the server. `ValidationError` is a custom exception for invalid user input. Data is stored as tab-separated text in `data/scapms.tsv`, with escape handling for tabs, line breaks and backslashes. Each normal change saves through a temporary file before replacing the data file.

## Reports and design decisions

Reports show patient category distribution, patients with the most completed appointments, scheduled appointments that remain pending and a daily appointment status summary. Binary search is restricted to exact names because its search step relies on a sorted sequence; linear search supports category and substring queries. The browser dashboard was chosen to keep the project dependency-free while providing a graphical interface around the C++ application.

## Challenges and resolutions

The assessment calls for GUI events and layout management as well as C++ algorithms and class separation. The project uses a local C++ HTTP server with a browser interface so the application can run without installing a GUI framework. Data operations remain in C++; the front end focuses on presentation and user events. Undo restores complete patient snapshots so a change to a patient's category or appointment history can be reversed consistently.
