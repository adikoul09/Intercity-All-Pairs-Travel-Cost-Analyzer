# FareGrid: Intercity All-Pairs Travel Cost Analyzer

**AOA Mini Project · Sr. No. 17 · Module 4: Dynamic Programming**
Vidyalankar Institute of Technology · Analysis & Design of Algorithms

FareGrid is a menu-driven C program that computes the **minimum travel cost between every pair of cities** using the **Floyd–Warshall algorithm**. After processing, you can ask for the cheapest cost and route between any two cities. It also reports unreachable pairs and detects negative cycles.

| | |
| --- | --- |
| **Language** | C (C99) |
| **Interface** | Console, menu driven |
| **GUI / Database** | Not used |
| **Source file** | `floyd_warshall.c` (single file, standard library only) |
| **Team** | Advik, Varad, Siddhesh, Aniket |

---

## Problem statement

> Create a console application that computes the minimum travel cost between every pair of cities. A cost matrix is updated by considering each city as an allowed intermediate point. Users can query any source–destination pair after processing.

- **Input:** number and names of cities, adjacency cost matrix, no-edge indicator, route queries.
- **Output:** final all-pairs distance matrix, cheapest cost for any pair, reconstructed route, unreachable pairs, negative-cycle warning.

---

## Algorithm: Floyd–Warshall (Dynamic Programming)

`D(k)[i][j]` is the cheapest cost from city *i* to city *j* using only cities 1…k as stops in between.

```
D(0)[i][j] = cost[i][j]          (0 when i = j, INF when there is no direct road)
D(k)[i][j] = min( D(k-1)[i][j],  D(k-1)[i][k] + D(k-1)[k][j] )
```

**Steps implemented**

1. Initialise `dist` from the cost matrix and set the diagonal to 0.
2. Keep a `next` matrix: `next[i][j]` is the first city to visit after *i* on the way to *j*.
3. For every intermediate city *k*, update `dist[i][j]` when `i → k → j` is cheaper, and set `next[i][j] = next[i][k]`.
4. Display the matrices after the stages you choose.
5. Answer repeated pair queries and rebuild routes by following `next`.
6. After the last stage, any `dist[i][i] < 0` means a **negative cycle**. Pairs whose route can pass through it have no cheapest cost and are shown as `-INF`.

| Complexity | |
| --- | --- |
| Time | Θ(n³) |
| Space | Θ(n²) |

---

## Compile and run

```bash
gcc -Wall -std=c99 -o floyd_warshall floyd_warshall.c
./floyd_warshall
```

On Windows (MinGW): `gcc -std=c99 -o floyd_warshall.exe floyd_warshall.c`, then run `floyd_warshall.exe`.
In VS Code, open `floyd_warshall.c` and run the default build task (`.vscode/tasks.json`).

---

## Menu

```
=====================================================
   FAREGRID - INTERCITY ALL-PAIRS TRAVEL COST ANALYZER
              (Floyd-Warshall Algorithm)
=====================================================
  Network
   1. Enter cities and cost matrix
   2. Load sample dataset
   3. Generate random network
   4. Edit network (cities / roads)
   5. Display cities and cost matrix
  Algorithm
   6. Run Floyd-Warshall algorithm
  Results
   7. Display final distance matrix
   8. Display next-hop (route) matrix
   9. Route planner (cheapest cost and route)
  10. List unreachable city pairs
  11. Negative cycle report
  12. Analysis & insights
   0. Exit
```

Options 5–12 need a network, and options 7–12 need the algorithm to have run first. Editing the network clears old results.

### Features

