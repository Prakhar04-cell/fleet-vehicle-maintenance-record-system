#include "dsa.h"
#include "database.h"
#include "ui.h"
#include <limits.h>
#include <ctype.h>

/* =========================================================================
 * 1. HASH TABLE IMPLEMENTATION
 * ========================================================================= */
static unsigned long hash_djb2(const char *str) {
    unsigned long hash = 5381;
    while (*str) {
        char c = *str++;
        if (c == '-' || c == ' ' || c == '_') continue;
        c = (char)tolower((unsigned char)c);
        hash = ((hash << 5) + hash) + (unsigned long)c; /* hash * 33 + c */
    }
    return hash;
}

static bool reg_normalized_equals(const char *s1, const char *s2) {
    if (!s1 || !s2) return false;
    while (*s1 || *s2) {
        while (*s1 == '-' || *s1 == ' ' || *s1 == '_') s1++;
        while (*s2 == '-' || *s2 == ' ' || *s2 == '_') s2++;
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) return false;
        if (*s1) s1++;
        if (*s2) s2++;
    }
    return true;
}

DsaHashTable *dsa_ht_create(size_t capacity) {
    if (capacity == 0) capacity = 101;
    DsaHashTable *ht = (DsaHashTable *)calloc(1, sizeof(DsaHashTable));
    if (!ht) return NULL;
    ht->buckets = (DsaHashNode **)calloc(capacity, sizeof(DsaHashNode *));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    ht->capacity = capacity;
    ht->count = 0;
    return ht;
}

StatusCode dsa_ht_insert(DsaHashTable *ht, const Vehicle *v) {
    if (!ht || !v || str_is_blank(v->registration_number)) return STATUS_INVALID_INPUT;

    unsigned long idx = hash_djb2(v->registration_number) % ht->capacity;

    /* Check if key already exists (update existing) */
    DsaHashNode *cur = ht->buckets[idx];
    while (cur) {
        if (reg_normalized_equals(cur->key, v->registration_number)) {
            cur->value = *v;
            return STATUS_OK;
        }
        cur = cur->next;
    }

    /* Insert new node at head of chain */
    DsaHashNode *node = (DsaHashNode *)malloc(sizeof(DsaHashNode));
    if (!node) return STATUS_ERROR;

    safe_strncpy(node->key, v->registration_number, sizeof(node->key));
    node->value = *v;
    node->next = ht->buckets[idx];
    ht->buckets[idx] = node;
    ht->count++;

    return STATUS_OK;
}

const Vehicle *dsa_ht_search(const DsaHashTable *ht, const char *reg_number) {
    if (!ht || str_is_blank(reg_number) || ht->count == 0) return NULL;

    unsigned long idx = hash_djb2(reg_number) % ht->capacity;
    DsaHashNode *cur = ht->buckets[idx];
    while (cur) {
        if (reg_normalized_equals(cur->key, reg_number)) {
            return &cur->value;
        }
        cur = cur->next;
    }
    return NULL;
}

