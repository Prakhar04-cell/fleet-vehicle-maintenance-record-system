#ifndef DSA_H
#define DSA_H

#include "common.h"
#include "vehicle.h"
#include "maintenance.h"
#include "schedule.h"
#include "expense.h"

/* =========================================================================
 * 1. HASH TABLE: Efficient Vehicle Lookup by Registration Number
 * Time Complexity: O(1) average lookup, O(1) average insertion, O(N) worst
 * Space Complexity: O(N)
 * ========================================================================= */
typedef struct DsaHashNode {
    char key[MAX_BUFFER_SM];       /* Vehicle Registration Number */
    Vehicle value;                 /* Vehicle record */
    struct DsaHashNode *next;      /* Separate chaining for collision resolution */
} DsaHashNode;

typedef struct {
    DsaHashNode **buckets;
    size_t capacity;
    size_t count;
} DsaHashTable;

DsaHashTable *dsa_ht_create(size_t capacity);
StatusCode dsa_ht_insert(DsaHashTable *ht, const Vehicle *v);
const Vehicle *dsa_ht_search(const DsaHashTable *ht, const char *reg_number);
bool dsa_ht_remove(DsaHashTable *ht, const char *reg_number);
size_t dsa_ht_size(const DsaHashTable *ht);
void dsa_ht_destroy(DsaHashTable *ht);
DsaHashTable *dsa_ht_load_all_vehicles(void);

/* =========================================================================
 * 2. BINARY SEARCH TREE (BST): Ordered Indexing & Range Queries
 * Time Complexity: O(log N) average search/insert, O(N) worst case (unbalanced)
 * Space Complexity: O(N)
 * Key: Vehicle Odometer Reading (km) or ID
 * ========================================================================= */
typedef struct DsaBSTNode {
    int key;                       /* e.g. odometer_km */
    Vehicle vehicle;
    struct DsaBSTNode *left;
    struct DsaBSTNode *right;
} DsaBSTNode;

DsaBSTNode *dsa_bst_insert(DsaBSTNode *root, const Vehicle *v);
const DsaBSTNode *dsa_bst_search(const DsaBSTNode *root, int key);
DsaBSTNode *dsa_bst_delete(DsaBSTNode *root, int key);
void dsa_bst_inorder(const DsaBSTNode *root, Vehicle *out_arr, int *count, int max_capacity);
void dsa_bst_range_query(const DsaBSTNode *root, int min_key, int max_key, Vehicle *out_arr, int *count, int max_capacity);
const DsaBSTNode *dsa_bst_min(const DsaBSTNode *root);
const DsaBSTNode *dsa_bst_max(const DsaBSTNode *root);
void dsa_bst_destroy(DsaBSTNode *root);

/* =========================================================================
 * 3. MIN-HEAP (PRIORITY QUEUE): Upcoming & Overdue Maintenance Prioritization
 * Time Complexity: O(1) peek min, O(log N) insert, O(log N) extract min
 * Space Complexity: O(N)
 * Priority Key: Days remaining until due (negative = overdue = highest priority)
 * ========================================================================= */
typedef struct {
    int priority_score;            /* Lower value = higher priority (e.g. days_remaining) */
    MaintenanceSchedule schedule;
} DsaHeapItem;

typedef struct {
    DsaHeapItem *items;
    size_t capacity;
    size_t size;
} DsaMinHeap;

DsaMinHeap *dsa_heap_create(size_t capacity);
StatusCode dsa_heap_insert(DsaMinHeap *heap, const MaintenanceSchedule *schedule, int priority_score);
bool dsa_heap_extract_min(DsaMinHeap *heap, MaintenanceSchedule *out_schedule, int *out_priority);
const DsaHeapItem *dsa_heap_peek(const DsaMinHeap *heap);
size_t dsa_heap_size(const DsaMinHeap *heap);
bool dsa_heap_is_empty(const DsaMinHeap *heap);
void dsa_heap_destroy(DsaMinHeap *heap);
DsaMinHeap *dsa_heap_build_from_active_schedules(void);

/* =========================================================================
 * 4. QUEUE (FIFO): Workshop Maintenance Service Dispatch Pipeline
 * Time Complexity: O(1) enqueue, O(1) dequeue
 * Space Complexity: O(N)
 * ========================================================================= */
typedef struct DsaQueueNode {
    MaintenanceRecord task;
    struct DsaQueueNode *next;
} DsaQueueNode;

typedef struct {
    DsaQueueNode *front;
    DsaQueueNode *rear;
    size_t size;
} DsaQueue;

DsaQueue *dsa_queue_create(void);
StatusCode dsa_queue_enqueue(DsaQueue *q, const MaintenanceRecord *task);
bool dsa_queue_dequeue(DsaQueue *q, MaintenanceRecord *out_task);
const MaintenanceRecord *dsa_queue_peek(const DsaQueue *q);
size_t dsa_queue_size(const DsaQueue *q);
bool dsa_queue_is_empty(const DsaQueue *q);
void dsa_queue_destroy(DsaQueue *q);

