#include <stdio.h>   /* input/output, snprintf */
#include <string.h>  /* strlen, strcmp, memset, memcpy */
#include <stdlib.h>  /* strtol (safe number parsing), exit */
#include <time.h>    /* portable study session timestamps */
#include <errno.h>   /* detect numeric overflow */
#include <errno.h>   /* numeric overflow detection */

/* Forward declarations for profile lifecycle hooks. */
static void removeProfileForRoll(int roll);
static void changeProfileRoll(int oldRoll, int newRoll);
static void resetStudyProfiles(void);

/* ================= 1. CONSTANTS, TYPES, GLOBAL DATA ================= */

#define MAX_STUDENTS 100
#define SUBJECTS     5
#define EXAMS        3
#define MAX_MARK     100
#define PASS_MARK    40
#define MAX_TOTAL    (SUBJECTS * MAX_MARK)
#define NAME_LEN     50
#define INPUT_LEN    128
#define GRADES       7
#define WIDTH        72              /* width of every heading / rule      */
#define BOX_INNER    (WIDTH - 2)     /* characters between the two '|'     */
#define CHART_W      25              /* bar length: 1 '#' = 4% on a 100 scale */
#define MAX_SUG      8
#define SUG_LEN      160
#define NAMES_LEN    64             /* longest joined list of subject names is 58 chars */

/* Optional ANSI colours. They are only printed when useColor is 1. */
#define ANSI_RESET  "\033[0m"
#define ANSI_BOLD   "\033[1m"
#define ANSI_HEAD   "\033[1;36m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED    "\033[31m"
#define ANSI_CYAN   "\033[36m"

typedef struct {
    int marks[SUBJECTS];
    int entered;                 /* 0 = no record, 1 = marks stored        */
} Exam;

typedef struct {
    int  rollNo;
    char name[NAME_LEN];
    Exam exams[EXAMS];
} Student;

/* Computed on demand by computeResult(); never stored in the database. */
typedef struct {
    int    total;                /* out of 500                             */
    double percentage;
    int    percentGrade;         /* grade index from percentage alone      */
    int    grade;                /* final grade index (F if any subject failed) */
    int    passedCount, failedCount;
    int    passed;               /* 1 only if every subject >= PASS_MARK   */
    int    best, worst;          /* subject indices; first one wins ties   */
} Result;

typedef enum { RISK_CRITICAL, RISK_HIGH, RISK_IMPROVE, RISK_STABLE, RISK_EXCELLENT } Risk;
typedef enum { TREND_NONE, TREND_UP, TREND_DOWN, TREND_SAME } Trend;

typedef struct {
    int    registered, count, pass, fail;
    double avgPct;
    int    maxTotal, minTotal;
    int    gradeCount[GRADES];
} ClassStats;

typedef struct { int sum, high, low, failed, count; } SubjectStats;

typedef struct {
    int marks[SUBJECTS];         /* suggested marks                        */
    int required;                /* total needed for the target percentage */
    int passFix;                 /* marks used to lift failed subjects to 40 */
    int spread;                  /* marks spread over the weakest subjects */
    int feasible;
} RecoveryPlan;

static Student students[MAX_STUDENTS];   /* the fixed-capacity database */
static int studentCount = 0;
static int demoLoaded   = 0;
static int useColor     = 0;

static const char *subjectNames[SUBJECTS] = {
    "Mathematics", "Physics", "Chemistry", "Programming in C", "English"
};
static const char *examNames[EXAMS] = {
    "Internal Assessment 1", "Internal Assessment 2", "Final Examination"
};
static const char *examShort[EXAMS] = { "IA-1", "IA-2", "Final" };
static const char *gradeNames[GRADES] = { "A+", "A", "B+", "B", "C", "D", "F" };
static const char *gradeRange[GRADES] = {
    "90-100%", "80-89.99%", "70-79.99%", "60-69.99%", "50-59.99%", "40-49.99%",
    "<40% or any fail"
};
/* Grades are decided from the INTEGER total (percentage = total / 5), so the
 * boundaries are exact and never suffer floating-point rounding errors. */
static const int gradeMinTotal[GRADES] = { 450, 400, 350, 300, 250, 200, 0 };
static const char *riskNames[5] = {
    "Critical Support", "High Priority", "Improvement Needed",
    "Stable Performance", "Excellent Performance"
};
static const char *trendNames[4] = {
    "INSUFFICIENT DATA", "IMPROVING", "DECLINING", "UNCHANGED"
};

/* ================= 2. TERMINAL UI HELPERS (Engine E) ================= */

static void setColor(const char *code)
{
    if (useColor && code[0] != '\0') fputs(code, stdout);
}

static void resetColor(void)
{
    if (useColor) fputs(ANSI_RESET, stdout);
}

static void printRule(char c)
{
    int i;
    for (i = 0; i < WIDTH; i++) putchar(c);
    putchar('\n');
}

static void printCentered(const char *text)
{
    int pad = (WIDTH - (int)strlen(text)) / 2;
    if (pad < 0) pad = 0;
    printf("%*s%s\n", pad, "", text);
}

/* Big boxed title used at the top of every screen. */
static void printHeading(const char *title)
{
    putchar('\n');
    setColor(ANSI_HEAD);
    printRule('=');
    printCentered(title);
    printRule('=');
    resetColor();
}

/* Smaller section title with a thin rule underneath. */
static void printSection(const char *title)
{
    putchar('\n');
    setColor(ANSI_HEAD);
    printf("  %s\n", title);
    resetColor();
    printRule('-');
}

/* Status messages: the [TAG] keeps them readable even without colours. */
static void printMessage(const char *tag, const char *color, const char *msg)
{
    setColor(color);
    printf("  [%s] %s\n", tag, msg);
    resetColor();
}
static void printSuccess(const char *m) { printMessage("OK",    ANSI_GREEN,  m); }
static void printWarning(const char *m) { printMessage("WARN",  ANSI_YELLOW, m); }
static void printError(const char *m)   { printMessage("ERROR", ANSI_RED,    m); }
static void printInfo(const char *m)    { printMessage("INFO",  ANSI_CYAN,   m); }

static void printPrompt(const char *text)
{
    printf("  ");
    setColor(ANSI_BOLD);
    printf("%s", text);
    resetColor();
}

/* Prints text in a colour inside a fixed-width field.
 * width > 0 right-aligns, width < 0 left-aligns. Colour codes take no space. */
static void printField(const char *color, const char *text, int width)
{
    int len = (int)strlen(text);
    int pad = (width < 0 ? -width : width) - len;
    if (pad < 0) pad = 0;
    if (width > 0) printf("%*s", pad, "");
    setColor(color);
    fputs(text, stdout);
    resetColor();
    if (width < 0) printf("%*s", pad, "");
}

static const char *passColor(int passed) { return passed ? ANSI_GREEN : ANSI_RED; }

static const char *gradeColor(int g)
{
    if (g <= 2) return ANSI_GREEN;
    if (g == 5) return ANSI_YELLOW;
    if (g == 6) return ANSI_RED;
    return "";
}

static const char *riskColor(Risk r)
{
    switch (r) {
        case RISK_CRITICAL:
        case RISK_HIGH:      return ANSI_RED;
        case RISK_IMPROVE:   return ANSI_YELLOW;
        case RISK_STABLE:    return ANSI_CYAN;
        default:             return ANSI_GREEN;
    }
}

static const char *trendColor(Trend t)
{
    if (t == TREND_UP)   return ANSI_GREEN;
    if (t == TREND_DOWN) return ANSI_RED;
    if (t == TREND_NONE) return ANSI_YELLOW;
    return "";
}

/* Scaled bar: filled length = value / maxValue * width (rounded). */
static void printBar(double value, double maxValue, int width, const char *color)
{
    int filled = 0, i;
    if (maxValue > 0.0) filled = (int)(value / maxValue * width + 0.5);
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    putchar('[');
    setColor(color);
    for (i = 0; i < filled; i++) putchar('#');
    resetColor();
    for (i = filled; i < width; i++) putchar('-');
    putchar(']');
}

/* ---- boxed layout helpers (report card) ---- */

static void boxRule(void)
{
    int i;
    putchar('+');
    for (i = 0; i < BOX_INNER; i++) putchar('-');
    puts("+");
}

static void boxText(const char *color, const char *text)
{
    putchar('|');
    setColor(color);
    printf(" %-*.*s", BOX_INNER - 1, BOX_INNER - 1, text);
    resetColor();
    puts("|");
}

static void boxCentered(const char *color, const char *text)
{
    char line[INPUT_LEN];
    int pad = (BOX_INNER - 1 - (int)strlen(text)) / 2;
    if (pad < 0) pad = 0;
    snprintf(line, sizeof line, "%*s%s", pad, "", text);
    boxText(color, line);
}

/* "label : value" row; only the value is coloured. */
static void boxKV(const char *label, const char *value, const char *color)
{
    int room = BOX_INNER - 16;
    printf("| %-13s: ", label);
    setColor(color);
    printf("%-*.*s", room, room, value);
    resetColor();
    puts("|");
}

/* Word-wraps text. First line starts with prefix; later lines are indented
 * by the same amount. boxed = 1 prints inside the report-card box. */
static void printWrapped(const char *prefix, const char *text, int width, int boxed)
{
    char blank[32], line[INPUT_LEN * 2];
    int pl = (int)strlen(prefix), avail, first = 1;
    const char *p = text;

    if (pl > 31) pl = 31;
    memset(blank, ' ', (size_t)pl);
    blank[pl] = '\0';
    avail = width - pl;

    while (*p != '\0') {
        int len = (int)strlen(p), cut;
        if (len <= avail) {
            cut = len;
        } else {
            cut = avail;
            while (cut > 0 && p[cut] != ' ') cut--;   /* break at last space */
            if (cut == 0) cut = avail;
        }
        snprintf(line, sizeof line, "%s%.*s", first ? prefix : blank, cut, p);
        if (boxed) boxText("", line);
        else       printf("%s\n", line);
        p += cut;
        while (*p == ' ') p++;
        first = 0;
    }
}

static void printMenuOption(int n, const char *text)
{
    printf("   [%d]  %s\n", n, text);
}

/* ================= 3. INPUT LAYER (safe, standard C) ================= */

/* Reads one line with fgets. Removes the newline. If the user typed more than
 * the buffer holds, the rest of the line is thrown away and 0 is returned.
 * If the input stream is closed (EOF) the program exits cleanly, so there is
 * never an infinite re-prompt loop. */
static int readLine(char *buf, int size)
{
    size_t len;
    int c, tooLong = 0;

    if (fgets(buf, size, stdin) == NULL) {
        printf("\n  Input closed. Exiting ScholarIQ.\n");
        exit(0);
    }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
        if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';
        return 1;
    }
    while ((c = getchar()) != '\n' && c != EOF) tooLong = 1;
    return !tooLong;
}

static void trim(char *s)
{
    size_t start = 0, len;
    while (s[start] == ' ' || s[start] == '\t') start++;
    if (start > 0) memmove(s, s + start, strlen(s + start) + 1);
    len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) s[--len] = '\0';
}