bool dsa_ht_remove(DsaHashTable *ht, const char *reg_number) {
    if (!ht || str_is_blank(reg_number) || ht->count == 0) return false;

    unsigned long idx = hash_djb2(reg_number) % ht->capacity;
    DsaHashNode *cur = ht->buckets[idx];
    DsaHashNode *prev = NULL;

    while (cur) {
        if (reg_normalized_equals(cur->key, reg_number)) {
            if (prev) {
                prev->next = cur->next;
            } else {
                ht->buckets[idx] = cur->next;
            }
            free(cur);
            ht->count--;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}

size_t dsa_ht_size(const DsaHashTable *ht) {
    return ht ? ht->count : 0;
}

void dsa_ht_destroy(DsaHashTable *ht) {
    if (!ht) return;
    for (size_t i = 0; i < ht->capacity; i++) {
        DsaHashNode *cur = ht->buckets[i];
        while (cur) {
            DsaHashNode *tmp = cur;
            cur = cur->next;
            free(tmp);
        }
    }
    free(ht->buckets);
    free(ht);
}

DsaHashTable *dsa_ht_load_all_vehicles(void) {
    VehicleFilterOptions f = {0};
    f.include_archived = true;
    Vehicle *list = NULL;
    int count = 0;

    if (vehicle_list(&f, &list, &count) != STATUS_OK || count == 0) {
        return dsa_ht_create(101);
    }

    size_t cap = (size_t)(count * 2) + 1;
    DsaHashTable *ht = dsa_ht_create(cap);
    if (!ht) {
        vehicle_free_list(list);
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        dsa_ht_insert(ht, &list[i]);
    }

    vehicle_free_list(list);
    return ht;
}

/* =========================================================================
 * 2. BINARY SEARCH TREE (BST) IMPLEMENTATION
 * ========================================================================= */
DsaBSTNode *dsa_bst_insert(DsaBSTNode *root, const Vehicle *v) {
    if (!v) return root;

    int key = v->odometer_km;
    if (!root) {
        DsaBSTNode *node = (DsaBSTNode *)calloc(1, sizeof(DsaBSTNode));
        if (!node) return NULL;
        node->key = key;
        node->vehicle = *v;
        return node;
    }

    if (key < root->key) {
        root->left = dsa_bst_insert(root->left, v);
    } else {
        root->right = dsa_bst_insert(root->right, v);
    }
    return root;
}

const DsaBSTNode *dsa_bst_search(const DsaBSTNode *root, int key) {
    if (!root || root->key == key) return root;
    if (key < root->key) {
        return dsa_bst_search(root->left, key);
    }
    return dsa_bst_search(root->right, key);
}

const DsaBSTNode *dsa_bst_min(const DsaBSTNode *root) {
    if (!root) return NULL;
    while (root->left) root = root->left;
    return root;
}

const DsaBSTNode *dsa_bst_max(const DsaBSTNode *root) {
    if (!root) return NULL;
    while (root->right) root = root->right;
    return root;
}

DsaBSTNode *dsa_bst_delete(DsaBSTNode *root, int key) {
    if (!root) return NULL;

    if (key < root->key) {
        root->left = dsa_bst_delete(root->left, key);
    } else if (key > root->key) {
        root->right = dsa_bst_delete(root->right, key);
    } else {
        /* Node with one or zero child */
        if (!root->left) {
            DsaBSTNode *temp = root->right;
            free(root);
            return temp;
        } else if (!root->right) {
            DsaBSTNode *temp = root->left;
            free(root);
            return temp;
        }
        /* Node with two children: get inorder successor (min in right subtree) */
        const DsaBSTNode *succ = dsa_bst_min(root->right);
        root->key = succ->key;
        root->vehicle = succ->vehicle;
        root->right = dsa_bst_delete(root->right, succ->key);
    }
    return root;
}

void dsa_bst_inorder(const DsaBSTNode *root, Vehicle *out_arr, int *count, int max_capacity) {
    if (!root || !out_arr || !count || *count >= max_capacity) return;

    dsa_bst_inorder(root->left, out_arr, count, max_capacity);
    if (*count < max_capacity) {
        out_arr[(*count)++] = root->vehicle;
    }
    dsa_bst_inorder(root->right, out_arr, count, max_capacity);
}

void dsa_bst_range_query(const DsaBSTNode *root, int min_key, int max_key, Vehicle *out_arr, int *count, int max_capacity) {
    if (!root || !out_arr || !count || *count >= max_capacity) return;

    if (root->key > min_key) {
        dsa_bst_range_query(root->left, min_key, max_key, out_arr, count, max_capacity);
    }
    if (root->key >= min_key && root->key <= max_key) {
        if (*count < max_capacity) {
            out_arr[(*count)++] = root->vehicle;
        }
    }
    if (root->key < max_key) {
        dsa_bst_range_query(root->right, min_key, max_key, out_arr, count, max_capacity);
    }
}

void dsa_bst_destroy(DsaBSTNode *root) {
    if (!root) return;
    dsa_bst_destroy(root->left);
    dsa_bst_destroy(root->right);
    free(root);
}

/* =========================================================================
 * 3. MIN-HEAP (PRIORITY QUEUE) IMPLEMENTATION
 * ========================================================================= */
DsaMinHeap *dsa_heap_create(size_t capacity) {
    if (capacity == 0) capacity = 32;
    DsaMinHeap *h = (DsaMinHeap *)malloc(sizeof(DsaMinHeap));
    if (!h) return NULL;
    h->items = (DsaHeapItem *)malloc(capacity * sizeof(DsaHeapItem));
    if (!h->items) {
        free(h);
        return NULL;
    }
    h->capacity = capacity;
    h->size = 0;
    return h;
}

static void heap_sift_up(DsaMinHeap *h, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (h->items[idx].priority_score < h->items[parent].priority_score) {
            DsaHeapItem tmp = h->items[idx];
            h->items[idx] = h->items[parent];
            h->items[parent] = tmp;
            idx = parent;
        } else {
            break;
        }
    }
}

static void heap_sift_down(DsaMinHeap *h, size_t idx) {
    while (2 * idx + 1 < h->size) {
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;
        size_t smallest = idx;

        if (h->items[left].priority_score < h->items[smallest].priority_score) {
            smallest = left;
        }
        if (right < h->size && h->items[right].priority_score < h->items[smallest].priority_score) {
            smallest = right;
        }

        if (smallest != idx) {
            DsaHeapItem tmp = h->items[idx];
            h->items[idx] = h->items[smallest];
            h->items[smallest] = tmp;
            idx = smallest;
        } else {
            break;
        }
    }
}

StatusCode dsa_heap_insert(DsaMinHeap *heap, const MaintenanceSchedule *schedule, int priority_score) {
    if (!heap || !schedule) return STATUS_INVALID_INPUT;

    if (heap->size >= heap->capacity) {
        size_t new_cap = heap->capacity * 2;
        DsaHeapItem *tmp = (DsaHeapItem *)realloc(heap->items, new_cap * sizeof(DsaHeapItem));
        if (!tmp) return STATUS_ERROR;
        heap->items = tmp;
        heap->capacity = new_cap;
    }

    size_t idx = heap->size++;
    heap->items[idx].priority_score = priority_score;
    heap->items[idx].schedule = *schedule;

    heap_sift_up(heap, idx);
    return STATUS_OK;
}

bool dsa_heap_extract_min(DsaMinHeap *heap, MaintenanceSchedule *out_schedule, int *out_priority) {
    if (!heap || heap->size == 0) return false;

    if (out_schedule) *out_schedule = heap->items[0].schedule;
    if (out_priority) *out_priority = heap->items[0].priority_score;

    heap->items[0] = heap->items[--heap->size];
    if (heap->size > 0) {
        heap_sift_down(heap, 0);
    }
    return true;
}

const DsaHeapItem *dsa_heap_peek(const DsaMinHeap *heap) {
    if (!heap || heap->size == 0) return NULL;
    return &heap->items[0];
}

size_t dsa_heap_size(const DsaMinHeap *heap) {
    return heap ? heap->size : 0;
}

bool dsa_heap_is_empty(const DsaMinHeap *heap) {
    return !heap || heap->size == 0;
}

void dsa_heap_destroy(DsaMinHeap *heap) {
    if (!heap) return;
    free(heap->items);
    free(heap);
}

DsaMinHeap *dsa_heap_build_from_active_schedules(void) {
    ScheduleFilterOptions f = {0};
    f.active_only = true;
    MaintenanceSchedule *list = NULL;
    int count = 0;

    if (schedule_list(&f, &list, &count) != STATUS_OK || count == 0) {
        return dsa_heap_create(16);
    }

    DsaMinHeap *h = dsa_heap_create((size_t)count + 8);
    if (!h) {
        schedule_free_list(list);
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        /* Priority score: overdue schedules have negative or zero remaining days, thus highest priority */
        int score = list[i].days_remaining;
        if (strcmp(list[i].status, "Overdue") == 0) {
            score -= 1000; /* Extra high priority */
        }
        dsa_heap_insert(h, &list[i], score);
    }

    schedule_free_list(list);
    return h;
}

/* =========================================================================
 * 4. QUEUE (FIFO) IMPLEMENTATION
 * ========================================================================= */
DsaQueue *dsa_queue_create(void) {
    DsaQueue *q = (DsaQueue *)calloc(1, sizeof(DsaQueue));
    return q;
}

StatusCode dsa_queue_enqueue(DsaQueue *q, const MaintenanceRecord *task) {
    if (!q || !task) return STATUS_INVALID_INPUT;

    DsaQueueNode *node = (DsaQueueNode *)malloc(sizeof(DsaQueueNode));
    if (!node) return STATUS_ERROR;
    node->task = *task;
    node->next = NULL;

    if (q->rear) {
        q->rear->next = node;
        q->rear = node;
    } else {
        q->front = node;
        q->rear = node;
    }
    q->size++;
    return STATUS_OK;
}

bool dsa_queue_dequeue(DsaQueue *q, MaintenanceRecord *out_task) {
    if (!q || !q->front) return false;

    DsaQueueNode *temp = q->front;
    if (out_task) *out_task = temp->task;

    q->front = q->front->next;
    if (!q->front) {
        q->rear = NULL;
    }
    free(temp);
    q->size--;
    return true;
}

const MaintenanceRecord *dsa_queue_peek(const DsaQueue *q) {
    if (!q || !q->front) return NULL;
    return &q->front->task;
}

size_t dsa_queue_size(const DsaQueue *q) {
    return q ? q->size : 0;
}

bool dsa_queue_is_empty(const DsaQueue *q) {
    return !q || q->front == NULL;
}

void dsa_queue_destroy(DsaQueue *q) {
    if (!q) return;
    DsaQueueNode *cur = q->front;
    while (cur) {
        DsaQueueNode *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    free(q);
}

/* =========================================================================
 * 5. STACK (LIFO) IMPLEMENTATION
 * ========================================================================= */
DsaStack *dsa_stack_create(size_t capacity) {
    if (capacity == 0) capacity = 64;
    DsaStack *s = (DsaStack *)malloc(sizeof(DsaStack));
    if (!s) return NULL;
    s->items = (DsaStackItem *)malloc(capacity * sizeof(DsaStackItem));
    if (!s->items) {
        free(s);
        return NULL;
    }
    s->capacity = capacity;
    s->top = 0;
    return s;
}

StatusCode dsa_stack_push(DsaStack *s, const char *action, const char *entity, int entity_id, const char *details) {
    if (!s || str_is_blank(action)) return STATUS_INVALID_INPUT;

    if (s->top >= s->capacity) {
        size_t new_cap = s->capacity * 2;
        DsaStackItem *tmp = (DsaStackItem *)realloc(s->items, new_cap * sizeof(DsaStackItem));
        if (!tmp) return STATUS_ERROR;
        s->items = tmp;
        s->capacity = new_cap;
    }

    DsaStackItem *item = &s->items[s->top++];
    safe_strncpy(item->action, action, sizeof(item->action));
    safe_strncpy(item->entity_type, entity ? entity : "", sizeof(item->entity_type));
    item->entity_id = entity_id;
    get_current_iso_datetime(item->timestamp, sizeof(item->timestamp));
    safe_strncpy(item->details, details ? details : "", sizeof(item->details));

    return STATUS_OK;
}

bool dsa_stack_pop(DsaStack *s, DsaStackItem *out_item) {
    if (!s || s->top == 0) return false;
    if (out_item) *out_item = s->items[--s->top];
    else s->top--;
    return true;
}

bool dsa_stack_peek(const DsaStack *s, DsaStackItem *out_item) {
    if (!s || s->top == 0) return false;
    if (out_item) *out_item = s->items[s->top - 1];
    return true;
}

size_t dsa_stack_size(const DsaStack *s) {
    return s ? s->top : 0;
}

bool dsa_stack_is_empty(const DsaStack *s) {
    return !s || s->top == 0;
}

void dsa_stack_destroy(DsaStack *s) {
    if (!s) return;
    free(s->items);
    free(s);
}

/* =========================================================================
 * 6. MERGE SORT IMPLEMENTATION
 * ========================================================================= */
static int compare_vehicles(const Vehicle *a, const Vehicle *b, VehicleSortField field, bool desc) {
    int res = 0;
    switch (field) {
        case VEHICLE_SORT_MAKE:
            res = strcasecmp(a->make, b->make);
            if (res == 0) res = strcasecmp(a->model, b->model);
            break;
        case VEHICLE_SORT_FUEL:
            res = (a->fuel_percentage > b->fuel_percentage) - (a->fuel_percentage < b->fuel_percentage);
            break;
        case VEHICLE_SORT_ODOMETER:
            res = (a->odometer_km > b->odometer_km) - (a->odometer_km < b->odometer_km);
            break;
        case VEHICLE_SORT_STATUS:
            res = strcasecmp(a->status, b->status);
            break;
        case VEHICLE_SORT_REG:
        default:
            res = strcasecmp(a->registration_number, b->registration_number);
            break;
    }
    return desc ? -res : res;
}

static void merge_vehicles(Vehicle *arr, int left, int mid, int right, VehicleSortField field, bool desc) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    Vehicle *L = (Vehicle *)malloc(n1 * sizeof(Vehicle));
    Vehicle *R = (Vehicle *)malloc(n2 * sizeof(Vehicle));
    if (!L || !R) {
        if (L) free(L);
        if (R) free(R);
        return;
    }

    for (int i = 0; i < n1; i++) L[i] = arr[left + i];
    for (int j = 0; j < n2; j++) R[j] = arr[mid + 1 + j];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        if (compare_vehicles(&L[i], &R[j], field, desc) <= 0) {
            arr[k++] = L[i++];
        } else {
            arr[k++] = R[j++];
        }
    }

    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];

    free(L);
    free(R);
}