/* =========================================================================
 * 5. STACK (LIFO): Action History & Operational Undo/Backtrack
 * Time Complexity: O(1) push, O(1) pop
 * Space Complexity: O(N)
 * ========================================================================= */
typedef struct {
    char action[64];
    char entity_type[32];
    int entity_id;
    char timestamp[32];
    char details[128];
} DsaStackItem;

typedef struct {
    DsaStackItem *items;
    size_t capacity;
    size_t top;
} DsaStack;

DsaStack *dsa_stack_create(size_t capacity);
StatusCode dsa_stack_push(DsaStack *s, const char *action, const char *entity, int entity_id, const char *details);
bool dsa_stack_pop(DsaStack *s, DsaStackItem *out_item);
bool dsa_stack_peek(const DsaStack *s, DsaStackItem *out_item);
size_t dsa_stack_size(const DsaStack *s);
bool dsa_stack_is_empty(const DsaStack *s);
void dsa_stack_destroy(DsaStack *s);

/* =========================================================================
 * 6. MERGE SORT: Stable O(N log N) Sorting
 * Time Complexity: O(N log N) worst/average/best case
 * Space Complexity: O(N) auxiliary
 * ========================================================================= */
void dsa_mergesort_vehicles(Vehicle *arr, int left, int right, VehicleSortField sort_field, bool descending);
void dsa_mergesort_maintenance(MaintenanceRecord *arr, int left, int right, bool by_cost, bool descending);
void dsa_mergesort_expenses(Expense *arr, int left, int right, bool by_amount, bool descending);

/* =========================================================================
 * 7. GRAPH & DIJKSTRA'S ALGORITHM: Fleet Hubs, Depots & Shortest Route Routing
 * Time Complexity: O((V + E) log V) with adjacency matrix / priority queue
 * Space Complexity: O(V^2) / O(V + E)
 * ========================================================================= */
#define DSA_MAX_GRAPH_VERTICES 32
#define DSA_INF 99999999

typedef struct {
    int id;
    char name[64];
    char type[32];                 /* "Depot", "Workshop", "Fuel Station", "Hub" */
} DsaGraphVertex;

typedef struct {
    int num_vertices;
    DsaGraphVertex vertices[DSA_MAX_GRAPH_VERTICES];
    int adj_matrix[DSA_MAX_GRAPH_VERTICES][DSA_MAX_GRAPH_VERTICES]; /* edge weights: distance in km */
} DsaGraph;

DsaGraph *dsa_graph_create(int num_vertices);
void dsa_graph_set_vertex(DsaGraph *g, int u, const char *name, const char *type);
void dsa_graph_add_edge(DsaGraph *g, int u, int v, int distance_km);
bool dsa_graph_dijkstra(const DsaGraph *g, int start_node, int end_node, int *out_distance, int *out_path, int *out_path_length);
void dsa_graph_destroy(DsaGraph *g);
DsaGraph *dsa_graph_create_sample_fleet_network(void);

/* =========================================================================
 * 8. DYNAMIC PROGRAMMING: Fleet Maintenance Budget Allocation (0-1 Knapsack)
 * Formulation: Maximize total safety/criticality points without exceeding
 * available quarterly fleet maintenance budget B (in rupees/minor units).
 * Time Complexity: O(N * B)
 * Space Complexity: O(N * B) with full backtracking reconstruction
 * ========================================================================= */
typedef struct {
    int id;
    char vehicle_reg[MAX_BUFFER_SM];
    char service_name[MAX_BUFFER_MD];
    int cost_units;                /* Weight in Knapsack */
    int criticality_score;         /* Value in Knapsack (1 to 100) */
} DsaMaintenanceJob;

int dsa_dp_knapsack_maintenance(const DsaMaintenanceJob *jobs, int n, int budget_units, int *out_selected_indices, int *out_selected_count);

/* =========================================================================
 * 9. BACKTRACKING & BRANCH AND BOUND: Fleet Service Dispatch Tour (TSP)
 * Formulation: Mobile mechanic inspector must visit K service sites and
 * return to central depot with minimum total kilometers traveled.
 * Pruning: Branch & bound lower bounds prevent exploring subtrees exceeding
 * best-known tour distance.
 * Time Complexity: O(N!) worst-case, drastically reduced via B&B bounding
 * Space Complexity: O(N) recursion stack
 * ========================================================================= */
#define DSA_TSP_MAX 16

int dsa_tsp_branch_and_bound(int dist_matrix[DSA_TSP_MAX][DSA_TSP_MAX], int n, int *out_best_tour);

/* =========================================================================
 * 10. DSA Demonstrator & Benchmark Routine
 * ========================================================================= */
void dsa_run_full_demonstration(void);

#endif /* DSA_H */
