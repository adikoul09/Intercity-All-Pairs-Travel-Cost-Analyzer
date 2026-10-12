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
printf("%-14s", "From \\ To");
    for (j = 0; j < n; j++) printf("%6d", j + 1);
    printf("\n");
    printLine(14 + 6 * n);
    for (i = 0; i < n; i++) {
        printf("%2d.%-11.10s", i + 1, city[i]);
        for (j = 0; j < n; j++) {
            if (nxt[i][j] == -1) printf("%6s", "-");
            else                 printf("%6d", nxt[i][j] + 1);
        }
        printf("\n");
    }
    printLine(14 + 6 * n);
}

/* Counts direct roads (and negative ones) in the input matrix. */
int countRoads(int *negRoads)
{
    int i, j, edges = 0, neg = 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            if (i != j && cost[i][j] < INF) {
                edges++;
                if (cost[i][j] < 0) neg++;
            }
    if (negRoads) *negRoads = neg;
    return edges;
}

void showNetwork(void)
{
    int neg, edges = countRoads(&neg);
    printCities();
    printMatrix(cost, "Input cost matrix D(0):", NULL);
    printf("Direct roads: %d (negative: %d). Roads are one-way: row city -> column city.\n",
           edges, neg);
}

/* ---------------------------- data entry -------------------------------- */

void resetResults(void)
{
    if (processed)
        printf("  (Previous results cleared. Run the algorithm again.)\n");
    processed   = 0;
    hasNegCycle = 0;
    updates     = 0;
    relaxTests  = 0;
}

void readGraph(void)
{
    char buf[LINE_LEN], *tok, prompt[64];
    long long row[MAX];
    int i, j, count, ok;

    n = 0;
    count = readInt("\nEnter number of cities (1-20): ", 1, MAX);
    for (i = 0; i < count; i++) {
        snprintf(prompt, sizeof prompt, "  Name of city %d: ", i + 1);
        readCityName(prompt, city[i], -1);
        n = i + 1;                    /* so duplicate check sees this city */
    }

    printf("\nEnter the cost matrix row by row (%d values per row, space separated).\n", n);
    printf("Use INF, x or - for 'no direct road'. Negative costs (adjustments) are allowed.\n");
    printf("Diagonal (city to itself) is always taken as 0.\n");

    for (i = 0; i < n; i++) {
        while (1) {
            printf("  From %-12s: ", city[i]);
            readLine(buf, sizeof buf);
            count = 0;
            ok = 1;
            for (tok = strtok(buf, " \t,"); tok != NULL; tok = strtok(NULL, " \t,")) {
                if (count >= n || !parseCost(tok, &row[count])) { ok = 0; break; }
                count++;
            }
            if (!ok || count != n) {
                printf("  Invalid row. Enter exactly %d values (integers within +/-%d or INF).\n",
                       n, COST_LIMIT);
                continue;
            }
            break;
        }
        for (j = 0; j < n; j++) cost[i][j] = row[j];
        if (cost[i][i] != 0)
            printf("  Note: diagonal value for %s set to 0.\n", city[i]);
        cost[i][i] = 0;
    }

    dataLoaded = 1;
    resetResults();
    printf("\nGraph stored successfully.\n");
}

/* Fills cost matrix with INF and 0 diagonal. */
void clearCost(void)
{
    int i, j;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            cost[i][j] = (i == j) ? 0 : INF;
}

void setNames(const char *names[], int count)
{
    int i;
    n = count;
    for (i = 0; i < n; i++) {
        strncpy(city[i], names[i], NAME_LEN - 1);
        city[i][NAME_LEN - 1] = '\0';
    }
    clearCost();
}