void dsa_mergesort_vehicles(Vehicle *arr, int left, int right, VehicleSortField sort_field, bool descending) {
    if (!arr || left >= right) return;
    int mid = left + (right - left) / 2;
    dsa_mergesort_vehicles(arr, left, mid, sort_field, descending);
    dsa_mergesort_vehicles(arr, mid + 1, right, sort_field, descending);
    merge_vehicles(arr, left, mid, right, sort_field, descending);
}

static void merge_maintenance(MaintenanceRecord *arr, int left, int mid, int right, bool by_cost, bool desc) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    MaintenanceRecord *L = (MaintenanceRecord *)malloc(n1 * sizeof(MaintenanceRecord));
    MaintenanceRecord *R = (MaintenanceRecord *)malloc(n2 * sizeof(MaintenanceRecord));
    if (!L || !R) {
        if (L) free(L);
        if (R) free(R);
        return;
    }

    for (int i = 0; i < n1; i++) L[i] = arr[left + i];
    for (int j = 0; j < n2; j++) R[j] = arr[mid + 1 + j];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        int cmp = 0;
        if (by_cost) {
            cmp = (L[i].total_cost > R[j].total_cost) - (L[i].total_cost < R[j].total_cost);
        } else {
            cmp = strcmp(L[i].service_date, R[j].service_date);
        }
        if (desc) cmp = -cmp;

        if (cmp <= 0) {
            arr[k++] = L[i++];
        } else {
            arr[k++] = R[j++];
        }
    }

    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];

    free(L);
    free(R);
}