| Option | What it does |
| --- | --- |
| **1. Enter network** | Enter 1–20 unique city names, then the cost matrix row by row. Use `INF`, `x` or `-` for "no direct road". Negative costs (discounts) are allowed. The diagonal is always 0. |
| **2. Sample datasets** | Three ready-made networks (see below). |
| **3. Random network** | Pick the number of cities (2–20) and whether to include negative costs. A ring of roads keeps every city reachable. |
| **4. Edit network** | Add, remove or rename a city, and set, update or remove a single road. |
| **5. Display** | The city list, the input cost matrix, and the number of direct and negative roads. |
| **6. Run Floyd–Warshall** | Four trace modes: **(1)** final result only, **(2)** matrix after every stage, **(3)** matrix after selected stages, **(4)** step by step. Step by step explains every improvement with real numbers and marks changed cells with `*`. |
| **7. Final matrix** | The cheapest cost between every pair. Pairs affected by a negative cycle show as `-INF`. |
| **8. Next-hop matrix** | The first city to visit on each cheapest route. |
| **9. Route planner** | Cheapest cost and full route with a running total for each leg, number of stops, the direct road cost, and the saving. Also lists the cheapest cost from one city to every other city. |
| **10. Unreachable pairs** | All unreachable pairs, with reasons (cities with no outgoing or incoming roads; roads are one-way). |
| **11. Negative-cycle report** | Checks the diagonal, lists the cities on a cycle and every pair whose cost has no lower limit. |
| **12. Analysis & insights** | Relaxation checks against n³, updates, unreachable count, a bar chart of improvements per stage, and the recurrence and complexity. |

### Sample datasets

| # | Network | Shows |
| --- | --- | --- |
| 1 | Mumbai, Pune, Nashik, Nagpur, Goa | Positive road costs, multi-stop routes, savings over direct roads |
| 2 | Surat, Vadodara, Ahmedabad, Rajkot, Bhuj | Negative edges without a negative cycle (the CLRS textbook example) |
| 3 | Delhi, Agra, Jaipur, Udaipur, Shimla | A negative cycle (Delhi → Agra → Jaipur → Delhi = −1) and unreachable cities |

---

## Sample output

**Step-by-step trace (sample 2, stage 1):**

```
=========== Stage 1 of 5 : via Surat ===========
Rule: D[i][j] = min( D[i][j] , D[i][Surat] + D[Surat][j] )
    1. Rajkot -> Vadodara : min( INF , 2 + 3 ) = 5
    2. Rajkot -> Bhuj : min( INF , 2 + (-4) ) = -2

D(1): after allowing Surat as intermediate city (2 improved)
----------------------------------------------------------------
From \ To          Surat  Vadodara Ahmedabad    Rajkot      Bhuj
----------------------------------------------------------------
Surat                 0         3         8       INF        -4
Vadodara            INF         0       INF         1         7
Ahmedabad           INF         4         0       INF       INF
Rajkot                2         5*       -5         0        -2*
Bhuj                INF       INF       INF         6         0
----------------------------------------------------------------
(* = improved in this stage)
```

**Route query (sample 1, Pune → Nagpur):**

```
  Cheapest travel cost: 660
  Route : Pune -> Nashik -> Nagpur
  Legs  :
     Pune           -> Nashik         :      210   (running total 210)
     Nashik         -> Nagpur         :      450   (running total 660)
  Roads used         : 2 (stops in between: 1)
  Direct road        : 700
  Saved vs direct    : 40 (5.7% cheaper)
```

---

## Testing

| Case | Expected result |
| --- | --- |
| CLRS 5-vertex example (sample 2) | Final matrix matches the textbook answer, with 18 updates |
| Positive network (sample 1) | Pune → Nagpur = 660 via Nashik; Nagpur → Goa = 1390 via Mumbai |
| Negative cycle (sample 3) | Warning shown; affected pairs show as `-INF`; no route printed for them |
| Unreachable pairs | Reported with the reason ("Udaipur has no outgoing road") |
| Single city / empty network | Handled; menu guards prevent running without data |
| Invalid input | Rejects non-numbers, out-of-range costs, duplicate names and wrong row lengths, then asks again |

Built and tested with `gcc -Wall -Wextra -std=c99 -pedantic` (no warnings) and with AddressSanitizer and UndefinedBehaviorSanitizer enabled.

---

## Limits

- Up to **20 cities**; names up to 29 characters.
- Road costs are integers within **±1,000,000**.
- Roads are **one-way**: row city → column city. Enter both directions for a two-way road.