void loadSample(void)
{
    int c;
    printf("\nSample datasets:\n");
    printf("  1. Maharashtra/Goa road network (positive costs)\n");
    printf("  2. Network with negative adjustments, no negative cycle (CLRS example)\n");
    printf("  3. Network containing a negative cycle (and unreachable cities)\n");
    c = readInt("Choose sample (1-3): ", 1, 3);

    if (c == 1) {
        const char *names[] = {"Mumbai", "Pune", "Nashik", "Nagpur", "Goa"};
        setNames(names, 5);
        cost[0][1] = 150; cost[0][2] = 170; cost[0][4] = 590;  /* Mumbai */
        cost[1][0] = 150; cost[1][2] = 210; cost[1][3] = 700;  /* Pune   */
        cost[1][4] = 450;
        cost[2][0] = 170; cost[2][3] = 450;                    /* Nashik */
        cost[3][0] = 800;                                      /* Nagpur */
        cost[4][0] = 590; cost[4][1] = 450;                    /* Goa    */
    } else if (c == 2) {
        const char *names[] = {"Surat", "Vadodara", "Ahmedabad", "Rajkot", "Bhuj"};
        setNames(names, 5);
        cost[0][1] = 3;  cost[0][2] = 8;  cost[0][4] = -4;
        cost[1][3] = 1;  cost[1][4] = 7;
        cost[2][1] = 4;
        cost[3][0] = 2;  cost[3][2] = -5;
        cost[4][3] = 6;
    } else {
        const char *names[] = {"Delhi", "Agra", "Jaipur", "Udaipur", "Shimla"};
        setNames(names, 5);
        cost[0][1] = 4;                    /* Delhi  -> Agra    */
        cost[1][2] = -6;                   /* Agra   -> Jaipur  */
        cost[2][0] = 1;                    /* Jaipur -> Delhi   (cycle sum = -1) */
        cost[2][3] = 2;                    /* Jaipur -> Udaipur */
        cost[4][0] = 3;                    /* Shimla -> Delhi   */
    }

    dataLoaded = 1;
    resetResults();
    printf("\nSample %d loaded.\n", c);
    showNetwork();
}

/* Builds a random network; a ring of roads keeps every city reachable. */
void randomNetwork(void)
{
    int idx[POOL_SIZE], i, j, t, count, withNeg;
    const char *names[MAX];

    count   = readInt("\nNumber of cities (2-20): ", 2, MAX);
    withNeg = readInt("Include some negative costs (discounts)? (1 = Yes, 0 = No): ", 0, 1);

    for (i = 0; i < POOL_SIZE; i++) idx[i] = i;
    for (i = POOL_SIZE - 1; i > 0; i--) {          /* Fisher-Yates shuffle */
        j = rand() % (i + 1);
        t = idx[i]; idx[i] = idx[j]; idx[j] = t;
    }
    for (i = 0; i < count; i++) names[i] = CITY_POOL[idx[i]];
    setNames(names, count);

    for (i = 0; i < n; i++)
        cost[i][(i + 1) % n] = (4 + rand() % 86) * 10;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            if (i != j && cost[i][j] >= INF && rand() % 100 < 22)
                cost[i][j] = (4 + rand() % 86) * 10;
    if (withNeg)
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++)
                if (i != j && cost[i][j] < INF && rand() % 100 < 12)
                    cost[i][j] = -(1 + rand() % 9) * 10;

    dataLoaded = 1;
    resetResults();
    printf("\nRandom network with %d cities generated.\n", n);
    showNetwork();
}

/* ---------------------------- network editor ---------------------------- */

void networkChanged(void)
{
    dataLoaded = (n > 0);
    resetResults();
}

void addCity(void)
{
    int j;
    if (n >= MAX) { printf("  At most %d cities are supported.\n", MAX); return; }
    readCityName("  New city name: ", city[n], -1);
    for (j = 0; j <= n; j++) { cost[n][j] = INF; cost[j][n] = INF; }
    cost[n][n] = 0;
    n++;
    printf("  Added %s as city %d (no roads yet). Use 'Set / update a road' to connect it.\n",
           city[n - 1], n);
    networkChanged();
}

void removeCity(void)
{
    int r, i, j;
    char name[NAME_LEN];
    printCities();
    r = readCity("  City to remove (number or name): ");
    strcpy(name, city[r]);
    for (i = r; i < n - 1; i++) {
        strcpy(city[i], city[i + 1]);
        for (j = 0; j < n; j++) cost[i][j] = cost[i + 1][j];
    }
    for (i = 0; i < n - 1; i++)
        for (j = r; j < n - 1; j++) cost[i][j] = cost[i][j + 1];
    n--;
    printf("  Removed %s and all its roads.\n", name);
    networkChanged();
}

void renameCity(void)
{
    int r;
    char old[NAME_LEN];
    printCities();
    r = readCity("  City to rename (number or name): ");
    strcpy(old, city[r]);
    readCityName("  New name: ", city[r], r);
readCityName("  New name: ", city[r], r);
    printf("  Renamed %s to %s.\n", old, city[r]);
    if (processed) printf("  (Results are unchanged; only the name was updated.)\n");
}