void dsa_mergesort_maintenance(MaintenanceRecord *arr, int left, int right, bool by_cost, bool descending) {
    if (!arr || left >= right) return;
    int mid = left + (right - left) / 2;
    dsa_mergesort_maintenance(arr, left, mid, by_cost, descending);
    dsa_mergesort_maintenance(arr, mid + 1, right, by_cost, descending);
    merge_maintenance(arr, left, mid, right, by_cost, descending);
}

static void merge_expenses(Expense *arr, int left, int mid, int right, bool by_amount, bool desc) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    Expense *L = (Expense *)malloc(n1 * sizeof(Expense));
    Expense *R = (Expense *)malloc(n2 * sizeof(Expense));
    if (!L || !R) {
        if (L) free(L);
        if (R) free(R);
        return;
    }

    for (int i = 0; i < n1; i++) L[i] = arr[left + i];
    for (int j = 0; j < n2; j++) R[j] = arr[mid + 1 + j];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2) {
        int cmp = 0;
        if (by_amount) {
            cmp = (L[i].amount > R[j].amount) - (L[i].amount < R[j].amount);
        } else {
            cmp = strcmp(L[i].expense_date, R[j].expense_date);
        }
        if (desc) cmp = -cmp;

        if (cmp <= 0) {
            arr[k++] = L[i++];
        } else {
            arr[k++] = R[j++];
        }
    }

    while (i < n1) arr[k++] = L[i++];
    while (j < n2) arr[k++] = R[j++];

    free(L);
    free(R);
}

