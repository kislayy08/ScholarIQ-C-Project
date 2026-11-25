# ScholarIQ

## 1. Project Header & Metadata

| Attribute | Details |
| :--- | :--- |
| **Project Title** | **ScholarIQ — Student Result & Academic Intelligence System** |
| **Subtitle** | Beyond Marks. Understand Performance. Plan Improvement. |
| **Programming Language** | C (ISO C99 Standard) |
| **User Interface** | Interactive Console / Terminal UI (with optional ANSI Color Rendering) |
| **Target Platform** | Cross-Platform (macOS, Linux, Windows via GCC / Clang / MinGW / MSVC) |
| **Project Category** | Academic Mini-Hackathon Project |
| **Academic Level** | First-Year B.Tech Computer Science & Engineering (School of Computer Science, UPES) |

---

## 2. Project Overview

**ScholarIQ** is an academic management and student intelligence system written in pure C. Built to move beyond traditional gradebook calculators that merely tally numbers, ScholarIQ evaluates holistic student performance through automated risk classification, trajectory tracking, actionable improvement suggestions, and interactive goal-setting tools. It is engineered with robust defensive programming principles to maintain strict data integrity within an intuitive terminal interface.

The application serves two distinct user roles: **Administrators / Faculty** and **Students**. Administrators can manage student records, record assessment scores individually or via fast bulk entry, inspect class-wide pass rates and grade distributions, and run comparative what-if simulations. Students can sign in using their unique SAP ID and a dedicated ScholarIQ PIN to access a personalized Study Companion featuring subject-specific study tasks, an interactive practice quiz, a timed focus tracker, hydration reminders, and motivational achievement badges.

---

## 3. Problem Statement

Standard student grading applications in academic settings frequently suffer from several critical shortcomings:
1. **Aggregations Mask Individual Subject Failures:** When evaluation is based solely on total marks or averages, a student who scores 95% in four subjects but 25% in one subject might appear to be an honors student, masking a subject-level failure that prevents progression.
2. **Lack of Early Risk Detection:** Traditional spreadsheets record marks post-facto but do not automatically categorize students who need urgent academic support, intervention, or core concept reinforcement.
3. **No Trajectory or Trend Insights:** Examining isolated semester scores obscures whether a student's performance is improving, declining, or stagnating across continuous internal assessments.
4. **Absence of Actionable Planning Aids:** Students who perform poorly are told what they scored, but are rarely given concrete targets showing *where* and *how many* marks they must gain to achieve their target academic standing.
5. **No Integrated Study Companion:** Traditional record systems isolate administration from the student's daily preparation routine, lacking task trackers, self-quizzing, and structured focus habits.

ScholarIQ directly resolves these problems using rule-based diagnostic algorithms, strict institutional pass/fail evaluation, and a dedicated student-side study companion.

---

## 4. Project Objectives

1. **Implement Institutional Result Calculation Rules:** Enforce strict subject-level passing criteria where any score below 40 marks automatically yields a final grade of `F` regardless of overall percentage.
2. **Deliver Actionable Academic Intelligence:** Classify students into 5 transparent risk tiers, detect multi-assessment trends, and auto-generate personalized revision recommendations.
3. **Provide Goal-Setting & Simulation Tools:** Implement a Target Percentage Calculator, a 3-step Weakest-First Recovery Planner, and a non-destructive What-If Result Simulator.
4. **Empower Student Self-Regulation:** Offer a dedicated Student Portal with task checklists, timed focus sessions, hydration check-ins, a 15-question subject practice quiz, and achievement badges.
5. **Ensure Uncompromising Terminal Stability:** Guarantee safe input parsing using `fgets`, `strtol`, buffer-clearing loops, and range guards to eliminate runtime crashes, infinite prompt loops, and numeric overflows.
6. **Demonstrate Practical C Foundations:** Showcase mastery of arrays, nested structures, enumerations, pointers, multi-attribute sorting, search algorithms, string formatting, and modular code architecture in standard C.

---

## 5. Main Features

### A. Admin & Faculty Module

