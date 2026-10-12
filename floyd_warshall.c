/*
 * ============================================================================
 *  FareGrid - Intercity All-Pairs Travel Cost Analyzer
 *  AOA Mini Project - Sr. No. 17 (Floyd-Warshall Algorithm)
 *  Module 4: Dynamic Programming Approach
 *
 *  Implementation : C (console, menu driven)
 *  GUI / Database : Not used
 *
 *  Features
 *    - Manual entry, sample datasets and random network generation
 *    - Network editing: add / remove / rename cities, set / remove roads
 *    - Floyd-Warshall with four trace modes, including a step-by-step mode
 *      that explains every improvement and marks changed cells
 *    - Route planner: cheapest cost, route, legs, savings vs direct road
 *    - Unreachable pairs with reasons, negative-cycle report
 *    - Analysis and insights: counts, improvements per stage, complexity
 *
 *  DP recurrence:
 *      D0[i][j] = cost[i][j]            (0 on diagonal, INF if no edge)
 *      Dk[i][j] = min( Dk-1[i][j], Dk-1[i][k] + Dk-1[k][j] )
 *
 *  Time complexity  : Theta(n^3)
 *  Space complexity : Theta(n^2)
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX        20
#define NAME_LEN   30
#define LINE_LEN   512
#define INF        1000000000LL      /* "no edge" / unreachable          */
#define NEG_LIMIT  (-INF)            /* clamp for values in neg. cycles  */
#define COST_LIMIT 1000000           /* allowed |edge cost|              */
#define BAR_WIDTH  30                /* width of insight bar charts      */

/* ---------------------------- global data ------------------------------- */
char      city[MAX][NAME_LEN];       /* city names                        */
int       n = 0;                     /* number of cities                  */
long long cost[MAX][MAX];            /* original input cost matrix        */
long long dist[MAX][MAX];            /* DP distance matrix                */
int       nxt[MAX][MAX];             /* next hop on shortest route, -1=none */
int       changed[MAX][MAX];         /* cells improved in the current stage */
int       stageUpd[MAX];             /* improvements found in each stage  */

int dataLoaded  = 0;
int processed   = 0;
int hasNegCycle = 0;
long long updates    = 0;            /* successful relaxations            */
long long relaxTests = 0;            /* i->k->j checks performed          */

const char *CITY_POOL[] = {
    "Mumbai", "Pune", "Nashik", "Nagpur", "Goa", "Surat", "Indore", "Bhopal",
    "Jaipur", "Delhi", "Agra", "Hyderabad", "Bengaluru", "Chennai", "Kochi",
    "Kolkata", "Lucknow", "Ahmedabad", "Udaipur", "Mysuru", "Aurangabad", "Kolhapur"
};
#define POOL_SIZE ((int)(sizeof CITY_POOL / sizeof CITY_POOL[0]))

/* ------------------------- input helper functions ----------------------- */

/* Reads one line from stdin. Exits cleanly on end of input. */
void readLine(char *buf, int size)
{
    if (fgets(buf, size, stdin) == NULL) {
        printf("\n[End of input] Exiting.\n");
        exit(0);
    }
    buf[strcspn(buf, "\r\n")] = '\0';
}

/* Reads an integer in [lo, hi], re-prompting on invalid input. */
int readInt(const char *prompt, int lo, int hi)
{
    char buf[LINE_LEN], *end;
    long v;
    while (1) {
        printf("%s", prompt);
        readLine(buf, sizeof buf);
        v = strtol(buf, &end, 10);
        while (isspace((unsigned char)*end)) end++;
        if (end != buf && *end == '\0' && v >= lo && v <= hi)
            return (int)v;
        printf("  Invalid input. Enter an integer between %d and %d.\n", lo, hi);
    }
}