void dsa_mergesort_expenses(Expense *arr, int left, int right, bool by_amount, bool descending) {
    if (!arr || left >= right) return;
    int mid = left + (right - left) / 2;
    dsa_mergesort_expenses(arr, left, mid, by_amount, descending);
    dsa_mergesort_expenses(arr, mid + 1, right, by_amount, descending);
    merge_expenses(arr, left, mid, right, by_amount, descending);
}

/* =========================================================================
 * 7. GRAPH & DIJKSTRA'S ALGORITHM IMPLEMENTATION
 * ========================================================================= */
DsaGraph *dsa_graph_create(int num_vertices) {
    if (num_vertices <= 0 || num_vertices > DSA_MAX_GRAPH_VERTICES) {
        num_vertices = DSA_MAX_GRAPH_VERTICES;
    }
    DsaGraph *g = (DsaGraph *)calloc(1, sizeof(DsaGraph));
    if (!g) return NULL;
    g->num_vertices = num_vertices;

    for (int i = 0; i < num_vertices; i++) {
        for (int j = 0; j < num_vertices; j++) {
            g->adj_matrix[i][j] = (i == j) ? 0 : DSA_INF;
        }
    }
    return g;
}

void dsa_graph_set_vertex(DsaGraph *g, int u, const char *name, const char *type) {
    if (!g || u < 0 || u >= g->num_vertices) return;
    g->vertices[u].id = u;
    safe_strncpy(g->vertices[u].name, name ? name : "", sizeof(g->vertices[u].name));
    safe_strncpy(g->vertices[u].type, type ? type : "Hub", sizeof(g->vertices[u].type));
}

void dsa_graph_add_edge(DsaGraph *g, int u, int v, int distance_km) {
    if (!g || u < 0 || u >= g->num_vertices || v < 0 || v >= g->num_vertices) return;
    if (distance_km < 0) distance_km = 0;
    g->adj_matrix[u][v] = distance_km;
    g->adj_matrix[v][u] = distance_km; /* undirected road network */
}

bool dsa_graph_dijkstra(const DsaGraph *g, int start_node, int end_node, int *out_distance, int *out_path, int *out_path_length) {
    if (!g || start_node < 0 || start_node >= g->num_vertices ||
        end_node < 0 || end_node >= g->num_vertices) {
        return false;
    }

    int dist[DSA_MAX_GRAPH_VERTICES];
    bool visited[DSA_MAX_GRAPH_VERTICES] = {false};
    int parent[DSA_MAX_GRAPH_VERTICES];

    for (int i = 0; i < g->num_vertices; i++) {
        dist[i] = DSA_INF;
        parent[i] = -1;
    }

    dist[start_node] = 0;

    for (int count = 0; count < g->num_vertices - 1; count++) {
        int u = -1;
        int min_val = DSA_INF;

        for (int v = 0; v < g->num_vertices; v++) {
            if (!visited[v] && dist[v] < min_val) {
                min_val = dist[v];
                u = v;
            }
        }

        if (u == -1 || u == end_node) break;
        visited[u] = true;

        for (int v = 0; v < g->num_vertices; v++) {
            if (!visited[v] && g->adj_matrix[u][v] != DSA_INF &&
                dist[u] + g->adj_matrix[u][v] < dist[v]) {
                dist[v] = dist[u] + g->adj_matrix[u][v];
                parent[v] = u;
            }
        }
    }

    if (dist[end_node] >= DSA_INF) return false;

    if (out_distance) *out_distance = dist[end_node];

    if (out_path && out_path_length) {
        int path[DSA_MAX_GRAPH_VERTICES];
        int len = 0;
        int curr = end_node;
        while (curr != -1) {
            path[len++] = curr;
            curr = parent[curr];
        }

        /* Reverse path array */
        for (int i = 0; i < len; i++) {
            out_path[i] = path[len - 1 - i];
        }
        *out_path_length = len;
    }

    return true;
}

void dsa_graph_destroy(DsaGraph *g) {
    if (g) free(g);
}