* **Student Record Management:** Register new students with guaranteed unique roll numbers, edit names or roll numbers, safely delete records (with array shifting and profile cascade cleanup), and view profiles across all three assessments.
* **Flexible Search Operations:** Perform exact roll number lookup or case-insensitive partial substring search by student name.
* **Assessment Marks Entry:** Enter or update subject marks for any of the 3 assessments (`IA-1`, `IA-2`, `Final`) with range validation ($0 \le \text{mark} \le 100$) and overwrite safeguards.
* **Quick Bulk Marks Entry:** Accelerate data entry through an interactive batch loop supporting `Enter` (input marks), `S` (skip student), and `Q` (quit early).
* **Class Performance Analytics:** Compute class averages, highest/lowest scores, pass/fail ratios, and grade distribution histograms scaled using ASCII progress bars.
* **Subject Difficulty Analysis:** Determine class-wide averages, high/low scores, and failure counts per subject to objectively highlight subjects requiring faculty intervention.
* **Student Account Administration:** Provision student portal accounts by linking a demo SAP ID and a 4–8 digit numeric ScholarIQ PIN to an existing roll number.

### B. Core Result Engine

* **Comprehensive Scorecard Metrics:** Calculates Total Marks (out of 500), Percentage, Passed Subject Count, Failed Subject Count, Overall Result (`PASS` or `FAIL`), and Grade (`A+` to `F`).
* **Strict Institutional Pass Rule:** A student passes *only* if every subject score is $\ge 40$. If even one subject score is $< 40$, the final grade is forced to `F` (with an explicit note indicating the percentage-alone grade).
* **Strongest & Weakest Subject Detection:** Evaluates subject performance with tie-awareness, identifying all subjects sharing the highest or lowest mark.
* **Boxed Academic Report Card:** Formats a clean ASCII report card complete with score progress bars, status badges, and personalized suggestions.

### C. Academic Intelligence & Planning

* **Academic Risk Classification (5 Tiers):** Evaluates student performance into distinct support tiers:
  * `Critical Support`: 2 or more failed subjects.
  * `High Priority`: Exactly 1 failed subject.
  * `Improvement Needed`: All subjects passed, but percentage $< 60\%$.
  * `Stable Performance`: All subjects passed, percentage between $60\%$ and $79.99\%$.
  * `Excellent Performance`: All subjects passed, percentage $\ge 80\%$.
* **Performance Trend Analyzer:** Compares scores between the first and last recorded assessments to determine whether a student's trajectory is `IMPROVING`, `DECLINING`, `UNCHANGED`, or has `INSUFFICIENT DATA`.
* **Personalized Improvement Suggestions:** Dynamically generates prioritized study guidance based on failed subjects, subjects in the 40–59 border zone, score trajectories, and honors performance.
* **Target Percentage Calculator:** Computes the exact integer total required to achieve any target percentage up to 100% and determines remaining mark deficits.
* **Smart Marks Recovery Planner:** Formulates a step-by-step recovery plan to achieve a target percentage by prioritizing failed subjects first, then leveling up weakest subjects.
* **What-If Result Simulator:** Allows inputting hypothetical marks to preview impacts on total, percentage, grade, and pass status side-by-side with current records without modifying stored data.

### D. Student Study Companion

* **Personalized Student Dashboard:** Summarizes linked roll number, completed tasks, focus hours, hydration check-ins, readiness index, and unlocked badges.
* **Study Task Checklist:** Add, complete, toggle, or delete subject-tagged study action items.
* **Timed Focus Tracker:** Records wall-clock study sessions using `time()` and `difftime()`, logging elapsed minutes and tracking hydration intervals.
* **Hydration Reminders & Check-Ins:** Configurable water interval timer (10–120 minutes) with check-in recording.
* **Subject Practice Quiz:** Interactive 15-question bank (3 per subject across Mathematics, Physics, Chemistry, C Programming, and English) with immediate scoring and accuracy tracking.
* **Study Readiness Score & Streak:** A composite metric evaluating readiness out of 100 (Tasks: 40 pts, Quizzes: 40 pts, Focus: 20 pts) alongside UTC calendar-day study streaks.
* **Achievement Badges:** 7 unlockable milestone badges (`First Task`, `Task Master`, `First Focus`, `Focus Hour`, `Water Check`, `Quiz Starter`, `Quiz Ace`).