int equalsIgnoreCase(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* Returns 1 if token means "no edge" (INF / x / -). */
int isNoEdgeToken(const char *t)
{
    return equalsIgnoreCase(t, "INF") || equalsIgnoreCase(t, "x") ||
           strcmp(t, "-") == 0;
}

/* Parses a cost token. Returns 1 on success and stores value in *out. */
int parseCost(const char *t, long long *out)
{
    char *end;
    long v;
    if (isNoEdgeToken(t)) { *out = INF; return 1; }
    v = strtol(t, &end, 10);
    if (end == t || *end != '\0') return 0;
    if (v > COST_LIMIT || v < -COST_LIMIT) return 0;
    *out = v;
    return 1;
}

/* Reads a single cost value (integer or INF), re-prompting on invalid input. */
long long readCost(const char *prompt)
{
    char buf[LINE_LEN];
    long long v;
    while (1) {
        printf("%s", prompt);
        readLine(buf, sizeof buf);
        if (parseCost(buf, &v)) return v;
        printf("  Invalid cost. Enter an integer within +/-%d, or INF for no road.\n",
               COST_LIMIT);
    }
}

/* Lets the user pick a city by number (1..n) or by name. Returns index. */
int readCity(const char *prompt)
{
    char buf[LINE_LEN], *end;
    long v;
    int i;
    while (1) {
        printf("%s", prompt);
        readLine(buf, sizeof buf);
        v = strtol(buf, &end, 10);
        if (end != buf && *end == '\0' && v >= 1 && v <= n)
            return (int)v - 1;
        for (i = 0; i < n; i++)
            if (equalsIgnoreCase(buf, city[i])) return i;
        printf("  Unknown city. Enter a number 1-%d or a city name.\n", n);
    }
}

/*
 * Reads a non-empty city name that is unique among the current cities.
 * skip = index allowed to keep the same name (for renaming), or -1.
 */
void readCityName(const char *prompt, char *out, int skip)
{
    char buf[LINE_LEN];
    int k, dup;
    while (1) {
        printf("%s", prompt);
        readLine(buf, sizeof buf);
        if (buf[0] == '\0') { printf("  Name cannot be empty.\n"); continue; }
        buf[NAME_LEN - 1] = '\0';
        dup = 0;
        for (k = 0; k < n; k++)
            if (k != skip && equalsIgnoreCase(buf, city[k])) dup = 1;
        if (dup) { printf("  City already exists. Use a unique name.\n"); continue; }
        strcpy(out, buf);
        return;
    }
}

/* Formats a cost for display: INF, -INF or the number (buf >= 32 chars). */
const char *fmtCost(long long v, char *buf)
{
    if (v >= INF)            strcpy(buf, "INF");
    else if (v <= NEG_LIMIT) strcpy(buf, "-INF");
    else                     snprintf(buf, 32, "%lld", v);
    return buf;
}

/* Like fmtCost, but wraps negative numbers in brackets for use in a sum. */
const char *fmtTerm(long long v, char *buf)
{
    if (v < 0 && v > NEG_LIMIT) snprintf(buf, 32, "(%lld)", v);
    else                        fmtCost(v, buf);
    return buf;
}

/* ------------------------------ display --------------------------------- */

void printLine(int width)
{
    int i;
    for (i = 0; i < width; i++) putchar('-');
    putchar('\n');
}

void printCities(void)
{
    int i;
    printf("\nCities (%d):\n", n);
    for (i = 0; i < n; i++)
        printf("  %2d. %s\n", i + 1, city[i]);
}

/*
 * Prints an n x n cost/distance matrix with INF for missing values.
 * mark (optional) flags cells to show with a trailing '*'.
 */
void printMatrix(long long m[MAX][MAX], const char *title, int mark[MAX][MAX])
{
    int i, j;
    char buf[32];
    printf("\n%s\n", title);
    printLine(14 + 10 * n);
    printf("%-14s", "From \\ To");
    for (j = 0; j < n; j++) printf("%10.9s", city[j]);
    printf("\n");
    printLine(14 + 10 * n);
    for (i = 0; i < n; i++) {
        printf("%-14.13s", city[i]);
        for (j = 0; j < n; j++)
            printf("%9s%c", fmtCost(m[i][j], buf), (mark && mark[i][j]) ? '*' : ' ');
        printf("\n");
    }
    printLine(14 + 10 * n);
    if (mark) printf("(* = improved in this stage)\n");
}

/* Prints next-hop matrix: entry = city number of the first hop from i to j. */
void printNextMatrix(void)
{
    int i, j;
    printf("\nNext-hop (route) matrix: entry = number of the first city to\n"
           "visit after the source on the cheapest route ('-' = no route)\n");
    printLine(14 + 6 * n);