void setRoad(int removing)
{
    int a, b;
    long long v;
    char o[32], w[32];
    printCities();
    a = readCity("  From city (number or name): ");
    b = readCity("  To city   (number or name): ");
    if (a == b) { printf("  A city's cost to itself is always 0.\n"); return; }
    v = removing ? INF : readCost("  Road cost (integer, or INF for no road): ");
    printf("  %s -> %s : %s -> %s\n", city[a], city[b], fmtCost(cost[a][b], o), fmtCost(v, w));
    cost[a][b] = v;
    networkChanged();
}

void editNetwork(void)
{
    int c;
    while (1) {
        printf("\n---------------- Edit network (%d cities) ----------------\n", n);
        printf("  1. Add a city\n");
        printf("  2. Remove a city\n");
        printf("  3. Rename a city\n");
        printf("  4. Set / update a road cost\n");
        printf("  5. Remove a road\n");
        printf("  6. Show current network\n");
        printf("  0. Back to main menu\n");
        c = readInt("Choose: ", 0, 6);
        if (c == 0) return;
        if (c != 1 && n == 0) { printf("  The network is empty. Add a city first.\n"); continue; }
        switch (c) {
        case 1: addCity();      break;
        case 2: removeCity();   break;
        case 3: renameCity();   break;
        case 4: setRoad(0);     break;
        case 5: setRoad(1);     break;
        case 6: showNetwork();  break;
        }
    }
}

/* ------------------------- Floyd-Warshall core -------------------------- */

void printStage(int k)
{
    char title[128];
    if (k < 0) {
        snprintf(title, sizeof title, "D(0): initial matrix (no intermediate city)");
        printMatrix(dist, title, NULL);
    } else {
        snprintf(title, sizeof title, "D(%d): after allowing %s as intermediate city (%d improved)",
                 k + 1, city[k], stageUpd[k]);
        printMatrix(dist, title, changed);
    }
}

/*
 * traceMode: 0 = no trace, 1 = print every stage, 2 = print selected stages,
 *            3 = step by step (explain every improvement, pause per stage)
 * showStage[k] is used only when traceMode == 2.
 */
void floydWarshall(int traceMode, const int showStage[])
{
    int i, j, k, explain, skip = 0;
    long long through;
    char a[32], b[32], c[32], d[32], buf[LINE_LEN];

    /* Step 1 & 2: initialise dist and next matrices from the cost matrix */
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            dist[i][j] = cost[i][j];
            nxt[i][j]  = (cost[i][j] < INF) ? j : -1;
        }
        dist[i][i] = 0;
        nxt[i][i]  = i;
    }
    updates = relaxTests = 0;

    if (traceMode != 0) printStage(-1);

    /* Step 3: consider each city k as an allowed intermediate point */
    for (k = 0; k < n; k++) {
        memset(changed, 0, sizeof changed);
        stageUpd[k] = 0;
        explain = (traceMode == 3 && !skip);
        if (explain) {
            printf("\n=========== Stage %d of %d : via %s ===========\n", k + 1, n, city[k]);
            printf("Rule: D[i][j] = min( D[i][j] , D[i][%s] + D[%s][j] )\n", city[k], city[k]);
        }

        for (i = 0; i < n; i++) {
            if (dist[i][k] >= INF) continue;          /* i cannot reach k */
            for (j = 0; j < n; j++) {
                if (dist[k][j] >= INF) continue;      /* k cannot reach j */
                relaxTests++;
                through = dist[i][k] + dist[k][j];
                if (through < NEG_LIMIT) through = NEG_LIMIT;
                if (through < dist[i][j]) {
                    stageUpd[k]++;
                    if (explain)
                        printf("  %3d. %s -> %s : min( %s , %s + %s ) = %s\n",
                               stageUpd[k], city[i], city[j], fmtCost(dist[i][j], a),
                               fmtTerm(dist[i][k], b), fmtTerm(dist[k][j], c),
                               fmtCost(through, d));
                    dist[i][j] = through;
                    nxt[i][j]  = nxt[i][k];
                    changed[i][j] = 1;
                    updates++;
                }
            }
        }

        /* Step 4: display matrices after chosen stages */
        if (explain) {
            if (stageUpd[k] == 0) printf("  No route improves via %s.\n", city[k]);
            printStage(k);
            if (k < n - 1) {
                printf("Press Enter for the next stage, or type s + Enter to skip to the end: ");
                readLine(buf, sizeof buf);
                if (buf[0] == 's' || buf[0] == 'S') skip = 1;
            }
        } else if (traceMode == 1 || (traceMode == 2 && showStage[k])) {
            printStage(k);
        }
    }

    /* Step 6: negative diagonal entries indicate a negative cycle */
    hasNegCycle = 0;
    for (i = 0; i < n; i++)
        if (dist[i][i] < 0) hasNegCycle = 1;

    processed = 1;
}

