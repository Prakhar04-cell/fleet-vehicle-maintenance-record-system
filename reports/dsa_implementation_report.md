# Data Structures & Algorithms (DSA) Implementation Report
## Fleet Vehicle Maintenance Record System

**Document Version:** 1.0  
**Language Standard:** ISO/IEC 9899:2011 (C11)  
**Author:** Antigravity Senior Systems & Algorithms Engineer  
**Files:** [include/dsa.h](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/include/dsa.h), [src/dsa.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/src/dsa.c), [tests/test_main.c](file:///c:/Users/singh/Desktop/fleet%20vehicle%20maintenance%20record%20system/tests/test_main.c)

---

### 1. Architectural Philosophy: Persistent Store vs. Computational In-Memory Structures

In industrial fleet management, persistence, ACID transactions, and relational referential integrity are strictly governed by **SQLite 3**. However, high-throughput fleet telematics, emergency routing, urgency prioritization, and combinatorial budget optimization demand in-memory algorithmic data structures.

This project implements all core computer science structures in **pure C11** without external libraries, using pointer manipulation, explicit memory management (`malloc`, `calloc`, `free`), dynamic capacity scaling, and recursive algorithms.

---

### 2. Comprehensive DSA Module Specifications

#### 1. Hash Table with Separate Chaining (Vehicle Lookup)
* **Real-World Application:** Instant $O(1)$ lookup of vehicle telemetry and driver assignments using the vehicle registration number (license plate) as the alphanumeric key.
* **Algorithm & Design:**
  * **Hash Function:** DJB2 (Daniel J. Bernstein) polynomial hash function:
    $$\text{hash} = ((\text{hash} \ll 5) + \text{hash}) + c$$
  * **Collision Resolution:** Separate chaining using singly linked list nodes (`DsaHashNode`).
  * **Operations:** `dsa_ht_create`, `dsa_ht_insert`, `dsa_ht_search`, `dsa_ht_remove`, `dsa_ht_destroy`.
* **Complexity:**
  * **Time:** Average Case: $O(1)$ for insert, lookup, and delete. Worst Case: $O(N)$ (degenerate hash collisions).
  * **Space:** $O(M + N)$ where $M$ is bucket capacity and $N$ is total vehicle count.

#### 2. Binary Search Tree (BST) (Odometer Indexing & Range Queries)
* **Real-World Application:** Indexing the fleet by operational wear (odometer reading in kilometers) to support continuous range queries (e.g., finding all vehicles due for 20,000 km–50,000 km mid-life maintenance).
* **Algorithm & Design:**
  * Node-based binary tree (`DsaBSTNode`) storing vehicle records ordered by `odometer_km`.
  * Recursive insertion, deletion (handling 0, 1, or 2 children via in-order successor), in-order traversal, and range filtering.
  * Extrema functions: `dsa_bst_min` and `dsa_bst_max`.
* **Complexity:**
  * **Time:** Average Case: $O(\log N)$ for search, insertion, and deletion. Range Query: $O(\log N + K)$ where $K$ is match count. Worst Case: $O(N)$ (skewed tree).
  * **Space:** $O(N)$ for node allocation and $O(\log N)$ recursion stack depth.

#### 3. Min-Heap Priority Queue (Preventive Maintenance Urgency Engine)
* **Real-World Application:** Prioritizing scheduled preventive maintenance tasks across hundreds of vehicles. The vehicle most overdue or closest to its due date must be serviced first.
* **Algorithm & Design:**
  * Array-backed complete binary tree (`DsaMinHeap`).
  * **Priority Metric:** `priority_score` calculated as days remaining until due date:
    * Negative score: Overdue service (e.g., $-5$ days overdue has higher urgency than $-1$).
    * Positive score: Upcoming service (e.g., due in $+2$ days has higher urgency than $+14$ days).
  * **Heap Operations:** Bubble-up (`heapify_up`) on insertion and trickle-down (`heapify_down`) on `extract_min`.
* **Complexity:**
  * **Time:** Peek: $O(1)$. Insert: $O(\log N)$. Extract Min: $O(\log N)$. Build Heap: $O(N)$.
  * **Space:** $O(N)$ contiguous memory buffer.

#### 4. FIFO Queue (Workshop Service Dispatch Pipeline)
* **Real-World Application:** Dispatching vehicles to active repair bays in strict chronological order of intake.
* **Algorithm & Design:**
  * Singly linked list queue (`DsaQueue`) maintaining explicit `front` and `rear` pointers for instantaneous operations without array shifting.
  * Safe cleanup: `dsa_queue_destroy` frees all pending nodes.
* **Complexity:**
  * **Time:** Enqueue: $O(1)$. Dequeue: $O(1)$. Peek: $O(1)$.
  * **Space:** $O(N)$ dynamic memory allocations.

#### 5. LIFO Stack (Operational Undo & Audit History)
* **Real-World Application:** Operational rollback and audit backtracking of recent user modifications (e.g., undoing an accidental odometer adjustment or status change).
* **Algorithm & Design:**
  * Dynamically resizable array stack (`DsaStack`) with automatic capacity doubling when saturated.
  * Records action, entity type, entity ID, ISO 8601 timestamp, and modification details.
* **Complexity:**
  * **Time:** Push: $O(1)$ amortized. Pop: $O(1)$. Peek: $O(1)$.
  * **Space:** $O(N)$ dynamically allocated capacity.

#### 6. Merge Sort (Stable Multi-Attribute Fleet Sorting)
* **Real-World Application:** Sorting fleet records by odometer, maintenance cost, or date without altering the relative order of equal elements (stability guarantee).
* **Algorithm & Design:**
  * Classic recursive divide-and-conquer merge sort.
  * Sub-arrays are recursively partitioned until base case $N=1$, then merged using a temporary auxiliary array with stable comparison predicates.
  * Separate implementations: `dsa_mergesort_vehicles`, `dsa_mergesort_maintenance`, and `dsa_mergesort_expenses`.
* **Complexity:**
  * **Time:** $O(N \log N)$ guaranteed in Best, Average, and Worst cases.
  * **Space:** $O(N)$ auxiliary heap memory required for merge step.

#### 7. Graph & Dijkstra's Algorithm (Fleet Route Optimization)
* **Real-World Application:** Determining the shortest travel route in kilometers between central fleet headquarters, express workshops, cargo terminals, and freight delivery hubs.
* **Algorithm & Design:**
  * Weighted graph (`DsaGraph`) represented via an adjacency matrix `adj_matrix[V][V]`.
  * **Dijkstra's Algorithm:** Computes the shortest path from single source to destination using iterative distance relaxation with predecessor tracking array to reconstruct the full path sequence.
  * Sample fleet logistics topology includes 6 real hubs:
    1. Central Fleet Depot (HQ)
    2. North Express Workshop
    3. South Industrial Hub
    4. East Cargo Logistics Center
    5. West Fleet Heavy Workshop
    6. Airport Freight Terminal
* **Complexity:**
  * **Time:** $O(V^2)$ with adjacency matrix / $O((V + E) \log V)$ with heap.
  * **Space:** $O(V^2)$ representation space and $O(V)$ predecessor tracking array.

#### 8. Dynamic Programming (0-1 Knapsack Maintenance Budget Optimizer)
* **Real-World Application:** Optimal capital expenditure under quarterly budget constraints. Given $N$ pending maintenance jobs—each with a cost in rupees and a safety criticality score (1–100)—select the exact combination of jobs that maximizes overall fleet reliability without exceeding the monetary budget $B$.
* **Mathematical Formulation:**
  * Let $DP[i][w]$ denote the maximum safety score using a subset of the first $i$ jobs with total cost $\le w$:
    $$DP[i][w] = \max\Big(DP[i-1][w], \; DP[i-1][w - \text{cost}_i] + \text{score}_i\Big) \quad \text{for } w \ge \text{cost}_i$$
  * After populating the $DP$ table, the optimal subset of jobs is reconstructed via backward traversal from $DP[n][B]$.
* **Complexity:**
  * **Time:** $\Theta(N \cdot B)$ pseudo-polynomial time.
  * **Space:** $\Theta(N \cdot B)$ table space allocated dynamically and freed immediately after path reconstruction.

#### 9. Backtracking with Branch & Bound (Traveling Salesperson Dispatch Tour)
* **Real-World Application:** Mobile Mechanic Dispatch. A specialized mobile maintenance van stationed at HQ must visit $K$ broken-down vehicle sites across the city and return to HQ with the minimum total travel kilometers.
* **Algorithm & Design:**
  * Depth-First Search (DFS) recursive state-space tree traversal over vehicle inspection waypoints.
  * **Branch and Bound Pruning:** At each step, a lower bound is calculated. If the current accumulated tour cost exceeds the `best_cost` already found, the entire branch is pruned immediately, avoiding factorial explosion.
* **Complexity:**
  * **Time:** Worst Case: $O((N-1)!)$ without pruning. In practice: drastically reduced to sub-exponential time via aggressive bounding.
  * **Space:** $O(N)$ recursion depth and path history buffers.

---

### 3. Summary of Asymptotic Time & Space Complexities

| Algorithm / Data Structure | Time Complexity (Best) | Time Complexity (Average) | Time Complexity (Worst) | Space Complexity | Real-World Fleet Usage |
|---|---|---|---|---|---|
| **Hash Table** | $\Omega(1)$ | $\Theta(1)$ | $O(N)$ | $O(N)$ | Vehicle lookup by registration |
| **Binary Search Tree** | $\Omega(1)$ | $\Theta(\log N)$ | $O(N)$ | $O(N)$ | Wear / odometer indexing & range queries |
| **Min-Heap (Priority Queue)** | $\Omega(1)$ | $\Theta(\log N)$ | $O(\log N)$ | $O(N)$ | Maintenance urgency schedule queue |
| **FIFO Queue** | $\Omega(1)$ | $\Theta(1)$ | $O(1)$ | $O(N)$ | Workshop repair bay dispatch |
| **LIFO Stack** | $\Omega(1)$ | $\Theta(1)$ | $O(1)$ | $O(N)$ | Action history & undo backtracking |
| **Merge Sort** | $\Omega(N \log N)$ | $\Theta(N \log N)$ | $O(N \log N)$ | $O(N)$ | Stable sorting of vehicles/expenses |
| **Graph & Dijkstra** | $\Omega(V + E)$ | $\Theta((V+E)\log V)$ | $O(V^2)$ | $O(V^2)$ | Depot-to-site shortest route routing |
| **DP 0-1 Knapsack** | $\Omega(N \cdot B)$ | $\Theta(N \cdot B)$ | $O(N \cdot B)$ | $O(N \cdot B)$ | Maintenance budget spend optimization |
| **Branch & Bound (TSP)** | $\Omega(N)$ | Pruned $O(K^N)$ | $O(N!)$ | $O(N)$ | Mobile mechanic inspection circuit |

---

### 4. Verification and Benchmark Results

The DSA test suite was verified via two complementary mechanisms:
1. **Automated Unit Testing (`tests/test_main.c`):** 25 automated assertions covering boundary conditions (empty structures, duplicate keys, capacity limits, range limits, budget overshoots, and TSP permutations).
2. **Interactive CLI Demonstrator (`./fleet.exe --dsa`):** Runs live benchmarks with concrete numbers, routes, and budget calculations on real terminal output.
3. **Web Dashboard API (`/api/dsa/run`):** Returns the live benchmark and optimization results as structured JSON rendered interactively in the browser interface.

All tests passed with zero failures and verified memory deallocation.