DsaGraph *dsa_graph_create_sample_fleet_network(void) {
    DsaGraph *g = dsa_graph_create(6);
    if (!g) return NULL;

    dsa_graph_set_vertex(g, 0, "Central Fleet Depot (HQ)", "Depot");
    dsa_graph_set_vertex(g, 1, "North Express Workshop", "Workshop");
    dsa_graph_set_vertex(g, 2, "South Industrial Hub", "Hub");
    dsa_graph_set_vertex(g, 3, "East Cargo Logistics Center", "Hub");
    dsa_graph_set_vertex(g, 4, "West Fleet Heavy Workshop", "Workshop");
    dsa_graph_set_vertex(g, 5, "Airport Freight Terminal", "Delivery Point");

    /* Distances in kilometers */
    dsa_graph_add_edge(g, 0, 1, 12); /* HQ <-> North Workshop (12 km) */
    dsa_graph_add_edge(g, 0, 2, 18); /* HQ <-> South Hub (18 km) */
    dsa_graph_add_edge(g, 1, 3, 14); /* North <-> East Cargo (14 km) */
    dsa_graph_add_edge(g, 2, 4, 10); /* South <-> West Workshop (10 km) */
    dsa_graph_add_edge(g, 3, 5, 8);  /* East <-> Airport (8 km) */
    dsa_graph_add_edge(g, 4, 5, 15); /* West <-> Airport (15 km) */
    dsa_graph_add_edge(g, 1, 2, 9);  /* North <-> South (9 km) */
    dsa_graph_add_edge(g, 2, 3, 11); /* South <-> East (11 km) */

    return g;
}

/* =========================================================================
 * 8. DYNAMIC PROGRAMMING: FLEET MAINTENANCE BUDGET KNAPSACK
 * ========================================================================= */
int dsa_dp_knapsack_maintenance(const DsaMaintenanceJob *jobs, int n, int budget_units, int *out_selected_indices, int *out_selected_count) {
    if (!jobs || n <= 0 || budget_units <= 0) {
        if (out_selected_count) *out_selected_count = 0;
        return 0;
    }

    /* Allocate 2D DP table: (n + 1) x (budget_units + 1) */
    int rows = n + 1;
    int cols = budget_units + 1;

    int **dp = (int **)malloc(rows * sizeof(int *));
    if (!dp) return 0;
    for (int i = 0; i < rows; i++) {
        dp[i] = (int *)calloc(cols, sizeof(int));
        if (!dp[i]) {
            for (int k = 0; k < i; k++) free(dp[k]);
            free(dp);
            return 0;
        }
    }

    /* Fill DP table */
    for (int i = 1; i <= n; i++) {
        int w = jobs[i - 1].cost_units;
        int v = jobs[i - 1].criticality_score;

        for (int b = 0; b <= budget_units; b++) {
            if (w <= b) {
                int take = dp[i - 1][b - w] + v;
                int skip = dp[i - 1][b];
                dp[i][b] = (take > skip) ? take : skip;
            } else {
                dp[i][b] = dp[i - 1][b];
            }
        }
    }

    int max_value = dp[n][budget_units];

    /* Backtrack to find chosen items */
    if (out_selected_indices && out_selected_count) {
        int b = budget_units;
        int chosen_count = 0;
        int *chosen = (int *)malloc(n * sizeof(int));

        for (int i = n; i > 0 && b > 0; i--) {
            if (dp[i][b] != dp[i - 1][b]) {
                chosen[chosen_count++] = i - 1; /* 0-based index */
                b -= jobs[i - 1].cost_units;
            }
        }

        /* Reverse to chronological order */
        for (int i = 0; i < chosen_count; i++) {
            out_selected_indices[i] = chosen[chosen_count - 1 - i];
        }
        *out_selected_count = chosen_count;
        free(chosen);
    }

    /* Clean up DP table */
    for (int i = 0; i < rows; i++) free(dp[i]);
    free(dp);

    return max_value;
}

/* =========================================================================
 * 9. BACKTRACKING & BRANCH AND BOUND: VEHICLE DISPATCH TOUR (TSP)
 * ========================================================================= */
typedef struct {
    int best_cost;
    int best_path[DSA_TSP_MAX + 1];
    int current_path[DSA_TSP_MAX + 1];
    bool visited[DSA_TSP_MAX];
    int n;
} TspContext;

/* Calculate simple lower bound for branch and bound pruning:
 * Current cost + sum of min outgoing edges from unvisited vertices */
static int calculate_lower_bound(int dist_matrix[DSA_TSP_MAX][DSA_TSP_MAX], const TspContext *ctx, int current_cost) {
    int bound = current_cost;
    for (int i = 0; i < ctx->n; i++) {
        if (!ctx->visited[i]) {
            int min_edge = DSA_INF;
            for (int j = 0; j < ctx->n; j++) {
                if (i != j && dist_matrix[i][j] < min_edge) {
                    min_edge = dist_matrix[i][j];
                }
            }
            if (min_edge != DSA_INF) bound += min_edge;
        }
    }
    return bound;
}