/* A pair's cost is undefined (-infinity) if its path can pass a negative cycle. */
int isPairAffected(int i, int j)
{
    int k;
    for (k = 0; k < n; k++)
        if (dist[k][k] < 0 && dist[i][k] < INF && dist[k][j] < INF)
            return 1;
    return 0;
}

/* Final matrix, with pairs affected by a negative cycle shown as -INF. */
void printFinalMatrix(void)
{
    long long shown[MAX][MAX];
    int i, j;
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            shown[i][j] = isPairAffected(i, j) ? NEG_LIMIT : dist[i][j];
    printMatrix(shown, "Final all-pairs cheapest cost matrix D(n):", NULL);
    if (hasNegCycle)
        printf("WARNING: negative cycle present. -INF pairs have no cheapest cost (option 11).\n");
}

/* Explains why s cannot reach d (roads are one-way). */
void explainUnreachable(int s, int d)
{
    int i, outS = 0, inD = 0;
    for (i = 0; i < n; i++) {
        if (i != s && cost[s][i] < INF) outS++;
        if (i != d && cost[i][d] < INF) inD++;
    }
    printf("  Reason: roads are one-way (row city -> column city), and ");
    if (outS == 0)     printf("%s has no outgoing road.\n", city[s]);
    else if (inD == 0) printf("%s has no incoming road.\n", city[d]);
    else               printf("none of the cities reachable from %s has a road into %s.\n",
                              city[s], city[d]);
}

/* Reconstructs and prints the cheapest route from s to d using nxt[][]. */
void printRoute(int s, int d)
{
    int path[MAX + 1], len = 0, cur = s, h;
    long long total = 0, direct = cost[s][d], best = dist[s][d];
    char buf[32];

    path[len++] = cur;
    while (cur != d) {
        cur = nxt[cur][d];
        if (cur == -1 || len > n) {
            printf("  Route could not be reconstructed.\n");
            return;
        }
        path[len++] = cur;
    }

    printf("  Route : ");
    for (h = 0; h < len; h++)
        printf("%s%s", city[path[h]], (h < len - 1) ? " -> " : "\n");

    printf("  Legs  :\n");
    for (h = 0; h < len - 1; h++) {
        total += cost[path[h]][path[h + 1]];
printf("     %-14s -> %-14s : %8lld   (running total %lld)\n",
               city[path[h]], city[path[h + 1]], cost[path[h]][path[h + 1]], total);
    }
    printf("  Roads used         : %d (stops in between: %d)\n", len - 1, len - 2);
    printf("  Direct road        : %s\n", direct < INF ? fmtCost(direct, buf) : "none");
    if (direct < INF && direct > best)
        printf("  Saved vs direct    : %lld (%.1f%% cheaper)\n", direct - best,
               direct > 0 ? 100.0 * (double)(direct - best) / (double)direct : 0.0);
    else if (direct < INF)
        printf("  Saved vs direct    : none (the direct road is already cheapest)\n");
}

/* --------------------------- menu operations ---------------------------- */

void runAlgorithm(void)
{
    int mode, showStage[MAX] = {0}, count, ok;
    char buf[LINE_LEN], *tok, *end;
    long v;

    printf("\nTrace options:\n");
    printf("  1. No intermediate matrices\n");
    printf("  2. Show matrix after every stage k\n");
    printf("  3. Show matrix after selected stages only\n");
    printf("  4. Step by step: explain every improvement, pause after each stage\n");
    mode = readInt("Choose (1-4): ", 1, 4) - 1;

    if (mode == 2) {
        printCities();
        while (1) {
            printf("Enter stage numbers to display (1-%d, space separated): ", n);
            readLine(buf, sizeof buf);
            memset(showStage, 0, sizeof showStage);
            count = 0;
            ok = 1;
            for (tok = strtok(buf, " \t,"); tok != NULL; tok = strtok(NULL, " \t,")) {
                v = strtol(tok, &end, 10);
                if (*end != '\0' || v < 1 || v > n) { ok = 0; break; }
                showStage[v - 1] = 1;
                count++;
            }
            if (ok && count > 0) break;
            printf("  Invalid list.\n");
        }
    }

    floydWarshall(mode, showStage);

    printf("\nFloyd-Warshall completed: %d stages, %lld relaxation checks, %lld updates.\n",
           n, relaxTests, updates);
    printFinalMatrix();
}