---

## 6. System Workflow

The program operates as a state-driven console application with dual authentication realms:

```text
                           +---------------------------+
                           |       Program Start       |
                           +---------------------------+
                                         |
                                         v
                           +---------------------------+
                           | CLI Color Flag / Prompt   |
                           | (--color / --no-color)    |
                           +---------------------------+
                                         |
                                         v
                         +-------------------------------+
                         |    Welcome / Role Selection   |
                         |  [1] Admin  [2] Student [0]   |
                         +-------------------------------+
                                  /             \
                   [Option 1]    /               \    [Option 2]
                                v                 v
                 +---------------------+   +---------------------+
                 |     Admin Login     |   |    Student Login    |
                 | (ID: admin, PIN)    |   | (SAP ID, PIN)       |
                 +---------------------+   +---------------------+
                            |                         |
               (Valid)      v            (Valid)      v
                 +---------------------+   +---------------------+
                 |    Admin Portal     |   |   Student Portal    |
                 |  - Student Mgmt     |   |  - My Dashboard     |
                 |  - Exam Marks Mgmt  |   |  - My Academics     |
                 |  - Report Cards     |   |  - Study Tasks      |
                 |  - Merit Rankings   |   |  - Focus & Wellness |
                 |  - Class Analytics  |   |  - Prep & Badges    |
                 |  - Intelligence     |   +---------------------+
                 |  - Recovery Planner |              |
                 |  - What-If Simulator|              | [Logout]
                 |  - Demo Data Load   |              v
                 |  - Create Student   |   +---------------------+
                 +---------------------+   | Return to Welcome   |
                            |              +---------------------+
                            | [Logout]                |
                            v                         | [Choice = 0]
                 +---------------------+              v
                 | Return to Welcome   |   +---------------------+
                 +---------------------+   |  Graceful Exit &    |
                                           |  RAM Memory Cleanup |
                                           +---------------------+
```

---

## 7. Algorithms Used

ScholarIQ relies entirely on robust, deterministic algorithms implemented without external libraries:

### 1. Multi-Attribute Bubble Sort with Competition Ranking
* **Functions:** `buildRanking()`, `ranksBefore()`, `assignRanks()`
* **Purpose:** Sorts class results and assigns standard competition ranks (e.g., $1, 2, 2, 4$).
* **Mechanism:** 
  1. Primary key: Pass status (students passing all subjects rank strictly above students who failed).
  2. Secondary key: Total marks in descending order.
  3. Tertiary tie-breaker: Lower roll number first (determines display order only).
  4. Ranks are assigned such that identical pass status and total marks share the exact same rank.
* **C Concepts:** Bubble sort loop, array indexing, function pointers/modular comparison functions.

### 2. Standardized Core Result Engine
* **Function:** `computeResult()`
* **Purpose:** Acts as the single source of truth for all modules (Report Cards, Merit Lists, Simulator, Class Stats).
* **Mechanism:** Iterates through all 5 subject marks in one pass, calculating total score, percentage, best subject, worst subject, passed count, and failed count. Overrides grade to `F` if any individual mark is below 40.
* **C Concepts:** `struct Result`, linear array scan, floating-point type casting (`(double)total / SUBJECTS`).

### 3. Exact Integer Ceiling Target Calculation
* **Function:** `requiredTotal()`
* **Purpose:** Determines the minimal integer marks required to reach a fractional target percentage without rounding drift.
* **Formula:**
  $$\text{Required Total} = \left\lfloor \frac{\text{Target Hundredths} + 19}{20} \right\rfloor$$
* **C Concepts:** Pure integer arithmetic, avoidance of IEEE-754 floating-point inaccuracies.

### 4. Weakest-First Recovery Planning Algorithm
* **Function:** `buildRecoveryPlan()`
* **Purpose:** Formulates an equitable and realistic recovery roadmap to achieve a target score.
* **3-Step Process:**
  1. **Feasibility Check:** Confirms $\text{Required Total} \le 500$.
  2. **Deficit Clearance:** First lifts all failed subjects ($< 40$) to exactly 40 marks, calculating `passFix` deficit.
  3. **Greedy Leveling:** Adds remaining required marks one by one to the lowest-scoring subject that is currently $< 100$, prioritizing the weakest subjects until the target total is achieved.
