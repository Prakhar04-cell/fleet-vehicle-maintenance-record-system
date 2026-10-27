-- Seed Sample Data for Fleet Vehicle Maintenance Record System
-- Separated development/demo dataset

PRAGMA foreign_keys = ON;

-- Sample Drivers
INSERT OR IGNORE INTO drivers (id, name, phone, license_number, license_expiry, active, created_at, updated_at)
VALUES 
  (1, 'Ramesh Verma', '+91-9876500001', 'DL-MH-2018-10001', '2028-11-30', 1, datetime('now'), datetime('now')),
  (2, 'Sunil Yadav', '+91-9876500002', 'DL-MH-2020-20002', '2030-06-15', 1, datetime('now'), datetime('now')),
  (3, 'Karan Grover', '+91-9876500003', 'DL-MH-2021-30003', '2031-01-20', 1, datetime('now'), datetime('now'));

-- Sample Vehicles
INSERT OR IGNORE INTO vehicles (id, registration_number, make, model, manufacturing_year, vin, fuel_type, fuel_percentage, odometer_km, status, assigned_driver_id, insurance_expiry, notes, created_at, updated_at, archived_at)
VALUES
  (1, 'MH-04-AZ-4521', 'Tata', 'Ace Gold Petrol Cx', 2022, 'MAT612001A88901', 'Petrol', 75, 34200, 'Available', 1, '2027-03-31', 'Primary inter-city logistics van', datetime('now'), datetime('now'), NULL),
  (2, 'MH-12-BQ-8934', 'Mahindra', 'Bolero Maxi Truck Plus', 2023, 'MA1612002B77802', 'Diesel', 18, 48500, 'On Trip', 2, '2027-07-15', 'Equipped with heavy load suspension', datetime('now'), datetime('now'), NULL),
  (3, 'MH-02-CP-1145', 'BharatBenz', '1217R Medium Duty Truck', 2021, 'MBB712003C66703', 'Diesel', 90, 89200, 'Under Maintenance', 3, '2026-12-10', 'Under scheduled axle and brake overhaul', datetime('now'), datetime('now'), NULL),
  (4, 'MH-03-DR-9022', 'Ashok Leyland', 'Dost Strong', 2023, 'MAL812004D55604', 'CNG', 15, 21300, 'Available', NULL, '2027-09-25', 'Spare urban delivery vehicle', datetime('now'), datetime('now'), NULL);

-- Sample Maintenance Records
INSERT OR IGNORE INTO maintenance_records (id, vehicle_id, service_type, service_date, odometer_km, description, workshop_name, mechanic_name, labour_cost, parts_cost, tax_amount, total_cost, next_service_date, next_service_odometer_km, invoice_doc_path, created_by, created_at, updated_at, archived_at)
VALUES
  (1, 1, 'Preventive Maintenance', '2026-08-15', 30000, '30,000 KM Periodic Service: Mobil Delvac Engine Oil, Oil Filter, Fuel Filter Replacement', 'Tata Authorized Service Center, Thane', 'Suresh Shinde', 180000, 350000, 95400, 625400, '2027-02-15', 40000, 'docs/invoices/inv_001.pdf', NULL, datetime('now'), datetime('now'), NULL),
  (2, 2, 'Brake Service', '2026-09-10', 45000, 'Front brake pads replacement, rear drum inspection and brake fluid bleeding', 'Mahindra First Choice Workshop', 'Vinod Patil', 120000, 280000, 72000, 472000, '2027-03-10', 55000, 'docs/invoices/inv_002.pdf', NULL, datetime('now'), datetime('now'), NULL),
  (3, 3, 'Major Overhaul', '2026-10-20', 89000, 'Clutch plate and pressure plate overhaul, flywheel resurfacing, transmission fluid change', 'BharatBenz Commercial Care', 'Pradeep K', 450000, 1250000, 306000, 2006000, '2027-04-20', 105000, 'docs/invoices/inv_003.pdf', NULL, datetime('now'), datetime('now'), NULL);

-- Sample Preventive Maintenance Schedules
INSERT OR IGNORE INTO maintenance_schedules (id, vehicle_id, service_name, description, due_date, due_odometer_km, reminder_days_before, status, recurring_interval_days, recurring_interval_km, last_notified_at, created_at, updated_at)
VALUES
  (1, 1, 'Oil & Filter Replacement', 'Engine oil, oil filter, air filter cleaning', '2026-11-05', 40000, 7, 'Due', 180, 10000, NULL, datetime('now'), datetime('now')),
  (2, 2, '50,000 KM Major Inspection', 'Differential oil check, injector cleaning, coolant flush', '2026-10-20', 50000, 7, 'Overdue', 365, 20000, NULL, datetime('now'), datetime('now')),
  (3, 4, 'CNG Kit Leakage & Pressure Inspection', 'Mandatory regulatory CNG cylinder hydrostatic compliance', '2026-11-25', 25000, 10, 'Scheduled', 365, 0, NULL, datetime('now'), datetime('now'));

-- Sample Expenses
INSERT OR IGNORE INTO expenses (id, vehicle_id, maintenance_record_id, category, description, amount, currency, expense_date, vendor, created_by, created_at)
VALUES
  (1, 1, 1, 'Maintenance', '30,000 KM Periodic Service', 625400, 'INR', '2026-08-15', 'Tata Authorized Service Center', NULL, datetime('now')),
  (2, 2, 2, 'Maintenance', 'Front brake pads replacement & bleeding', 472000, 'INR', '2026-09-10', 'Mahindra First Choice Workshop', NULL, datetime('now')),
  (3, 3, 3, 'Maintenance', 'Clutch plate & transmission overhaul', 2006000, 'INR', '2026-10-20', 'BharatBenz Commercial Care', NULL, datetime('now')),
  (4, 1, NULL, 'Fuel', 'Petrol Tank Full (35 Litres)', 367500, 'INR', '2026-10-18', 'HPCL Retail', NULL, datetime('now')),
  (5, 2, NULL, 'Fuel', 'Diesel Full Tank (45 Litres)', 414000, 'INR', '2026-10-22', 'BPCL Highway Station', NULL, datetime('now')),
  (6, 2, NULL, 'Toll/Tax', 'State Border Commercial Permit & FASTag', 250000, 'INR', '2026-10-24', 'NHAI FASTag', NULL, datetime('now')),
  (7, 3, NULL, 'Insurance', 'Annual Commercial Vehicle Comprehensive Insurance', 3250000, 'INR', '2026-09-01', 'ICICI Lombard GIC', NULL, datetime('now'));

-- Sample Audit Logs
INSERT OR IGNORE INTO audit_logs (id, user_id, action, entity_type, entity_id, timestamp, details)
VALUES
  (1, NULL, 'SYSTEM_INIT', 'system', 0, datetime('now', '-30 days'), 'Initial system installation and schema provisioning'),
  (2, NULL, 'VEHICLE_CREATE', 'vehicles', 1, datetime('now', '-25 days'), 'Added vehicle MH-04-AZ-4521 (Tata Ace Gold)'),
  (3, NULL, 'MAINTENANCE_CREATE', 'maintenance_records', 1, datetime('now', '-20 days'), 'Recorded periodic service for MH-04-AZ-4521');
