#ifndef INSPECTION_H
#define INSPECTION_H

#include "common.h"

typedef struct {
    int id;
    int inspection_id;
    char item_name[MAX_BUFFER_MD];
    char status[MAX_BUFFER_SM];   /* Pass, Fail, N/A */
    char severity[MAX_BUFFER_SM]; /* Normal, Minor Defect, Critical Defect */
    char notes[MAX_BUFFER_LG];
    int work_order_id;
} InspectionItem;

typedef struct {
    int id;
    char inspection_number[MAX_BUFFER_SM];
    int vehicle_id;
    char vehicle_reg[MAX_BUFFER_SM];
    int inspector_id;
    char inspector_name[MAX_BUFFER_SM];
    char inspection_date[MAX_BUFFER_SM];
    int odometer_km;
    char overall_status[MAX_BUFFER_SM]; /* Pass, Defective, Fail */
    int defect_count;
    char notes[MAX_BUFFER_LG];
    char created_at[MAX_BUFFER_SM];
} Inspection;

typedef struct {
    int vehicle_id;
    char overall_status[MAX_BUFFER_SM];
} InspectionFilter;

StatusCode inspection_create(Inspection *insp, const InspectionItem *items, int item_count, int user_id);
StatusCode inspection_get_by_id(int id, Inspection *out_insp, InspectionItem **out_items, int *out_item_count);
StatusCode inspection_list(const InspectionFilter *filter, Inspection **out_list, int *out_count);
StatusCode inspection_get_defects(int vehicle_id, InspectionItem **out_defects, int *out_count);
StatusCode inspection_create_work_order_from_defect(int inspection_item_id, int user_id, const char *user_name, int *out_work_order_id);
void inspection_free_list(Inspection *list);
void inspection_free_items(InspectionItem *items);

#endif /* INSPECTION_H */