* **C Concepts:** While loop, iterative optimization, array search.

### 5. Composite Readiness Metric Calculation
* **Function:** `readinessScore()`
* **Purpose:** Translates student effort across tasks, quizzes, and focus into a single readiness indicator (0–100 scale).
* **Formula:**
  $$\text{Score} = \underbrace{\left(\frac{40 \times \text{Completed Tasks}}{\text{Total Tasks}}\right)}_{\text{Task Completion (max 40)}} + \underbrace{\left(\frac{40 \times \text{Quiz Correct}}{\text{Quiz Questions}}\right)}_{\text{Quiz Accuracy (max 40)}} + \underbrace{\min\left(20, \left\lfloor\frac{\text{Focus Minutes}}{6}\right\rfloor\right)}_{\text{Focus Time (max 20)}}$$
* **C Concepts:** Integer math, ternary conditional operators, bounds clipping.

### 6. UTC Calendar-Day Study Streak Tracking
* **Function:** `studyProgress()`
* **Purpose:** Computes unbroken daily study habits across UTC calendar days without date libraries.
* **Mechanism:** Converts Unix timestamps into epoch day integers ($\lfloor\text{time\_t} / 86400\rfloor$) and tracks whether consecutive day keys exist in the student's 60-day circular history array.
* **C Concepts:** Time manipulation (`time(NULL)`), day-key indexing, array sliding.

---

## 8. C Programming Concepts Used

| C Language Concept | Implementation in ScholarIQ | Source References |
| :--- | :--- | :--- |
| **Structures (`struct`)** | Defines composite entities: `Student`, `Exam`, `Result`, `ClassStats`, `SubjectStats`, `RecoveryPlan`, `StudyTask`, `StudySession`, `StudyProfile`, `QuizQuestion`, `DemoStudent`, `DemoRecord`. | `Exam`, `Student`, `StudyProfile` |
| **Enumerations (`enum`)** | Implements strongly typed categorical states: `Risk` (Critical to Excellent) and `Trend` (None, Up, Down, Same). | `enum Risk`, `enum Trend` |
| **Fixed-Size Arrays** | Houses database tables and history: `students[MAX_STUDENTS]`, `profiles[MAX_STUDENTS]`, `studyDays[60]`, `marks[SUBJECTS]`. | `students[100]`, `profiles[100]` |
| **String Handling** | Safe text manipulation using `snprintf()`, `strlen()`, `strcmp()`, `memcpy()`, and `memmove()` for in-place trimming. | `trim()`, `boxKV()`, `printWrapped()` |
| **Pointers & References** | Pass-by-reference for output parameters (`long *out`, `int *si`, `int *ei`, `int *hund`) and efficient struct passing via `const` pointers. | `parseInt()`, `computeResult()`, `pickStudentExam()` |
| **Control Flow & Loops** | Heavy utilization of `for`, `while`, and `do-while` loops for menu navigation, sorting, and input retry loops. | `main()`, `adminPortal()`, `buildRanking()` |
| **Switch Statements** | Routes menu selections across all 10 modules cleanly and handles risk tier mapping. | `adminPortal()`, `riskColor()`, `classifyRisk()` |
| **Defensive Input Parsing** | Uses `fgets()` for safe line reads, flushes residual characters, validates conversion with `strtol()`, and checks `errno == ERANGE`. | `readLine()`, `parseInt()`, `readInt()` |
| **Time Management** | Captures wall-clock session timing using `<time.h>` functions: `time()`, `difftime()`, and epoch day division. | `startStudySession()`, `studyProgress()` |
| **Memory Initialization** | Cleans struct instances and shifts deleted array elements using `memset()` and `memmove()`. | `deleteStudent()`, `loadDemoData()` |

---

## 9. Project Structure

The project follows a clean, single-compilation-unit design adhering strictly to academic hackathon standards:

```text
ScholarIQ-C-Project/
├── ScholarIQ.c       # Complete C source code (Engine, Database, UI, Portals)
├── README.md         # Comprehensive project documentation
└── test_cases.txt    # Verified execution logs & representative test cases
```

