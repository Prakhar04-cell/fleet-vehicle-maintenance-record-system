-- Migration: 003_live_monitoring_and_dsa.sql
-- Live Monitoring, Persistent Deduplicated Notification Center, and Operational Workflows
-- SQLite 3 compatible

PRAGMA foreign_keys = ON;

-- System Notifications Table with Deduplication Keys
CREATE TABLE IF NOT EXISTS system_notifications (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    event_key TEXT UNIQUE NOT NULL, -- Deterministic deduplication key
    category TEXT NOT NULL CHECK(category IN ('Maintenance', 'Defect', 'WorkOrder', 'System')),
    severity TEXT NOT NULL DEFAULT 'Info' CHECK(severity IN ('Info', 'Warning', 'Critical')),
    title TEXT NOT NULL,
    message TEXT NOT NULL,
    entity_type TEXT NOT NULL, -- 'vehicle', 'schedule', 'inspection', 'work_order'
    entity_id INTEGER,
    role_visibility TEXT NOT NULL DEFAULT 'All' CHECK(role_visibility IN ('All', 'Admin', 'Manager', 'Staff')),
    is_read INTEGER NOT NULL DEFAULT 0 CHECK(is_read IN (0, 1)),
    acknowledged_by INTEGER REFERENCES users(id) ON DELETE SET NULL,
    acknowledged_at TEXT,
    created_at TEXT NOT NULL
);

-- Performance Indexes
CREATE INDEX IF NOT EXISTS idx_sys_notif_key ON system_notifications(event_key);
CREATE INDEX IF NOT EXISTS idx_sys_notif_read ON system_notifications(is_read);
CREATE INDEX IF NOT EXISTS idx_sys_notif_cat ON system_notifications(category);
CREATE INDEX IF NOT EXISTS idx_sys_notif_sev ON system_notifications(severity);
CREATE INDEX IF NOT EXISTS idx_sys_notif_role ON system_notifications(role_visibility);
CREATE INDEX IF NOT EXISTS idx_sys_notif_created ON system_notifications(created_at);

-- Record migration 3
INSERT OR IGNORE INTO schema_migrations (version, name, applied_at)
VALUES (3, '003_live_monitoring_and_dsa', datetime('now'));
