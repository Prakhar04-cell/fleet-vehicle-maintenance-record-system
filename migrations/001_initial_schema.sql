-- Migration: 001_initial_schema.sql
-- Fleet Vehicle Maintenance Record System Schema
-- SQLite 3 compatible

PRAGMA foreign_keys = ON;

-- Migrations tracking table
CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    applied_at TEXT NOT NULL
);

-- Users & Authentication
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL COLLATE NOCASE,
    password_hash TEXT NOT NULL,
    password_salt TEXT NOT NULL,
    password_algorithm TEXT NOT NULL DEFAULT 'PBKDF2-HMAC-SHA256',
    iterations INTEGER NOT NULL DEFAULT 100000,
    role TEXT NOT NULL CHECK(role IN ('admin', 'manager', 'staff', 'viewer')),
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0, 1)),
    failed_attempts INTEGER NOT NULL DEFAULT 0 CHECK(failed_attempts >= 0),
    locked_until TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

-- Drivers
CREATE TABLE IF NOT EXISTS drivers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    phone TEXT NOT NULL,
    license_number TEXT UNIQUE NOT NULL,
    license_expiry TEXT NOT NULL,
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0, 1)),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

-- Vehicles
CREATE TABLE IF NOT EXISTS vehicles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    registration_number TEXT UNIQUE NOT NULL COLLATE NOCASE,
    make TEXT NOT NULL,
    model TEXT NOT NULL,
    manufacturing_year INTEGER NOT NULL CHECK(manufacturing_year >= 1900 AND manufacturing_year <= 2100),
    vin TEXT UNIQUE NOT NULL COLLATE NOCASE,
    fuel_type TEXT NOT NULL CHECK(fuel_type IN ('Petrol', 'Diesel', 'Electric', 'Hybrid', 'CNG', 'Other')),
    fuel_percentage INTEGER NOT NULL DEFAULT 100 CHECK(fuel_percentage >= 0 AND fuel_percentage <= 100),
    odometer_km INTEGER NOT NULL DEFAULT 0 CHECK(odometer_km >= 0),
    status TEXT NOT NULL DEFAULT 'Available' CHECK(status IN ('Available', 'On Trip', 'Under Maintenance', 'Out of Service', 'Decommissioned')),
    assigned_driver_id INTEGER REFERENCES drivers(id) ON DELETE SET NULL,
    insurance_expiry TEXT,
    notes TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    archived_at TEXT
);

-- Maintenance & Repair Records
CREATE TABLE IF NOT EXISTS maintenance_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
    service_type TEXT NOT NULL,
    service_date TEXT NOT NULL,
    odometer_km INTEGER NOT NULL CHECK(odometer_km >= 0),
    description TEXT NOT NULL,
    workshop_name TEXT NOT NULL,
    mechanic_name TEXT,
    labour_cost INTEGER NOT NULL DEFAULT 0 CHECK(labour_cost >= 0),
    parts_cost INTEGER NOT NULL DEFAULT 0 CHECK(parts_cost >= 0),
    tax_amount INTEGER NOT NULL DEFAULT 0 CHECK(tax_amount >= 0),
    total_cost INTEGER NOT NULL CHECK(total_cost >= 0),
    next_service_date TEXT,
    next_service_odometer_km INTEGER CHECK(next_service_odometer_km IS NULL OR next_service_odometer_km >= odometer_km),
    invoice_doc_path TEXT,
    created_by INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    archived_at TEXT
);