---

3. **Run the program:**
   ```bash
   # Interactive prompt for ANSI color support
   ./ScholarIQ

   # Force ANSI colors enabled
   ./ScholarIQ --color

   # Force plain monochrome output (recommended for log dumps)
   ./ScholarIQ --no-color
   ```
---

## 11. Sample Test Cases and Verified Outputs

| Test Case | Scenario / Feature Tested | Input Highlights | Expected Outcome | Verified Status |
| :--- | :--- | :--- | :--- | :---: |
| **TC-01** | Admin Authentication & Security | `wronguser` / `9999` $\to$ `admin` / `2468` | Unauthorized access rejected; demo admin logged in. | **PASSED** |
| **TC-02** | Registration & Input Validation | `abc`, `-5` $\to$ `101`, `12345` $\to$ `Aarav Sharma` | Letters rejected for roll; duplicate 101 blocked. | **PASSED** |
| **TC-03** | Marks Entry & Range Guards | Math `105`, Physics `-10` | Out-of-bounds rejected ($0\le\text{mark}\le 100$); summary shown. | **PASSED** |
| **TC-04** | Subject-Level Failure Rule | Roll 103 Final (4 passed, English: 35) | Grade forced to `F` despite 85.00% overall. | **PASSED** |
| **TC-05** | Search by Roll & Name | Roll `105`, Name `sharma` | Exact roll match; case-insensitive substring match. | **PASSED** |
| **TC-06** | Merit Ranking & Class Toppers | Exam: `IA-1` (Demo Data) | Topper: Aarav Sharma (87.80%); rank order verified. | **PASSED** |
| **TC-07** | What-If Result Simulator | Roll 105 IA-1 (+37 marks simulated) | Shows Before vs After diff; original RAM unchanged. | **PASSED** |
| **TC-08** | Smart Marks Recovery Planner | Roll 105 IA-1 (Target: 60%) | Failed cleared first (+7), remainder leveled (+85). | **PASSED** |
| **TC-09** | Student Account Creation | Roll 101, SAP `500101`, PIN `1234` | Account linked; student authentication enabled. | **PASSED** |
| **TC-10** | Student Portal & Study Companion | Login `500101` $\to$ Tasks, Quiz, Dashboard | Task added; Quiz scored 3/3; dashboard updated. | **PASSED** |

### Output Highlight 1: Academic Report Card (Subject-Level Failure Rule)
```text
+----------------------------------------------------------------------+
|                         ACADEMIC REPORT CARD                         |
|          ScholarIQ - Student Result & Academic Intelligence          |
+----------------------------------------------------------------------+
| Student      : Rohan Mehta                                           |
| Roll No      : 103                                                   |
| Assessment   : Final Examination                                     |
+----------------------------------------------------------------------+
| Subject                  Marks   Status  Score (full bar=100)        |
+----------------------------------------------------------------------+
| Mathematics                 99   PASS    [####################]      |
| Physics                     96   PASS    [###################-]      |
| Chemistry                   97   PASS    [###################-]      |
| Programming in C            98   PASS    [####################]      |
| English                     35   FAIL    [#######-------------]      |
+----------------------------------------------------------------------+
| Total        : 425 / 500                                             |
| Percentage   : 85.00 %                                               |
| Grade        : F                                                     |
| Subjects     : 4 of 5 passed                                         |
| Result       : FAIL                                                  |
| Grade is F because a subject is below 40 (percentage alone: A).      |
+----------------------------------------------------------------------+
| Strongest    : Mathematics (99)                                      |
| Weakest      : English (35)                                          |
+----------------------------------------------------------------------+
| Status       : High Priority                                         |
| Why: Exactly one subject (English) is below the pass mark of 40.     |
+----------------------------------------------------------------------+
| PERSONALISED SUGGESTIONS                                             |
|  1. Priority: clear English (scored 35, needs 5 more mark(s) to      |
|     pass).                                                           |
|  2. Total rose by 11 marks since IA-2: keep the same study routine.  |
+----------------------------------------------------------------------+
```