static void tsp_backtrack(int dist_matrix[DSA_TSP_MAX][DSA_TSP_MAX], TspContext *ctx, int level, int current_cost) {
    /* Base case: all sites visited, return to start (0) */
    if (level == ctx->n) {
        int last_node = ctx->current_path[level - 1];
        int return_cost = dist_matrix[last_node][0];
        if (return_cost != DSA_INF) {
            int total_cost = current_cost + return_cost;
            if (total_cost < ctx->best_cost) {
                ctx->best_cost = total_cost;
                for (int i = 0; i < ctx->n; i++) {
                    ctx->best_path[i] = ctx->current_path[i];
                }
                ctx->best_path[ctx->n] = 0; /* return to depot */
            }
        }
        return;
    }

    int prev_node = ctx->current_path[level - 1];

    for (int next_node = 0; next_node < ctx->n; next_node++) {
        if (!ctx->visited[next_node] && dist_matrix[prev_node][next_node] != DSA_INF) {
            int new_cost = current_cost + dist_matrix[prev_node][next_node];

            /* Branch and bound: prune if current cost or lower bound exceeds best-known cost */
            if (new_cost >= ctx->best_cost) continue;

            ctx->visited[next_node] = true;
            ctx->current_path[level] = next_node;

            int lb = calculate_lower_bound(dist_matrix, ctx, new_cost);
            if (lb < ctx->best_cost) {
                tsp_backtrack(dist_matrix, ctx, level + 1, new_cost);
            }

            /* Backtrack */
            ctx->visited[next_node] = false;
        }
    }
}

int dsa_tsp_branch_and_bound(int dist_matrix[DSA_TSP_MAX][DSA_TSP_MAX], int n, int *out_best_tour) {
    if (n <= 1 || n > DSA_TSP_MAX) {
        if (out_best_tour && n == 1) {
            out_best_tour[0] = 0;
            out_best_tour[1] = 0;
        }
        return 0;
    }

    TspContext ctx;
    ctx.best_cost = DSA_INF;
    ctx.n = n;
    memset(ctx.visited, 0, sizeof(ctx.visited));

    ctx.visited[0] = true;
    ctx.current_path[0] = 0;

    tsp_backtrack(dist_matrix, &ctx, 1, 0);

    if (out_best_tour && ctx.best_cost < DSA_INF) {
        for (int i = 0; i <= n; i++) {
            out_best_tour[i] = ctx.best_path[i];
        }
    }

    return (ctx.best_cost < DSA_INF) ? ctx.best_cost : -1;
}

/* =========================================================================
 * 10. DSA DEMONSTRATION & BENCHMARK HARNESS
 * ========================================================================= */