static char lowerChar(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static int isLetter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int equalsIgnoreCase(const char *a, const char *b)
{
    while (*a && *b) {
        if (lowerChar(*a) != lowerChar(*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static int containsIgnoreCase(const char *hay, const char *needle)
{
    size_t i, j, hl = strlen(hay), nl = strlen(needle);
    if (nl == 0) return 0;
    for (i = 0; i + nl <= hl; i++) {
        for (j = 0; j < nl; j++)
            if (lowerChar(hay[i + j]) != lowerChar(needle[j])) break;
        if (j == nl) return 1;
    }
    return 0;
}

/* Whole-number parser: rejects empty text, letters and trailing junk. */
static int parseInt(const char *s, long *out)
{
    char *end;
    if (*s == '\0') return 0;
    errno = 0;
    errno = 0;
    *out = strtol(s, &end, 10);
    if (errno == ERANGE) return 0;
    if (errno == ERANGE) return 0;
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t') end++;
    return *end == '\0';
}

/* Keeps asking until the user enters a whole number in [min, max]. */
static int readInt(const char *prompt, int min, int max)
{
    char buf[INPUT_LEN], msg[96];
    long v;
    for (;;) {
        printPrompt(prompt);
        if (readLine(buf, sizeof buf) && parseInt(buf, &v) && v >= min && v <= max)
            return (int)v;
        snprintf(msg, sizeof msg, "Invalid input. Enter a whole number from %d to %d.", min, max);
        printError(msg);
    }
}

static int validName(const char *s)
{
    size_t i, len = strlen(s);
    int letters = 0;
    if (len < 1 || len >= NAME_LEN) return 0;
    for (i = 0; i < len; i++) {
        if (isLetter(s[i])) letters++;
        else if (s[i] != ' ' && s[i] != '.' && s[i] != '-') return 0;
    }
    return letters > 0;
}

static void readName(const char *prompt, char *out)
{
    char buf[INPUT_LEN];
    for (;;) {
        printPrompt(prompt);
        if (readLine(buf, sizeof buf)) {
            trim(buf);
            if (validName(buf)) { memcpy(out, buf, strlen(buf) + 1); return; }  /* length already checked */
        }
        printError("Name must be 1-49 characters: letters, spaces, '.' or '-' only.");
    }
}

/* Parses "75", "82.5" or "82.25" into hundredths of a percent (7500, 8250...).
 * Using integers avoids floating-point errors in later calculations. */
static int parsePercent(const char *s, int *hund)
{
    int whole = 0, frac = 0, fd = 0, wd = 0;
    while (*s >= '0' && *s <= '9') {
        whole = whole * 10 + (*s - '0');
        wd++;
        if (whole > 200) return 0;
        s++;
    }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            if (fd == 2) return 0;
            frac = frac * 10 + (*s - '0');
            fd++;
            s++;
        }
    }
    if (wd + fd == 0) return 0;
    if (fd == 1) frac *= 10;
    while (*s == ' ' || *s == '\t') s++;
    if (*s != '\0') return 0;
    *hund = whole * 100 + frac;
    return *hund >= 1 && *hund <= 20000;
}

static int readPercent(const char *prompt)
{
    char buf[INPUT_LEN];
    int h;
    for (;;) {
        printPrompt(prompt);
        if (readLine(buf, sizeof buf) && parsePercent(buf, &h)) return h;
        printError("Enter a number such as 75 or 82.5 (0.01 to 200, max 2 decimals).");
    }
}

static int confirm(const char *question)
{
    char buf[INPUT_LEN];
    for (;;) {
        printPrompt(question);
        if (readLine(buf, sizeof buf)) {
            trim(buf);
            if (equalsIgnoreCase(buf, "y") || equalsIgnoreCase(buf, "yes")) return 1;
            if (equalsIgnoreCase(buf, "n") || equalsIgnoreCase(buf, "no"))  return 0;
        }
        printWarning("Please answer y or n.");
    }
}

/* ================= 4. DATABASE HELPERS AND PICKERS ================= */

static int findByRoll(int roll)
{
    int i;
    for (i = 0; i < studentCount; i++)
        if (students[i].rollNo == roll) return i;
    return -1;
}

/* A roll number is unique if nobody except 'skipIndex' already uses it. */
static int isRollUnique(int roll, int skipIndex)
{
    int i;
    for (i = 0; i < studentCount; i++)
        if (i != skipIndex && students[i].rollNo == roll) return 0;
    return 1;
}

static int countRecords(int e)
{
    int i, n = 0;
    for (i = 0; i < studentCount; i++)
        if (students[i].exams[e].entered) n++;
    return n;
}

/* Returns a student index, or -1 if the user cancels / nobody is registered. */
static int pickStudent(void)
{
    int roll, idx;
    if (studentCount == 0) {
        printWarning("No students registered yet. Register one or load demo data.");
        return -1;
    }
    for (;;) {
        roll = readInt("Enter roll number (0 to cancel): ", 0, 999999);
        if (roll == 0) return -1;
        idx = findByRoll(roll);
        if (idx >= 0) return idx;
        printError("No student with that roll number.");
    }
}

/* Returns an exam index 0..EXAMS-1, or -1 if cancelled. */
static int pickExam(void)
{
    int i;
    printf("\n  Select assessment:\n");
    for (i = 0; i < EXAMS; i++)
        printf("   [%d]  %-24s (%d record(s))\n", i + 1, examNames[i], countRecords(i));
    return readInt("Assessment (0 to cancel): ", 0, EXAMS) - 1;
}

/* Picks a student AND an exam that really has stored marks. */
static int pickStudentExam(int *si, int *ei)
{
    char msg[160];
    *si = pickStudent();
    if (*si < 0) return 0;
    *ei = pickExam();
    if (*ei < 0) return 0;
    if (!students[*si].exams[*ei].entered) {
        snprintf(msg, sizeof msg, "No marks recorded for %s in %s. Use Exam Management > Enter marks.",
                 students[*si].name, examNames[*ei]);
        printWarning(msg);
        return 0;
    }
    return 1;
}

/* Picks an exam for class-wide features; refuses empty exams. */
static int pickClassExam(void)
{
    int e;
    char msg[120];
    if (studentCount == 0) {
        printWarning("No students registered yet. Register students or load demo data.");
        return -1;
    }
    e = pickExam();
    if (e < 0) return -1;
    if (countRecords(e) == 0) {
        snprintf(msg, sizeof msg, "No marks have been recorded for %s yet.", examNames[e]);
        printWarning(msg);
        return -1;
    }
    return e;
}

/* ================= 5. CORE RESULT ENGINE (single source of truth) ================= */

static int gradeFromTotal(int total)
{
    int g;
    for (g = 0; g < GRADES - 1; g++)
        if (total >= gradeMinTotal[g]) return g;
    return GRADES - 1;
}

/* Every feature (report card, ranking, simulator, planner) calls this one
 * function, so calculation logic is never duplicated. */
static Result computeResult(const int marks[SUBJECTS])
{
    Result r;
    int s;
    r.total = 0;
    r.passedCount = 0;
    r.best = 0;
    r.worst = 0;
    for (s = 0; s < SUBJECTS; s++) {
        r.total += marks[s];
        if (marks[s] >= PASS_MARK) r.passedCount++;
        if (marks[s] > marks[r.best])  r.best = s;
        if (marks[s] < marks[r.worst]) r.worst = s;
    }
    r.failedCount  = SUBJECTS - r.passedCount;
    r.percentage   = r.total / (double)SUBJECTS;
    r.passed       = (r.failedCount == 0);        /* every subject must pass */
    r.percentGrade = gradeFromTotal(r.total);
    r.grade        = r.passed ? r.percentGrade : GRADES - 1;  /* fail => F */
    return r;
}

/* Rule-based support category. Checked in order, so categories never overlap.
 * 60% of 500 = 300 marks and 80% of 500 = 400 marks. */
static Risk classifyRisk(const Result *r)
{
    if (r->failedCount >= 2) return RISK_CRITICAL;
    if (r->failedCount == 1) return RISK_HIGH;
    if (r->total < 60 * SUBJECTS) return RISK_IMPROVE;
    if (r->total < 80 * SUBJECTS) return RISK_STABLE;
    return RISK_EXCELLENT;
}

/* Joins the names of subjects whose marks lie in [lo, hi]. */
static void joinSubjects(const int marks[SUBJECTS], int lo, int hi, char *out, int size)
{
    int s;
    size_t used;
    out[0] = '\0';
    for (s = 0; s < SUBJECTS; s++) {
        if (marks[s] >= lo && marks[s] <= hi) {
            used = strlen(out);
            snprintf(out + used, (size_t)size - used, "%s%s", used ? ", " : "", subjectNames[s]);
        }
    }
}

static void riskReason(const int marks[SUBJECTS], const Result *r, char *out, int size)
{
    char names[NAMES_LEN];
    switch (classifyRisk(r)) {
        case RISK_CRITICAL:
            joinSubjects(marks, 0, PASS_MARK - 1, names, sizeof names);
            snprintf(out, (size_t)size, "%d subjects are below the pass mark of %d (%s).",
                     r->failedCount, PASS_MARK, names);
            break;
        case RISK_HIGH:
            joinSubjects(marks, 0, PASS_MARK - 1, names, sizeof names);
            snprintf(out, (size_t)size, "Exactly one subject (%s) is below the pass mark of %d.",
                     names, PASS_MARK);
            break;
        case RISK_IMPROVE:
            snprintf(out, (size_t)size, "All subjects passed, but the percentage is %.2f%% (below 60%%).",
                     r->percentage);
            break;
        case RISK_STABLE:
            snprintf(out, (size_t)size, "All subjects passed with %.2f%% (between 60%% and 79.99%%).",
                     r->percentage);
            break;
        default:
            snprintf(out, (size_t)size, "All subjects passed with %.2f%% (80%% or above).",
                     r->percentage);
            break;
    }
}

/* Index of the latest recorded exam BEFORE exam e, or -1. */
static int previousExam(const Student *s, int e)
{
    int p;
    for (p = e - 1; p >= 0; p--)
        if (s->exams[p].entered) return p;
    return -1;
}

/* Trend = compare total of the first and the last recorded assessment. */
static Trend studentTrend(const Student *s, int *first, int *last)
{
    int e, f = -1, l = -1, ft, lt;
    for (e = 0; e < EXAMS; e++) {
        if (s->exams[e].entered) {
            if (f < 0) f = e;
            l = e;
        }
    }
    *first = f;
    *last = l;
    if (f < 0 || f == l) return TREND_NONE;
    ft = computeResult(s->exams[f].marks).total;
    lt = computeResult(s->exams[l].marks).total;
    if (lt > ft) return TREND_UP;
    if (lt < ft) return TREND_DOWN;
    return TREND_SAME;
}

/* ================= 6. SHARED DISPLAY HELPERS ================= */

static void printQuickResult(const int marks[SUBJECTS])
{
    Result r = computeResult(marks);
    printf("  Total: %d/%d   Percentage: %.2f%%   Grade: ", r.total, MAX_TOTAL, r.percentage);
    printField(gradeColor(r.grade), gradeNames[r.grade], 0);
    printf("   Result: ");
    printField(passColor(r.passed), r.passed ? "PASS" : "FAIL", 0);
    putchar('\n');
}

static void printChange(int d)
{
    printf("%+8d  ", d);
    if (d > 0)      printField(ANSI_GREEN, "UP", -4);
    else if (d < 0) printField(ANSI_RED, "DOWN", -4);
    else            printField("", "SAME", -4);
}

/* Subject-wise marks across all assessments. "--" means NOT RECORDED, which
 * is different from a real score of 0. withChange adds a first->last column. */
static void printHistoryTable(const Student *s, int withChange)
{
    int sub, e, first, last;
    Result res[EXAMS];
    char buf[24];

    printf("  %-20s", "Subject");
    for (e = 0; e < EXAMS; e++) printf("%8s", examShort[e]);
    if (withChange) printf("%14s", "Change");
    putchar('\n');
    printRule('-');

    for (sub = 0; sub < SUBJECTS; sub++) {
        int fm = 0, lm = 0;
        first = -1;
        last = -1;
        printf("  %-20.20s", subjectNames[sub]);
        for (e = 0; e < EXAMS; e++) {
            if (s->exams[e].entered) {
                int m = s->exams[e].marks[sub];
                snprintf(buf, sizeof buf, "%d", m);
                printField(m < PASS_MARK ? ANSI_RED : "", buf, 8);
                if (first < 0) { first = e; fm = m; }
                last = e;
                lm = m;
            } else {
                printf("%8s", "--");
            }
        }
        if (withChange && last > first) printChange(lm - fm);
        putchar('\n');
    }
    printRule('-');

    for (e = 0; e < EXAMS; e++)
        if (s->exams[e].entered) res[e] = computeResult(s->exams[e].marks);

    /* find first/last recorded for the summary-row changes */
    first = -1;
    last = -1;
    for (e = 0; e < EXAMS; e++) {
        if (s->exams[e].entered) {
            if (first < 0) first = e;
            last = e;
        }
    }

    printf("  %-20s", "Total (/500)");
    for (e = 0; e < EXAMS; e++) {
        if (s->exams[e].entered) printf("%8d", res[e].total);
        else printf("%8s", "--");
    }
    if (withChange && last > first) printChange(res[last].total - res[first].total);
    putchar('\n');

    printf("  %-20s", "Percentage");
    for (e = 0; e < EXAMS; e++) {
        if (s->exams[e].entered) {
            snprintf(buf, sizeof buf, "%.1f%%", res[e].percentage);
            printField("", buf, 8);
        } else {
            printf("%8s", "--");
        }
    }
    if (withChange && last > first)
        printf("%+8.2f  points", res[last].percentage - res[first].percentage);
    putchar('\n');

    printf("  %-20s", "Grade");
    for (e = 0; e < EXAMS; e++) {
        if (s->exams[e].entered) printField(gradeColor(res[e].grade), gradeNames[res[e].grade], 8);
        else printf("%8s", "--");
    }
    putchar('\n');

    printf("  %-20s", "Result");
    for (e = 0; e < EXAMS; e++) {
        if (s->exams[e].entered) printField(passColor(res[e].passed), res[e].passed ? "PASS" : "FAIL", 8);
        else printf("%8s", "--");
    }
    putchar('\n');
    printf("  (-- = not recorded, which is different from a score of 0)\n");
}

/* Compact student table: percentage per assessment, coloured by pass/fail. */
static void printStudentTable(const int idx[], int n)
{
    int i, e;
    char buf[24];
    printf("  %-6s%-22s%8s%8s%8s\n", "Roll", "Name", examShort[0], examShort[1], examShort[2]);
    printRule('-');
    for (i = 0; i < n; i++) {
        const Student *s = &students[idx[i]];
        printf("  %-6d%-22.22s", s->rollNo, s->name);
        for (e = 0; e < EXAMS; e++) {
            if (s->exams[e].entered) {
                Result r = computeResult(s->exams[e].marks);
                snprintf(buf, sizeof buf, "%.1f%%", r.percentage);
                printField(passColor(r.passed), buf, 8);
            } else {
                printf("%8s", "--");
            }
        }
        putchar('\n');
    }
    printRule('-');
    printf("  %d student(s) listed.  Green = pass, red = fail, -- = not recorded.\n", n);
}

/* Result table for one exam (used by the passed / failed / support lists). */
static void printResultTable(const int idx[], int n, int e)
{
    int i;
    printf("  %-6s%-18s%7s%8s%6s  %-22s\n", "Roll", "Name", "Total", "Pct", "Grade", "Academic status");
    printRule('-');
    for (i = 0; i < n; i++) {
        const Student *s = &students[idx[i]];
        Result r = computeResult(s->exams[e].marks);
        Risk k = classifyRisk(&r);
        printf("  %-6d%-18.18s%7d%7.2f%% ", s->rollNo, s->name, r.total, r.percentage);
        printField(gradeColor(r.grade), gradeNames[r.grade], 5);
        printf("  ");
        printField(riskColor(k), riskNames[k], -22);
        putchar('\n');
    }
    printRule('-');
    printf("  %d student(s) listed for %s.\n", n, examNames[e]);
}

static void printStudentProfile(int idx)
{
    const Student *s = &students[idx];
    int e;
    printSection("STUDENT PROFILE");
    printf("  Roll number : %d\n", s->rollNo);
    printf("  Name        : %s\n\n", s->name);
    for (e = 0; e < EXAMS; e++) {
        printf("  %-24s ", examNames[e]);
        if (s->exams[e].entered) {
            Result r = computeResult(s->exams[e].marks);
            printf("%3d/%d  %6.2f%%  ", r.total, MAX_TOTAL, r.percentage);
            printField(gradeColor(r.grade), gradeNames[r.grade], -3);
            printField(passColor(r.passed), r.passed ? "PASS" : "FAIL", -4);
        } else {
            printf("-- not recorded --");
        }
        putchar('\n');
    }
}

/* ================= 7. ENGINE A: STUDENT & EXAM DATA (M1) ================= */

static void addStudent(void)
{
    int roll;
    Student *s;
    printSection("REGISTER NEW STUDENT");
    if (studentCount >= MAX_STUDENTS) {
        printError("Database full (100 students). Delete a record before adding another.");
        return;
    }
    for (;;) {
        roll = readInt("Roll number (1-999999): ", 1, 999999);
        if (isRollUnique(roll, -1)) break;
        printError("That roll number already exists. Choose a different one.");
    }
    s = &students[studentCount];
    memset(s, 0, sizeof *s);                 /* all exams start as "not recorded" */
    s->rollNo = roll;
    readName("Full name: ", s->name);
    studentCount++;
    printSuccess("Student registered. Add marks from Exam Management.");
}

static void viewStudent(void)
{
    int idx;
    printSection("VIEW STUDENT");
    idx = pickStudent();
    if (idx >= 0) printStudentProfile(idx);
}

static void editStudentName(void)
{
    int idx;
    printSection("EDIT STUDENT NAME");
    idx = pickStudent();
    if (idx < 0) return;
    printf("  Current name: %s\n", students[idx].name);
    readName("New name: ", students[idx].name);
    printSuccess("Name updated.");
}

static void editStudentRoll(void)
{
    int idx, roll;
    printSection("EDIT ROLL NUMBER");
    idx = pickStudent();
    if (idx < 0) return;
    printf("  Student: %s (current roll %d)\n", students[idx].name, students[idx].rollNo);
    for (;;) {
        roll = readInt("New roll number (1-999999): ", 1, 999999);
        if (isRollUnique(roll, idx)) break;   /* ignore the student's own record */
        printError("That roll number belongs to another student.");
    }
    changeProfileRoll(students[idx].rollNo, roll);
    students[idx].rollNo = roll;
    printSuccess("Roll number updated.");
}

static void deleteStudent(void)
{
    int idx, i;
    char msg[160];
    printSection("DELETE STUDENT");
    idx = pickStudent();
    if (idx < 0) return;
    snprintf(msg, sizeof msg, "Delete %s (roll %d) and ALL their exam records? (y/n): ",
             students[idx].name, students[idx].rollNo);
    if (!confirm(msg)) {
        printInfo("Deletion cancelled. Nothing was changed.");
        return;
    }
    removeProfileForRoll(students[idx].rollNo);
    for (i = idx; i < studentCount - 1; i++)      /* shift later records left */
        students[i] = students[i + 1];
    memset(&students[studentCount - 1], 0, sizeof(Student));
    studentCount--;
    printSuccess("Student and associated exam records deleted.");
}

static void searchByRoll(void)
{
    int roll, idx;
    printSection("SEARCH BY ROLL NUMBER");
    if (studentCount == 0) {
        printWarning("No students registered yet.");
        return;
    }
    roll = readInt("Roll number (0 to cancel): ", 0, 999999);
    if (roll == 0) return;
    idx = findByRoll(roll);
    if (idx < 0) printError("No student found with that roll number.");
    else printStudentProfile(idx);
}

static void searchByName(void)
{
    char buf[INPUT_LEN];
    int idx[MAX_STUDENTS], n = 0, i;
    printSection("SEARCH BY NAME");
    if (studentCount == 0) {
        printWarning("No students registered yet.");
        return;
    }
    for (;;) {
        printPrompt("Name or part of a name: ");
        if (readLine(buf, sizeof buf)) {
            trim(buf);
            if (buf[0] != '\0' && strlen(buf) < NAME_LEN) break;
        }
        printError("Enter 1 to 49 characters.");
    }
    for (i = 0; i < studentCount; i++)
        if (containsIgnoreCase(students[i].name, buf)) idx[n++] = i;   /* case-insensitive */
    if (n == 0) printInfo("No student name matches your search.");
    else { putchar('\n'); printStudentTable(idx, n); }
}

/* Reads five marks into a temporary array. Used by single entry, bulk entry
 * and the what-if simulator, so validation exists only once. */
static void readFiveMarks(int out[SUBJECTS])
{
    char prompt[64];
    int s;
    for (s = 0; s < SUBJECTS; s++) {
        snprintf(prompt, sizeof prompt, "%-18s (0-100): ", subjectNames[s]);
        out[s] = readInt(prompt, 0, MAX_MARK);
    }
}

static void storeMarks(Student *s, int e, const int marks[SUBJECTS])
{
    memcpy(s->exams[e].marks, marks, sizeof s->exams[e].marks);
    s->exams[e].entered = 1;
}

static void enterMarks(void)
{
    int si, e, tmp[SUBJECTS];
    printSection("ENTER / UPDATE MARKS");
    si = pickStudent();
    if (si < 0) return;
    printf("  Student: %s (roll %d)\n", students[si].name, students[si].rollNo);
    e = pickExam();
    if (e < 0) return;
    if (students[si].exams[e].entered) {
        printWarning("Marks already exist for this assessment:");
        printQuickResult(students[si].exams[e].marks);
        if (!confirm("Overwrite the existing marks? (y/n): ")) {
            printInfo("Cancelled. Existing marks kept.");
            return;
        }
    }
    printf("\n");
    readFiveMarks(tmp);
    storeMarks(&students[si], e, tmp);
    printSuccess("Marks saved.");
    printQuickResult(tmp);
}

static void bulkMarksEntry(void)
{
    int e, i, updated = 0, skipped = 0, tmp[SUBJECTS];
    char buf[INPUT_LEN], head[160];
    printSection("QUICK BULK MARKS ENTRY");
    if (studentCount == 0) {
        printWarning("No students registered yet.");
        return;
    }
    e = pickExam();
    if (e < 0) return;
    printInfo("For each student: Enter = type marks, S = skip, Q = stop.");
    for (i = 0; i < studentCount; i++) {
        char action = 'e';
        snprintf(head, sizeof head, "Student %d of %d: %d - %s", i + 1, studentCount,
                 students[i].rollNo, students[i].name);
        printf("\n");
        setColor(ANSI_HEAD);
        printf("  %s\n", head);
        resetColor();
        for (;;) {
            printPrompt("[Enter] enter marks   [S] skip   [Q] stop: ");
            if (readLine(buf, sizeof buf)) {
                trim(buf);
                if (buf[0] == '\0') { action = 'e'; break; }
                if (equalsIgnoreCase(buf, "s")) { action = 's'; break; }
                if (equalsIgnoreCase(buf, "q")) { action = 'q'; break; }
            }
            printWarning("Press Enter, or type S or Q.");
        }
        if (action == 'q') break;
        if (action == 's') { skipped++; continue; }
        if (students[i].exams[e].entered) {
            printWarning("This student already has marks for this assessment:");
            printQuickResult(students[i].exams[e].marks);
            if (!confirm("Overwrite? (y/n): ")) { skipped++; continue; }
        }
        readFiveMarks(tmp);
        storeMarks(&students[i], e, tmp);
        updated++;
        printQuickResult(tmp);
    }
    snprintf(head, sizeof head, "Bulk entry finished: %d record(s) saved, %d skipped.", updated, skipped);
    printSuccess(head);
}

static void showHistory(void)
{
    int si;
    printSection("ACADEMIC HISTORY");
    si = pickStudent();
    if (si < 0) return;
    printf("  %s (roll %d)\n\n", students[si].name, students[si].rollNo);
    printHistoryTable(&students[si], 0);
}

/* ================= 8. ENGINE B: RESULT PROCESSING (M2) ================= */

static void tiedNames(const int marks[SUBJECTS], int value, char *out, int size)
{
    joinSubjects(marks, value, value, out, size);   /* every subject with that mark */
}

/* Builds personalised suggestions from the stored marks. Returns the count. */
static int buildSuggestions(const Student *s, int e, char lines[][SUG_LEN], int maxLines)
{
    const int *m = s->exams[e].marks;
    Result r = computeResult(m);
    int n = 0, sub, prev, weak = 0, diff;
    char tmp[SUG_LEN], names[NAMES_LEN];

    for (sub = 0; sub < SUBJECTS && n < maxLines; sub++) {
        if (m[sub] < PASS_MARK) {
            snprintf(lines[n++], SUG_LEN,
                     "Priority: clear %s (scored %d, needs %d more mark(s) to pass).",
                     subjectNames[sub], m[sub], PASS_MARK - m[sub]);
        }
    }
    for (sub = 0; sub < SUBJECTS && n < maxLines; sub++) {
        if (m[sub] >= PASS_MARK && m[sub] < 60) {
            weak++;
            snprintf(lines[n++], SUG_LEN,
                     "Strengthen fundamentals in %s (%d marks): revise core topics and practise basic problems.",
                     subjectNames[sub], m[sub]);
        }
    }
    prev = previousExam(s, e);
    if (prev >= 0 && n < maxLines) {
        diff = r.total - computeResult(s->exams[prev].marks).total;
        if (diff < 0)
            snprintf(tmp, sizeof tmp, "Total fell by %d marks since %s: review recent mistakes and revise those topics.",
                     -diff, examShort[prev]);
        else if (diff > 0)
            snprintf(tmp, sizeof tmp, "Total rose by %d marks since %s: keep the same study routine.",
                     diff, examShort[prev]);
        else
            snprintf(tmp, sizeof tmp, "Total is unchanged since %s: add a weekly self-test to move ahead.",
                     examShort[prev]);
        snprintf(lines[n++], SUG_LEN, "%s", tmp);
    }
    if (n < maxLines) {
        if (r.passed && r.total >= 80 * SUBJECTS) {
            tiedNames(m, m[r.best], names, sizeof names);
            snprintf(lines[n++], SUG_LEN,
                     "Excellent result: attempt advanced problems (strongest: %s) and help classmates.", names);
        } else if (r.passed && weak == 0) {
            snprintf(lines[n++], SUG_LEN,
                     "Consistent performance: maintain regular practice and weekly revision.");
        }
    }
    if (n == 0)
        snprintf(lines[n++], SUG_LEN, "Keep practising regularly across all subjects.");
    return n;
}

static void printReportCard(const Student *s, int e)
{
    const int *m = s->exams[e].marks;
    Result r = computeResult(m);
    Risk k = classifyRisk(&r);
    char buf[INPUT_LEN], names[NAMES_LEN], reason[SUG_LEN], sug[MAX_SUG][SUG_LEN], prefix[16];
    int sub, n, i;

    putchar('\n');
    boxRule();
    boxCentered(ANSI_HEAD, "ACADEMIC REPORT CARD");
    boxCentered("", "ScholarIQ - Student Result & Academic Intelligence");
    boxRule();
    boxKV("Student", s->name, "");
    snprintf(buf, sizeof buf, "%d", s->rollNo);
    boxKV("Roll No", buf, "");
    boxKV("Assessment", examNames[e], "");
    boxRule();

    printf("| %-24s%6s   %-6s  %-22s%6s|\n", "Subject", "Marks", "Status", "Score (full bar=100)", "");
    boxRule();
    for (sub = 0; sub < SUBJECTS; sub++) {
        printf("| %-24.24s%6d   ", subjectNames[sub], m[sub]);
        printField(passColor(m[sub] >= PASS_MARK), m[sub] >= PASS_MARK ? "PASS" : "FAIL", -6);
        printf("  ");
        printBar(m[sub], MAX_MARK, 20, passColor(m[sub] >= PASS_MARK));
        printf("%6s|\n", "");
    }
    boxRule();

    snprintf(buf, sizeof buf, "%d / %d", r.total, MAX_TOTAL);
    boxKV("Total", buf, "");
    snprintf(buf, sizeof buf, "%.2f %%", r.percentage);
    boxKV("Percentage", buf, "");
    boxKV("Grade", gradeNames[r.grade], gradeColor(r.grade));
    snprintf(buf, sizeof buf, "%d of %d passed", r.passedCount, SUBJECTS);
    boxKV("Subjects", buf, "");
    boxKV("Result", r.passed ? "PASS" : "FAIL", passColor(r.passed));
    if (!r.passed && r.percentGrade != GRADES - 1) {
        snprintf(buf, sizeof buf, "Grade is F because a subject is below %d (percentage alone: %s).",
                 PASS_MARK, gradeNames[r.percentGrade]);
        boxText(ANSI_YELLOW, buf);
    }
    boxRule();

    tiedNames(m, m[r.best], names, sizeof names);
    snprintf(buf, sizeof buf, "%s (%d)", names, m[r.best]);
    boxKV("Strongest", buf, ANSI_GREEN);
    tiedNames(m, m[r.worst], names, sizeof names);
    snprintf(buf, sizeof buf, "%s (%d)", names, m[r.worst]);
    boxKV("Weakest", buf, m[r.worst] < PASS_MARK ? ANSI_RED : ANSI_YELLOW);
    boxRule();

    boxKV("Status", riskNames[k], riskColor(k));
    riskReason(m, &r, reason, sizeof reason);
    printWrapped("Why: ", reason, BOX_INNER - 1, 1);
    boxRule();

    boxText(ANSI_HEAD, "PERSONALISED SUGGESTIONS");
    n = buildSuggestions(s, e, sug, MAX_SUG);
    for (i = 0; i < n; i++) {
        snprintf(prefix, sizeof prefix, " %d. ", i + 1);
        printWrapped(prefix, sug[i], BOX_INNER - 1, 1);
    }
    boxRule();
    printf("  Demonstration grading rules - not official university regulations.\n");
}

static void showReportCard(void)
{
    int si, e;
    printSection("DETAILED REPORT CARD");
    if (pickStudentExam(&si, &e)) printReportCard(&students[si], e);
}

/* ---- ranking ---- */

/* Ordering rule: (1) students who passed every subject rank above those who
 * failed, (2) higher total first, (3) equal -> lower roll number first.
 * Step 3 only fixes the PRINT ORDER; equal students share the same rank. */
static int ranksBefore(int a, int b, int e)
{
    Result ra = computeResult(students[a].exams[e].marks);
    Result rb = computeResult(students[b].exams[e].marks);
    if (ra.passed != rb.passed) return ra.passed > rb.passed;
    if (ra.total != rb.total)   return ra.total > rb.total;
    return students[a].rollNo < students[b].rollNo;
}

/* Fills order[] with student indices (bubble sort). Records are never moved. */
static int buildRanking(int e, int order[])
{
    int n = 0, i, j, tmp;
    for (i = 0; i < studentCount; i++)
        if (students[i].exams[e].entered) order[n++] = i;
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - 1 - i; j++) {
            if (ranksBefore(order[j + 1], order[j], e)) {
                tmp = order[j]; order[j] = order[j + 1]; order[j + 1] = tmp;
            }
        }
    }
    return n;
}

/* Competition ranking: equal (pass status, total) share a rank, e.g. 1,2,3,3,5. */
static void assignRanks(int e, const int order[], int n, int rank[])
{
    int i;
    for (i = 0; i < n; i++) {
        Result cur = computeResult(students[order[i]].exams[e].marks);
        if (i > 0) {
            Result prv = computeResult(students[order[i - 1]].exams[e].marks);
            if (cur.passed == prv.passed && cur.total == prv.total) {
                rank[i] = rank[i - 1];
                continue;
            }
        }
        rank[i] = i + 1;
    }
}

static void printToppers(int e)
{
    int order[MAX_STUDENTS], rank[MAX_STUDENTS], n, i;
    n = buildRanking(e, order);
    if (n == 0) { printInfo("No marks recorded for this assessment."); return; }
    assignRanks(e, order, n, rank);
    if (!computeResult(students[order[0]].exams[e].marks).passed)
        printWarning("No student passed every subject. Showing the highest scorer(s).");
    for (i = 0; i < n && rank[i] == 1; i++) {
        Result r = computeResult(students[order[i]].exams[e].marks);
        printf("  * %s (roll %d) - %d/%d, %.2f%%, grade %s\n", students[order[i]].name,
               students[order[i]].rollNo, r.total, MAX_TOTAL, r.percentage, gradeNames[r.grade]);
    }
}

static void printRankHeader(void)
{
    printf("  %-6s%-6s%-18s%7s%8s%6s  %-6s\n", "Rank", "Roll", "Name", "Total", "Pct", "Grade", "Result");
    printRule('-');
}

static void printRankRow(int rank, int si, int e)
{
    Result r = computeResult(students[si].exams[e].marks);
    printf("  %-6d%-6d%-18.18s%7d%7.2f%% ", rank, students[si].rollNo, students[si].name,
           r.total, r.percentage);
    printField(gradeColor(r.grade), gradeNames[r.grade], 5);
    printf("  ");
    printField(passColor(r.passed), r.passed ? "PASS" : "FAIL", -6);
    putchar('\n');
}

static void showRanking(void)
{
    int e, order[MAX_STUDENTS], rank[MAX_STUDENTS], n, i;
    printSection("MERIT RANKING & CLASS TOPPER");
    e = pickClassExam();
    if (e < 0) return;
    n = buildRanking(e, order);
    assignRanks(e, order, n, rank);

    printSection("CLASS TOPPER(S)");
    printToppers(e);

    printSection("TOP THREE RANKS");
    printRankHeader();
    for (i = 0; i < n && rank[i] <= 3; i++) printRankRow(rank[i], order[i], e);
    printf("  Tie rule: same pass status + same total = same rank (shown by roll no).\n");

    if (n > 0 && rank[n - 1] > 3) {          /* more students than the top-three table */
        putchar('\n');
        if (confirm("Show the complete merit list? (y/n): ")) {
            printSection("COMPLETE MERIT LIST");
            printRankHeader();
            for (i = 0; i < n; i++) printRankRow(rank[i], order[i], e);
            printf("  Students with all subjects passed rank above students with a failed subject.\n");
        }
    }
}

/* ---- class statistics ---- */

static ClassStats computeClassStats(int e)
{
    ClassStats cs;
    int i;
    double sum = 0.0;
    memset(&cs, 0, sizeof cs);
    cs.registered = studentCount;
    for (i = 0; i < studentCount; i++) {
        Result r;
        if (!students[i].exams[e].entered) continue;      /* missing records are excluded */
        r = computeResult(students[i].exams[e].marks);
        cs.count++;
        if (r.passed) cs.pass++; else cs.fail++;
        sum += r.percentage;
        cs.gradeCount[r.grade]++;
        if (cs.count == 1 || r.total > cs.maxTotal) cs.maxTotal = r.total;
        if (cs.count == 1 || r.total < cs.minTotal) cs.minTotal = r.total;
    }
    cs.avgPct = cs.count > 0 ? sum / cs.count : 0.0;      /* no division by zero */
    return cs;
}

static void namesWithTotal(int e, int total, char *out, int size)
{
    int i;
    size_t used;
    out[0] = '\0';
    for (i = 0; i < studentCount; i++) {
        if (students[i].exams[e].entered &&
            computeResult(students[i].exams[e].marks).total == total) {
            used = strlen(out);
            if (used < (size_t)size - 1)
                snprintf(out + used, (size_t)size - used, "%s%s", used ? ", " : "", students[i].name);
        }
    }
}

static void printGradeDistribution(const ClassStats *cs)
{
    int g;
    for (g = 0; g < GRADES; g++) {
        printf("  %-3s %-18s", gradeNames[g], gradeRange[g]);
        printBar(cs->gradeCount[g], cs->count, CHART_W, gradeColor(g));
        printf(" %2d (%4.1f%%)\n", cs->gradeCount[g],
               cs->count > 0 ? 100.0 * cs->gradeCount[g] / cs->count : 0.0);
    }
    printf("  Full bar = all %d student(s) with marks. Failed students count as F.\n", cs->count);
}

static void showClassReport(void)
{
    int e;
    ClassStats cs;
    char names[SUG_LEN];
    printSection("CLASS PERFORMANCE REPORT");
    e = pickClassExam();
    if (e < 0) return;
    cs = computeClassStats(e);

    printSection(examNames[e]);
    printf("  %-28s: %d\n", "Registered students", cs.registered);
    printf("  %-28s: %d\n", "Marks recorded (included)", cs.count);
    printf("  %-28s: %d\n", "Not recorded (excluded)", cs.registered - cs.count);
    printf("  %-28s: %d\n", "Passed every subject", cs.pass);
    printf("  %-28s: %d\n", "Failed", cs.fail);
    printf("  %-28s: %.1f%%\n", "Class pass rate", cs.count > 0 ? 100.0 * cs.pass / cs.count : 0.0);
    printf("  %-28s: %.2f%%\n", "Class average percentage", cs.avgPct);
    namesWithTotal(e, cs.maxTotal, names, sizeof names);
    printf("  %-28s: %.2f%%  (%s)\n", "Highest percentage", cs.maxTotal / (double)SUBJECTS, names);
    namesWithTotal(e, cs.minTotal, names, sizeof names);
    printf("  %-28s: %.2f%%  (%s)\n", "Lowest percentage", cs.minTotal / (double)SUBJECTS, names);

    printSection("CLASS TOPPER(S)");
    printToppers(e);
    printSection("GRADE DISTRIBUTION");
    printGradeDistribution(&cs);
}

/* ================= 9. ENGINE C: ACADEMIC ANALYTICS (M3) ================= */

static void showStrongWeak(void)
{
    int si, e, sub;
    char names[NAMES_LEN];
    const int *m;
    Result r;
    printSection("STRONGEST & WEAKEST SUBJECTS");
    if (!pickStudentExam(&si, &e)) return;
    m = students[si].exams[e].marks;
    r = computeResult(m);
    printf("\n  %s - %s\n\n", students[si].name, examNames[e]);

    for (sub = 0; sub < SUBJECTS; sub++) {
        const char *tag, *col;
        if (m[sub] < PASS_MARK)      { tag = "FAILED - immediate attention";       col = ANSI_RED; }
        else if (m[sub] < 50)        { tag = "Barely passing - needs attention";   col = ANSI_YELLOW; }
        else if (m[sub] < 60)        { tag = "Below 60 - strengthen";              col = ANSI_YELLOW; }
        else                         { tag = "Satisfactory";                       col = ANSI_GREEN; }
        printf("  %-18.18s%4d  ", subjectNames[sub], m[sub]);
        printBar(m[sub], MAX_MARK, 20, col);
        printf("  ");
        setColor(col);
        printf("%s", tag);
        resetColor();
        putchar('\n');
    }
    putchar('\n');
    if (m[r.best] == m[r.worst]) {
        printInfo("All subjects have the same marks, so there is no single strongest or weakest.");
    } else {
        tiedNames(m, m[r.best], names, sizeof names);
        printf("  Strongest subject(s): %s (%d marks)\n", names, m[r.best]);
        tiedNames(m, m[r.worst], names, sizeof names);
        printf("  Weakest subject(s)  : %s (%d marks)\n", names, m[r.worst]);
    }
    joinSubjects(m, 0, PASS_MARK - 1, names, sizeof names);
    printf("  Failed subjects     : %s\n", names[0] ? names : "None");
    joinSubjects(m, 0, 59, names, sizeof names);
    printf("  Subjects below 60   : %s\n", names[0] ? names : "None");
}

static void computeSubjectStats(int e, SubjectStats st[SUBJECTS])
{
    int i, sub;
    for (sub = 0; sub < SUBJECTS; sub++) memset(&st[sub], 0, sizeof st[sub]);
    for (i = 0; i < studentCount; i++) {
        if (!students[i].exams[e].entered) continue;
        for (sub = 0; sub < SUBJECTS; sub++) {
            int v = students[i].exams[e].marks[sub];
            if (st[sub].count == 0 || v > st[sub].high) st[sub].high = v;
            if (st[sub].count == 0 || v < st[sub].low)  st[sub].low = v;
            if (v < PASS_MARK) st[sub].failed++;
            st[sub].sum += v;
            st[sub].count++;
        }
    }
}

static void showSubjectAnalysis(void)
{
    int e, sub, lowest = 0;
    SubjectStats st[SUBJECTS];
    printSection("SUBJECT ANALYSIS");
    e = pickClassExam();
    if (e < 0) return;
    computeSubjectStats(e, st);
    printSection(examNames[e]);
    printf("  %-20s%8s%8s%8s%8s%10s\n", "Subject", "Average", "High", "Low", "Failed", "Pass %");
    printRule('-');
    for (sub = 0; sub < SUBJECTS; sub++) {
        printf("  %-20.20s%8.2f%8d%8d%8d%9.1f%%\n", subjectNames[sub],
               (double)st[sub].sum / st[sub].count, st[sub].high, st[sub].low, st[sub].failed,
               100.0 * (st[sub].count - st[sub].failed) / st[sub].count);
        if (st[sub].sum < st[lowest].sum) lowest = sub;   /* sums compare exactly */
    }
    printRule('-');
    printf("  Based on %d student(s) with marks.\n\n", st[0].count);
    printf("  Lowest-performing subject (lowest class average): ");
    setColor(ANSI_YELLOW);
    for (sub = 0; sub < SUBJECTS; sub++)
        if (st[sub].sum == st[lowest].sum) printf("%s%s", sub != lowest ? ", " : "", subjectNames[sub]);
    resetColor();
    putchar('\n');
    printInfo("This describes this class's marks in this exam, not whether the subject is objectively harder.");
}

static void showRiskAnalysis(void)
{
    int e, i, n = 0, counts[5] = { 0, 0, 0, 0, 0 };
    char why[48];
    printSection("ACADEMIC RISK CLASSIFICATION");
    e = pickClassExam();
    if (e < 0) return;
    printSection(examNames[e]);
    printf("  %-6s%-18s%8s  %-23s%s\n", "Roll", "Name", "Pct", "Category", "Reason");
    printRule('-');
    for (i = 0; i < studentCount; i++) {
        Result r;
        Risk k;
        if (!students[i].exams[e].entered) continue;
        r = computeResult(students[i].exams[e].marks);
        k = classifyRisk(&r);
        counts[k]++;
        n++;
        switch (k) {
            case RISK_CRITICAL: snprintf(why, sizeof why, "%d subjects failed", r.failedCount); break;
            case RISK_HIGH:     snprintf(why, sizeof why, "1 subject failed"); break;
            case RISK_IMPROVE:  snprintf(why, sizeof why, "Below 60%%"); break;
            case RISK_STABLE:   snprintf(why, sizeof why, "60%% to 79.99%%"); break;
            default:            snprintf(why, sizeof why, "80%% or above"); break;
        }
        printf("  %-6d%-18.18s%7.2f%%  ", students[i].rollNo, students[i].name, r.percentage);
        printField(riskColor(k), riskNames[k], -23);
        printf("%s\n", why);
    }
    printRule('-');
    printf("  Summary of %d student(s):\n", n);
    for (i = 0; i < 5; i++) {
        printf("   %-22s", riskNames[i]);
        printBar(counts[i], n, 20, riskColor((Risk)i));
        printf(" %d\n", counts[i]);
    }
    printInfo("Rule-based support categories only - not a prediction of student success.");
}

static void showPerformanceTrend(void)
{
    int si, first, last;
    Trend t;
    const Student *s;
    int ft, lt;
    printSection("PERFORMANCE TREND ANALYZER");
    si = pickStudent();
    if (si < 0) return;
    s = &students[si];
    printf("  %s (roll %d)\n\n", s->name, s->rollNo);
    printHistoryTable(s, 1);
    t = studentTrend(s, &first, &last);
    putchar('\n');
    printf("  Overall trend: ");
    printField(trendColor(t), trendNames[t], 0);
    putchar('\n');
    if (t == TREND_NONE) {
        printInfo("At least two recorded assessments are needed. Missing data is never invented.");
    } else {
        ft = computeResult(s->exams[first].marks).total;
        lt = computeResult(s->exams[last].marks).total;
        printf("  %s (%d) -> %s (%d): total %+d marks, percentage %+.2f points\n",
               examShort[first], ft, examShort[last], lt, lt - ft, (lt - ft) / (double)SUBJECTS);
    }
    printSection("PERFORMANCE ACROSS ASSESSMENTS");
    {
        int e;
        char buf[16];
        for (e = 0; e < EXAMS; e++) {
            printf("  %-24s", examNames[e]);
            if (s->exams[e].entered) {
                Result r = computeResult(s->exams[e].marks);
                printBar(r.percentage, 100.0, CHART_W, passColor(r.passed));
                snprintf(buf, sizeof buf, "%.2f%%", r.percentage);
                printf(" %7s\n", buf);
            } else {
                printf("not recorded\n");
            }
        }
        printf("  Full bar = 100%%. Green = passed all subjects, red = failed a subject.\n");
    }
}

static void showSuggestions(void)
{
    int si, e, n, i;
    char sug[MAX_SUG][SUG_LEN], prefix[16];
    printSection("PERSONALISED IMPROVEMENT SUGGESTIONS");
    if (!pickStudentExam(&si, &e)) return;
    printf("\n  %s - %s\n", students[si].name, examNames[e]);
    printQuickResult(students[si].exams[e].marks);
    putchar('\n');
    n = buildSuggestions(&students[si], e, sug, MAX_SUG);
    for (i = 0; i < n; i++) {
        snprintf(prefix, sizeof prefix, "  %d. ", i + 1);
        printWrapped(prefix, sug[i], WIDTH - 2, 0);
    }
    printInfo("Suggestions follow simple rules applied to the stored marks.");
}

/* ================= 10. ENGINE D: SMART ACADEMIC PLANNING (M4) ================= */

/* Smallest total that reaches the target. Percentage = total / 5, so we need
 * total >= target/20 where target is in hundredths. Integer ceiling = exact. */
static int requiredTotal(int targetHund)
{
    return (targetHund + 19) / 20;
}

static void showTargetCalculator(void)
{
    int si, e, tHund, req, need;
    Result r;
    char msg[160];
    printSection("TARGET PERCENTAGE CALCULATOR");
    if (!pickStudentExam(&si, &e)) return;
    r = computeResult(students[si].exams[e].marks);
    printf("\n  %s - %s\n", students[si].name, examNames[e]);
    tHund = readPercent("Target percentage (e.g. 75 or 82.5): ");
    req = requiredTotal(tHund);
    need = req - r.total;

    putchar('\n');
    printf("  %-24s: %d / %d\n", "Current total", r.total, MAX_TOTAL);
    printf("  %-24s: %.2f%%\n", "Current percentage", r.percentage);
    printf("  %-24s: %d.%02d%%\n", "Target percentage", tHund / 100, tHund % 100);
    if (req > MAX_TOTAL) {
        printf("  %-24s: %d / %d\n", "Required total", req, MAX_TOTAL);
        putchar('\n');
        printError("Impossible target: no student can score above 100% (500 / 500).");
        return;
    }
    printf("  %-24s: %d / %d\n", "Required total", req, MAX_TOTAL);
    if (need <= 0) {
        printf("  %-24s: 0\n", "Additional marks needed");
        putchar('\n');
        printSuccess("Target already achieved with the current marks.");
    } else {
        printf("  %-24s: %d (about %.1f per subject)\n", "Additional marks needed", need,
               need / (double)SUBJECTS);
        printf("  %-24s: %d\n", "Maximum possible gain", MAX_TOTAL - r.total);
    }
    if (!r.passed) {
        snprintf(msg, sizeof msg,
                 "%d subject(s) are below %d. A percentage target alone does not give a PASS - use the Recovery Planner.",
                 r.failedCount, PASS_MARK);
        printWarning(msg);
    }
    printInfo("Hypothetical calculation only. It does not predict real exam performance.");
}

static void showWhatIf(void)
{
    int si, e, sim[SUBJECTS], sub;
    const int *orig;
    Result b, a;
    printSection("WHAT-IF RESULT SIMULATOR");
    if (!pickStudentExam(&si, &e)) return;
    orig = students[si].exams[e].marks;       /* read-only: we only copy from it */
    printf("\n  %s - %s\n  Current marks:", students[si].name, examNames[e]);
    for (sub = 0; sub < SUBJECTS; sub++) printf(" %d", orig[sub]);
    printf("\n");
    printInfo("Enter HYPOTHETICAL marks. Stored marks will not be changed.");
    readFiveMarks(sim);                        /* stored only in this temporary array */
    b = computeResult(orig);
    a = computeResult(sim);

    printSection("BEFORE vs AFTER");
    printf("  %-20s%8s%8s%8s\n", "Subject", "Before", "After", "Diff");
    printRule('-');
    for (sub = 0; sub < SUBJECTS; sub++)
        printf("  %-20.20s%8d%8d%+8d\n", subjectNames[sub], orig[sub], sim[sub], sim[sub] - orig[sub]);
    printRule('-');
    printf("  %-20s%8d%8d%+8d\n", "Total (/500)", b.total, a.total, a.total - b.total);
    printf("  %-20s%7.2f%%%7.2f%%%+7.2f%%\n", "Percentage", b.percentage, a.percentage,
           a.percentage - b.percentage);
    printf("  %-20s", "Grade");
    printField(gradeColor(b.grade), gradeNames[b.grade], 8);
    printField(gradeColor(a.grade), gradeNames[a.grade], 8);
    if (a.grade < b.grade)      printField(ANSI_GREEN, "UP", 8);
    else if (a.grade > b.grade) printField(ANSI_RED, "DOWN", 8);
    else                        printField("", "SAME", 8);
    printf("\n  %-20s", "Result");
    printField(passColor(b.passed), b.passed ? "PASS" : "FAIL", 8);
    printField(passColor(a.passed), a.passed ? "PASS" : "FAIL", 8);
    printField(b.passed != a.passed ? ANSI_YELLOW : "", b.passed != a.passed ? "CHANGED" : "SAME", 8);
    printf("\n");
    printRule('-');
    printSuccess("Simulation finished. The original stored marks were NOT modified.");
}

/* Recovery plan (integer arithmetic only):
 *  1. Work out the total needed for the target percentage.
 *  2. First lift every failed subject to the pass mark (40).
 *  3. If more marks are still needed, add them one at a time to the
 *     lowest-scoring subject that is still below 100 (weakest first).
 * Marks are never reduced and never exceed 100. */
static RecoveryPlan buildRecoveryPlan(const int cur[SUBJECTS], int targetHund)
{
    RecoveryPlan p;
    int sub, sum = 0, deficit, low;
    p.passFix = 0;
    p.spread = 0;
    p.required = requiredTotal(targetHund);
    p.feasible = (p.required <= MAX_TOTAL);
    for (sub = 0; sub < SUBJECTS; sub++) p.marks[sub] = cur[sub];
    if (!p.feasible) return p;

    for (sub = 0; sub < SUBJECTS; sub++) {          /* step 2: clear failed subjects */
        if (p.marks[sub] < PASS_MARK) {
            p.passFix += PASS_MARK - p.marks[sub];
            p.marks[sub] = PASS_MARK;
        }
        sum += p.marks[sub];
    }
    deficit = p.required - sum;
    while (deficit > 0) {                           /* step 3: level up the weakest */
        low = -1;
        for (sub = 0; sub < SUBJECTS; sub++)
            if (p.marks[sub] < MAX_MARK && (low < 0 || p.marks[sub] < p.marks[low])) low = sub;
        if (low < 0) break;
        p.marks[low]++;
        p.spread++;
        deficit--;
    }
    return p;
}

static void showRecoveryPlanner(void)
{
    int si, e, tHund, sub, allPass = 1;
    const int *cur;
    Result r, pr;
    RecoveryPlan p;
    char buf[INPUT_LEN];

    printSection("SMART MARKS RECOVERY PLANNER");
    if (!pickStudentExam(&si, &e)) return;
    cur = students[si].exams[e].marks;
    r = computeResult(cur);
    printf("\n  %s - %s\n", students[si].name, examNames[e]);
    printQuickResult(cur);
    tHund = readPercent("Desired overall percentage (e.g. 60 or 72.5): ");
    p = buildRecoveryPlan(cur, tHund);

    if (!p.feasible) {
        printf("\n  Target %d.%02d%% needs %d marks out of %d.\n", tHund / 100, tHund % 100, p.required, MAX_TOTAL);
        printError("Impossible target: the maximum possible is 100% (500 / 500). No plan can be built.");
        return;
    }
    pr = computeResult(p.marks);
    for (sub = 0; sub < SUBJECTS; sub++) if (p.marks[sub] < PASS_MARK) allPass = 0;

    printSection("SUGGESTED SUBJECT-WISE PLAN");
    printf("  %-20s%9s%11s%6s   %s\n", "Subject", "Current", "Suggested", "Add", "Status");
    printRule('-');
    for (sub = 0; sub < SUBJECTS; sub++) {
        printf("  %-20.20s%9d%11d%+6d   ", subjectNames[sub], cur[sub], p.marks[sub], p.marks[sub] - cur[sub]);
        if (cur[sub] < PASS_MARK)
            printField(ANSI_YELLOW, "FAIL -> PASS", -14);
        else if (p.marks[sub] > cur[sub])
            printField(ANSI_CYAN, "improved", -14);
        else
            printField("", "unchanged", -14);
        putchar('\n');
    }
    printRule('-');
    printf("  %-20s%9d%11d%+6d\n", "Total (/500)", r.total, pr.total, pr.total - r.total);
    printf("  %-20s%8.2f%%%10.2f%%\n", "Percentage", r.percentage, pr.percentage);

    putchar('\n');
    printf("  Target percentage reached : ");
    printField(passColor(pr.total >= p.required), pr.total >= p.required ? "YES" : "NO", 0);
    printf("  (%d.%02d%% needs %d marks)\n", tHund / 100, tHund % 100, p.required);
    printf("  Every subject passing     : ");
    printField(passColor(allPass), allPass ? "YES" : "NO", 0);
    printf("\n  Projected result          : %s, grade %s\n", pr.passed ? "PASS" : "FAIL", gradeNames[pr.grade]);

    printSection("HOW THE PLAN WAS BUILT");
    snprintf(buf, sizeof buf, "Required total = ceiling of %d.%02d%% of %d = %d marks.", tHund / 100,
             tHund % 100, MAX_TOTAL, p.required);
    printWrapped("  1. ", buf, WIDTH - 2, 0);
    snprintf(buf, sizeof buf, "Failed subjects come first: %d mark(s) added to lift every subject below %d up to %d.",
             p.passFix, PASS_MARK, PASS_MARK);
    printWrapped("  2. ", buf, WIDTH - 2, 0);
    snprintf(buf, sizeof buf,
             "%d remaining mark(s) added one at a time to the lowest-scoring subject (never above %d, never reducing a mark).",
             p.spread, MAX_MARK);
    printWrapped("  3. ", buf, WIDTH - 2, 0);
    if (r.total >= p.required && r.passed)
        printSuccess("The target is already achieved and every subject passes - no extra marks are needed.");
    else if (r.total >= p.required && !r.passed)
        printWarning("The percentage target is already met, but failed subjects still need marks to pass.");
    if (p.spread > 0 && pr.total * 20 > tHund)      /* overshoot caused only by whole marks */
        printInfo("Projected percentage can be slightly above the target because marks are whole numbers.");
    printInfo("A planning aid only. It does not predict real exam performance.");
}

/* ================= 11. ENGINE E: CHARTS, LISTS, DASHBOARD, DEMO DATA ================= */

static void chartSubjectAverages(void)
{
    int e, sub;
    SubjectStats st[SUBJECTS];
    e = pickClassExam();
    if (e < 0) return;
    computeSubjectStats(e, st);
    printHeading("SUBJECT PERFORMANCE");
    printf("  %s - %d student(s)\n\n", examNames[e], st[0].count);
    for (sub = 0; sub < SUBJECTS; sub++) {
        double avg = (double)st[sub].sum / st[sub].count;
        printf("  %-18.18s", subjectNames[sub]);
        printBar(avg, 100.0, CHART_W, avg >= 60 ? ANSI_GREEN : (avg >= PASS_MARK ? ANSI_YELLOW : ANSI_RED));
        printf(" %6.2f%%\n", avg);
    }
    printf("\n  Scale: full bar = 100%% (each # = %.1f%%).\n", 100.0 / CHART_W);
}

static void chartGradeDistribution(void)
{
    int e;
    ClassStats cs;
    e = pickClassExam();
    if (e < 0) return;
    cs = computeClassStats(e);
    printHeading("GRADE DISTRIBUTION");
    printf("  %s\n\n", examNames[e]);
    printGradeDistribution(&cs);
}

static void chartPassFail(void)
{
    int e;
    ClassStats cs;
    e = pickClassExam();
    if (e < 0) return;
    cs = computeClassStats(e);
    printHeading("CLASS PASS / FAIL DISTRIBUTION");
    printf("  %s\n\n", examNames[e]);
    printf("  %-8s", "Passed");
    printBar(cs.pass, cs.count, CHART_W, ANSI_GREEN);
    printf(" %d (%.1f%%)\n", cs.pass, 100.0 * cs.pass / cs.count);
    printf("  %-8s", "Failed");
    printBar(cs.fail, cs.count, CHART_W, ANSI_RED);
    printf(" %d (%.1f%%)\n", cs.fail, 100.0 * cs.fail / cs.count);
    printf("\n  Scale: full bar = all %d student(s) with marks.", cs.count);
    if (cs.registered > cs.count)
        printf(" %d without marks are excluded.", cs.registered - cs.count);
    putchar('\n');
}

static void chartStudentAssessments(void)
{
    int si, e;
    char buf[16];
    si = pickStudent();
    if (si < 0) return;
    printHeading("STUDENT PERFORMANCE ACROSS ASSESSMENTS");
    printf("  %s (roll %d)\n\n", students[si].name, students[si].rollNo);
    for (e = 0; e < EXAMS; e++) {
        printf("  %-24s", examNames[e]);
        if (students[si].exams[e].entered) {
            Result r = computeResult(students[si].exams[e].marks);
            printBar(r.percentage, 100.0, CHART_W, passColor(r.passed));
            snprintf(buf, sizeof buf, "%.2f%%", r.percentage);
            printf(" %7s\n", buf);
        } else {
            printf("not recorded\n");
        }
    }
    printf("\n  Full bar = 100%%. Green = passed all subjects, red = failed a subject.\n");
}

#define LIST_ALL     0
#define LIST_PASSED  1
#define LIST_FAILED  2
#define LIST_SUPPORT 3

static void showStudentList(int mode)
{
    int idx[MAX_STUDENTS], n = 0, i, e = 0;
    static const char *titles[4] = { "ALL STUDENTS", "STUDENTS WHO PASSED", "STUDENTS WHO FAILED",
                                     "STUDENTS REQUIRING ACADEMIC SUPPORT" };
    printSection(titles[mode]);
    if (mode == LIST_ALL) {
        if (studentCount == 0) {
            printWarning("No students registered yet. Register students or load demo data.");
            return;
        }
        for (i = 0; i < studentCount; i++) idx[n++] = i;
        printStudentTable(idx, n);
        return;
    }
    e = pickClassExam();
    if (e < 0) return;
    for (i = 0; i < studentCount; i++) {
        Result r;
        if (!students[i].exams[e].entered) continue;
        r = computeResult(students[i].exams[e].marks);
        if (mode == LIST_PASSED && r.passed) idx[n++] = i;
        else if (mode == LIST_FAILED && !r.passed) idx[n++] = i;
        /* support = Critical, High Priority or Improvement Needed */
        else if (mode == LIST_SUPPORT && classifyRisk(&r) <= RISK_IMPROVE) idx[n++] = i;
    }
    if (n == 0) { printInfo("No students match this filter for the selected assessment."); return; }
    putchar('\n');
    printResultTable(idx, n, e);
}

typedef struct { int roll; const char *name; } DemoStudent;
typedef struct { int roll; int exam; int marks[SUBJECTS]; } DemoRecord;

/* Demo data is stored as flat record lists (no 3-D arrays). */
static const DemoStudent demoStudents[] = {
    {101, "Aarav Sharma"}, {102, "Diya Patel"},  {103, "Rohan Mehta"}, {104, "Ananya Iyer"},
    {105, "Kabir Singh"},  {106, "Meera Nair"},  {107, "Arjun Verma"}, {108, "Sneha Gupta"},
    {109, "Vikram Rao"},   {110, "Isha Kapoor"}
};
static const DemoRecord demoRecords[] = {
    {101, 0, {88, 85, 90, 92, 84}}, {101, 1, {90, 88, 92, 95, 86}}, {101, 2, {94, 91, 93, 98, 89}},
    {102, 0, {85, 86, 88, 84, 82}}, {102, 1, {88, 87, 90, 88, 85}}, {102, 2, {91, 89, 94, 90, 86}},
    {103, 0, {92, 90, 88, 95, 48}}, {103, 1, {94, 92, 90, 96, 42}}, {103, 2, {99, 96, 97, 98, 35}},
    {104, 0, {80, 78, 82, 85, 76}}, {104, 1, {72, 70, 75, 78, 70}}, {104, 2, {62, 58, 65, 70, 60}},
    {105, 0, {38, 35, 44, 50, 41}}, {105, 1, {38, 36, 44, 50, 41}}, {105, 2, {35, 28, 45, 50, 38}},
    {106, 0, {70, 68, 70, 75, 72}}, {106, 1, {72, 70, 71, 78, 73}}, {106, 2, {75, 70, 72, 80, 75}},
    {107, 0, {78, 70, 75, 80, 82}}, {107, 1, {79, 68, 72, 79, 81}}, {107, 2, {80, 65, 70, 77, 80}},
    {108, 0, {40, 40, 40, 40, 40}}, {108, 1, {41, 40, 43, 40, 42}}, {108, 2, {40, 40, 40, 40, 40}},
    {109, 0, {55, 45, 58, 60, 60}}, {109, 2, {58, 38, 60, 65, 62}},
    {110, 0, {66, 72, 58, 70, 64}}
};
#define DEMO_STUDENT_COUNT ((int)(sizeof demoStudents / sizeof demoStudents[0]))
#define DEMO_RECORD_COUNT  ((int)(sizeof demoRecords / sizeof demoRecords[0]))

static void loadDemoData(void)
{
    int i, idx;
    char msg[160];
    printSection("LOAD DEMONSTRATION DATA");
    if (studentCount == 0) demoLoaded = 0;       /* everything was deleted: allow reload */
    if (demoLoaded) {
        printWarning("Demonstration data is already loaded. Loading it twice is blocked.");
        return;
    }
    if (studentCount > 0) {
        snprintf(msg, sizeof msg, "Loading demo data will ERASE the %d student(s) currently stored. Continue? (y/n): ",
                 studentCount);
        if (!confirm(msg)) {
            printInfo("Cancelled. Existing data kept.");
            return;
        }
    }
    resetStudyProfiles();
    memset(students, 0, sizeof students);
    studentCount = 0;
    for (i = 0; i < DEMO_STUDENT_COUNT; i++) {
        students[studentCount].rollNo = demoStudents[i].roll;
        snprintf(students[studentCount].name, NAME_LEN, "%s", demoStudents[i].name);
        studentCount++;
    }
    for (i = 0; i < DEMO_RECORD_COUNT; i++) {
        idx = findByRoll(demoRecords[i].roll);
        if (idx >= 0) storeMarks(&students[idx], demoRecords[i].exam, demoRecords[i].marks);
    }
    demoLoaded = 1;
    snprintf(msg, sizeof msg, "Loaded %d students with %d assessment records.", studentCount, DEMO_RECORD_COUNT);
    printSuccess(msg);
}

/* ================= 12. MENUS, DASHBOARD AND main() ================= */

static void printDashboard(void)
{
    int e;
    ClassStats cs;
    printSection("DASHBOARD (live data)");
    printf("  Students registered : %d / %d\n", studentCount, MAX_STUDENTS);
    printf("  Each assessment     : %d subjects x %d marks, pass mark %d per subject\n",
           SUBJECTS, MAX_MARK, PASS_MARK);
    if (studentCount == 0)
        printInfo("No records yet. Choose [9] to load demo data or [1] to register students.");
    printf("\n  %-24s%8s%8s%11s%11s\n", "Assessment", "Records", "Passed", "Pass rate", "Class avg");
    for (e = 0; e < EXAMS; e++) {
        cs = computeClassStats(e);
        if (cs.count == 0)
            printf("  %-24s%8s%8s%11s%11s\n", examNames[e], "--", "--", "--", "--");
        else
            printf("  %-24s%8d%8d%10.1f%%%10.2f%%\n", examNames[e], cs.count, cs.pass,
                   100.0 * cs.pass / cs.count, cs.avgPct);
    }
}

static void showMainMenu(void)
{
    putchar('\n');
    setColor(ANSI_HEAD);
    printRule('=');
    printCentered("S C H O L A R   I Q");
    printCentered("Student Result & Academic Intelligence System");
    printCentered("Beyond Marks. Understand Performance. Plan Improvement.");
    printRule('=');
    resetColor();
    printDashboard();
    printSection("MODULES");
    printf("  [1]  %-28s%s\n", "Student Management", "Register | Edit | Delete | Search");
    printf("  [2]  %-28s%s\n", "Exam Management", "Marks Entry | Bulk Entry | History");
    printf("  [3]  %-28s%s\n", "Result Processing", "Report Cards | Merit Ranking");
    printf("  [4]  %-28s%s\n", "Class Performance", "Class Report | Subject Analysis");
    printf("  [5]  %-28s%s\n", "Academic Intelligence", "Risk | Trends | Suggestions");
    printf("  [6]  %-28s%s\n", "Smart Academic Planner", "Target Calculator | Recovery");
    printf("  [7]  %-28s%s\n", "What-If Result Simulator", "Simulate and Compare Results");
    printf("  [8]  %-28s%s\n", "Reports & Visual Analytics", "Lists | Charts");
    printf("  [9]  %-28s%s\n", "Load Demonstration Data", "10 sample students");
    printf("  [0]  Logout\n");
    printRule('-');
}

static void studentMenu(void)
{
    int c;
    do {
        printHeading("STUDENT MANAGEMENT");
        printMenuOption(1, "Register new student");
        printMenuOption(2, "View student profile");
        printMenuOption(3, "Edit student name");
        printMenuOption(4, "Edit roll number");
        printMenuOption(5, "Delete student");
        printMenuOption(6, "Search by roll number");
        printMenuOption(7, "Search by name");
        printMenuOption(0, "Back to main menu");
        c = readInt("Enter your choice (0-7): ", 0, 7);
        switch (c) {
            case 1: addStudent();      break;
            case 2: viewStudent();     break;
            case 3: editStudentName(); break;
            case 4: editStudentRoll(); break;
            case 5: deleteStudent();   break;
            case 6: searchByRoll();    break;
            case 7: searchByName();    break;
            default: break;
        }
    } while (c != 0);
}

static void examMenu(void)
{
    int c;
    do {
        printHeading("EXAM MANAGEMENT");
        printMenuOption(1, "Enter / update marks for one student");
        printMenuOption(2, "Quick bulk marks entry");
        printMenuOption(3, "Academic history (all assessments)");
        printMenuOption(0, "Back to main menu");
        c = readInt("Enter your choice (0-3): ", 0, 3);
        switch (c) {
            case 1: enterMarks();       break;
            case 2: bulkMarksEntry();   break;
            case 3: showHistory();      break;
            default: break;
        }
    } while (c != 0);
}

static void resultMenu(void)
{
    int c;
    do {
        printHeading("RESULT PROCESSING");
        printMenuOption(1, "Detailed report card");
        printMenuOption(2, "Merit ranking & class topper");
        printMenuOption(0, "Back to main menu");
        c = readInt("Enter your choice (0-2): ", 0, 2);
        switch (c) {
            case 1: showReportCard(); break;
            case 2: showRanking();    break;
            default: break;
        }
    } while (c != 0);
}

static void classMenu(void)
{
    int c;
    do {
        printHeading("CLASS PERFORMANCE");
        printMenuOption(1, "Class performance report");
        printMenuOption(2, "Subject difficulty analysis");
        printMenuOption(0, "Back to main menu");
        c = readInt("Enter your choice (0-2): ", 0, 2);
        switch (c) {
            case 1: showClassReport();      break;
            case 2: showSubjectAnalysis();  break;
            default: break;
        }
    } while (c != 0);
}

static void intelligenceMenu(void)
{
    int c;
    do {
        printHeading("ACADEMIC INTELLIGENCE");
        printMenuOption(1, "Strongest & weakest subjects");
        printMenuOption(2, "Academic risk classification");
        printMenuOption(3, "Performance trend analyzer");
        printMenuOption(4, "Personalised improvement suggestions");
        printMenuOption(0, "Back to main menu");
        c = readInt("Enter your choice (0-4): ", 0, 4);
        switch (c) {
            case 1: showStrongWeak();        break;
            case 2: showRiskAnalysis();      break;
            case 3: showPerformanceTrend();  break;
            case 4: showSuggestions();       break;
            default: break;
        }
    } while (c != 0);
}

static void plannerMenu(void)
{
    int c;
    do {
        printHeading("SMART ACADEMIC PLANNER");
        printMenuOption(1, "Target percentage calculator");
        printMenuOption(2, "Smart marks recovery planner");
        printMenuOption(0, "Back to main menu");
        c = readInt("Enter your choice (0-2): ", 0, 2);
        switch (c) {
            case 1: showTargetCalculator(); break;
            case 2: showRecoveryPlanner();  break;
            default: break;
        }
    } while (c != 0);
}

static void reportsMenu(void)
{
    int c;
    do {
        printHeading("REPORTS & VISUAL ANALYTICS");
        printMenuOption(1, "All students");
        printMenuOption(2, "Students who passed");
        printMenuOption(3, "Students who failed");
        printMenuOption(4, "Students requiring academic support");
        printMenuOption(5, "Chart: subject averages");
        printMenuOption(6, "Chart: grade distribution");
        printMenuOption(7, "Chart: class pass / fail");
        printMenuOption(8, "Chart: one student across assessments");
        printMenuOption(0, "Back to main menu");
        c = readInt("Enter your choice (0-8): ", 0, 8);
        switch (c) {
            case 1: showStudentList(LIST_ALL);     break;
            case 2: showStudentList(LIST_PASSED);  break;
            case 3: showStudentList(LIST_FAILED);  break;
            case 4: showStudentList(LIST_SUPPORT); break;
            case 5: chartSubjectAverages();        break;
            case 6: chartGradeDistribution();      break;
            case 7: chartPassFail();               break;
            case 8: chartStudentAssessments();     break;
            default: break;
        }
    } while (c != 0);
}


/* ============== 13. PERSONAL STUDY COMPANION (NEW) ============== */
/* SAP IDs here are local DEMO identifiers, not official UPES authentication. */
#define MAX_TASKS 32
#define QUIZ_QUESTIONS 5
#define MAX_SESSIONS 40
#define TASK_LEN 64

typedef struct {
    char title[TASK_LEN];
    int subject;                  /* 0..4 */
    int done;
} StudyTask;

typedef struct {
    time_t started;
    int durationMinutes;         /* elapsed whole minutes */
    int subject;
} StudySession;

typedef struct {
    int sapId;
    char pin[16]; /* demo-only ScholarIQ PIN, not UPES password */
    int studentRoll;
    StudyTask tasks[MAX_TASKS];
    int taskCount;
    StudySession sessions[MAX_SESSIONS];
    int sessionCount;
    int totalMinutes;
    int waterCheckins;
    int reminderMinutes;
    int quizAttempts;
    int quizCorrect;
    int quizQuestions;
    int examDaysRemaining; /* manually maintained demo countdown */
    int studyDays[60]; /* day keys for last 60 recorded study days */
    int studyDayCount;
} StudyProfile;

static StudyProfile profiles[MAX_STUDENTS];
static int profileCount = 0;
static int activeProfile = -1;

static void resetStudyProfiles(void)
{
    memset(profiles, 0, sizeof profiles);
    profileCount = 0;
    activeProfile = -1;
}

static void removeProfileForRoll(int roll)
{
    int i, j;
    for (i = 0; i < profileCount; i++) {
        if (profiles[i].studentRoll == roll) {
            for (j = i; j < profileCount - 1; j++) profiles[j] = profiles[j + 1];
            profileCount--;
            activeProfile = -1;  /* require reselect after array shift */
            return;
        }
    }
}

static void changeProfileRoll(int oldRoll, int newRoll)
{
    int i;
    for (i = 0; i < profileCount; i++)
        if (profiles[i].studentRoll == oldRoll) profiles[i].studentRoll = newRoll;
}

static int findProfile(int sapId)
{
    int i;
    for (i = 0; i < profileCount; i++)
        if (profiles[i].sapId == sapId) return i;
    return -1;
}

/* Admin creates the student's local account. No official UPES credentials. */
static void createStudyProfile(void)
{
    int sap, roll, idx;
    char pin[INPUT_LEN];
    StudyProfile *p;
    printSection("CREATE STUDENT ACCOUNT (ADMIN)");
    printInfo("Use a demo SAP ID and a NEW ScholarIQ PIN, never a UPES password.");
    if (profileCount >= MAX_STUDENTS) { printError("Account limit reached."); return; }
    idx = pickStudent();
    if (idx < 0) return;
    roll = students[idx].rollNo;
    for (idx = 0; idx < profileCount; idx++)
        if (profiles[idx].studentRoll == roll) {
            printError("This student already has an account."); return;
        }
    sap = readInt("Demo SAP ID (1-999999999): ", 1, 999999999);
    if (findProfile(sap) >= 0) { printError("SAP ID already used."); return; }
    for (;;) {
        printPrompt("Set separate ScholarIQ PIN (4-8 digits): ");
        if (readLine(pin, sizeof pin)) {
            size_t i, n = strlen(pin);
            int valid = n >= 4 && n <= 8;
            for (i = 0; i < n; i++) if (pin[i] < '0' || pin[i] > '9') valid = 0;
            if (valid) break;
        }
        printError("Use 4 to 8 digits. Do NOT use a real UPES password.");
    }
    p = &profiles[profileCount++];
    memset(p, 0, sizeof *p);
    p->sapId = sap;
    memcpy(p->pin, pin, strlen(pin) + 1);
    p->studentRoll = roll;
    p->reminderMinutes = 30;
    p->examDaysRemaining = -1;
    printSuccess("Student account created. Student can now sign in.");
}

static StudyProfile *currentProfile(void)
{
    if (activeProfile < 0 || activeProfile >= profileCount) {
        printWarning("Select or create a study profile first.");
        return NULL;
    }
    return &profiles[activeProfile];
}

static int studentLogin(void)
{
    int sap, idx, attempts;
    char pin[INPUT_LEN];
    printHeading("STUDENT LOGIN");
    printInfo("ScholarIQ DEMO PIN only. Never type your official UPES password.");
    activeProfile = -1;
    for (attempts = 0; attempts < 3; attempts++) {
        sap = readInt("SAP ID (0 to cancel): ", 0, 999999999);
        if (!sap) return 0;
        printPrompt("ScholarIQ PIN: ");
        if (!readLine(pin, sizeof pin)) continue;
        idx = findProfile(sap);
        if (idx >= 0 && strcmp(profiles[idx].pin, pin) == 0 &&
            findByRoll(profiles[idx].studentRoll) >= 0) {
            activeProfile = idx;
            printSuccess("Signed in to your personal dashboard.");
            return 1;
        }
        printError("Invalid SAP ID or ScholarIQ PIN.");
    }
    printWarning("Three unsuccessful attempts. Returning to welcome menu.");
    return 0;
}

static int adminLogin(void)
{
    int attempts;
    char user[INPUT_LEN], pin[INPUT_LEN];
    printHeading("ADMIN LOGIN");
    printInfo("Demo admin only. ID: admin, PIN: 2468 (change before a public demo).");
    for (attempts = 0; attempts < 3; attempts++) {
        printPrompt("Admin ID: ");
        if (!readLine(user, sizeof user)) continue;
        printPrompt("Demo admin PIN: ");
        if (!readLine(pin, sizeof pin)) continue;
        if (strcmp(user, "admin") == 0 && strcmp(pin, "2468") == 0) return 1;
        printError("Incorrect admin ID or PIN.");
    }
    return 0;
}

static void addStudyTask(void)
{
    StudyProfile *p = currentProfile();
    char title[INPUT_LEN];
    int sub;
    if (!p) return;
    if (p->taskCount == MAX_TASKS) { printWarning("Task list full."); return; }
    for (;;) {
        printPrompt("Study task (1-63 characters): ");
        if (readLine(title, sizeof title)) {
            trim(title);
            if (title[0] && strlen(title) < TASK_LEN) break;
        }
        printError("Enter a shorter nonempty task.");
    }
    for (sub = 0; sub < SUBJECTS; sub++)
        printf("  [%d] %s\n", sub + 1, subjectNames[sub]);
    sub = readInt("Choose subject (1-5): ", 1, SUBJECTS) - 1;
    memcpy(p->tasks[p->taskCount].title, title, strlen(title) + 1);
    p->tasks[p->taskCount].subject = sub;
    p->tasks[p->taskCount].done = 0;
    p->taskCount++;
    printSuccess("Task added to this student's profile.");
}

static int completedTasks(const StudyProfile *p)
{
    int i, done = 0;
    for (i = 0; i < p->taskCount; i++) done += p->tasks[i].done;
    return done;
}

static void listStudyTasks(void)
{
    StudyProfile *p = currentProfile();
    int i;
    if (!p) return;
    printSection("MY STUDY TASKS");
    if (!p->taskCount) { printInfo("No tasks yet."); return; }
    for (i = 0; i < p->taskCount; i++)
        printf("  [%2d] [%c] %-18s %s\n", i + 1,
               p->tasks[i].done ? 'X' : ' ',
               subjectNames[p->tasks[i].subject], p->tasks[i].title);
    printf("  Completed: %d / %d\n", completedTasks(p), p->taskCount);
}

static void finishStudyTask(void)
{
    StudyProfile *p = currentProfile();
    int n;
    if (!p) return;
    if (!p->taskCount) { printWarning("Add a task first."); return; }
    listStudyTasks();
    n = readInt("Task number to complete (0 to cancel): ", 0, p->taskCount);
    if (!n) return;
    if (p->tasks[n - 1].done) { printInfo("Already completed."); return; }
    p->tasks[n - 1].done = 1;
    printSuccess("Task completed! Badge progress updated.");
}

static void configureHydration(void)
{
    StudyProfile *p = currentProfile();
    if (!p) return;
    p->reminderMinutes = readInt("Water reminder interval (10-120 minutes): ", 10, 120);
    printSuccess("Reminder interval updated.");
}

/* Standard C cannot refresh a timer while waiting for terminal input.
 * This records actual wall-clock time when the student presses Enter to stop. */
static void startStudySession(void)
{
    StudyProfile *p = currentProfile();
    char input[INPUT_LEN];
    int sub, minutes, reminders;
    time_t start, end;
    double seconds;
    if (!p) return;
    if (p->sessionCount == MAX_SESSIONS) { printWarning("Session history full."); return; }
    for (sub = 0; sub < SUBJECTS; sub++)
        printf("  [%d] %s\n", sub + 1, subjectNames[sub]);
    sub = readInt("Study subject (1-5): ", 1, SUBJECTS) - 1;
    printInfo("Session starts now. Press Enter to finish. This is an elapsed-time tracker.");
    printInfo("Reminders are summarized when you finish (not live popups).");
    start = time(NULL);
    printPrompt("Press Enter when finished: ");
    readLine(input, sizeof input);
    end = time(NULL);
    seconds = difftime(end, start);
    if (seconds < 0) seconds = 0;
    minutes = (int)(seconds / 60.0);
    p->sessions[p->sessionCount].started = start;
    p->sessions[p->sessionCount].durationMinutes = minutes;
    p->sessions[p->sessionCount].subject = sub;
    p->sessionCount++;
    p->totalMinutes += minutes;
    if (minutes > 0) {
        int key = (int)(start / (time_t)86400);
        int found = 0, j;
        for (j = 0; j < p->studyDayCount; j++)
            if (p->studyDays[j] == key) found = 1;
        if (!found) {
            if (p->studyDayCount == 60) {
                for (j = 1; j < 60; j++) p->studyDays[j-1] = p->studyDays[j];
                p->studyDayCount--;
            }
            p->studyDays[p->studyDayCount++] = key;
        }
    }
    printf("  Session: %.0f seconds (%d complete minute(s)).\n", seconds, minutes);
    reminders = minutes / p->reminderMinutes;
    if (reminders > 0)
        printf("  [WATER] %d reminder interval(s) elapsed. Consider a water break.\n", reminders);
    printSuccess("Session recorded for this student.");
}

static void hydrationCheckin(void)
{
    StudyProfile *p = currentProfile();
    if (!p) return;
    if (confirm("Confirm you drank water? (y/n): ")) {
        p->waterCheckins++;
        printSuccess("Hydration check-in recorded.");
    }
}

static void studyDashboard(void)
{
    StudyProfile *p = currentProfile();
    int done;
    if (!p) return;
    done = completedTasks(p);
    printHeading("MY STUDY DASHBOARD");
    printf("  Demo SAP ID           : %d\n", p->sapId);
    printf("  Linked student roll   : %d\n", p->studentRoll);
    printf("  Tasks completed       : %d / %d\n", done, p->taskCount);
    printf("  Sessions recorded     : %d\n", p->sessionCount);
    printf("  Study time            : %d h %d min\n", p->totalMinutes / 60, p->totalMinutes % 60);
    printf("  Water check-ins       : %d\n", p->waterCheckins);
    printf("  Reminder interval     : %d min\n", p->reminderMinutes);
    printSection("ACHIEVEMENTS");
    printf("  [%-8s] First Task     - complete one study task\n", done >= 1 ? "UNLOCKED" : "LOCKED");
    printf("  [%-8s] Task Master    - complete five study tasks\n", done >= 5 ? "UNLOCKED" : "LOCKED");
    printf("  [%-8s] First Focus    - record one study session\n", p->sessionCount >= 1 ? "UNLOCKED" : "LOCKED");
    printf("  [%-8s] Focus Hour     - accumulate 60 study minutes\n", p->totalMinutes >= 60 ? "UNLOCKED" : "LOCKED");
    printf("  [%-8s] Water Check    - record three hydration check-ins\n", p->waterCheckins >= 3 ? "UNLOCKED" : "LOCKED");
    printf("  [%-8s] Quiz Starter   - complete a practice quiz\n", p->quizAttempts >= 1 ? "UNLOCKED" : "LOCKED");
    printf("  [%-8s] Quiz Ace       - answer at least 10 questions correctly\n", p->quizCorrect >= 10 ? "UNLOCKED" : "LOCKED");
}

static void myAcademicResults(void)
{
    StudyProfile *p = currentProfile();
    int idx, e;
    if (!p) return;
    idx = findByRoll(p->studentRoll);
    if (idx < 0) { printError("Linked student record missing."); return; }
    printStudentProfile(idx);
    printSection("MY ASSESSMENTS");
    for (e = 0; e < EXAMS; e++)
        printf("  [%d] %-24s %s\n", e + 1, examNames[e],
               students[idx].exams[e].entered ? "Available" : "Not recorded");
    e = readInt("View report card (1-3, 0 back): ", 0, EXAMS) - 1;
    if (e < 0) return;
    if (!students[idx].exams[e].entered) { printWarning("No marks for this assessment."); return; }
    printReportCard(&students[idx], e);
}


/* ============ STUDY PREPARATION & PRODUCTIVITY ============ */
static void manageStudyTasks(void)
{
    StudyProfile *p = currentProfile();
    int n, i, choice;
    if (!p) return;
    if (!p->taskCount) { printInfo("No tasks yet. Add one first."); return; }
    listStudyTasks();
    printMenuOption(1, "Mark task complete / incomplete");
    printMenuOption(2, "Delete a task");
    printMenuOption(0, "Back");
    choice = readInt("Choice: ", 0, 2);
    if (!choice) return;
    n = readInt("Task number (0 to cancel): ", 0, p->taskCount);
    if (!n) return;
    if (choice == 1) {
        p->tasks[n-1].done = !p->tasks[n-1].done;
        printSuccess("Task status updated.");
    } else if (confirm("Delete this task? (y/n): ")) {
        for (i = n; i < p->taskCount; i++) p->tasks[i-1] = p->tasks[i];
        p->taskCount--;
        printSuccess("Task deleted.");
    }
}

typedef struct { int subject; const char *question; const char *a; const char *b; const char *c; int correct; } QuizQuestion;
static const QuizQuestion quizBank[] = {
    {0,"What is 12 x 8?","96","84","108",1},
    {0,"Derivative of x squared?","x","2x","x cubed",2},
    {0,"Square root of 144?","11","13","12",3},
    {1,"SI unit of force?","Joule","Newton","Watt",2},
    {1,"Speed = distance divided by?","Time","Mass","Force",1},
    {1,"SI unit of electrical resistance?","Volt","Ampere","Ohm",3},
    {2,"Chemical symbol of sodium?","So","Na","S",2},
    {2,"Water has which formula?","H2O","CO2","O2",1},
    {2,"Atomic number of carbon?","8","12","6",3},
    {3,"Which C loop runs at least once?","while","for","do while",3},
    {3,"Index of first C array element?","0","1","-1",1},
    {3,"Which format prints an integer?","%f","%d","%c",2},
    {4,"Plural of child?","Childs","Children","Childes",2},
    {4,"Opposite of scarce?","Rare","Abundant","Little",2},
    {4,"Past tense of write?","Wrote","Written","Writing",1}
};

static void practiceQuiz(void)
{
    StudyProfile *p = currentProfile();
    int subject, i, q = 0, score = 0, ans;
    if (!p) return;
    printHeading("SUBJECT PRACTICE QUIZ");
    for (i = 0; i < SUBJECTS; i++) printf("  [%d] %s\n", i+1, subjectNames[i]);
    subject = readInt("Subject (1-5): ", 1, SUBJECTS) - 1;
    for (i = 0; i < (int)(sizeof quizBank / sizeof quizBank[0]); i++) {
        const QuizQuestion *x = &quizBank[i];
        if (x->subject != subject) continue;
        printf("\n  Q%d. %s\n   [1] %s\n   [2] %s\n   [3] %s\n", ++q, x->question, x->a, x->b, x->c);
        ans = readInt("Answer (1-3): ", 1, 3);
        if (ans == x->correct) { score++; printSuccess("Correct!"); }
        else printf("  Correct option: %d\n", x->correct);
    }
    p->quizAttempts++;
    p->quizCorrect += score;
    p->quizQuestions += q;
    printf("\n  Quiz score: %d/%d (%.1f%%)\n", score, q, q ? 100.0*score/q : 0.0);
    printInfo("Demo questions only; these are not official UPES exam questions.");
}

static int readinessScore(const StudyProfile *p)
{
    int tasks = p->taskCount ? 40 * completedTasks(p) / p->taskCount : 0;
    int quizzes = p->quizQuestions ? 40 * p->quizCorrect / p->quizQuestions : 0;
    int focus = p->totalMinutes >= 120 ? 20 : p->totalMinutes / 6;
    return tasks + quizzes + focus;
}

static void examCountdown(void)
{
    StudyProfile *p = currentProfile();
    int days;
    if (!p) return;
    printSection("EXAM COUNTDOWN (MANUAL)");
    if (p->examDaysRemaining >= 0)
        printf("  Saved countdown: %d day(s) at last update.\n", p->examDaysRemaining);
    printInfo("No real calendar synchronization. Update days manually as the exam approaches.");
    days = readInt("Days until next exam (0-365): ", 0, 365);
    p->examDaysRemaining = days;
    printSuccess("Countdown updated.");
}

static void smartStudyPlan(void)
{
    StudyProfile *p = currentProfile();
    int idx, e, i, best = -1;
    if (!p) return;
    idx = findByRoll(p->studentRoll);
    if (idx < 0) return;
    printHeading("MY SMART STUDY PRIORITIES");
    for (e = EXAMS-1; e >= 0; e--) if (students[idx].exams[e].entered) { best = e; break; }
    if (best < 0) { printWarning("Enter assessment marks through admin to enable personalized priorities."); return; }
    printf("  Based on %s (latest recorded assessment).\n", examNames[best]);
    for (i = 0; i < SUBJECTS; i++) {
        int mark = students[idx].exams[best].marks[i];
        const char *level = mark < PASS_MARK ? "URGENT: reach passing level" :
                            mark < 60 ? "HIGH: revise fundamentals" :
                            mark < 80 ? "MEDIUM: practise questions" : "MAINTAIN: periodic revision";
        printf("  %-19s %3d/100  %s\n", subjectNames[i], mark, level);
    }
    printInfo("Choose the weakest subjects for tasks and focus sessions; no automatic grade predictions.");
}

static void studyProgress(void)
{
    StudyProfile *p = currentProfile();
    int i, sub, mins, streak = 0, day, found;
    time_t now = time(NULL);
    if (!p) return;
    printHeading("MY PREPARATION & PROGRESS");
    printf("  Preparation indicator : %d/100\n", readinessScore(p));
    printf("  Task completion       : %d/%d\n", completedTasks(p), p->taskCount);
    printf("  Practice quiz accuracy: ");
    if (p->quizQuestions) printf("%.1f%% (%d attempts)\n", 100.0*p->quizCorrect/p->quizQuestions, p->quizAttempts);
    else printf("No quiz attempted\n");
    printf("  Focus minutes         : %d\n", p->totalMinutes);
    if (p->examDaysRemaining >= 0) printf("  Exam countdown        : %d day(s), manually set\n", p->examDaysRemaining);
    day = (int)(now / (time_t)86400);
    for (;;) {
        found = 0;
        for (i = 0; i < p->studyDayCount; i++) if (p->studyDays[i] == day) found = 1;
        if (!found) {
            if (streak == 0) { day--; for (i = 0; i < p->studyDayCount; i++) if (p->studyDays[i] == day) found = 1; }
            if (!found) break;
        }
        streak++; day--;
    }
    printf("  Recorded study streak : %d day(s) (UTC calendar days)\n", streak);
    printSection("SUBJECT-WISE FOCUS MINUTES");
    for (sub = 0; sub < SUBJECTS; sub++) {
        mins = 0;
        for (i = 0; i < p->sessionCount; i++)
            if (p->sessions[i].subject == sub) mins += p->sessions[i].durationMinutes;
        printf("  %-19s %d min\n", subjectNames[sub], mins);
    }
    printInfo("Readiness is a simple activity indicator: tasks 40, quizzes 40, focus 20.");
    printInfo("This is not a predicted examination grade.");
}

static void taskMenu(void)
{
    int c;
    do {
        printHeading("MY STUDY TASKS");
        printMenuOption(1, "Add task");
        printMenuOption(2, "View tasks");
        printMenuOption(3, "Complete task");
        printMenuOption(4, "Edit completion / delete task");
        printMenuOption(0, "Back");
        c = readInt("Choose (0-4): ", 0, 4);
        switch (c) {
            case 1: addStudyTask(); break;
            case 2: listStudyTasks(); break;
            case 3: finishStudyTask(); break;
            case 4: manageStudyTasks(); break;
        }
    } while (c);
}

static void focusMenu(void)
{
    int c;
    do {
        printHeading("FOCUS & WELLNESS");
        printMenuOption(1, "Record timed focus session");
        printMenuOption(2, "Set hydration reminder interval");
        printMenuOption(3, "Record water check-in");
        printMenuOption(0, "Back");
        c = readInt("Choose (0-3): ", 0, 3);
        switch (c) {
            case 1: startStudySession(); break;
            case 2: configureHydration(); break;
            case 3: hydrationCheckin(); break;
        }
    } while (c);
}

static void academicMenuStudent(void)
{
    int c, idx;
    do {
        printHeading("MY ACADEMICS");
        printMenuOption(1, "My results and report cards");
        printMenuOption(2, "My performance trends");
        printMenuOption(3, "My study priorities");
        printMenuOption(0, "Back");
        c = readInt("Choose (0-3): ", 0, 3);
        switch (c) {
            case 1: myAcademicResults(); break;
            case 2:
                idx = findByRoll(profiles[activeProfile].studentRoll);
                if (idx >= 0) {
                    int f, l;
                    printHistoryTable(&students[idx], 1);
                    printf("  Trend: %s\n", trendNames[studentTrend(&students[idx], &f, &l)]);
                }
                break;
            case 3: smartStudyPlan(); break;
        }
    } while (c);
}

static void preparationMenu(void)
{
    int c;
    do {
        printHeading("PREPARATION & ACHIEVEMENTS");
        printMenuOption(1, "Practice quiz");
        printMenuOption(2, "Preparation progress & streak");
        printMenuOption(3, "Set exam countdown");
        printMenuOption(4, "Dashboard & badges");
        printMenuOption(0, "Back");
        c = readInt("Choose (0-4): ", 0, 4);
        switch (c) {
            case 1: practiceQuiz(); break;
            case 2: studyProgress(); break;
            case 3: examCountdown(); break;
            case 4: studyDashboard(); break;
        }
    } while (c);
}
static void studentPortal(void)
{
    int c;
    if (activeProfile < 0) return;
    do {
        printHeading("MY SCHOLARIQ - STUDENT PORTAL");
        printMenuOption(1, "My dashboard");
        printMenuOption(2, "My academics");
        printMenuOption(3, "My study tasks");
        printMenuOption(4, "Focus & wellness");
        printMenuOption(5, "Preparation & achievements");
        printMenuOption(0, "Logout");
        c = readInt("Choose (0-5): ", 0, 5);
        switch (c) {
            case 1: studyDashboard(); break;
            case 2: academicMenuStudent(); break;
            case 3: taskMenu(); break;
            case 4: focusMenu(); break;
            case 5: preparationMenu(); break;
        }
    } while (c);
    activeProfile = -1;
    printInfo("Student logged out.");
}

static void adminPortal(void)
{
    int choice;
    do {
        showMainMenu();
        printMenuOption(10, "Create student SAP account");
        printMenuOption(0, "Logout to welcome screen");
        choice = readInt("Choose admin module (0-10): ", 0, 10);
        switch (choice) {
            case 1: studentMenu(); break;
            case 2: examMenu(); break;
            case 3: resultMenu(); break;
            case 4: classMenu(); break;
            case 5: intelligenceMenu(); break;
            case 6: plannerMenu(); break;
            case 7: showWhatIf(); break;
            case 8: reportsMenu(); break;
            case 9: loadDemoData(); break;
            case 10: createStudyProfile(); break;
        }
    } while (choice != 0);
    printInfo("Admin logged out.");
}

int main(int argc, char *argv[])
{
    int i, choice, colorChosen = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-color") == 0) { useColor = 0; colorChosen = 1; }
        else if (strcmp(argv[i], "--color") == 0) { useColor = 1; colorChosen = 1; }
    }
    if (!colorChosen) {
        printf("\n  ScholarIQ - optional terminal colours.\n");
        useColor = confirm("Enable ANSI colours? (y/n): ");
    }
    do {
        printHeading("SCHOLARIQ - UPES STUDY COMPANION (DEMO)");
        printMenuOption(1, "Admin login");
        printMenuOption(2, "Student login (SAP ID + ScholarIQ PIN)");
        printMenuOption(0, "Exit");
        choice = readInt("Choose (0-2): ", 0, 2);
        if (choice == 1 && adminLogin()) adminPortal();
        if (choice == 2 && studentLogin()) studentPortal();
    } while (choice != 0);
    printHeading("THANK YOU FOR USING SCHOLARIQ");
    printCentered("Demo records were held in RAM and are now discarded.");
    return 0;
}