### Output Highlight 2: What-If Comparative Simulator
```text
  BEFORE vs AFTER
------------------------------------------------------------------------
  Subject               Before   After    Diff
------------------------------------------------------------------------
  Mathematics               38      45      +7
  Physics                   35      45     +10
  Chemistry                 44      50      +6
  Programming in C          50      55      +5
  English                   41      50      +9
------------------------------------------------------------------------
  Total (/500)             208     245     +37
  Percentage            41.60%  49.00%  +7.40%
  Grade                      F       D      UP
  Result                  FAIL    PASS CHANGED
------------------------------------------------------------------------
  [OK] Simulation finished. The original stored marks were NOT modified.
```

---

## 12. Input Validation & Defensive Design

ScholarIQ enforces rigorous input hygiene to guarantee stability under all terminal conditions:

1. **Safe String Ingestion (`readLine`):**
   * Uses `fgets()` into fixed-size buffers rather than dangerous functions like `gets()` or `scanf("%s")`.
   * Automatically strips trailing `\n` and `\r`.
   * Discards excess characters exceeding buffer capacity and safely detects EOF (closing the program cleanly instead of spinning in infinite retry loops).
2. **Numeric Whole Number Parsing (`parseInt`):**
   * Wraps `strtol()` with `errno = 0` checking to detect numeric overflow/underflow (`ERANGE`).
   * Rejects non-numeric characters, embedded whitespace, and trailing garbage.
3. **Student Name Rules (`validName`):**
   * Length must be between 1 and 49 characters.
   * Restricts character set to alphabetic letters, spaces, periods (`.`), and hyphens (`-`).
   * Rejects purely numeric or whitespace-only inputs.
4. **Assessment Mark Validation:**
   * Integer bounds checking enforces $0 \le \text{mark} \le 100$.
5. **Target Percentage Parsing (`parsePercent`):**
   * Parses decimals up to two places (e.g., `82.5` or `75.25`) into integer hundredths ($8250$, $7525$) to avoid floating-point rounding errors during recovery calculations.
6. **Authentication PIN Enforcement:**
   * Validates ScholarIQ PINs as strictly 4 to 8 numeric digits.

---

## 13. System Limitations

In the interest of full technical transparency, the current C terminal implementation has the following verified limitations:
* **In-Memory Storage:** All records, marks, and student profiles reside in RAM. Closing or terminating the application discards session data (a built-in 10-student demonstration dataset can be reloaded at any time).
* **Fixed Database Capacity:** Sized for classroom cohorts with `MAX_STUDENTS = 100`, `MAX_TASKS = 32` per profile, and `MAX_SESSIONS = 40` focus sessions.
* **Predefined Curriculum Schema:** Hardcoded for 5 subjects across 3 assessments (`IA-1`, `IA-2`, `Final Examination`).
* **Synchronous Terminal Focus Timer:** Standard C console applications cannot refresh live clocks in the background while blocking on `fgets()`. Focus durations are calculated upon session completion via `difftime()`.
* **Demonstration Authentication:** Administrative and student credentials are designed for academic demonstration and do not integrate with production UPES LDAP or SSO services.

---

## 14. Future Scope

* **File System & Database Persistence:** Adding binary file serialization (`fread`/`fwrite`) or SQLite integration to persist data across executions.
* **Document Export:** Generating formatted PDF report cards and CSV class summaries using headless PDF generation libraries.
* **Dynamic Curricula:** Allowing administrators to configure custom subject counts, subject names, credit weights, and assessment counts dynamically.
* **Cryptographic Security:** Upgrading plain-text demo PINs to salted cryptographic hashes (e.g., SHA-256 or bcrypt).
* **Multi-Threaded Live Terminal Timers:** Implementing POSIX threads (`pthread`) or Windows threads to render live countdowns and real-time break notifications.

---

---

## 16. Conclusion

**ScholarIQ** demonstrates how fundamental C programming concepts—arrays, structures, pointers, input parsing, sorting, and modular function design—can be unified to create a comprehensive academic management solution. By shifting the paradigm from passive grade calculation to proactive academic intelligence, ScholarIQ empowers faculty with deep cohort insights while providing students with the concrete tools needed to direct their own academic recovery and growth.
