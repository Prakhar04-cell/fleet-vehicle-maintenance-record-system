-- Migration: 002_smart_fleet_management.sql
-- Smart Preventive Maintenance, Work Orders, and Vehicle Inspections
-- SQLite 3 compatible

PRAGMA foreign_keys = ON;

-- Work Orders Table
CREATE TABLE IF NOT EXISTS work_orders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    wo_number TEXT UNIQUE NOT NULL COLLATE NOCASE,
    vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
    schedule_id INTEGER REFERENCES maintenance_schedules(id) ON DELETE SET NULL,
    inspection_id INTEGER,
    title TEXT NOT NULL,
    description TEXT,
    priority TEXT NOT NULL DEFAULT 'Medium' CHECK(priority IN ('Low', 'Medium', 'High', 'Critical')),
    status TEXT NOT NULL DEFAULT 'Open' CHECK(status IN ('Open', 'Assigned', 'In Progress', 'Waiting for Parts', 'Completed', 'Cancelled')),
    assigned_to_user_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    assigned_to_name TEXT,
    odometer_km INTEGER CHECK(odometer_km IS NULL OR odometer_km >= 0),
    start_time TEXT,
    completion_time TEXT,
    labour_cost INTEGER NOT NULL DEFAULT 0 CHECK(labour_cost >= 0),
    parts_cost INTEGER NOT NULL DEFAULT 0 CHECK(parts_cost >= 0),
    total_cost INTEGER NOT NULL DEFAULT 0 CHECK(total_cost >= 0),
    maintenance_record_id INTEGER REFERENCES maintenance_records(id) ON DELETE SET NULL,
    notes TEXT,
    created_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

-- Work Order Status & Action History
CREATE TABLE IF NOT EXISTS work_order_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    work_order_id INTEGER NOT NULL REFERENCES work_orders(id) ON DELETE CASCADE,
    from_status TEXT NOT NULL,
    to_status TEXT NOT NULL,
    changed_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
    changed_by_name TEXT,
    notes TEXT,
    timestamp TEXT NOT NULL
);

-- Vehicle Inspections Table
CREATE TABLE IF NOT EXISTS inspections (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    inspection_number TEXT UNIQUE NOT NULL COLLATE NOCASE,
    vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
    inspector_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    inspector_name TEXT NOT NULL,
    inspection_date TEXT NOT NULL,
    odometer_km INTEGER NOT NULL CHECK(odometer_km >= 0),
    overall_status TEXT NOT NULL DEFAULT 'Pass' CHECK(overall_status IN ('Pass', 'Defective', 'Fail')),
    defect_count INTEGER NOT NULL DEFAULT 0 CHECK(defect_count >= 0),
    notes TEXT,
    created_at TEXT NOT NULL
);

-- Inspection Checklist Items Table
CREATE TABLE IF NOT EXISTS inspection_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    inspection_id INTEGER NOT NULL REFERENCES inspections(id) ON DELETE CASCADE,
    item_name TEXT NOT NULL,
    status TEXT NOT NULL CHECK(status IN ('Pass', 'Fail', 'N/A')),
    severity TEXT NOT NULL DEFAULT 'Normal' CHECK(severity IN ('Normal', 'Minor Defect', 'Critical Defect')),
    notes TEXT,
    work_order_id INTEGER REFERENCES work_orders(id) ON DELETE SET NULL
);

-- Performance Indexes
CREATE INDEX IF NOT EXISTS idx_wo_vehicle ON work_orders(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_wo_status ON work_orders(status);
CREATE INDEX IF NOT EXISTS idx_wo_priority ON work_orders(priority);
CREATE INDEX IF NOT EXISTS idx_wo_assigned ON work_orders(assigned_to_user_id);
CREATE INDEX IF NOT EXISTS idx_wo_schedule ON work_orders(schedule_id);
CREATE INDEX IF NOT EXISTS idx_wo_history_wo ON work_order_history(work_order_id);
CREATE INDEX IF NOT EXISTS idx_inspections_vehicle ON inspections(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_inspections_status ON inspections(overall_status);
CREATE INDEX IF NOT EXISTS idx_insp_items_inspection ON inspection_items(inspection_id);
CREATE INDEX IF NOT EXISTS idx_insp_items_status ON inspection_items(status);
CREATE INDEX IF NOT EXISTS idx_insp_items_wo ON inspection_items(work_order_id);

-- Record migration 2
INSERT OR IGNORE INTO schema_migrations (version, name, applied_at)
VALUES (2, '002_smart_fleet_management', datetime('now'));