void showRoute(int s, int d)
{
    printf("\n  %s -> %s\n", city[s], city[d]);
    if (isPairAffected(s, d)) {
        printf("  Cost  : -INFINITY (route can loop through a negative cycle).\n");
        printf("  No finite cheapest route exists.\n");
    } else if (dist[s][d] >= INF) {
        printf("  Cost  : UNREACHABLE (no route from %s to %s).\n", city[s], city[d]);
        explainUnreachable(s, d);
    } else if (s == d) {
        printf("  Cost  : 0 (source and destination are the same city).\n");
    } else {
        printf("  Cheapest travel cost: %lld\n", dist[s][d]);
        printRoute(s, d);
    }
}

/* Cheapest cost from one city to every other city. */
void routesFromCity(int s)
{
    int j;
    char buf[32];
    printf("\n  Cheapest costs from %s:\n", city[s]);
    for (j = 0; j < n; j++) {
        if (j == s) continue;
        if (isPairAffected(s, j))     printf("    -> %-14s : -INF (negative cycle)\n", city[j]);
        else if (dist[s][j] >= INF)   printf("    -> %-14s : unreachable\n", city[j]);
        else                          printf("    -> %-14s : %s\n", city[j], fmtCost(dist[s][j], buf));
    }
}

void routePlanner(void)
{
    int c, s, d;
    while (1) {
        printf("\n---------------- Route planner ----------------\n");
        printf("  1. Cheapest cost and route between two cities\n");
        printf("  2. Cheapest costs from one city to all others\n");
        printf("  0. Back to main menu\n");
        c = readInt("Choose: ", 0, 2);
        if (c == 0) return;
        printCities();
        if (c == 1) {
            s = readCity("Source city (number or name)      : ");
            d = readCity("Destination city (number or name) : ");
            showRoute(s, d);
        } else {
            routesFromCity(readCity("Source city (number or name): "));
        }
    }
}

void listUnreachable(void)
{
    int i, j, count = 0, noOut, noIn;
    printf("\nUnreachable ordered pairs (source -> destination):\n");
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            if (i != j && dist[i][j] >= INF) {
                printf("  %-14s -> %s\n", city[i], city[j]);
                count++;
            }
    if (count == 0) {
        printf("  None. Every city can reach every other city.\n");
        return;
    }
    printf("Total unreachable pairs: %d of %d\n", count, n * (n - 1));

    printf("\nCities with no outgoing road :");
    noOut = 0;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n && (j == i || cost[i][j] >= INF); j++) ;
        if (j == n) { printf(" %s", city[i]); noOut++; }
    }
    printf("%s\n", noOut ? "" : " none");
    printf("Cities with no incoming road :");
    noIn = 0;
    for (j = 0; j < n; j++) {
        for (i = 0; i < n && (i == j || cost[i][j] >= INF); i++) ;
        if (i == n) { printf(" %s", city[j]); noIn++; }
    }
    printf("%s\n", noIn ? "" : " none");
}

void negativeCycleReport(void)
{
    int i, j, count = 0;
    printf("\nDiagonal check (dist[i][i] after completion):\n");
    for (i = 0; i < n; i++) {
        printf("  %-14s : ", city[i]);
        if (dist[i][i] < 0) printf("%lld  <-- lies on a negative cycle\n", dist[i][i]);
        else                printf("0\n");
    }
    if (!hasNegCycle) {
        printf("\nNo negative cycle detected. All finite distances are valid.\n");
        return;
    }
    printf("\n*** NEGATIVE CYCLE DETECTED ***\n");
    printf("Pairs whose cheapest cost is -INFINITY (undefined):\n");
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            if (isPairAffected(i, j)) {
                printf("  %-14s -> %s\n", city[i], city[j]);
                count++;
            }
    printf("Total affected pairs: %d\n", count);
}