-- Preventive Maintenance Schedules
CREATE TABLE IF NOT EXISTS maintenance_schedules (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE CASCADE,
    service_name TEXT NOT NULL,
    description TEXT,
    due_date TEXT,
    due_odometer_km INTEGER CHECK(due_odometer_km IS NULL OR due_odometer_km >= 0),
    reminder_days_before INTEGER NOT NULL DEFAULT 7 CHECK(reminder_days_before >= 0),
    status TEXT NOT NULL DEFAULT 'Scheduled' CHECK(status IN ('Scheduled', 'Due', 'Overdue', 'Completed', 'Cancelled')),
    recurring_interval_days INTEGER DEFAULT 0 CHECK(recurring_interval_days >= 0),
    recurring_interval_km INTEGER DEFAULT 0 CHECK(recurring_interval_km >= 0),
    last_notified_at TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

-- Expenses
CREATE TABLE IF NOT EXISTS expenses (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    vehicle_id INTEGER NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
    maintenance_record_id INTEGER REFERENCES maintenance_records(id) ON DELETE SET NULL,
    category TEXT NOT NULL CHECK(category IN ('Fuel', 'Maintenance', 'Insurance', 'Toll/Tax', 'Permit', 'Tires', 'Parts', 'Miscellaneous')),
    description TEXT NOT NULL,
    amount INTEGER NOT NULL CHECK(amount >= 0),
    currency TEXT NOT NULL DEFAULT 'INR',
    expense_date TEXT NOT NULL,
    vendor TEXT,
    created_by INTEGER REFERENCES users(id),
    created_at TEXT NOT NULL
);

-- In-App Notifications
CREATE TABLE IF NOT EXISTS notifications (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    schedule_id INTEGER NOT NULL REFERENCES maintenance_schedules(id) ON DELETE CASCADE,
    notification_type TEXT NOT NULL CHECK(notification_type IN ('Upcoming Date', 'Upcoming Odometer', 'Overdue Date', 'Overdue Odometer')),
    status TEXT NOT NULL DEFAULT 'Active' CHECK(status IN ('Active', 'Dismissed', 'In-App')),
    scheduled_at TEXT NOT NULL,
    sent_at TEXT NOT NULL,
    failure_reason TEXT,
    created_at TEXT NOT NULL,
    UNIQUE(schedule_id, notification_type, scheduled_at)
);

-- Audit Logs
CREATE TABLE IF NOT EXISTS audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER REFERENCES users(id) ON DELETE SET NULL,
    action TEXT NOT NULL,
    entity_type TEXT NOT NULL,
    entity_id INTEGER,
    timestamp TEXT NOT NULL,
    details TEXT
);

-- Indexes for performance
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_vehicles_registration ON vehicles(registration_number);
CREATE INDEX IF NOT EXISTS idx_vehicles_status ON vehicles(status);
CREATE INDEX IF NOT EXISTS idx_vehicles_assigned_driver ON vehicles(assigned_driver_id);
CREATE INDEX IF NOT EXISTS idx_maintenance_vehicle_id ON maintenance_records(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_maintenance_service_date ON maintenance_records(service_date);
CREATE INDEX IF NOT EXISTS idx_schedules_vehicle_id ON maintenance_schedules(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_schedules_status ON maintenance_schedules(status);
CREATE INDEX IF NOT EXISTS idx_schedules_due_date ON maintenance_schedules(due_date);
CREATE INDEX IF NOT EXISTS idx_expenses_vehicle_id ON expenses(vehicle_id);
CREATE INDEX IF NOT EXISTS idx_expenses_category ON expenses(category);
CREATE INDEX IF NOT EXISTS idx_expenses_date ON expenses(expense_date);
CREATE INDEX IF NOT EXISTS idx_expenses_maintenance_id ON expenses(maintenance_record_id);
CREATE INDEX IF NOT EXISTS idx_notifications_schedule ON notifications(schedule_id);
CREATE INDEX IF NOT EXISTS idx_audit_logs_timestamp ON audit_logs(timestamp);
CREATE INDEX IF NOT EXISTS idx_audit_logs_entity ON audit_logs(entity_type, entity_id);

-- Record migration 1
INSERT OR IGNORE INTO schema_migrations (version, name, applied_at)
VALUES (1, '001_initial_schema', datetime('now'));
