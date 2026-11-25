# ScholarIQ-C-Project

A C-based Student Result and Academic Intelligence System featuring result analysis, grade calculation, academic risk detection, performance tracking, recovery planning, and personal study management.

## Overview

**ScholarIQ** is a terminal-based C application that helps administrators manage student records and examination marks while giving students a clearer understanding of their academic performance. Instead of stopping at totals and grades, it identifies weaker subjects, classifies academic risk, compares assessments and helps plan possible improvements.

It also includes a personal study companion with tasks, focus-session tracking, hydration check-ins, practice quizzes and achievements. The project demonstrates how core C programming concepts can solve a practical academic problem without a database or external framework.

## Problem Statement

Traditional result systems mainly show marks, percentages and grades. Students may still struggle to identify weak subjects, understand academic risk or determine the marks needed for a target result. Manual record management and class analysis can also take time and lead to mistakes. **ScholarIQ brings result processing, rule-based analysis and study support together in one console application.**

## Main Features

| Module | What it does |
|---|---|
| **Student Management** | Register, view, edit, delete and search student records; check unique roll numbers. |
| **Examination Management** | Enter or update marks for IA-1, IA-2 and Final Examination; support bulk entry and history. |
| **Result Engine** | Calculate total, percentage, grade, pass/fail and best/worst subjects. |
| **Academic Intelligence** | Classify academic risk, compare assessment trends and suggest improvements. |
| **Smart Planning** | Calculate marks needed for a target and propose subject-wise recovery. |
| **What-If Simulator** | Try hypothetical marks without overwriting the stored examination record. |
| **Class Analytics** | Show rankings, class summaries, subject analysis and grade/pass-fail distributions. |
| **Reports & Charts** | Display report cards, student lists and text-based charts in the terminal. |
| **Student Study Companion** | Student demo login, study tasks, focus sessions, hydration check-ins, quizzes, readiness and badges. |

### Two user sections

**Admin:** Manages student records, examinations, reports, class analysis and student demo accounts.

**Student:** Signs in with a ScholarIQ demo SAP ID and PIN to view academic information and manage study activities.

> Authentication is for demonstration only. Do not use a real university password or private student data.

## How ScholarIQ Works

```text
             START
               |
          Main Menu
           /     \
        Admin   Student
          |        |
    Manage data   View results
    Enter marks   Tasks / Focus / Quiz
          |        |
       Validation and shared result calculations
                       |
          Reports / Analysis / Planning
                       |
                   Logout / Exit
```

The same `computeResult()` function is reused for result-related features so totals, grades and pass/fail rules stay consistent.

## Academic Calculation Rules

ScholarIQ uses **five subjects**, each marked out of **100**: Mathematics, Physics, Chemistry, Programming in C and English.

- **Total marks:** Sum of the five subject marks (maximum 500).
- **Percentage:** `total / 5.0`.
- **Pass rule:** Every subject must have **at least 40 marks**. If any subject is below 40, the overall result is **FAIL** and the final grade is **F**, even with a high aggregate.
- **Grade thresholds (demo rules):** A+ ≥ 90%, A ≥ 80%, B+ ≥ 70%, B ≥ 60%, C ≥ 50%, D ≥ 40%, F below 40% or any failed subject.

**Example:** `90, 92, 88, 95, 35` gives **400/500 = 80%**, but **FAIL** because English is below 40.

### Academic risk categories

| Condition | Classification |
|---|---|
| Two or more failed subjects | Critical Support |
| One failed subject | High Priority |
| All passed, overall below 60% | Improvement Needed |
| All passed, overall 60% to below 80% | Stable |
| All passed, overall 80% or higher | Excellent |

These are **project demonstration rules**, not official UPES grading regulations.

### Smart planning and simulation

The **Target Calculator** determines the total marks required for a requested percentage. The **Recovery Planner** first addresses failed subjects, then allocates additional improvement to weaker subjects while respecting the 100-mark limit. The **What-If Simulator** calculates a temporary result and leaves original marks unchanged.

### Preparation readiness

The study companion calculates a rule-based activity score out of 100:

| Activity | Maximum contribution |
|---|---:|
| Completed study tasks | 40 |
| Practice quiz accuracy | 40 |
| Recorded focus time | 20 |

This is a **study activity indicator**, not a prediction of exam grades.

## C Programming Concepts Used

| Concept | Use in this project |
|---|---|
| `struct` | Student, exam, result and study-profile records |
| Arrays | Fixed-capacity student records and subject marks |
| Functions | Separate menus, calculations, validation and reports |
| `if` / `else` and `switch` | Grade rules, risk decisions and menu actions |
| Loops | Menus, mark processing, searching and reporting |
| Linear search | Finding student records by roll number |
| Sorting | Arranging academic rankings |
| Input validation | Checking numbers, names, marks and confirmations |
| `time.h` | Recording elapsed study-session time |

## Project Structure

```text
ScholarIQ-C-Project/
├── scholariq_complete.c   # Main C source code
├── README.md              # Project documentation
└── test_cases.md          # Sample tests and results
```

## Compile and Run

**Requirements:** A C11-compatible compiler such as GCC or Clang.

**macOS / Linux:**

```bash
gcc -std=c11 -Wall -Wextra -pedantic scholariq_complete.c -o scholariq
./scholariq --no-color
```

**Windows (GCC/MinGW):**

```powershell
gcc -std=c11 -Wall -Wextra -pedantic scholariq_complete.c -o scholariq.exe
.\scholariq.exe --no-color
```

Remove `--no-color` to choose whether to enable terminal colours at startup, or use `--color` on a compatible terminal.

### Quick demonstration

1. Launch the application and select **Admin login**.
2. Use the demo credentials supplied for the lab demonstration.
3. Choose **Load demonstration data** (Admin option 9).
4. Explore student records, marks, reports and planning tools.
5. Create a fictional student demo account (Admin option 10).
6. Log out and sign in through **Student login** using that demo account.
7. Try tasks, focus tracking, a practice quiz and readiness/achievements.

| Test | Expected behavior |
|---|---|
| Invalid menu choice | Rejects values outside the menu range |
| Duplicate roll number | Prevents duplicate registration |
| Invalid marks | Rejects marks outside 0–100 |
| Normal result | Calculates total, percentage and PASS |
| Failed subject | Shows FAIL even if the aggregate is high |
| What-If | Keeps the original marks unchanged |

**Testing note:** The test-case file distinguishes executed tests from cases awaiting verification; do not treat unexecuted expected results as observed output.

## Current Limitations

- Records are held **in RAM** and disappear when the application closes; there is no file handling or database.
- Maximum of **100 students**, with three examination types and five subjects.
- Demo login credentials and PIN handling are **not production-grade security**.
- Study sessions are recorded using elapsed time when the user returns/presses Enter; there is no live background countdown.
- Hydration check-ins are manual; water reminders are not live push notifications.
- Exam countdown days are manually entered, not automatically decremented.
- Practice quizzes use a small demonstration question bank (three questions per subject), not official university questions.
- Academic rules are rule-based; **no AI or machine learning** is implemented in this C application.

## Conclusion

ScholarIQ demonstrates how **structures, arrays, functions, loops, conditions, searching and sorting** can be combined to build a useful student result and academic-support system. Its aim is simple: **turn marks into understandable information and practical next steps.**