void analysisSummary(void)
{
    int i, j, k, edges, negEdges, unreach = 0, cycleCities = 0, maxUpd = 1, w;
    edges = countRoads(&negEdges);
    for (i = 0; i < n; i++) {
        if (dist[i][i] < 0) cycleCities++;
        for (j = 0; j < n; j++)
            if (i != j && dist[i][j] >= INF) unreach++;
    }
    for (k = 0; k < n; k++)
        if (stageUpd[k] > maxUpd) maxUpd = stageUpd[k];

    printf("\n================ Analysis & Insights ================\n");
    printf("Number of cities (n)            : %d\n", n);
    printf("Direct roads (edges)            : %d (negative: %d)\n", edges, negEdges);
    printf("Stages (intermediate cities k)  : %d\n", n);
    printf("Max i-k-j combinations (n^3)    : %d\n", n * n * n);
    printf("Relaxation checks performed     : %lld (%.1f%% of n^3; pairs that\n"
           "                                  cannot reach k are skipped)\n",
           relaxTests, 100.0 * (double)relaxTests / (double)(n * n * n));
    printf("Successful updates              : %lld\n", updates);
    printf("Unreachable pairs               : %d of %d\n", unreach, n * (n - 1));
    printf("Negative cycle                  : %s", hasNegCycle ? "YES" : "NO");
    if (hasNegCycle) printf(" (%d cities on a cycle)", cycleCities);
    printf("\n");

    printf("\nImprovements per stage (routes made cheaper via city k):\n");
    for (k = 0; k < n; k++) {
        printf("  k=%-2d %-12.12s |", k + 1, city[k]);
        w = stageUpd[k] * BAR_WIDTH / maxUpd;
        for (i = 0; i < w; i++) putchar('#');
        printf("%*s %d\n", BAR_WIDTH - w, "", stageUpd[k]);
    }

    printf("\nHow it works:\n");
    printf("  Recurrence : D(k)[i][j] = min( D(k-1)[i][j], D(k-1)[i][k] + D(k-1)[k][j] )\n");
    printf("  Subproblem : D(k)[i][j] is the cheapest cost from i to j using only\n"
           "               cities 1..k as stops in between.\n");
    printf("  Base case  : D(0) is the direct road matrix (0 on diagonal, INF if no road).\n");
    printf("  Routes     : next[i][j] stores the first city to visit; following it\n"
           "               rebuilds the full route.\n");
    printf("  Neg. cycle : any D[i][i] < 0 at the end means costs fall without limit.\n");
    printf("  Time       : Theta(n^3)     Space : Theta(n^2)\n");
    printf("=====================================================\n");
}

/* --------------------------------- main --------------------------------- */

void showMenu(void)
{
    printf("\n=====================================================\n");
    printf("   FAREGRID - INTERCITY ALL-PAIRS TRAVEL COST ANALYZER\n");
    printf("              (Floyd-Warshall Algorithm)\n");
    printf("=====================================================\n");
    printf("  Network%s\n", dataLoaded ? "" : "  (no network loaded)");
    printf("   1. Enter cities and cost matrix\n");
    printf("   2. Load sample dataset\n");
    printf("   3. Generate random network\n");
    printf("   4. Edit network (cities / roads)\n");
    printf("   5. Display cities and cost matrix\n");
    printf("  Algorithm%s\n", processed ? "" : "  (not run yet)");
    printf("   6. Run Floyd-Warshall algorithm\n");
    printf("  Results\n");
    printf("   7. Display final distance matrix\n");
    printf("   8. Display next-hop (route) matrix\n");
    printf("   9. Route planner (cheapest cost and route)\n");
    printf("  10. List unreachable city pairs\n");
    printf("  11. Negative cycle report\n");
    printf("  12. Analysis & insights\n");
    printf("   0. Exit\n");
    printf("-----------------------------------------------------\n");
}

int main(void)
{
    int choice;

    srand((unsigned)time(NULL));

    while (1) {
        showMenu();
        choice = readInt("Enter your choice: ", 0, 12);

        if (choice == 0) {
            printf("\nThank you for using FareGrid. Exiting.\n");
            break;
        }
        if (choice >= 5 && !dataLoaded) {
            printf("\nNo network available. Use option 1, 2, 3 or 4 first.\n");
            continue;
        }
        if (choice >= 7 && !processed) {
            printf("\nRun the Floyd-Warshall algorithm (option 6) first.\n");
            continue;
        }

        switch (choice) {
        case 1:  readGraph();             break;
        case 2:  loadSample();            break;
        case 3:  randomNetwork();         break;
        case 4:  editNetwork();           break;
        case 5:  showNetwork();           break;
        case 6:  runAlgorithm();          break;
        case 7:  printFinalMatrix();      break;
        case 8:  printNextMatrix();       break;
        case 9:  routePlanner();          break;
        case 10: listUnreachable();       break;
        case 11: negativeCycleReport();   break;
        case 12: analysisSummary();       break;
        }
    }
    return 0;
}