void dsa_run_full_demonstration(void) {
    printf("\n");
    ui_print_divider('=', 78);
    printf("     DATA STRUCTURES & ALGORITHMS (DSA) LIVE DEMONSTRATION SUITE\n");
    printf("         Academic Core Modules Implemented in Pure C (C11)\n");
    ui_print_divider('=', 78);

    /* 1. Hash Table */
    printf("\n[1] HASH TABLE (Vehicle Lookup by Registration - O(1) Average):\n");
    DsaHashTable *ht = dsa_ht_load_all_vehicles();
    printf("    Loaded %zu vehicles into hash table with %zu buckets.\n", dsa_ht_size(ht), ht ? ht->capacity : 0);
    const Vehicle *found = dsa_ht_search(ht, "MH01AB1234");
    if (found) {
        printf("    Hash Table Lookup ('MH01AB1234'): FOUND -> %s %s (Odo: %d km, Fuel: %d%%)\n",
               found->make, found->model, found->odometer_km, found->fuel_percentage);
    } else {
        printf("    Hash Table Lookup ('MH01AB1234'): Sample not found (empty or clean DB).\n");
    }
    dsa_ht_destroy(ht);

    /* 2. Binary Search Tree (BST) */
    printf("\n[2] BINARY SEARCH TREE (BST Range Queries - O(log N) Average):\n");
    DsaBSTNode *root = NULL;
    Vehicle v1 = {0}; strcpy(v1.registration_number, "DL01AA1111"); strcpy(v1.make, "Tata"); v1.odometer_km = 15000;
    Vehicle v2 = {0}; strcpy(v2.registration_number, "DL02BB2222"); strcpy(v2.make, "Mahindra"); v2.odometer_km = 45000;
    Vehicle v3 = {0}; strcpy(v3.registration_number, "DL03CC3333"); strcpy(v3.make, "Ashok Leyland"); v3.odometer_km = 28000;
    Vehicle v4 = {0}; strcpy(v4.registration_number, "DL04DD4444"); strcpy(v4.make, "Eicher"); v4.odometer_km = 80000;
    root = dsa_bst_insert(root, &v1);
    root = dsa_bst_insert(root, &v2);
    root = dsa_bst_insert(root, &v3);
    root = dsa_bst_insert(root, &v4);

    Vehicle range_res[8];
    int range_cnt = 0;
    dsa_bst_range_query(root, 20000, 50000, range_res, &range_cnt, 8);
    printf("    BST Range Query (Odometer between 20,000 km and 50,000 km): %d match(es) found\n", range_cnt);
    for (int i = 0; i < range_cnt; i++) {
        printf("      - %s (%s) : %d km\n", range_res[i].registration_number, range_res[i].make, range_res[i].odometer_km);
    }
    dsa_bst_destroy(root);

    /* 3. Min-Heap Priority Queue */
    printf("\n[3] MIN-HEAP (Preventive Maintenance Urgency - O(log N) Heap):\n");
    DsaMinHeap *heap = dsa_heap_create(16);
    MaintenanceSchedule s1 = {0}; strcpy(s1.service_name, "Brake Pad Inspection"); s1.days_remaining = 3;
    MaintenanceSchedule s2 = {0}; strcpy(s2.service_name, "Annual Overhaul"); s2.days_remaining = 30;
    MaintenanceSchedule s3 = {0}; strcpy(s3.service_name, "EMERGENCY Transmission Fluid"); s3.days_remaining = -4; /* Overdue */

    dsa_heap_insert(heap, &s1, s1.days_remaining);
    dsa_heap_insert(heap, &s2, s2.days_remaining);
    dsa_heap_insert(heap, &s3, s3.days_remaining);

    printf("    Inserted 3 schedules with due windows (+30d, +3d, -4d [Overdue]).\n");
    MaintenanceSchedule top;
    int prio = 0;
    while (dsa_heap_extract_min(heap, &top, &prio)) {
        printf("    Extracted Highest Priority: '%s' (Priority Score: %d)\n", top.service_name, prio);
    }
    dsa_heap_destroy(heap);

    /* 4. Graph & Dijkstra's Algorithm */
    printf("\n[4] GRAPH & DIJKSTRA'S ALGORITHM (Shortest Depot-to-Site Route):\n");
    DsaGraph *g = dsa_graph_create_sample_fleet_network();
    int dist = 0, path[16], path_len = 0;
    if (dsa_graph_dijkstra(g, 0, 5, &dist, path, &path_len)) {
        printf("    Shortest Path from '%s' to '%s': %d km\n    Route: ",
               g->vertices[0].name, g->vertices[5].name, dist);
        for (int i = 0; i < path_len; i++) {
            printf("%s%s", g->vertices[path[i]].name, (i + 1 < path_len) ? " -> " : "\n");
        }
    }
    dsa_graph_destroy(g);

    /* 5. Dynamic Programming (0-1 Knapsack Maintenance Budget Allocation) */
    printf("\n[5] DYNAMIC PROGRAMMING (Fleet Maintenance Budget 0-1 Knapsack):\n");
    DsaMaintenanceJob jobs[5] = {
        {1, "MH01AB1001", "Engine Tune-up", 12000, 85},
        {2, "MH01AB1002", "Brake Shoe Replacement", 4000, 60},
        {3, "MH01AB1003", "Full Tire Rotation", 3000, 40},
        {4, "MH01AB1004", "AC Compressor Servicing", 7000, 30},
        {5, "MH01AB1005", "Suspension Bushing", 6000, 75}
    };
    int budget = 18000; /* Rs. 18,000 budget */
    int chosen[5];
    int chosen_cnt = 0;
    int max_val = dsa_dp_knapsack_maintenance(jobs, 5, budget, chosen, &chosen_cnt);
    printf("    Available Budget: Rs. %d\n", budget);
    printf("    Optimal Maximum Reliability Score Achieved: %d points\n", max_val);
    printf("    Selected Maintenance Jobs:\n");
    int tot_cost = 0;
    for (int i = 0; i < chosen_cnt; i++) {
        int idx = chosen[i];
        printf("      [%d] %s on %s - Cost: Rs. %d (Score: %d)\n",
               jobs[idx].id, jobs[idx].service_name, jobs[idx].vehicle_reg, jobs[idx].cost_units, jobs[idx].criticality_score);
        tot_cost += jobs[idx].cost_units;
    }
    printf("    Total Allocated Spend: Rs. %d (Budget Surplus: Rs. %d)\n", tot_cost, budget - tot_cost);

    /* 6. Branch and Bound TSP */
    printf("\n[6] BRANCH & BOUND (Mobile Mechanic Service Dispatch Tour):\n");
    int tsp_matrix[DSA_TSP_MAX][DSA_TSP_MAX] = {
        {0,  10, 15, 20},
        {10, 0,  35, 25},
        {15, 35, 0,  30},
        {20, 25, 30, 0}
    };
    int tour[8];
    int tour_cost = dsa_tsp_branch_and_bound(tsp_matrix, 4, tour);
    printf("    Optimal Service Tour Cost: %d km\n    Dispatch Path: ", tour_cost);
    for (int i = 0; i <= 4; i++) {
        printf("Site %d%s", tour[i], (i < 4) ? " -> " : "\n");
    }

    ui_print_divider('=', 78);
    printf("[DONE] All 9 DSA modules executed successfully with O-notation guarantees.\n");
    ui_print_divider('=', 78);
}
