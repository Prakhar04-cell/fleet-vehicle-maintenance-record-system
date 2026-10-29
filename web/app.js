// ==========================================================================
// FLEET VEHICLE MAINTENANCE RECORD SYSTEM — WEB CLIENT CONTROLLER
// Architecture: Pure Vanilla JavaScript, Zero External Dependencies
// Backend: C11 HTTP/1.1 Winsock2 Server + SQLite Persistent Engine
// ==========================================================================

const state = {
    // RBAC Authentication & Session State
    currentUser: null,
    sessionToken: sessionStorage.getItem('fleet_session_token') || null,
    currentRole: 'Admin',

    dashboard: null,
    vehicles: [],
    drivers: [],
    maintenance: [],
    schedules: [],
    expenses: [],
    notifications: [],
    audit: [],
    activeVehicle: null,
    
    // Smart Fleet Management State
    workOrders: [],
    inspections: [],
    defects: [],
    analytics: null,
    activeWorkOrder: null,
    activeInspection: null,

    // Live Monitoring & Demonstration State
    isLivePolling: false,
    pollIntervalMs: 10000,
    pollTimerId: null,
    lastSuccessfulSync: null,
    isDemoMode: false,
    demoDatabasePath: '',
    notifFilter: 'all',
    
    // Vehicles table state
    sortField: 'reg',
    sortAsc: true,
    currentPage: 1,
    pageSize: 10,
    searchTerm: '',
    statusFilter: '',
    fuelFilter: '',
    
    // Column visibility
    visibleCols: {
        make: true,
        year: true,
        fuel: true,
        odometer: true,
        driver: true,
        insurance: true,
        status: true
    }
};

document.addEventListener('DOMContentLoaded', async () => {
    initNavigation();
    initGlobalShortcuts();
    initColumnPicker();
    initModals();
    initForms();
    initTableEvents();
    
    // Server-verified session initialization and RBAC policy application
    await initSessionAndApp();

    // Live Telemetry, Notifications, Demo Mode, and DSA Integration
    initLiveMonitoring();
    checkDemoStatus();
    initNotificationCenter();
    initDsaOperations();
});

// ==========================================================================
// 1. Navigation & UI Controls
// ==========================================================================
function initNavigation() {
    // Sidebar Navigation Buttons
    const navButtons = document.querySelectorAll('.nav-item[data-tab]');
    navButtons.forEach(btn => {
        btn.addEventListener('click', () => {
            const tabId = btn.getAttribute('data-tab');
            switchTab(tabId);
        });
    });

    // Links with data-tab-trigger
    document.querySelectorAll('[data-tab-trigger]').forEach(el => {
        el.addEventListener('click', (e) => {
            e.preventDefault();
            const target = el.getAttribute('data-tab-trigger');
            switchTab(target);
        });
    });

    // Sidebar Collapse Toggle
    const toggleBtn = document.getElementById('btn-toggle-sidebar');
    const sidebar = document.getElementById('app-sidebar');
    if (toggleBtn && sidebar) {
        toggleBtn.addEventListener('click', () => {
            sidebar.classList.toggle('collapsed');
        });
    }

    // Mobile Sidebar Toggle
    const mobileBtn = document.getElementById('btn-mobile-menu');
    if (mobileBtn && sidebar) {
        mobileBtn.addEventListener('click', () => {
            sidebar.classList.toggle('mobile-open');
        });
    }

    // Refresh Data Button
    document.getElementById('btn-refresh-data')?.addEventListener('click', () => {
        loadAllData();
        showAlert('Data refreshed from SQLite database.', 'success');
    });

    // Evaluate Thresholds Quick Button
    document.getElementById('btn-evaluate-thresholds')?.addEventListener('click', evaluateThresholds);
    document.getElementById('btn-reminders-evaluate')?.addEventListener('click', evaluateThresholds);

    // Quick Hot Backup Button
    document.getElementById('btn-quick-backup')?.addEventListener('click', triggerHotBackup);
    document.getElementById('btn-trigger-backup-tab')?.addEventListener('click', triggerHotBackup);

    // Notification Flyout Toggle
    const notifBtn = document.getElementById('btn-notifications-toggle');
    const notifFlyout = document.getElementById('notification-flyout');
    if (notifBtn && notifFlyout) {
        notifBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            notifFlyout.classList.toggle('show');
        });
        document.addEventListener('click', (e) => {
            if (!notifFlyout.contains(e.target) && e.target !== notifBtn) {
                notifFlyout.classList.remove('show');
            }
        });
    }

    // DSA Run Button
    document.getElementById('btn-run-dsa-benchmarks')?.addEventListener('click', runDsaBenchmarks);

    // Audit Refresh
    document.getElementById('btn-refresh-audit')?.addEventListener('click', loadAudit);

    // Drawer Tabs
    document.querySelectorAll('.drawer-tab-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const target = btn.getAttribute('data-drawer-tab');
            document.querySelectorAll('.drawer-tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.drawer-tab-content').forEach(c => c.classList.remove('active'));
            btn.classList.add('active');
            document.getElementById(`drawer-content-${target}`)?.classList.add('active');
        });
    });

    // Drawer Close
    document.getElementById('btn-close-drawer')?.addEventListener('click', closeVehicleDrawer);
    document.getElementById('vehicle-drawer-backdrop')?.addEventListener('click', (e) => {
        if (e.target.id === 'vehicle-drawer-backdrop') closeVehicleDrawer();
    });

    // Quick Actions Bar Buttons
    document.getElementById('btn-quick-add-vehicle')?.addEventListener('click', () => openModal('modal-vehicle'));
    document.getElementById('btn-quick-log-service')?.addEventListener('click', () => openLogMaintenanceModal());
    document.getElementById('btn-quick-add-expense')?.addEventListener('click', () => openRecordExpenseModal());
    document.getElementById('btn-quick-add-schedule')?.addEventListener('click', () => openCreateScheduleModal());
    document.getElementById('btn-quick-export')?.addEventListener('click', exportVehiclesCSV);

    // Direct Backend C11 Reports & Exports (CSV and PDF) with Bearer token authentication query param
    document.getElementById('btn-rep-fleet-pdf')?.addEventListener('click', () => {
        showAlert('Generating C11 PDF Fleet Executive Summary...', 'info');
        window.open(`/api/reports/pdf?type=fleet&token=${encodeURIComponent(state.sessionToken || '')}`, '_blank');
    });
    document.getElementById('btn-rep-expenses-pdf')?.addEventListener('click', () => {
        showAlert('Generating C11 PDF Operating Expenses Ledger...', 'info');
        window.open(`/api/reports/pdf?type=expenses&token=${encodeURIComponent(state.sessionToken || '')}`, '_blank');
    });
    document.getElementById('btn-rep-maint-pdf')?.addEventListener('click', () => {
        const vid = state.vehicles[0]?.id || 1;
        showAlert(`Generating C11 PDF Maintenance Dossier for Vehicle #${vid}...`, 'info');
        window.open(`/api/reports/pdf?type=maintenance&vehicle_id=${vid}&token=${encodeURIComponent(state.sessionToken || '')}`, '_blank');
    });
    document.getElementById('btn-rep-vehicles')?.addEventListener('click', () => {
        showAlert('Downloading Vehicles Register CSV...', 'info');
        window.location.href = `/api/reports/csv?type=vehicles&token=${encodeURIComponent(state.sessionToken || '')}`;
    });
    document.getElementById('btn-rep-maint')?.addEventListener('click', () => {
        showAlert('Downloading Service & Maintenance CSV...', 'info');
        window.location.href = `/api/reports/csv?type=maintenance&token=${encodeURIComponent(state.sessionToken || '')}`;
    });
    document.getElementById('btn-rep-expenses')?.addEventListener('click', () => {
        showAlert('Downloading Operating Expenditures CSV...', 'info');
        window.location.href = `/api/reports/csv?type=expenses&token=${encodeURIComponent(state.sessionToken || '')}`;
    });
    document.getElementById('btn-rep-schedules')?.addEventListener('click', () => {
        showAlert('Downloading Preventive Milestones CSV...', 'info');
        window.location.href = `/api/reports/csv?type=schedules&token=${encodeURIComponent(state.sessionToken || '')}`;
    });
    document.getElementById('btn-rep-spending-csv')?.addEventListener('click', () => {
        showAlert('Downloading Monthly Spending CSV...', 'info');
        window.location.href = `/api/reports/csv?type=spending&token=${encodeURIComponent(state.sessionToken || '')}`;
    });
    document.getElementById('btn-rep-cost-csv')?.addEventListener('click', () => {
        showAlert('Downloading Lifetime Vehicle Costs CSV...', 'info');
        window.location.href = `/api/reports/csv?type=vehicle_costs&token=${encodeURIComponent(state.sessionToken || '')}`;
    });
    document.getElementById('btn-export-vehicles-csv')?.addEventListener('click', exportVehiclesCSV);
    document.getElementById('btn-export-maint-csv')?.addEventListener('click', exportMaintenanceCSV);
    document.getElementById('btn-export-expenses-csv')?.addEventListener('click', exportExpensesCSV);

    // Fleet Manager Dashboard Quick Action Buttons
    document.getElementById('btn-mgr-add-vehicle')?.addEventListener('click', () => openModal('modal-vehicle'));
    document.getElementById('btn-mgr-log-service')?.addEventListener('click', () => openLogMaintenanceModal());
    document.getElementById('btn-mgr-add-schedule')?.addEventListener('click', () => openCreateScheduleModal());
    document.getElementById('btn-mgr-add-expense')?.addEventListener('click', () => openRecordExpenseModal());
    document.getElementById('btn-mgr-export')?.addEventListener('click', exportVehiclesCSV);

    // Maintenance Staff Dashboard Quick Action Buttons
    document.getElementById('btn-staff-quick-service')?.addEventListener('click', () => openLogMaintenanceModal());
    document.getElementById('btn-staff-quick-schedule')?.addEventListener('click', () => openCreateScheduleModal());

    // Work Orders Toolbar & Modals
    document.getElementById('btn-refresh-work-orders')?.addEventListener('click', () => {
        loadWorkOrders();
        showAlert('Work orders refreshed from SQLite.', 'info');
    });
    document.getElementById('btn-add-wo-modal')?.addEventListener('click', openCreateWorkOrderModal);
    document.getElementById('wo-search-input')?.addEventListener('input', renderWorkOrdersTable);
    document.getElementById('wo-status-filter')?.addEventListener('change', renderWorkOrdersTable);
    document.getElementById('wo-priority-filter')?.addEventListener('change', renderWorkOrdersTable);

    // Digital Inspections Toolbar & Modals
    document.getElementById('btn-refresh-inspections')?.addEventListener('click', () => {
        loadInspections();
        loadDefects();
        showAlert('Inspections and defect register refreshed.', 'info');
    });
    document.getElementById('btn-add-inspection-modal')?.addEventListener('click', openCreateInspectionModal);
    document.getElementById('insp-status-filter')?.addEventListener('change', renderInspectionsTable);

    // Fleet Cost Analytics
    document.getElementById('btn-refresh-analytics')?.addEventListener('click', () => {
        loadFleetAnalytics();
        showAlert('Fleet analytics recalculated from SQL tables.', 'success');
    });

    // Work order completion cancel button
    document.getElementById('btn-cancel-wo-completion')?.addEventListener('click', () => {
        document.getElementById('wo-completion-panel')?.classList.add('hidden');
    });

    // Authentication & Role Switcher
    document.getElementById('user-profile-btn')?.addEventListener('click', () => openModal('modal-auth-login'));
    document.getElementById('btn-auth-logout')?.addEventListener('click', handleLogout);
    document.getElementById('btn-fill-admin')?.addEventListener('click', () => {
        document.getElementById('login-username').value = 'admin';
        document.getElementById('login-password').value = 'Admin@123';
    });
    document.getElementById('btn-fill-manager')?.addEventListener('click', () => {
        document.getElementById('login-username').value = 'manager';
        document.getElementById('login-password').value = 'Manager@123';
    });
    document.getElementById('btn-fill-staff')?.addEventListener('click', () => {
        document.getElementById('login-username').value = 'staff';
        document.getElementById('login-password').value = 'Staff@123';
    });
    document.getElementById('form-auth-login')?.addEventListener('submit', handleLoginSubmit);
}

function switchTab(tabId) {
    const navButtons = document.querySelectorAll('.nav-item');
    const tabPanes = document.querySelectorAll('.tab-pane');

    navButtons.forEach(btn => {
        btn.classList.toggle('active', btn.getAttribute('data-tab') === tabId);
    });

    tabPanes.forEach(pane => {
        pane.classList.toggle('active', pane.id === `tab-${tabId}`);
    });

    // Update Top Breadcrumb
    const breadcrumbTitles = {
        dashboard: 'Dashboard Overview',
        vehicles: 'Vehicles Register',
        drivers: 'Commercial Drivers',
        maintenance: 'Service History',
        schedules: 'Preventive Schedules',
        'work-orders': 'Work Orders Management',
        inspections: 'Digital Vehicle Inspections',
        reminders: 'Maintenance Reminders',
        expenses: 'Operating Expenses',
        analytics: 'Fleet Cost Analytics',
        reports: 'Reports & Analytics',
        dsa: 'DSA Optimization Suite',
        audit: 'System Audit Trail',
        backup: 'Disaster Recovery'
    };

    const breadcrumb = document.getElementById('breadcrumb-current');
    if (breadcrumb && breadcrumbTitles[tabId]) {
        breadcrumb.textContent = breadcrumbTitles[tabId];
    }

    // Lazy load specific tab data
    if (tabId === 'audit') loadAudit();
    if (tabId === 'work-orders') loadWorkOrders();
    if (tabId === 'inspections') { loadInspections(); loadDefects(); }
    if (tabId === 'analytics') loadFleetAnalytics();
    if (tabId === 'dsa') runDsaBenchmarks();
}

function initGlobalShortcuts() {
    const searchInput = document.getElementById('global-search-input');
    document.addEventListener('keydown', (e) => {
        if (e.key === '/' && document.activeElement.tagName !== 'INPUT' && document.activeElement.tagName !== 'TEXTAREA') {
            e.preventDefault();
            searchInput?.focus();
        }
        if (e.key === 'Escape') {
            closeVehicleDrawer();
            closeAllModals();
        }
    });

    if (searchInput) {
        searchInput.addEventListener('input', (e) => {
            const query = e.target.value.toLowerCase().trim();
            if (query.length > 0) {
                // Switch to vehicles tab and filter
                switchTab('vehicles');
                const vSearch = document.getElementById('vehicle-search-input');
                if (vSearch) {
                    vSearch.value = query;
                    state.searchTerm = query;
                    state.currentPage = 1;
                    renderVehiclesTable();
                }
            }
        });
    }
}

// ==========================================================================
// 2. Security, Authentication & Centralized API Gateway
// ==========================================================================

function escapeHTML(str) {
    if (str === null || str === undefined) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;');
}

async function fetchAPI(url, options = {}) {
    options.headers = options.headers || {};
    if (state.sessionToken) {
        options.headers['Authorization'] = `Bearer ${state.sessionToken}`;
    }
    options.credentials = 'same-origin';

    const res = await fetch(url, options);

    if (res.status === 401) {
        // If not explicit login/session check, invalidate state and prompt login
        if (!url.includes('/api/auth/login') && !url.includes('/api/auth/session')) {
            state.sessionToken = null;
            state.currentUser = null;
            sessionStorage.removeItem('fleet_session_token');
            showAlert('Session expired or authentication required. Please sign in.', 'danger');
            openModal('modal-auth-login');
        }
    } else if (res.status === 403) {
        try {
            const errJson = await res.clone().json();
            showAlert(`Access Denied (403): ${errJson.message || 'Operation prohibited for your role.'}`, 'danger');
        } catch (_) {
            showAlert('Access Denied (403): Insufficient permissions for this action.', 'danger');
        }
    }

    return res;
}

async function initSessionAndApp() {
    // 1. Check existing session token or HttpOnly cookie
    try {
        const res = await fetchAPI('/api/auth/session');
        if (res.ok) {
            const json = await res.json();
            if (json.success && json.user) {
                applyUserSession(json.user, state.sessionToken);
                await loadAllData();
                return;
            }
        }
    } catch (e) {
        console.warn('Session check failed:', e);
    }

    // 2. If no valid session, auto-authenticate with demo admin for initial dev load
    // while keeping authentication real and server-verified
    try {
        const loginRes = await fetch('/api/auth/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username: 'admin', password: 'Admin@123' })
        });
        const loginJson = await loginRes.json();
        if (loginJson.success && loginJson.user) {
            applyUserSession(loginJson.user, loginJson.token);
            await loadAllData();
            return;
        }
    } catch (e) {
        console.error('Initial login attempt failed:', e);
    }

    openModal('modal-auth-login');
}

function applyUserSession(user, token) {
    state.currentUser = user;
    state.currentRole = user.role || 'Admin';
    if (token) {
        state.sessionToken = token;
        sessionStorage.setItem('fleet_session_token', token);
    }
    applyRolePermissions(user);
}

function applyRolePermissions(user) {
    const role = (user.role || 'Admin').toLowerCase();

    // 1. Sidebar Nav Items and Group Titles
    document.querySelectorAll('[data-roles]').forEach(el => {
        const allowedRoles = el.getAttribute('data-roles').toLowerCase().split(',').map(r => r.trim());
        if (allowedRoles.includes(role)) {
            el.classList.remove('role-hidden');
        } else {
            el.classList.add('role-hidden');
        }
    });

    // 2. Role Dashboard Views
    const adminView = document.getElementById('dashboard-view-admin');
    const mgrView = document.getElementById('dashboard-view-manager');
    const staffView = document.getElementById('dashboard-view-staff');

    if (adminView) adminView.classList.toggle('role-hidden', role !== 'admin');
    if (mgrView) mgrView.classList.toggle('role-hidden', role !== 'manager');
    if (staffView) staffView.classList.toggle('role-hidden', role !== 'staff');

    // 3. Tab Pane Redirection if active tab is forbidden
    const activeTab = document.querySelector('.nav-item.active')?.getAttribute('data-tab') || 'dashboard';
    const forbiddenTabsForRole = {
        staff: ['expenses', 'reports', 'dsa', 'audit', 'backup', 'drivers'],
        manager: ['dsa', 'audit', 'backup'],
        admin: []
    };
    if (forbiddenTabsForRole[role]?.includes(activeTab)) {
        switchTab('dashboard');
    }

    // 4. Update Header indicators
    const userNameEl = document.getElementById('header-user-name');
    const userRoleEl = document.getElementById('header-user-role');
    const userAvatarEl = document.getElementById('header-user-avatar');
    const modalSessionEl = document.getElementById('modal-session-user');

    if (userNameEl) userNameEl.textContent = user.username;
    if (userRoleEl) userRoleEl.textContent = `Role: ${user.role}`;
    if (userAvatarEl) userAvatarEl.textContent = (user.username || 'AD').slice(0, 2).toUpperCase();
    if (modalSessionEl) modalSessionEl.textContent = `Signed in as: ${user.username} (${user.role})`;

    // 5. Hide / Show operational buttons based on role capabilities
    const btnAddVehicle = document.getElementById('btn-add-vehicle-modal');
    if (btnAddVehicle) btnAddVehicle.classList.toggle('role-hidden', !user.can_write_vehicle);

    const btnAddDriver = document.getElementById('btn-add-driver-modal');
    if (btnAddDriver) btnAddDriver.classList.toggle('role-hidden', !user.can_write_driver);

    const btnAddExpense = document.getElementById('btn-record-expense-modal');
    if (btnAddExpense) btnAddExpense.classList.toggle('role-hidden', !user.can_write_expense);

    const btnQuickBackup = document.getElementById('btn-quick-backup');
    if (btnQuickBackup) btnQuickBackup.classList.toggle('role-hidden', role !== 'admin');
}

async function handleLogout() {
    try {
        await fetchAPI('/api/auth/logout', { method: 'POST' });
    } catch (e) {
        console.error('Logout error:', e);
    }
    state.sessionToken = null;
    state.currentUser = null;
    state.currentRole = 'Guest';
    sessionStorage.removeItem('fleet_session_token');

    showAlert('Signed out successfully. Session invalidated on server.', 'info');
    document.getElementById('header-user-name').textContent = 'Signed Out';
    document.getElementById('header-user-role').textContent = 'Not Authenticated';
    document.getElementById('modal-session-user').textContent = 'No active session';

    applyRolePermissions({ role: 'Guest' });
    openModal('modal-auth-login');
}

// ==========================================================================
// 3. Data Loading & Role Dashboards Engine
// ==========================================================================
async function loadAllData() {
    const role = (state.currentRole || 'Admin').toLowerCase();
    const tasks = [
        loadDashboard(),
        loadVehicles(),
        loadSchedules(),
        loadNotifications(),
        loadMaintenance(),
        loadWorkOrders(),
        loadInspections(),
        loadDefects()
    ];
    if (role !== 'staff') {
        tasks.push(loadDrivers());
        tasks.push(loadExpenses());
        tasks.push(loadFleetAnalytics());
    }
    if (role === 'admin') {
        tasks.push(loadAudit());
    }
    await Promise.all(tasks);

    // Populate role-specific dashboard views
    if (role === 'manager') {
        loadManagerDashboard();
    } else if (role === 'staff') {
        loadStaffDashboard();
    }
}

async function loadDashboard() {
    try {
        const res = await fetchAPI('/api/dashboard');
        const json = await res.json();
        if (!json.success || !json.stats) return;

        state.dashboard = json.stats;
        const s = json.stats;

        // Admin Dashboard KPI Cards
        const kpiTotal = document.getElementById('kpi-total-vehicles');
        if (kpiTotal) kpiTotal.textContent = s.total_active_vehicles || 0;
        const kpiAvail = document.getElementById('kpi-avail-vehicles');
        if (kpiAvail) kpiAvail.textContent = s.available_vehicles || 0;
        const kpiTrip = document.getElementById('kpi-on-trip');
        if (kpiTrip) kpiTrip.textContent = `${s.on_trip_vehicles || 0} on trips`;
        const kpiMaint = document.getElementById('kpi-under-maint');
        if (kpiMaint) kpiMaint.textContent = s.under_maintenance_vehicles || 0;
        const kpiOut = document.getElementById('kpi-out-of-service');
        if (kpiOut) kpiOut.textContent = s.out_of_service_vehicles || 0;
        const kpiOverdue = document.getElementById('kpi-overdue');
        if (kpiOverdue) kpiOverdue.textContent = s.overdue_services_count || 0;
        const kpiUpcoming = document.getElementById('kpi-upcoming');
        if (kpiUpcoming) kpiUpcoming.textContent = s.upcoming_services_count || 0;
        const kpiSpend = document.getElementById('kpi-total-spend');
        if (kpiSpend) kpiSpend.textContent = s.total_maintenance_spending_str || 'Rs. 0';
        const kpiMonth = document.getElementById('kpi-month-spend');
        if (kpiMonth) kpiMonth.textContent = `This month: ${s.current_month_spending_str || 'Rs. 0'}`;

        // Update Nav Badges
        const navOverdue = document.getElementById('nav-badge-overdue');
        if (navOverdue) navOverdue.textContent = s.overdue_services_count || 0;

        // Render SVG Charts for Admin
        renderFleetDonutChart(s);
    } catch (e) {
        console.error('Failed to load dashboard:', e);
    }
}

async function loadVehicles() {
    try {
        const res = await fetchAPI('/api/vehicles');
        const json = await res.json();
        if (json.success && json.data) {
            state.vehicles = json.data;
            document.getElementById('nav-badge-vehicles').textContent = json.count || json.data.length;
            renderVehiclesTable();
            populateVehicleDropdowns();
        }
    } catch (e) {
        console.error('Failed to load vehicles:', e);
    }
}

async function loadDrivers() {
    try {
        const res = await fetchAPI('/api/drivers');
        const json = await res.json();
        if (json.success && json.data) {
            state.drivers = json.data;
            document.getElementById('nav-badge-drivers').textContent = json.count || json.data.length;
            renderDriversTable();
            populateDriverDropdowns();
        }
    } catch (e) {
        console.error('Failed to load drivers:', e);
    }
}

async function loadMaintenance() {
    try {
        const res = await fetchAPI('/api/maintenance');
        const json = await res.json();
        if (json.success && json.data) {
            state.maintenance = json.data;
            renderMaintenanceTable();
            renderRecentActivityTimeline();
        }
    } catch (e) {
        console.error('Failed to load maintenance records:', e);
    }
}

async function loadSchedules() {
    try {
        const res = await fetchAPI('/api/schedules');
        const json = await res.json();
        if (json.success && json.data) {
            state.schedules = json.data;
            renderSchedulesTable();
            renderUrgentRemindersTable();
            renderRemindersTab();
        }
    } catch (e) {
        console.error('Failed to load schedules:', e);
    }
}

async function loadExpenses() {
    try {
        const res = await fetchAPI('/api/expenses');
        const json = await res.json();
        if (json.success && json.data) {
            state.expenses = json.data;
            renderExpensesTable();
            renderCategoryBreakdown();
            renderMonthlySpendChart();
        }
    } catch (e) {
        console.error('Failed to load expenses:', e);
    }
}

async function loadNotifications() {
    try {
        const res = await fetchAPI('/api/notifications?unread_only=0');
        const json = await res.json();
        if (json.success && json.data) {
            state.notifications = json.data;
            renderNotifications();
        }
    } catch (e) {
        console.error('Failed to load notifications:', e);
    }
}

async function ackNotification(eventKey) {
    try {
        const res = await fetchAPI('/api/notifications/ack', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ event_key: eventKey })
        });
        const json = await res.json();
        if (json.success) {
            // Update local state item
            const item = (state.notifications || []).find(n => n.event_key === eventKey);
            if (item) item.is_read = 1;
            renderNotifications();
            showAlert('Notification acknowledged.', 'info');
        } else {
            showAlert('Failed to acknowledge notification: ' + (json.message || ''), 'danger');
        }
    } catch (e) {
        console.error('Failed to ack notification:', e);
        showAlert('Network error acknowledging alert.', 'danger');
    }
}

async function ackAllNotifications() {
    try {
        const res = await fetchAPI('/api/notifications/ack-all', {
            method: 'POST'
        });
        const json = await res.json();
        if (json.success) {
            (state.notifications || []).forEach(n => n.is_read = 1);
            renderNotifications();
            showAlert(`All notifications acknowledged (${json.acknowledged_count || 0}).`, 'success');
        } else {
            showAlert('Failed to acknowledge all notifications.', 'danger');
        }
    } catch (e) {
        console.error('Failed to ack all notifications:', e);
    }
}

async function loadAudit() {
    try {
        const res = await fetchAPI('/api/audit');
        const json = await res.json();
        if (json.success && json.data) {
            state.audit = json.data;
            renderAuditTable();
            renderAdminAuditTable();
        }
    } catch (e) {
        console.error('Failed to load audit trail:', e);
    }
}

function renderAdminAuditTable() {
    const tbody = document.querySelector('#table-admin-recent-audit tbody');
    if (!tbody) return;

    const recent = (state.audit || []).slice(0, 5);
    if (recent.length === 0) {
        tbody.innerHTML = '<tr><td colspan="4" class="text-center text-muted py-3">No recent security events recorded.</td></tr>';
        return;
    }

    tbody.innerHTML = recent.map(a => `
        <tr>
            <td class="text-sm font-mono">${escapeHTML(a.timestamp)}</td>
            <td class="font-bold text-sm">${escapeHTML(a.username)}</td>
            <td><span class="badge badge-muted text-xs">${escapeHTML(a.action)}</span></td>
            <td class="text-sm text-muted">${escapeHTML(a.details || a.entity_type)}</td>
        </tr>
    `).join('');
}

// --------------------------------------------------------------------------
// Role Dashboard 2: Fleet Manager Implementation
// --------------------------------------------------------------------------
function loadManagerDashboard() {
    if (!state.dashboard) return;
    const s = state.dashboard;

    const kpiTotal = document.getElementById('kpi-mgr-total');
    const kpiAvail = document.getElementById('kpi-mgr-avail');
    const kpiMaint = document.getElementById('kpi-mgr-maint');
    const kpiUpcoming = document.getElementById('kpi-mgr-upcoming');
    const kpiOverdue = document.getElementById('kpi-mgr-overdue');
    const kpiSpend = document.getElementById('kpi-mgr-spend');

    if (kpiTotal) kpiTotal.textContent = s.total_active_vehicles || 0;
    if (kpiAvail) kpiAvail.textContent = s.available_vehicles || 0;
    if (kpiMaint) kpiMaint.textContent = s.under_maintenance_vehicles || 0;
    if (kpiUpcoming) kpiUpcoming.textContent = s.upcoming_services_count || 0;
    if (kpiOverdue) kpiOverdue.textContent = s.overdue_services_count || 0;
    if (kpiSpend) kpiSpend.textContent = s.current_month_spending_str || 'Rs. 0';

    renderManagerAllocationsTable();
    renderManagerUrgentSchedules();
}

function renderManagerAllocationsTable() {
    const tbody = document.querySelector('#table-mgr-allocations tbody');
    if (!tbody) return;

    if (!state.drivers || state.drivers.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" class="text-center text-muted py-4">No commercial drivers registered.</td></tr>';
        return;
    }

    tbody.innerHTML = state.drivers.map(d => {
        const assignedBadge = d.assigned_vehicle_reg
            ? `<span class="badge badge-primary font-mono">${escapeHTML(d.assigned_vehicle_reg)}</span>`
            : `<span class="badge badge-secondary">Unassigned</span>`;
        const readinessBadge = (d.active)
            ? `<span class="badge badge-success">Ready</span>`
            : `<span class="badge badge-warning">Inactive</span>`;
        return `
            <tr>
                <td><strong>${escapeHTML(d.name)}</strong><br><small class="text-muted">${escapeHTML(d.phone || 'No phone')}</small></td>
                <td><span class="font-mono text-sm">${escapeHTML(d.license_number)}</span></td>
                <td>${assignedBadge}</td>
                <td class="text-sm">${escapeHTML(d.license_expiry || '—')}</td>
                <td>${readinessBadge}</td>
                <td class="text-right">
                    <button class="btn btn-outline btn-sm btn-mgr-reassign" data-driver-id="${d.id}" data-driver-name="${escapeHTML(d.name)}" data-vehicle-id="${d.assigned_vehicle_id || 0}">Reassign</button>
                </td>
            </tr>
        `;
    }).join('');

    tbody.querySelectorAll('.btn-mgr-reassign').forEach(btn => {
        btn.addEventListener('click', () => {
            const vid = parseInt(btn.getAttribute('data-vehicle-id'), 10) || state.vehicles[0]?.id || 1;
            openAssignDriverModal(vid);
        });
    });
}

function renderManagerUrgentSchedules() {
    const tbody = document.querySelector('#table-mgr-urgent-schedules tbody');
    if (!tbody) return;

    const urgentList = (state.schedules || []).filter(s => s.status === 'Overdue' || s.status === 'Due Soon').slice(0, 8);
    if (urgentList.length === 0) {
        tbody.innerHTML = '<tr><td colspan="4" class="text-center text-muted py-4">All preventive schedules within normal thresholds.</td></tr>';
        return;
    }

    tbody.innerHTML = urgentList.map(s => {
        const badgeClass = s.status === 'Overdue' ? 'badge-danger' : 'badge-warning';
        return `
            <tr>
                <td><span class="font-mono text-sm font-bold">${escapeHTML(s.vehicle_reg)}</span></td>
                <td class="text-sm">${escapeHTML(s.service_name)}</td>
                <td class="text-sm">${escapeHTML(s.due_date || '—')}</td>
                <td><span class="badge ${badgeClass}">${escapeHTML(s.status)}</span></td>
            </tr>
        `;
    }).join('');
}

// --------------------------------------------------------------------------
// Role Dashboard 3: Maintenance Staff Implementation
// --------------------------------------------------------------------------
function loadStaffDashboard() {
    const bayCount = (state.vehicles || []).filter(v => v.status === 'Maintenance').length;
    const overdueCount = (state.schedules || []).filter(s => s.status === 'Overdue').length;
    const upcomingCount = (state.schedules || []).filter(s => s.status === 'Due Soon' || (s.days_remaining >= 0 && s.days_remaining <= 7)).length;
    const completedCount = (state.maintenance || []).length;

    const kpiBay = document.getElementById('kpi-staff-bay');
    const kpiOverdue = document.getElementById('kpi-staff-overdue');
    const kpiUpcoming = document.getElementById('kpi-staff-upcoming');
    const kpiCompleted = document.getElementById('kpi-staff-completed');

    if (kpiBay) kpiBay.textContent = bayCount;
    if (kpiOverdue) kpiOverdue.textContent = overdueCount;
    if (kpiUpcoming) kpiUpcoming.textContent = upcomingCount;
    if (kpiCompleted) kpiCompleted.textContent = completedCount;

    renderStaffSchedulesTable();
    renderStaffBayQueue();
    renderStaffRecentMaintTable();
}

function renderStaffSchedulesTable() {
    const tbody = document.querySelector('#table-staff-schedules tbody');
    if (!tbody) return;

    const actionable = (state.schedules || []).filter(s => s.status !== 'Completed' && s.status !== 'Cancelled').slice(0, 10);
    if (actionable.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" class="text-center text-muted py-4">No pending maintenance work orders. All up to date!</td></tr>';
        return;
    }

    tbody.innerHTML = actionable.map(s => {
        const badgeClass = s.status === 'Overdue' ? 'badge-danger' : (s.status === 'Due Soon' ? 'badge-warning' : 'badge-info');
        return `
            <tr>
                <td><span class="font-mono font-bold">${escapeHTML(s.vehicle_reg)}</span></td>
                <td><strong>${escapeHTML(s.service_name)}</strong></td>
                <td class="text-sm">${escapeHTML(s.due_date || '—')}</td>
                <td class="text-sm">${s.due_odometer_km ? s.due_odometer_km.toLocaleString() + ' km' : '—'}</td>
                <td><span class="badge ${badgeClass}">${escapeHTML(s.status)}</span></td>
                <td class="text-right">
                    <button class="btn btn-primary btn-sm btn-staff-record-svc" data-vid="${s.vehicle_id}" data-vreg="${escapeHTML(s.vehicle_reg)}" data-svc="${escapeHTML(s.service_name)}" style="padding: 3px 8px; font-size: 11px;">Record Service</button>
                    <button class="btn btn-outline btn-sm btn-staff-complete-sched" data-sid="${s.id}" style="padding: 3px 8px; font-size: 11px; margin-left: 4px;">Complete</button>
                </td>
            </tr>
        `;
    }).join('');

    tbody.querySelectorAll('.btn-staff-record-svc').forEach(btn => {
        btn.addEventListener('click', () => {
            const vid = parseInt(btn.getAttribute('data-vid'), 10);
            const svc = btn.getAttribute('data-svc');
            openLogMaintenanceModal(vid, svc);
        });
    });

    tbody.querySelectorAll('.btn-staff-complete-sched').forEach(btn => {
        btn.addEventListener('click', () => {
            const sid = parseInt(btn.getAttribute('data-sid'), 10);
            completeSchedule(sid);
        });
    });
}

function renderStaffBayQueue() {
    const queueContainer = document.getElementById('staff-bay-queue');
    const badge = document.getElementById('staff-bay-badge');
    if (!queueContainer) return;

    const inBayVehicles = (state.vehicles || []).filter(v => v.status === 'Maintenance');
    if (badge) badge.textContent = `${inBayVehicles.length} vehicles`;

    if (inBayVehicles.length === 0) {
        queueContainer.innerHTML = '<div class="text-center text-muted py-4 text-sm">No vehicles currently in service bay.</div>';
        return;
    }

    queueContainer.innerHTML = inBayVehicles.map(v => `
        <div class="bay-queue-item">
            <div>
                <span class="bay-reg">${escapeHTML(v.registration_number)}</span>
                <div class="bay-details">${escapeHTML(v.make)} ${escapeHTML(v.model)} (${(v.odometer_km || 0).toLocaleString()} km)</div>
            </div>
            <div style="display: flex; gap: 6px; align-items: center;">
                <span class="badge badge-warning" style="font-size: 10px;">In Bay</span>
                <button class="btn btn-outline btn-sm btn-bay-log" data-vid="${v.id}" style="padding: 2px 6px; font-size: 11px;">Log Service</button>
            </div>
        </div>
    `).join('');

    queueContainer.querySelectorAll('.btn-bay-log').forEach(btn => {
        btn.addEventListener('click', () => {
            const vid = parseInt(btn.getAttribute('data-vid'), 10);
            openLogMaintenanceModal(vid);
        });
    });
}

function renderStaffRecentMaintTable() {
    const tbody = document.querySelector('#table-staff-recent-maint tbody');
    if (!tbody) return;

    const recent = (state.maintenance || []).slice(0, 8);
    if (recent.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" class="text-center text-muted py-3">No completed service records yet.</td></tr>';
        return;
    }

    tbody.innerHTML = recent.map(m => `
        <tr>
            <td class="text-sm font-mono">${escapeHTML(m.service_date)}</td>
            <td><span class="font-mono font-bold">${escapeHTML(m.vehicle_reg)}</span></td>
            <td><strong>${escapeHTML(m.service_type)}</strong></td>
            <td class="text-sm">${m.odometer_km ? m.odometer_km.toLocaleString() + ' km' : '—'}</td>
            <td class="text-sm">${escapeHTML(m.workshop_name || 'In-House')}</td>
            <td class="text-sm text-muted">${escapeHTML(m.description || '—')}</td>
        </tr>
    `).join('');
}

// ==========================================================================
// 3. SVG Charts Engine (Pure Native SVG, Zero External Libs)
// ==========================================================================

// Chart 1: Donut Chart for Fleet Status Distribution
function renderFleetDonutChart(stats) {
    const container = document.getElementById('chart-fleet-donut');
    const legendContainer = document.getElementById('donut-legend-container');
    if (!container) return;

    const avail = stats.available_vehicles || 0;
    const maint = stats.under_maintenance_vehicles || 0;
    const trip = stats.on_trip_vehicles || 0;
    const out = stats.out_of_service_vehicles || 0;
    const total = stats.total_active_vehicles || (avail + maint + trip + out);

    if (total === 0) {
        container.innerHTML = '<div class="empty-state"><p>No vehicle assets registered yet.</p></div>';
        if (legendContainer) legendContainer.innerHTML = '';
        return;
    }

    const segments = [
        { label: 'Available', value: avail, color: '#16a34a' },
        { label: 'On Trip', value: trip, color: '#2563eb' },
        { label: 'In Maintenance', value: maint, color: '#d97706' },
        { label: 'Out of Service', value: out, color: '#dc2626' }
    ].filter(s => s.value > 0);

    const radius = 70;
    const circumference = 2 * Math.PI * radius;
    let accumulatedOffset = 0;

    let circlesSvg = '';
    segments.forEach(s => {
        const strokeLength = (s.value / total) * circumference;
        const strokeSpace = circumference - strokeLength;
        const dashOffset = -accumulatedOffset;
        accumulatedOffset += strokeLength;

        circlesSvg += `
            <circle cx="100" cy="100" r="${radius}" fill="none" stroke="${s.color}" 
                    stroke-width="22" stroke-dasharray="${strokeLength} ${strokeSpace}" 
                    stroke-dashoffset="${dashOffset}" 
                    style="transition: stroke-dasharray 0.5s ease;"></circle>
        `;
    });

    const activePct = Math.round(((avail + trip) / total) * 100);

    container.innerHTML = `
        <svg class="chart-svg" viewBox="0 0 200 200" style="transform: rotate(-90deg);">
            <!-- Background Track -->
            <circle cx="100" cy="100" r="${radius}" fill="none" stroke="#f1f5f9" stroke-width="22"></circle>
            ${circlesSvg}
        </svg>
        <div style="position: absolute; text-align: center; pointer-events: none;">
            <div style="font-size: 24px; font-weight: 700; color: var(--text-primary);">${activePct}%</div>
            <div style="font-size: 11px; font-weight: 600; color: var(--text-muted); text-transform: uppercase;">Active Fleet</div>
        </div>
    `;

    // Populate Legend
    if (legendContainer) {
        legendContainer.innerHTML = segments.map(s => `
            <div class="donut-legend-item">
                <div class="donut-legend-left">
                    <span class="donut-color-dot" style="background-color: ${s.color};"></span>
                    <span>${s.label}</span>
                </div>
                <span style="font-weight: 600; color: var(--text-primary);">${s.value} (${Math.round((s.value / total) * 100)}%)</span>
            </div>
        `).join('');
    }
}

// Chart 2: Monthly Maintenance Expenditure Trend (SVG Bar Chart)
function renderMonthlySpendChart() {
    const container = document.getElementById('chart-monthly-bars');
    if (!container) return;

    // Group expenses by Month (YYYY-MM)
    const monthlyTotals = {};
    state.expenses.forEach(e => {
        const d = e.date ? e.date.substring(0, 7) : 'Other';
        monthlyTotals[d] = (monthlyTotals[d] || 0) + (e.amount || 0);
    });

    const sortedMonths = Object.keys(monthlyTotals).sort().slice(-6); // Last 6 active months
    if (sortedMonths.length === 0) {
        container.innerHTML = '<div class="empty-state"><p>No expense transactions recorded yet.</p></div>';
        return;
    }

    const maxVal = Math.max(...sortedMonths.map(m => monthlyTotals[m]), 5000);
    const width = 520;
    const height = 200;
    const paddingLeft = 60;
    const paddingBottom = 30;
    const chartW = width - paddingLeft - 20;
    const chartH = height - paddingBottom - 20;

    const barWidth = Math.min(42, Math.floor(chartW / sortedMonths.length) - 16);

    let barsSvg = '';
    sortedMonths.forEach((m, idx) => {
        const val = monthlyTotals[m];
        const barH = (val / maxVal) * chartH;
        const x = paddingLeft + (idx * (chartW / sortedMonths.length)) + ((chartW / sortedMonths.length - barWidth) / 2);
        const y = 20 + (chartH - barH);

        barsSvg += `
            <rect x="${x}" y="${y}" width="${barWidth}" height="${barH}" rx="4" fill="#2563eb" style="transition: height 0.4s ease;">
                <title>${m}: Rs. ${val.toLocaleString()}</title>
            </rect>
            <text x="${x + (barWidth / 2)}" y="${y - 6}" font-size="10.5" font-weight="600" text-anchor="middle" fill="#334155">
                ${val >= 1000 ? (val / 1000).toFixed(1) + 'k' : val}
            </text>
            <text x="${x + (barWidth / 2)}" y="${height - 8}" font-size="11" text-anchor="middle" fill="#64748b">
                ${m.substring(5)}
            </text>
        `;
    });

    container.innerHTML = `
        <svg class="chart-svg" viewBox="0 0 ${width} ${height}">
            <!-- Horizontal Gridlines -->
            <line x1="${paddingLeft}" y1="20" x2="${width - 10}" y2="20" stroke="#f1f5f9" stroke-width="1"></line>
            <line x1="${paddingLeft}" y1="${20 + chartH / 2}" x2="${width - 10}" y2="${20 + chartH / 2}" stroke="#f1f5f9" stroke-width="1"></line>
            <line x1="${paddingLeft}" y1="${20 + chartH}" x2="${width - 10}" y2="${20 + chartH}" stroke="#e2e8f0" stroke-width="1.5"></line>
            
            <!-- Axis Labels -->
            <text x="${paddingLeft - 8}" y="24" font-size="10" text-anchor="end" fill="#94a3b8">Rs. ${(maxVal / 1000).toFixed(0)}k</text>
            <text x="${paddingLeft - 8}" y="${24 + chartH / 2}" font-size="10" text-anchor="end" fill="#94a3b8">Rs. ${(maxVal / 2000).toFixed(0)}k</text>
            <text x="${paddingLeft - 8}" y="${20 + chartH}" font-size="10" text-anchor="end" fill="#94a3b8">0</text>
            
            ${barsSvg}
        </svg>
    `;
}

// Chart 3: Expenditure Breakdown by Category
function renderCategoryBreakdown() {
    const container = document.getElementById('category-progress-container');
    if (!container) return;

    const catTotals = { Fuel: 0, Periodic: 0, Parts: 0, Toll: 0, Other: 0 };
    let totalSpend = 0;

    state.expenses.forEach(e => {
        const cat = e.category || 'Other';
        if (cat.includes('Fuel')) catTotals.Fuel += e.amount || 0;
        else if (cat.includes('Service') || cat.includes('Maintenance')) catTotals.Periodic += e.amount || 0;
        else if (cat.includes('Part')) catTotals.Parts += e.amount || 0;
        else if (cat.includes('Toll')) catTotals.Toll += e.amount || 0;
        else catTotals.Other += e.amount || 0;

        totalSpend += (e.amount || 0);
    });

    if (totalSpend === 0) {
        container.innerHTML = '<div class="text-center text-muted py-4">No categorized expenditures found.</div>';
        return;
    }

    const categories = [
        { name: 'Fuel & Oil Refueling', amount: catTotals.Fuel, color: '#2563eb' },
        { name: 'Scheduled & Periodic Maintenance', amount: catTotals.Periodic, color: '#16a34a' },
        { name: 'Spare Parts & Overhauls', amount: catTotals.Parts, color: '#d97706' },
        { name: 'Highway Tolls & Permits', amount: catTotals.Toll, color: '#7c3aed' },
        { name: 'Other Operating Expenses', amount: catTotals.Other, color: '#64748b' }
    ].filter(c => c.amount > 0);

    container.innerHTML = categories.map(c => {
        const pct = Math.round((c.amount / totalSpend) * 100);
        return `
            <div class="category-bar-row">
                <div class="category-bar-label">
                    <span style="color: var(--text-primary); font-weight: 600;">${c.name}</span>
                    <span style="color: var(--text-muted); font-size: 11.5px;">Rs. ${c.amount.toLocaleString()} (${pct}%)</span>
                </div>
                <div class="category-bar-track">
                    <div class="category-bar-fill" style="width: ${pct}%; background-color: ${c.color};"></div>
                </div>
            </div>
        `;
    }).join('');
}

// ==========================================================================
// 4. Vehicles Register (Sort, Filter, Pagination, Column Visibility)
// ==========================================================================
function initTableEvents() {
    // Search Input
    document.getElementById('vehicle-search-input')?.addEventListener('input', (e) => {
        state.searchTerm = e.target.value.toLowerCase().trim();
        state.currentPage = 1;
        renderVehiclesTable();
    });

    // Filters
    document.getElementById('vehicle-filter-status')?.addEventListener('change', (e) => {
        state.statusFilter = e.target.value;
        state.currentPage = 1;
        renderVehiclesTable();
    });

    document.getElementById('vehicle-filter-fuel')?.addEventListener('change', (e) => {
        state.fuelFilter = e.target.value;
        state.currentPage = 1;
        renderVehiclesTable();
    });

    document.getElementById('btn-clear-vehicle-filters')?.addEventListener('click', () => {
        document.getElementById('vehicle-search-input').value = '';
        document.getElementById('vehicle-filter-status').value = '';
        document.getElementById('vehicle-filter-fuel').value = '';
        state.searchTerm = '';
        state.statusFilter = '';
        state.fuelFilter = '';
        state.currentPage = 1;
        renderVehiclesTable();
    });

    // Column Sorting
    document.querySelectorAll('#table-vehicles thead th.sortable').forEach(th => {
        th.addEventListener('click', () => {
            const field = th.getAttribute('data-sort');
            if (state.sortField === field) {
                state.sortAsc = !state.sortAsc;
            } else {
                state.sortField = field;
                state.sortAsc = true;
            }
            renderVehiclesTable();
        });
    });

    // Pagination
    document.getElementById('btn-vehicle-prev')?.addEventListener('click', () => {
        if (state.currentPage > 1) {
            state.currentPage--;
            renderVehiclesTable();
        }
    });

    document.getElementById('btn-vehicle-next')?.addEventListener('click', () => {
        const total = getFilteredVehicles().length;
        if (state.currentPage * state.pageSize < total) {
            state.currentPage++;
            renderVehiclesTable();
        }
    });

    document.getElementById('vehicle-page-size')?.addEventListener('change', (e) => {
        state.pageSize = parseInt(e.target.value, 10);
        state.currentPage = 1;
        renderVehiclesTable();
    });
}

function initColumnPicker() {
    const toggleBtn = document.getElementById('btn-column-picker-toggle');
    const menu = document.getElementById('column-picker-menu');
    if (!toggleBtn || !menu) return;

    toggleBtn.addEventListener('click', (e) => {
        e.stopPropagation();
        menu.classList.toggle('show');
    });

    document.addEventListener('click', (e) => {
        if (!menu.contains(e.target) && e.target !== toggleBtn) {
            menu.classList.remove('show');
        }
    });

    menu.querySelectorAll('input[type="checkbox"]').forEach(chk => {
        chk.addEventListener('change', () => {
            const col = chk.getAttribute('data-col');
            state.visibleCols[col] = chk.checked;
            applyColumnVisibility();
        });
    });
}

function applyColumnVisibility() {
    Object.keys(state.visibleCols).forEach(col => {
        const isVisible = state.visibleCols[col];
        document.querySelectorAll(`.col-${col}`).forEach(el => {
            el.style.display = isVisible ? '' : 'none';
        });
    });
}

function getFilteredVehicles() {
    return state.vehicles.filter(v => {
        // Text Search
        if (state.searchTerm) {
            const matchText = `${v.registration_number} ${v.make} ${v.model} ${v.vin} ${v.driver_name}`.toLowerCase();
            if (!matchText.includes(state.searchTerm)) return false;
        }
        // Status Filter
        if (state.statusFilter && v.status !== state.statusFilter) {
            return false;
        }
        // Fuel Filter
        if (state.fuelFilter && v.fuel_type !== state.fuelFilter) {
            return false;
        }
        return true;
    }).sort((a, b) => {
        let valA = a[state.sortField] || '';
        let valB = b[state.sortField] || '';
        if (typeof valA === 'string') valA = valA.toLowerCase();
        if (typeof valB === 'string') valB = valB.toLowerCase();
        if (valA < valB) return state.sortAsc ? -1 : 1;
        if (valA > valB) return state.sortAsc ? 1 : -1;
        return 0;
    });
}

function renderVehiclesTable() {
    const tbody = document.getElementById('tbody-vehicles');
    if (!tbody) return;

    const filtered = getFilteredVehicles();
    const total = filtered.length;
    const startIdx = (state.currentPage - 1) * state.pageSize;
    const pageItems = filtered.slice(startIdx, startIdx + state.pageSize);

    // Update Pagination Display
    const endIdx = Math.min(startIdx + pageItems.length, total);
    document.getElementById('vehicle-pagination-info').textContent = 
        total > 0 ? `Showing ${startIdx + 1} to ${endIdx} of ${total} vehicles` : 'No vehicles found';

    document.getElementById('btn-vehicle-prev').disabled = state.currentPage <= 1;
    document.getElementById('btn-vehicle-next').disabled = endIdx >= total;

    if (pageItems.length === 0) {
        tbody.innerHTML = `<tr><td colspan="10" class="text-center text-muted py-4">No matching fleet vehicles found.</td></tr>`;
        return;
    }

    tbody.innerHTML = pageItems.map(v => {
        const fuelPct = v.fuel_percentage || 0;
        let fuelColor = '#16a34a';
        if (fuelPct < 20) fuelColor = '#dc2626';
        else if (fuelPct < 50) fuelColor = '#d97706';

        let statusBadge = 'badge-success';
        if (v.status === 'Maintenance') statusBadge = 'badge-warning';
        else if (v.status === 'Out of Service') statusBadge = 'badge-danger';
        else if (v.status === 'On Trip') statusBadge = 'badge-info';

        return `
            <tr data-vehicle-id="${v.id}">
                <td><input type="checkbox" class="vehicle-select-chk" value="${v.id}"></td>
                <td class="primary-cell" style="cursor: pointer;" onclick="openVehicleDrawer(${v.id})">
                    <span style="color: var(--primary); font-weight: 700;">${v.registration_number}</span>
                </td>
                <td class="col-make">${v.make} ${v.model}</td>
                <td class="col-year font-mono" style="font-size: 11px;">
                    <div>${v.year || '-'}</div>
                    <div style="color: var(--text-subtle);">${v.vin ? v.vin.substring(0, 10) + '...' : '-'}</div>
                </td>
                <td class="col-fuel">
                    <div class="fuel-gauge-mini">
                        <div class="fuel-bar-wrap">
                            <div class="fuel-bar-fill" style="width: ${fuelPct}%; background-color: ${fuelColor};"></div>
                        </div>
                        <span style="font-size: 11px; font-weight: 600;">${fuelPct}%</span>
                    </div>
                </td>
                <td class="col-odometer font-mono">${(v.odometer_km || 0).toLocaleString()} km</td>
                <td class="col-driver">
                    ${v.driver_name ? `
                        <div style="display: flex; align-items: center; gap: 6px;">
                            <span style="width: 20px; height: 20px; border-radius: 50%; background-color: var(--primary-subtle); color: var(--primary); font-size: 10px; font-weight: 700; display: flex; align-items: center; justify-content: center;">
                                ${v.driver_name.charAt(0)}
                            </span>
                            <span>${v.driver_name}</span>
                        </div>
                    ` : '<span class="badge badge-muted">Unassigned</span>'}
                </td>
                <td class="col-insurance" style="font-size: 11.5px;">${v.insurance_expiry || '-'}</td>
                <td class="col-status">
                    <span class="badge ${statusBadge}">
                        <span class="badge-dot"></span>
                        ${v.status || 'Available'}
                    </span>
                </td>
                <td class="text-right">
                    <div class="table-actions" style="justify-content: flex-end;">
                        <button class="action-btn" title="View Detailed Profile" onclick="openVehicleDrawer(${v.id})">
                            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path><circle cx="12" cy="12" r="3"></circle></svg>
                        </button>
                        ${state.currentRole !== 'Staff' ? `
                        <button class="action-btn" title="Edit Vehicle" onclick="openEditVehicleModal(${v.id})">
                            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"></path><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"></path></svg>
                        </button>` : ''}
                        ${(state.currentUser?.can_delete_vehicle || state.currentRole === 'Admin') ? `
                        <button class="action-btn delete" title="Delete Asset" onclick="deleteVehicle(${v.id})">
                            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>
                        </button>` : ''}
                    </div>
                </td>
            </tr>
        `;
    }).join('');

    applyColumnVisibility();
}

// ==========================================================================
// 5. Slide-Over Vehicle Detail Drawer
// ==========================================================================
function openVehicleDrawer(vehicleId) {
    const v = state.vehicles.find(item => item.id === vehicleId);
    if (!v) return;

    state.activeVehicle = v;

    // Populate Drawer Header & Hero
    document.getElementById('drawer-vehicle-reg').textContent = v.registration_number;
    document.getElementById('drawer-vehicle-model').textContent = `${v.make} ${v.model} (${v.year || '-'})`;
    document.getElementById('drawer-odometer').textContent = `${(v.odometer_km || 0).toLocaleString()} km`;
    document.getElementById('drawer-fuel').textContent = `${v.fuel_percentage || 0}%`;
    document.getElementById('drawer-status').textContent = v.status || 'Available';

    // Populate Overview Specs
    document.getElementById('drawer-driver-name').textContent = v.driver_name || 'Unassigned';
    document.getElementById('drawer-vin').textContent = v.vin || '-';
    document.getElementById('drawer-fuel-type').textContent = v.fuel_type || 'Diesel';
    document.getElementById('drawer-year').textContent = v.year || '-';
    document.getElementById('drawer-insurance').textContent = v.insurance_expiry || 'Not Recorded';

    // Drawer Buttons Event Handlers
    document.getElementById('btn-drawer-edit-vehicle').onclick = () => openEditVehicleModal(v.id);
    document.getElementById('btn-drawer-assign-driver').onclick = () => openAssignDriverModal(v.id);
    document.getElementById('btn-drawer-log-service').onclick = () => openLogMaintenanceModal(v.id);
    document.getElementById('btn-drawer-add-expense').onclick = () => openRecordExpenseModal(v.id);

    // Populate Vehicle Service History Tab
    const vMaint = state.maintenance.filter(m => m.vehicle_id === v.id);
    const histContainer = document.getElementById('drawer-history-list');
    if (histContainer) {
        if (vMaint.length === 0) {
            histContainer.innerHTML = '<div class="text-center text-muted py-4">No maintenance history recorded for this asset.</div>';
        } else {
            histContainer.innerHTML = vMaint.map(m => `
                <div class="timeline-item">
                    <div class="timeline-dot"></div>
                    <div class="timeline-card">
                        <div class="timeline-date">${m.service_date} &bull; ${(m.odometer_km || 0).toLocaleString()} km</div>
                        <div class="timeline-title">${m.service_type} (${m.total_cost_str})</div>
                        <div class="timeline-desc">${m.description || ''} &bull; <em>${m.workshop_name}</em></div>
                    </div>
                </div>
            `).join('');
        }
    }

    // Populate Vehicle Preventive Schedules Tab
    const vSchedules = state.schedules.filter(s => s.vehicle_id === v.id);
    const schedContainer = document.getElementById('drawer-schedules-list');
    if (schedContainer) {
        if (vSchedules.length === 0) {
            schedContainer.innerHTML = '<div class="text-center text-muted py-4">No schedules configured for this asset.</div>';
        } else {
            schedContainer.innerHTML = vSchedules.map(s => {
                let badge = 'badge-info';
                if (s.status === 'Overdue') badge = 'badge-danger';
                else if (s.status === 'Due') badge = 'badge-warning';
                return `
                    <div class="detail-row mb-4" style="flex-direction: column; align-items: flex-start; gap: 4px;">
                        <div style="display: flex; justify-content: space-between; width: 100%;">
                            <strong style="color: var(--text-primary);">${s.service_name}</strong>
                            <span class="badge ${badge}">${s.status}</span>
                        </div>
                        <div style="font-size: 11.5px; color: var(--text-muted);">
                            Due: ${s.due_date} &bull; Threshold: ${(s.due_odometer_km || 0).toLocaleString()} km
                        </div>
                    </div>
                `;
            }).join('');
        }
    }

    // Populate Vehicle Expenses Tab
    const vExpenses = state.expenses.filter(e => e.vehicle_id === v.id);
    const expContainer = document.getElementById('drawer-expenses-list');
    if (expContainer) {
        if (vExpenses.length === 0) {
            expContainer.innerHTML = '<div class="text-center text-muted py-4">No expenses recorded for this vehicle.</div>';
        } else {
            expContainer.innerHTML = vExpenses.map(e => `
                <div class="detail-row mb-4">
                    <div>
                        <div style="font-weight: 600; color: var(--text-primary);">${e.category}: ${e.description}</div>
                        <div style="font-size: 11px; color: var(--text-muted);">${e.date}</div>
                    </div>
                    <div style="font-weight: 700; color: var(--text-primary);">${e.amount_str}</div>
                </div>
            `).join('');
        }
    }

    // Open Backdrop & Drawer
    document.getElementById('vehicle-drawer-backdrop')?.classList.add('open');
}

function closeVehicleDrawer() {
    document.getElementById('vehicle-drawer-backdrop')?.classList.remove('open');
    state.activeVehicle = null;
}

// ==========================================================================
// 6. Maintenance & Schedules Tables & Reminders
// ==========================================================================
function renderMaintenanceTable() {
    const tbody = document.getElementById('tbody-maintenance');
    if (!tbody) return;

    if (state.maintenance.length === 0) {
        tbody.innerHTML = `<tr><td colspan="8" class="text-center text-muted py-4">No maintenance records found.</td></tr>`;
        return;
    }

    tbody.innerHTML = state.maintenance.map(m => `
        <tr>
            <td class="font-mono">#${m.id}</td>
            <td class="primary-cell">${m.vehicle_reg}</td>
            <td><span class="badge badge-info">${m.service_type}</span></td>
            <td>${m.service_date}</td>
            <td class="font-mono">${(m.odometer_km || 0).toLocaleString()} km</td>
            <td>${m.workshop_name}</td>
            <td style="max-width: 250px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;">${m.description}</td>
            <td class="font-mono" style="font-weight: 700; color: var(--text-primary);">${m.total_cost_str}</td>
        </tr>
    `).join('');
}

function renderRecentActivityTimeline() {
    const container = document.getElementById('timeline-recent-activity');
    if (!container) return;

    if (state.maintenance.length === 0) {
        container.innerHTML = '<div class="text-center text-muted py-4">No recent maintenance activity.</div>';
        return;
    }

    const recent = state.maintenance.slice(0, 4);
    container.innerHTML = recent.map(m => `
        <div class="timeline-item">
            <div class="timeline-dot"></div>
            <div class="timeline-card">
                <div class="timeline-date">${m.service_date} &bull; ${m.vehicle_reg}</div>
                <div class="timeline-title">${m.service_type} &bull; <span style="color: var(--primary);">${m.total_cost_str}</span></div>
                <div class="timeline-desc">${m.workshop_name}</div>
            </div>
        </div>
    `).join('');
}

function renderSchedulesTable() {
    const tbody = document.getElementById('tbody-schedules');
    if (!tbody) return;

    if (state.schedules.length === 0) {
        tbody.innerHTML = `<tr><td colspan="8" class="text-center text-muted py-4">No preventive maintenance schedules defined.</td></tr>`;
        return;
    }

    tbody.innerHTML = state.schedules.map(s => {
        let badge = 'badge-info';
        if (s.status === 'Overdue') badge = 'badge-danger';
        else if (s.status === 'Due') badge = 'badge-warning';
        else if (s.status === 'Completed') badge = 'badge-success';

        return `
            <tr>
                <td class="font-mono">#${s.id}</td>
                <td class="primary-cell">${s.vehicle_reg}</td>
                <td><strong>${s.service_name}</strong></td>
                <td>${s.due_date}</td>
                <td class="font-mono">${(s.due_odometer_km || 0).toLocaleString()} km</td>
                <td><span class="badge ${badge}">${s.status}</span></td>
                <td>${s.days_remaining !== undefined ? `${s.days_remaining} days` : '-'}</td>
                <td class="text-right">
                    ${s.status !== 'Completed' ? `
                        <button class="btn btn-primary btn-sm" onclick="completeSchedule(${s.id})">Complete</button>
                        <button class="btn btn-secondary btn-sm" onclick="cancelSchedule(${s.id})">Cancel</button>
                    ` : '<span class="text-muted text-sm">Archived</span>'}
                </td>
            </tr>
        `;
    }).join('');
}

// Urgent Reminders on Dashboard
function renderUrgentRemindersTable() {
    const tbody = document.querySelector('#table-urgent-schedules tbody');
    if (!tbody) return;

    const urgent = state.schedules.filter(s => s.status === 'Overdue' || s.status === 'Due');
    if (urgent.length === 0) {
        tbody.innerHTML = `<tr><td colspan="6" class="text-center text-muted py-4">All preventive schedules are up to date. No urgent attention required.</td></tr>`;
        return;
    }

    tbody.innerHTML = urgent.map(s => {
        let badge = 'badge-warning';
        if (s.status === 'Overdue') badge = 'badge-danger';

        return `
            <tr>
                <td class="primary-cell">${s.vehicle_reg}</td>
                <td><strong>${s.service_name}</strong></td>
                <td>${s.due_date}</td>
                <td class="font-mono">${(s.due_odometer_km || 0).toLocaleString()} km</td>
                <td><span class="badge ${badge}"><span class="badge-dot"></span>${s.status}</span></td>
                <td class="text-right">
                    <button class="btn btn-primary btn-sm" onclick="openLogMaintenanceModal(${s.vehicle_id}, '${s.service_name}')">
                        Record Service
                    </button>
                </td>
            </tr>
        `;
    }).join('');
}

// Reminders Tab Cards
function renderRemindersTab() {
    const container = document.getElementById('reminders-container');
    if (!container) return;

    const overdue = state.schedules.filter(s => s.status === 'Overdue');
    const due = state.schedules.filter(s => s.status === 'Due');
    const upcoming = state.schedules.filter(s => s.status === 'Scheduled');

    let html = '';

    if (overdue.length > 0) {
        html += `<h4 class="text-danger font-bold mb-4" style="display: flex; align-items: center; gap: 6px;">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"></circle><line x1="12" y1="8" x2="12" y2="12"></line><line x1="12" y1="16" x2="12.01" y2="16"></line></svg>
            CRITICAL OVERDUE MILESTONES (${overdue.length})
        </h4>`;
        html += '<div class="dsa-grid mb-4">' + overdue.map(s => `
            <div class="dsa-card" style="border-left: 4px solid var(--status-danger);">
                <div class="dsa-header">
                    <h4>${s.vehicle_reg} &bull; ${s.service_name}</h4>
                    <span class="badge badge-danger">OVERDUE</span>
                </div>
                <p class="dsa-desc">Due date was ${s.due_date}. Immediate preventative maintenance is required to prevent component failure.</p>
                <div class="flex-between">
                    <span class="font-mono text-sm">${(s.due_odometer_km || 0).toLocaleString()} km threshold</span>
                    <button class="btn btn-danger btn-sm" onclick="openLogMaintenanceModal(${s.vehicle_id}, '${s.service_name}')">Record Maintenance</button>
                </div>
            </div>
        `).join('') + '</div>';
    }

    if (due.length > 0) {
        html += `<h4 class="text-warning font-bold mb-4">DUE SOON MILESTONES (${due.length})</h4>`;
        html += '<div class="dsa-grid mb-4">' + due.map(s => `
            <div class="dsa-card" style="border-left: 4px solid var(--status-warning);">
                <div class="dsa-header">
                    <h4>${s.vehicle_reg} &bull; ${s.service_name}</h4>
                    <span class="badge badge-warning">DUE SOON</span>
                </div>
                <p class="dsa-desc">Due date: ${s.due_date} (${s.days_remaining} days remaining window).</p>
                <div class="flex-between">
                    <span class="font-mono text-sm">${(s.due_odometer_km || 0).toLocaleString()} km</span>
                    <button class="btn btn-primary btn-sm" onclick="openLogMaintenanceModal(${s.vehicle_id}, '${s.service_name}')">Record Maintenance</button>
                </div>
            </div>
        `).join('') + '</div>';
    }

    if (upcoming.length > 0) {
        html += `<h4 class="text-primary font-bold mb-4">UPCOMING PREVENTATIVE SCHEDULES (${upcoming.length})</h4>`;
        html += '<div class="dsa-grid">' + upcoming.slice(0, 6).map(s => `
            <div class="dsa-card">
                <div class="dsa-header">
                    <h4>${s.vehicle_reg} &bull; ${s.service_name}</h4>
                    <span class="badge badge-info">SCHEDULED</span>
                </div>
                <p class="dsa-desc">Due date: ${s.due_date} &bull; ${(s.due_odometer_km || 0).toLocaleString()} km</p>
                <div class="flex-between">
                    <span class="text-muted text-sm">${s.days_remaining || '-'} days remaining</span>
                    <button class="btn btn-outline btn-sm" onclick="openLogMaintenanceModal(${s.vehicle_id}, '${s.service_name}')">Log Early</button>
                </div>
            </div>
        `).join('') + '</div>';
    }

    if (!html) {
        html = '<div class="text-center text-muted py-4">No active reminders found. All fleet maintenance thresholds are compliant.</div>';
    }

    container.innerHTML = html;
}

// ==========================================================================
// 7. Drivers, Expenses, Notifications, & Audit
// ==========================================================================
function renderDriversTable() {
    const tbody = document.getElementById('tbody-drivers');
    if (!tbody) return;

    if (state.drivers.length === 0) {
        tbody.innerHTML = `<tr><td colspan="7" class="text-center text-muted py-4">No commercial drivers registered.</td></tr>`;
        return;
    }

    tbody.innerHTML = state.drivers.map(d => `
        <tr>
            <td class="font-mono">#${d.id}</td>
            <td class="primary-cell">${d.name}</td>
            <td>${d.phone}</td>
            <td class="font-mono">${d.license_number}</td>
            <td>${d.license_expiry}</td>
            <td>
                <span class="badge ${d.active ? 'badge-success' : 'badge-danger'}">
                    ${d.active ? 'Active' : 'Deactivated'}
                </span>
            </td>
            <td class="text-right">
                ${state.currentRole === 'Admin' ? `
                <button class="action-btn delete" title="Delete Driver" onclick="deleteDriver(${d.id})">
                    <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>
                </button>` : '<span class="text-muted text-sm">—</span>'}
            </td>
        </tr>
    `).join('');
}

function renderExpensesTable() {
    const tbody = document.getElementById('tbody-expenses');
    if (!tbody) return;

    if (state.expenses.length === 0) {
        tbody.innerHTML = `<tr><td colspan="6" class="text-center text-muted py-4">No operational expenses recorded.</td></tr>`;
        return;
    }

    tbody.innerHTML = state.expenses.map(e => `
        <tr>
            <td class="font-mono">#${e.id}</td>
            <td class="primary-cell">${e.vehicle_reg}</td>
            <td><span class="badge badge-info">${e.category}</span></td>
            <td>${e.description}</td>
            <td class="font-mono" style="font-weight: 700; color: var(--text-primary);">${e.amount_str}</td>
            <td>${e.date}</td>
        </tr>
    `).join('');
}

function renderNotifications() {
    const countBadge = document.getElementById('header-notif-badge');
    const flyoutCount = document.getElementById('flyout-count');
    const flyoutList = document.getElementById('flyout-notification-list');

    const allNotifs = state.notifications || [];
    const unreadCount = allNotifs.filter(n => !n.is_read).length;

    if (countBadge) {
        if (unreadCount > 0) {
            countBadge.textContent = unreadCount;
            countBadge.classList.remove('hidden');
        } else {
            countBadge.classList.add('hidden');
        }
    }

    if (flyoutCount) flyoutCount.textContent = `${unreadCount} unread`;

    if (flyoutList) {
        // Apply category / read filter
        let filtered = allNotifs;
        const f = state.notifFilter || 'all';
        if (f === 'unread') {
            filtered = allNotifs.filter(n => !n.is_read);
        } else if (f === 'maintenance') {
            filtered = allNotifs.filter(n => (n.category || '').toLowerCase() === 'maintenance');
        } else if (f === 'defects') {
            filtered = allNotifs.filter(n => (n.category || '').toLowerCase() === 'inspection' || (n.category || '').toLowerCase() === 'defect');
        } else if (f === 'work-orders') {
            filtered = allNotifs.filter(n => (n.category || '').toLowerCase().includes('work_order'));
        }

        if (filtered.length === 0) {
            flyoutList.innerHTML = `<div class="text-center py-4 text-muted text-sm">No notifications matching '${escapeHTML(f)}'.</div>`;
        } else {
            flyoutList.innerHTML = filtered.map(n => {
                const sev = (n.severity || 'info').toLowerCase();
                const unreadCls = !n.is_read ? 'unread' : '';
                const sevCls = `severity-${sev}`;
                return `
                    <div class="notif-card-item ${unreadCls} ${sevCls}">
                        <div style="flex: 1;">
                            <div style="display: flex; align-items: center; gap: 6px; margin-bottom: 2px;">
                                <span class="badge ${sev === 'critical' ? 'badge-danger' : (sev === 'warning' ? 'badge-warning' : 'badge-info')}" style="font-size: 10px; padding: 1px 5px;">${escapeHTML(n.severity || 'INFO')}</span>
                                <span style="font-weight: 600; font-size: 12px; color: var(--text-primary);">${escapeHTML(n.title)}</span>
                            </div>
                            <div style="font-size: 11.5px; color: var(--text-secondary); line-height: 1.35; margin-bottom: 4px;">${escapeHTML(n.message)}</div>
                            <div style="font-size: 10.5px; color: var(--text-muted);">${escapeHTML(n.created_at || 'Just now')}</div>
                        </div>
                        <div style="display: flex; flex-direction: column; align-items: flex-end; gap: 4px;">
                            ${!n.is_read ? `
                                <button class="btn btn-xs btn-outline" style="font-size: 10px; padding: 2px 6px;" onclick="ackNotification('${escapeHTML(n.event_key)}')">Ack</button>
                            ` : `<span class="text-xs text-muted" style="font-size: 10px;">Read</span>`}
                        </div>
                    </div>
                `;
            }).join('');
        }
    }
}

function renderAuditTable() {
    const tbody = document.getElementById('tbody-audit');
    if (!tbody) return;

    if (state.audit.length === 0) {
        tbody.innerHTML = `<tr><td colspan="7" class="text-center text-muted py-4">No audit logs available.</td></tr>`;
        return;
    }

    tbody.innerHTML = state.audit.map(a => `
        <tr>
            <td class="font-mono">#${a.id}</td>
            <td style="font-size: 11.5px;">${a.timestamp}</td>
            <td class="primary-cell">${a.username}</td>
            <td><span class="badge badge-muted">${a.action}</span></td>
            <td>${a.entity_type}</td>
            <td class="font-mono">#${a.entity_id}</td>
            <td style="font-size: 12px; color: var(--text-secondary);">${a.details}</td>
        </tr>
    `).join('');
}

// ==========================================================================
// 8. Modals, Forms & CRUD Workflows
// ==========================================================================
function initModals() {
    document.querySelectorAll('[data-close]').forEach(btn => {
        btn.addEventListener('click', () => {
            const modalId = btn.getAttribute('data-close');
            closeModal(modalId);
        });
    });

    document.querySelectorAll('.modal-backdrop').forEach(backdrop => {
        backdrop.addEventListener('click', (e) => {
            if (e.target === backdrop) closeModal(backdrop.id);
        });
    });

    document.getElementById('btn-add-vehicle-modal')?.addEventListener('click', () => openModal('modal-vehicle'));
    document.getElementById('btn-add-driver-modal')?.addEventListener('click', () => openModal('modal-driver'));
    document.getElementById('btn-log-service-modal')?.addEventListener('click', () => openLogMaintenanceModal());
    document.getElementById('btn-add-schedule-modal')?.addEventListener('click', () => openCreateScheduleModal());
    document.getElementById('btn-record-expense-modal')?.addEventListener('click', () => openRecordExpenseModal());
}

function openModal(modalId) {
    document.getElementById(modalId)?.classList.remove('hidden');
}

function closeModal(modalId) {
    document.getElementById(modalId)?.classList.add('hidden');
}

function closeAllModals() {
    document.querySelectorAll('.modal-backdrop').forEach(m => m.classList.add('hidden'));
}

function populateVehicleDropdowns() {
    const selects = ['m-vehicle-id', 's-vehicle-id', 'e-vehicle-id', 'wo-vehicle-id', 'insp-vehicle-id'];
    selects.forEach(selId => {
        const el = document.getElementById(selId);
        if (!el) return;
        el.innerHTML = state.vehicles.map(v => 
            `<option value="${v.id}">${v.registration_number} &mdash; ${v.make} ${v.model} (${(v.odometer_km || 0).toLocaleString()} km)</option>`
        ).join('');
    });
}

function populateDriverDropdowns() {
    const el = document.getElementById('assign-driver-select');
    if (!el) return;
    el.innerHTML = '<option value="0">-- Unassigned --</option>' + 
        state.drivers.filter(d => d.active).map(d => 
            `<option value="${d.id}">${d.name} (${d.license_number})</option>`
        ).join('');
}

function openLogMaintenanceModal(vehicleId = null, serviceName = '') {
    populateVehicleDropdowns();
    const today = new Date().toISOString().split('T')[0];
    const dateInput = document.getElementById('m-date');
    if (dateInput) dateInput.value = today;

    if (vehicleId) {
        const sel = document.getElementById('m-vehicle-id');
        if (sel) sel.value = vehicleId;
    }

    if (serviceName) {
        const descInput = document.getElementById('m-desc');
        if (descInput) descInput.value = `Completed service milestone: ${serviceName}`;
    }

    openModal('modal-maintenance');
}

function openCreateScheduleModal(vehicleId = null) {
    populateVehicleDropdowns();
    if (vehicleId) {
        const sel = document.getElementById('s-vehicle-id');
        if (sel) sel.value = vehicleId;
    }
    openModal('modal-schedule');
}

function openRecordExpenseModal(vehicleId = null) {
    populateVehicleDropdowns();
    const today = new Date().toISOString().split('T')[0];
    const dateInput = document.getElementById('e-date');
    if (dateInput) dateInput.value = today;

    if (vehicleId) {
        const sel = document.getElementById('e-vehicle-id');
        if (sel) sel.value = vehicleId;
    }
    openModal('modal-expense');
}

function openEditVehicleModal(vehicleId) {
    const v = state.vehicles.find(item => item.id === vehicleId);
    if (!v) return;

    document.getElementById('edit-v-id').value = v.id;
    document.getElementById('edit-v-reg').value = v.registration_number;
    document.getElementById('edit-v-make').value = v.make;
    document.getElementById('edit-v-model').value = v.model;
    document.getElementById('edit-v-year').value = v.year || 2022;
    document.getElementById('edit-v-status').value = v.status || 'Available';
    document.getElementById('edit-v-odometer').value = v.odometer_km || 0;
    document.getElementById('edit-v-fuel').value = v.fuel_percentage || 0;
    document.getElementById('edit-v-insurance').value = v.insurance_expiry || '';

    openModal('modal-edit-vehicle');
}

function openAssignDriverModal(vehicleId) {
    const v = state.vehicles.find(item => item.id === vehicleId);
    if (!v) return;

    populateDriverDropdowns();
    document.getElementById('assign-vehicle-id').value = v.id;
    document.getElementById('assign-vehicle-reg').value = `${v.registration_number} (${v.make} ${v.model})`;
    
    // Find driver id if assigned
    const driver = state.drivers.find(d => d.name === v.driver_name);
    document.getElementById('assign-driver-select').value = driver ? driver.id : '0';

    openModal('modal-assign-driver');
}

function initForms() {
    // Form: Add Vehicle
    document.getElementById('form-add-vehicle')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const payload = {
            registration_number: document.getElementById('v-reg').value.trim(),
            make: document.getElementById('v-make').value.trim(),
            model: document.getElementById('v-model').value.trim(),
            manufacturing_year: parseInt(document.getElementById('v-year').value, 10),
            vin: document.getElementById('v-vin').value.trim(),
            fuel_type: document.getElementById('v-fuel-type').value,
            fuel_percentage: parseInt(document.getElementById('v-fuel-pct').value, 10),
            odometer_km: parseInt(document.getElementById('v-odometer').value, 10),
            insurance_expiry: document.getElementById('v-insurance').value
        };

        try {
            const res = await fetchAPI('/api/vehicles', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const json = await res.json();
            if (json.success) {
                showAlert('Vehicle registered successfully.', 'success');
                closeModal('modal-vehicle');
                e.target.reset();
                loadVehicles();
                loadDashboard();
            } else {
                showAlert(json.message || 'Failed to create vehicle.', 'danger');
            }
        } catch (err) {
            showAlert('Server network error.', 'danger');
        }
    });

    // Form: Edit Vehicle
    document.getElementById('form-edit-vehicle')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const vid = parseInt(document.getElementById('edit-v-id').value, 10);
        const payload = {
            action: 'update',
            id: vid,
            make: document.getElementById('edit-v-make').value.trim(),
            model: document.getElementById('edit-v-model').value.trim(),
            manufacturing_year: parseInt(document.getElementById('edit-v-year').value, 10),
            status: document.getElementById('edit-v-status').value,
            odometer_km: parseInt(document.getElementById('edit-v-odometer').value, 10),
            fuel_percentage: parseInt(document.getElementById('edit-v-fuel').value, 10),
            insurance_expiry: document.getElementById('edit-v-insurance').value
        };

        try {
            const res = await fetchAPI('/api/vehicles', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const json = await res.json();
            if (json.success) {
                showAlert('Vehicle updated successfully.', 'success');
                closeModal('modal-edit-vehicle');
                loadVehicles();
                loadDashboard();
                if (state.activeVehicle && state.activeVehicle.id === vid) {
                    openVehicleDrawer(vid);
                }
            } else {
                showAlert(json.message || 'Failed to update vehicle.', 'danger');
            }
        } catch (err) {
            showAlert('Server network error.', 'danger');
        }
    });

    // Form: Add Driver
    document.getElementById('form-add-driver')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const payload = {
            name: document.getElementById('d-name').value.trim(),
            phone: document.getElementById('d-phone').value.trim(),
            license_number: document.getElementById('d-license').value.trim(),
            license_expiry: document.getElementById('d-expiry').value
        };

        try {
            const res = await fetchAPI('/api/drivers', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const json = await res.json();
            if (json.success) {
                showAlert('Driver profile created.', 'success');
                closeModal('modal-driver');
                e.target.reset();
                loadDrivers();
            } else {
                showAlert(json.message || 'Failed to create driver.', 'danger');
            }
        } catch (err) {
            showAlert('Server network error.', 'danger');
        }
    });

    // Form: Assign Driver
    document.getElementById('form-assign-driver')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const vid = parseInt(document.getElementById('assign-vehicle-id').value, 10);
        const did = parseInt(document.getElementById('assign-driver-select').value, 10);

        try {
            const res = await fetchAPI('/api/vehicles', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ action: 'assign_driver', vehicle_id: vid, driver_id: did })
            });
            const json = await res.json();
            if (json.success) {
                showAlert('Driver allocation updated.', 'success');
                closeModal('modal-assign-driver');
                loadVehicles();
                if (state.activeVehicle && state.activeVehicle.id === vid) {
                    openVehicleDrawer(vid);
                }
            } else {
                showAlert(json.message || 'Failed to assign driver.', 'danger');
            }
        } catch (err) {
            showAlert('Server network error.', 'danger');
        }
    });

    // Form: Log Maintenance
    document.getElementById('form-add-maintenance')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const payload = {
            vehicle_id: parseInt(document.getElementById('m-vehicle-id').value, 10),
            service_type: document.getElementById('m-service-type').value,
            service_date: document.getElementById('m-date').value,
            odometer_km: parseInt(document.getElementById('m-odometer').value, 10),
            workshop_name: document.getElementById('m-workshop').value.trim(),
            labour_cost: parseInt(document.getElementById('m-labour').value, 10) || 0,
            parts_cost: parseInt(document.getElementById('m-parts').value, 10) || 0,
            tax_amount: 0,
            description: document.getElementById('m-desc').value.trim()
        };

        try {
            const res = await fetchAPI('/api/maintenance', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const json = await res.json();
            if (json.success) {
                showAlert('Maintenance logged with linked expense.', 'success');
                closeModal('modal-maintenance');
                e.target.reset();
                loadAllData();
                evaluateThresholds();
            } else {
                showAlert(json.message || 'Failed to record maintenance.', 'danger');
            }
        } catch (err) {
            showAlert('Server network error.', 'danger');
        }
    });

    // Form: Create Schedule
    document.getElementById('form-add-schedule')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const payload = {
            vehicle_id: parseInt(document.getElementById('s-vehicle-id').value, 10),
            service_name: document.getElementById('s-name').value.trim(),
            due_date: document.getElementById('s-due-date').value,
            due_odometer_km: parseInt(document.getElementById('s-due-odometer').value, 10) || 0
        };

        try {
            const res = await fetchAPI('/api/schedules', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const json = await res.json();
            if (json.success) {
                showAlert('Preventive maintenance milestone scheduled.', 'success');
                closeModal('modal-schedule');
                e.target.reset();
                loadSchedules();
                loadDashboard();
            } else {
                showAlert(json.message || 'Failed to schedule milestone.', 'danger');
            }
        } catch (err) {
            showAlert('Server network error.', 'danger');
        }
    });

    // Form: Record Expense
    document.getElementById('form-add-expense')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const payload = {
            vehicle_id: parseInt(document.getElementById('e-vehicle-id').value, 10),
            category: document.getElementById('e-category').value,
            amount: parseInt(document.getElementById('e-amount').value, 10),
            expense_date: document.getElementById('e-date').value,
            description: document.getElementById('e-desc').value.trim()
        };

        try {
            const res = await fetchAPI('/api/expenses', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const json = await res.json();
            if (json.success) {
                showAlert('Operational expense recorded in SQLite ledger.', 'success');
                closeModal('modal-expense');
                e.target.reset();
                loadExpenses();
                loadDashboard();
            } else {
                showAlert(json.message || 'Failed to record expense.', 'danger');
            }
        } catch (err) {
            showAlert('Server network error.', 'danger');
        }
    });

    // Form: Create Work Order
    document.getElementById('form-add-work-order')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const payload = {
            vehicle_id: parseInt(document.getElementById('wo-vehicle-id').value, 10),
            title: document.getElementById('wo-title').value.trim(),
            description: document.getElementById('wo-description').value.trim(),
            priority: document.getElementById('wo-priority').value,
            assigned_to: document.getElementById('wo-assigned-to').value,
            estimated_parts_cost: parseInt(document.getElementById('wo-est-parts').value, 10) || 0,
            estimated_labour_cost: parseInt(document.getElementById('wo-est-labour').value, 10) || 0
        };

        try {
            const res = await fetchAPI('/api/work-orders', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const json = await res.json();
            if (json.success) {
                showAlert(`Work Order #${json.work_order_number || json.id} created successfully.`, 'success');
                closeModal('modal-work-order-add');
                e.target.reset();
                loadWorkOrders();
                loadDashboard();
            } else {
                showAlert(json.message || 'Failed to create work order.', 'danger');
            }
        } catch (err) {
            showAlert('Server network error.', 'danger');
        }
    });

    // Form: Complete Work Order
    document.getElementById('form-complete-wo')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const woId = parseInt(document.getElementById('wo-complete-id').value, 10);
        const payload = {
            action: 'complete',
            id: woId,
            odometer_km: parseInt(document.getElementById('wo-complete-odometer').value, 10) || 0,
            workshop_name: document.getElementById('wo-complete-workshop').value.trim(),
            labour_cost: parseInt(document.getElementById('wo-complete-labour').value, 10) || 0,
            parts_cost: parseInt(document.getElementById('wo-complete-parts').value, 10) || 0,
            notes: document.getElementById('wo-complete-notes').value.trim()
        };

        try {
            const res = await fetchAPI('/api/work-orders', {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const json = await res.json();
            if (json.success) {
                showAlert('Work Order completed. Service history and expenses reconciled.', 'success');
                closeModal('modal-work-order-detail');
                loadWorkOrders();
                loadMaintenance();
                loadSchedules();
                loadExpenses();
                loadDashboard();
                loadFleetAnalytics();
            } else {
                showAlert(json.message || 'Failed to complete work order.', 'danger');
            }
        } catch (err) {
            showAlert('Server network error.', 'danger');
        }
    });

    // Form: Submit Digital Inspection
    document.getElementById('form-add-inspection')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const vehicleId = parseInt(document.getElementById('insp-vehicle-id').value, 10);
        const inspectorName = document.getElementById('insp-inspector').value.trim();
        const inspectionDate = document.getElementById('insp-date').value;
        const odometerKm = parseInt(document.getElementById('insp-odometer').value, 10) || 0;
        const notes = document.getElementById('insp-general-notes').value.trim();

        const items = INSPECTION_ITEMS.map((def, idx) => {
            const status = document.querySelector(`input[name="insp_item_${idx}"]:checked`)?.value || 'Pass';
            const desc = status === 'Fail' ? (document.getElementById(`insp_defect_desc_${idx}`)?.value.trim() || 'Defect noted during inspection') : '';
            const severity = status === 'Fail' ? (document.getElementById(`insp_defect_sev_${idx}`)?.value || 'Major') : 'None';
            return {
                item_name: def.name,
                status: status,
                defect_description: desc,
                severity: severity
            };
        });

        const payload = {
            vehicle_id: vehicleId,
            inspector_name: inspectorName,
            inspection_date: inspectionDate,
            odometer_km: odometerKm,
            notes: notes,
            items: items
        };

        try {
            const res = await fetchAPI('/api/inspections', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            const json = await res.json();
            if (json.success) {
                showAlert(`Inspection recorded (${json.overall_status}). ${json.defects_found} defect(s) logged.`, json.overall_status === 'Pass' ? 'success' : 'warning');
                closeModal('modal-inspection-add');
                e.target.reset();
                initInspectionChecklistForm();
                loadInspections();
                loadDefects();
                loadDashboard();
            } else {
                showAlert(json.message || 'Failed to submit inspection.', 'danger');
            }
        } catch (err) {
            showAlert('Server network error.', 'danger');
        }
    });
}

// ==========================================================================
// 8.1 SMART FLEET MANAGEMENT: WORK ORDERS CONTROLLER
// ==========================================================================
async function loadWorkOrders() {
    try {
        const res = await fetchAPI('/api/work-orders');
        const json = await res.json();
        if (json.success && json.data) {
            state.workOrders = json.data;
            renderWorkOrdersTable();
            updateNavigationBadges();
        }
    } catch (e) {
        console.error('Failed to load work orders:', e);
    }
}

function renderWorkOrdersTable() {
    const tbody = document.getElementById('tbody-work-orders');
    if (!tbody) return;

    let items = state.workOrders || [];
    const search = (document.getElementById('wo-search-input')?.value || '').toLowerCase().trim();
    const statusFilter = document.getElementById('wo-status-filter')?.value || '';
    const priorityFilter = document.getElementById('wo-priority-filter')?.value || '';

    if (search) {
        items = items.filter(w => 
            (w.work_order_number && w.work_order_number.toLowerCase().includes(search)) ||
            (w.vehicle_reg && w.vehicle_reg.toLowerCase().includes(search)) ||
            (w.title && w.title.toLowerCase().includes(search)) ||
            (w.assigned_to && w.assigned_to.toLowerCase().includes(search))
        );
    }
    if (statusFilter) {
        items = items.filter(w => w.status === statusFilter);
    }
    if (priorityFilter) {
        items = items.filter(w => w.priority === priorityFilter);
    }

    if (items.length === 0) {
        tbody.innerHTML = '<tr><td colspan="9" class="text-center text-muted py-4">No work orders match current filters.</td></tr>';
        return;
    }

    tbody.innerHTML = items.map(w => {
        let pBadge = 'badge-priority-medium';
        if (w.priority === 'Low') pBadge = 'badge-priority-low';
        else if (w.priority === 'High') pBadge = 'badge-priority-high';
        else if (w.priority === 'Critical') pBadge = 'badge-priority-critical';

        let sBadge = 'badge-wo-open';
        if (w.status === 'Assigned') sBadge = 'badge-wo-assigned';
        else if (w.status === 'In Progress') sBadge = 'badge-wo-inprogress';
        else if (w.status === 'Waiting for Parts') sBadge = 'badge-wo-waiting';
        else if (w.status === 'Completed') sBadge = 'badge-wo-completed';
        else if (w.status === 'Cancelled') sBadge = 'badge-wo-cancelled';

        const totalCost = (w.actual_parts_cost || 0) + (w.actual_labour_cost || 0);
        const costStr = totalCost > 0 ? `Rs. ${totalCost.toLocaleString()}` : `Est: Rs. ${((w.estimated_parts_cost || 0) + (w.estimated_labour_cost || 0)).toLocaleString()}`;

        return `
            <tr>
                <td class="font-mono font-bold">${escapeHTML(w.work_order_number)}</td>
                <td class="primary-cell">${escapeHTML(w.vehicle_reg)}</td>
                <td>
                    <div style="font-weight: 600; color: var(--text-primary);">${escapeHTML(w.title)}</div>
                    <div style="font-size: 11px; color: var(--text-muted); max-width: 220px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;">${escapeHTML(w.description)}</div>
                </td>
                <td><span class="badge ${pBadge}">${escapeHTML(w.priority)}</span></td>
                <td><span class="badge ${sBadge}">${escapeHTML(w.status)}</span></td>
                <td>${escapeHTML(w.assigned_to || 'Unassigned')}</td>
                <td class="font-mono">${costStr}</td>
                <td style="font-size: 11.5px;">${escapeHTML(w.updated_at || w.created_at)}</td>
                <td class="text-right">
                    <button class="btn btn-secondary btn-sm" onclick="openWorkOrderDetail(${w.id})">Manage</button>
                </td>
            </tr>
        `;
    }).join('');
}

function openCreateWorkOrderModal() {
    populateVehicleDropdowns();
    openModal('modal-work-order-add');
}

async function openWorkOrderDetail(woId) {
    const wo = (state.workOrders || []).find(w => w.id === woId);
    if (!wo) return;
    state.activeWorkOrder = wo;

    document.getElementById('wo-detail-number').textContent = wo.work_order_number;
    document.getElementById('wo-detail-subtitle').textContent = `${wo.vehicle_reg} &bull; ${wo.title}`;
    document.getElementById('wo-detail-desc').textContent = wo.description;
    document.getElementById('wo-detail-created').textContent = wo.created_at;
    document.getElementById('wo-detail-tech').textContent = wo.assigned_to || 'Unassigned';
    
    const estTotal = (wo.estimated_labour_cost || 0) + (wo.estimated_parts_cost || 0);
    document.getElementById('wo-detail-est').textContent = `Rs. ${estTotal.toLocaleString()} (Labour: ${wo.estimated_labour_cost}, Parts: ${wo.estimated_parts_cost})`;

    // Status Badge
    let sBadge = 'badge-wo-open';
    if (wo.status === 'Assigned') sBadge = 'badge-wo-assigned';
    else if (wo.status === 'In Progress') sBadge = 'badge-wo-inprogress';
    else if (wo.status === 'Waiting for Parts') sBadge = 'badge-wo-waiting';
    else if (wo.status === 'Completed') sBadge = 'badge-wo-completed';
    else if (wo.status === 'Cancelled') sBadge = 'badge-wo-cancelled';
    const statusBadgeEl = document.getElementById('wo-detail-status-badge');
    if (statusBadgeEl) {
        statusBadgeEl.className = `badge ${sBadge}`;
        statusBadgeEl.textContent = wo.status;
    }

    // Priority Badge
    let pBadge = 'badge-priority-medium';
    if (wo.priority === 'Low') pBadge = 'badge-priority-low';
    else if (wo.priority === 'High') pBadge = 'badge-priority-high';
    else if (wo.priority === 'Critical') pBadge = 'badge-priority-critical';
    const pBadgeEl = document.getElementById('wo-detail-priority-badge');
    if (pBadgeEl) {
        pBadgeEl.className = `badge ${pBadge}`;
        pBadgeEl.textContent = wo.priority;
    }

    // Action buttons based on status
    renderStatusActionButtons(wo);

    // Hide completion form initially
    document.getElementById('wo-completion-panel')?.classList.add('hidden');

    // Load transition history
    loadWorkOrderHistory(woId);

    openModal('modal-work-order-detail');
}

function renderStatusActionButtons(wo) {
    const container = document.getElementById('wo-transition-buttons');
    if (!container) return;

    const status = wo.status;
    let buttons = '';

    if (status === 'Open') {
        buttons = `
            <button type="button" class="btn btn-secondary btn-sm" onclick="handleUpdateWorkOrderStatus(${wo.id}, 'Assigned')">Assign Technician</button>
            <button type="button" class="btn btn-primary btn-sm" onclick="handleUpdateWorkOrderStatus(${wo.id}, 'In Progress')">Start Workshop Work</button>
            <button type="button" class="btn btn-danger btn-sm" onclick="handleUpdateWorkOrderStatus(${wo.id}, 'Cancelled')">Cancel Work Order</button>
        `;
    } else if (status === 'Assigned') {
        buttons = `
            <button type="button" class="btn btn-primary btn-sm" onclick="handleUpdateWorkOrderStatus(${wo.id}, 'In Progress')">Start Workshop Work</button>
            <button type="button" class="btn btn-secondary btn-sm" onclick="handleUpdateWorkOrderStatus(${wo.id}, 'Waiting for Parts')">Hold: Waiting for Parts</button>
            <button type="button" class="btn btn-danger btn-sm" onclick="handleUpdateWorkOrderStatus(${wo.id}, 'Cancelled')">Cancel Order</button>
        `;
    } else if (status === 'In Progress') {
        buttons = `
            <button type="button" class="btn btn-secondary btn-sm" onclick="handleUpdateWorkOrderStatus(${wo.id}, 'Waiting for Parts')">Hold: Waiting for Parts</button>
            <button type="button" class="btn btn-primary btn-sm" style="background: #16A34A; border-color: #16A34A;" onclick="showWorkOrderCompletionForm(${wo.id})">Complete Work Order</button>
            <button type="button" class="btn btn-danger btn-sm" onclick="handleUpdateWorkOrderStatus(${wo.id}, 'Cancelled')">Cancel Order</button>
        `;
    } else if (status === 'Waiting for Parts') {
        buttons = `
            <button type="button" class="btn btn-primary btn-sm" onclick="handleUpdateWorkOrderStatus(${wo.id}, 'In Progress')">Parts Received &mdash; Resume Work</button>
            <button type="button" class="btn btn-primary btn-sm" style="background: #16A34A; border-color: #16A34A;" onclick="showWorkOrderCompletionForm(${wo.id})">Complete Work Order</button>
            <button type="button" class="btn btn-danger btn-sm" onclick="handleUpdateWorkOrderStatus(${wo.id}, 'Cancelled')">Cancel Order</button>
        `;
    } else if (status === 'Completed') {
        buttons = `<span class="badge badge-success" style="font-size: 12px; padding: 5px 12px;">Work Order Completed & Closed</span>`;
    } else if (status === 'Cancelled') {
        buttons = `<span class="badge badge-danger" style="font-size: 12px; padding: 5px 12px;">Work Order Cancelled</span>`;
    }

    container.innerHTML = buttons;
}

function showWorkOrderCompletionForm(woId) {
    const panel = document.getElementById('wo-completion-panel');
    if (!panel) return;
    document.getElementById('wo-complete-id').value = woId;
    
    // Auto-fill vehicle current odometer if known
    const wo = (state.workOrders || []).find(w => w.id === woId);
    const v = (state.vehicles || []).find(veh => veh.id === wo?.vehicle_id);
    document.getElementById('wo-complete-odometer').value = v ? (v.odometer_km || 0) : 0;
    document.getElementById('wo-complete-labour').value = wo ? (wo.estimated_labour_cost || 0) : 0;
    document.getElementById('wo-complete-parts').value = wo ? (wo.estimated_parts_cost || 0) : 0;
    document.getElementById('wo-complete-notes').value = `Repairs completed according to work order specification ${wo?.work_order_number || ''}.`;

    panel.classList.remove('hidden');
    panel.scrollIntoView({ behavior: 'smooth' });
}

async function handleUpdateWorkOrderStatus(woId, newStatus) {
    try {
        const res = await fetchAPI('/api/work-orders', {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                action: 'update_status',
                id: woId,
                status: newStatus,
                notes: `Transitioned to ${newStatus}`
            })
        });
        const json = await res.json();
        if (json.success) {
            showAlert(`Work Order status transitioned to ${newStatus}.`, 'success');
            await loadWorkOrders();
            openWorkOrderDetail(woId);
            loadDashboard();
        } else {
            showAlert(json.message || 'Status transition rejected.', 'danger');
        }
    } catch (err) {
        showAlert('Server communication error.', 'danger');
    }
}

async function loadWorkOrderHistory(woId) {
    const container = document.getElementById('wo-history-timeline');
    if (!container) return;

    try {
        const res = await fetchAPI(`/api/work-orders/history?work_order_id=${woId}`);
        const json = await res.json();
        if (json.success && json.data && json.data.length > 0) {
            container.innerHTML = json.data.map(h => `
                <div class="timeline-item">
                    <div class="timeline-dot"></div>
                    <div>
                        <strong>${escapeHTML(h.old_status)} &rarr; ${escapeHTML(h.new_status)}</strong>
                        <div class="timeline-meta">${escapeHTML(h.changed_by)} &bull; ${escapeHTML(h.timestamp)}</div>
                        ${h.notes ? `<div style="font-size: 11.5px; color: var(--text-secondary); margin-top: 2px;">${escapeHTML(h.notes)}</div>` : ''}
                    </div>
                </div>
            `).join('');
        } else {
            container.innerHTML = '<div class="text-center text-muted text-xs py-2">Initial order creation recorded.</div>';
        }
    } catch (e) {
        container.innerHTML = '<div class="text-center text-muted text-xs py-2">History unavailable.</div>';
    }
}

// ==========================================================================
// 8.2 SMART FLEET MANAGEMENT: DIGITAL VEHICLE INSPECTION CONTROLLER
// ==========================================================================
const INSPECTION_ITEMS = [
    { name: 'Brakes & Calipers', critical: true },
    { name: 'Tyres & Tread Depth', critical: true },
    { name: 'Lights, Indicators & Horn', critical: false },
    { name: 'Engine & Exhaust Diagnostics', critical: true },
    { name: 'Battery & Electrical System', critical: false },
    { name: 'Steering & Suspension Alignment', critical: true },
    { name: 'Fluid Levels (Oil/Coolant/Brake)', critical: false },
    { name: 'Registration & Insurance Documents', critical: false }
];

function initInspectionChecklistForm() {
    const tbody = document.getElementById('insp-checklist-tbody');
    if (!tbody) return;

    tbody.innerHTML = INSPECTION_ITEMS.map((item, idx) => `
        <tr class="checklist-row">
            <td>
                <strong>${escapeHTML(item.name)}</strong>
                ${item.critical ? '<span class="badge badge-danger" style="margin-left: 4px; font-size: 10px; padding: 1px 4px;">Critical</span>' : ''}
            </td>
            <td>
                <div class="checklist-radio-group">
                    <label><input type="radio" name="insp_item_${idx}" value="Pass" checked onchange="toggleDefectInput(${idx})" /> Pass</label>
                    <label><input type="radio" name="insp_item_${idx}" value="Fail" onchange="toggleDefectInput(${idx})" /> <span style="color: #DC2626; font-weight: 600;">Fail</span></label>
                    <label><input type="radio" name="insp_item_${idx}" value="N/A" onchange="toggleDefectInput(${idx})" /> N/A</label>
                </div>
            </td>
            <td>
                <div id="insp_defect_box_${idx}" class="hidden" style="display: flex; gap: 8px; align-items: center;">
                    <input type="text" id="insp_defect_desc_${idx}" placeholder="Describe specific mechanical defect..." style="flex: 1; height: 30px; font-size: 12px; padding: 0 8px; border-radius: 4px; border: 1px solid var(--border-color);" />
                    <select id="insp_defect_sev_${idx}" style="height: 30px; font-size: 12px; padding: 0 6px; border-radius: 4px; border: 1px solid var(--border-color);">
                        <option value="Minor">Minor</option>
                        <option value="Major" selected>Major</option>
                        <option value="Critical">Critical</option>
                    </select>
                </div>
            </td>
        </tr>
    `).join('');
}

function toggleDefectInput(idx) {
    const selected = document.querySelector(`input[name="insp_item_${idx}"]:checked`)?.value;
    const box = document.getElementById(`insp_defect_box_${idx}`);
    if (box) {
        if (selected === 'Fail') {
            box.classList.remove('hidden');
        } else {
            box.classList.add('hidden');
        }
    }
}

async function loadInspections() {
    try {
        const res = await fetchAPI('/api/inspections');
        const json = await res.json();
        if (json.success && json.data) {
            state.inspections = json.data;
            renderInspectionsTable();
            updateNavigationBadges();
        }
    } catch (e) {
        console.error('Failed to load inspections:', e);
    }
}

function renderInspectionsTable() {
    const tbody = document.getElementById('tbody-inspections');
    if (!tbody) return;

    let items = state.inspections || [];
    const statusFilter = document.getElementById('insp-status-filter')?.value || '';
    if (statusFilter) {
        items = items.filter(i => i.overall_status === statusFilter);
    }

    if (items.length === 0) {
        tbody.innerHTML = '<tr><td colspan="8" class="text-center text-muted py-4">No inspection records found.</td></tr>';
        return;
    }

    tbody.innerHTML = items.map(i => {
        let badge = 'badge-insp-pass';
        if (i.overall_status === 'Defective') badge = 'badge-insp-defective';
        else if (i.overall_status === 'Fail') badge = 'badge-insp-fail';

        return `
            <tr>
                <td class="font-mono font-bold">#INSP-${i.id}</td>
                <td class="primary-cell">${escapeHTML(i.vehicle_reg)}</td>
                <td>${escapeHTML(i.inspector_name)}</td>
                <td>${escapeHTML(i.inspection_date)}</td>
                <td class="font-mono">${(i.odometer_km || 0).toLocaleString()} km</td>
                <td><span class="badge ${badge}">${escapeHTML(i.overall_status)}</span></td>
                <td>
                    ${i.defects_found > 0 ? 
                        `<span class="badge badge-danger">${i.defects_found} defect(s)</span>` : 
                        `<span class="text-muted text-xs">Zero defects</span>`}
                </td>
                <td class="text-right">
                    <button class="btn btn-secondary btn-sm" onclick="openInspectionDetail(${i.id})">View Checklist</button>
                </td>
            </tr>
        `;
    }).join('');
}

async function loadDefects() {
    try {
        const res = await fetchAPI('/api/inspections/defects');
        const json = await res.json();
        if (json.success && json.data) {
            state.defects = json.data;
            renderUnresolvedDefects();
            updateNavigationBadges();
        }
    } catch (e) {
        console.error('Failed to load defects:', e);
    }
}

function renderUnresolvedDefects() {
    const container = document.getElementById('inspection-defects-container');
    if (!container) return;

    const defects = state.defects || [];
    if (defects.length === 0) {
        container.innerHTML = '<div class="text-muted text-sm py-2">No active unresolved defects. Fleet safety criteria met.</div>';
        return;
    }

    container.innerHTML = defects.map(d => `
        <div class="defect-card">
            <div>
                <div class="defect-info-title">
                    <span>${escapeHTML(d.vehicle_reg)} &mdash; ${escapeHTML(d.item_name)}</span>
                    <span class="badge badge-danger">${escapeHTML(d.severity || 'Critical')}</span>
                </div>
                <div class="defect-info-desc">
                    ${escapeHTML(d.defect_description)} &bull; Logged ${escapeHTML(d.inspection_date)} by ${escapeHTML(d.inspector_name)}
                </div>
            </div>
            <div>
                <button class="btn btn-danger btn-sm" onclick="handleCreateWorkOrderFromDefect(${d.inspection_id}, ${d.item_index})">
                    Create Repair Work Order
                </button>
            </div>
        </div>
    `).join('');
}

function openCreateInspectionModal() {
    populateVehicleDropdowns();
    const today = new Date().toISOString().split('T')[0];
    document.getElementById('insp-date').value = today;
    initInspectionChecklistForm();
    openModal('modal-inspection-add');
}

function openInspectionDetail(inspId) {
    const insp = (state.inspections || []).find(i => i.id === inspId);
    if (!insp) return;

    document.getElementById('insp-detail-title').textContent = `Inspection #INSP-${insp.id} &bull; ${insp.vehicle_reg}`;
    document.getElementById('insp-detail-subtitle').textContent = `Conducted by ${insp.inspector_name} on ${insp.inspection_date}`;
    document.getElementById('insp-detail-odometer').textContent = `${(insp.odometer_km || 0).toLocaleString()} km`;
    document.getElementById('insp-detail-defects-count').textContent = insp.defects_found || 0;

    let badge = 'badge-insp-pass';
    if (insp.overall_status === 'Defective') badge = 'badge-insp-defective';
    else if (insp.overall_status === 'Fail') badge = 'badge-insp-fail';
    const statusEl = document.getElementById('insp-detail-status-badge');
    if (statusEl) {
        statusEl.className = `badge ${badge}`;
        statusEl.textContent = insp.overall_status;
    }

    const tbody = document.getElementById('tbody-inspection-items-detail');
    if (tbody && insp.items) {
        tbody.innerHTML = insp.items.map((item, idx) => {
            const isFail = item.status === 'Fail';
            return `
                <tr>
                    <td class="font-bold">${escapeHTML(item.item_name)}</td>
                    <td>
                        <span class="badge ${isFail ? 'badge-danger' : (item.status === 'Pass' ? 'badge-success' : 'badge-muted')}">
                            ${escapeHTML(item.status)}
                        </span>
                    </td>
                    <td>${item.severity !== 'None' ? `<span class="badge badge-warning">${escapeHTML(item.severity)}</span>` : '-'}</td>
                    <td style="font-size: 12px;">${escapeHTML(item.defect_description || '-')}</td>
                    <td class="text-right">
                        ${isFail ? `
                            <button class="btn btn-outline btn-sm" onclick="handleCreateWorkOrderFromDefect(${insp.id}, ${idx})">
                                Create Work Order
                            </button>
                        ` : '<span class="text-muted text-xs">&mdash;</span>'}
                    </td>
                </tr>
            `;
        }).join('');
    }

    openModal('modal-inspection-detail');
}

async function handleCreateWorkOrderFromDefect(inspId, itemIndex) {
    if (!confirm('Escalate this flagged defect to an active workshop Work Order?')) return;

    try {
        const res = await fetchAPI('/api/inspections/create-work-order', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                inspection_id: inspId,
                item_index: itemIndex,
                assigned_to: 'Amit Patil (Brakes & Suspension)'
            })
        });
        const json = await res.json();
        if (json.success) {
            showAlert(`Repair Work Order created: ${json.work_order_number || ('#' + json.work_order_id)}`, 'success');
            closeModal('modal-inspection-detail');
            await loadDefects();
            await loadWorkOrders();
            loadDashboard();
            switchTab('work-orders');
        } else {
            showAlert(json.message || 'Failed to create work order from defect.', 'danger');
        }
    } catch (err) {
        showAlert('Server network error.', 'danger');
    }
}

// ==========================================================================
// 8.3 SMART FLEET MANAGEMENT: COST ANALYTICS CONTROLLER
// ==========================================================================
async function loadFleetAnalytics() {
    try {
        const res = await fetchAPI('/api/analytics');
        const json = await res.json();
        if (json.success && json.analytics) {
            state.analytics = json.analytics;
            renderAnalyticsDashboard(json.analytics);
        }
    } catch (e) {
        console.error('Failed to load fleet analytics:', e);
    }
}

function renderAnalyticsDashboard(a) {
    if (!a) return;

    // KPI Cards
    const maintEl = document.getElementById('analytics-total-maint');
    if (maintEl) maintEl.textContent = `Rs. ${(a.total_maintenance_cost || 0).toLocaleString()}`;
    const expEl = document.getElementById('analytics-total-expenses');
    if (expEl) expEl.textContent = `Rs. ${(a.total_operating_expenses || 0).toLocaleString()}`;
    const spendEl = document.getElementById('analytics-total-spend');
    if (spendEl) spendEl.textContent = `Rs. ${(a.total_fleet_spend || 0).toLocaleString()}`;
    const avgEl = document.getElementById('analytics-avg-service');
    if (avgEl) avgEl.textContent = `Rs. ${Math.round(a.average_service_cost || 0).toLocaleString()}`;

    // Efficiency Metrics
    const totalWoEl = document.getElementById('stat-wo-total');
    if (totalWoEl) totalWoEl.textContent = (a.open_work_orders || 0) + (a.completed_work_orders || 0);
    const openWoEl = document.getElementById('stat-wo-open');
    if (openWoEl) openWoEl.textContent = a.open_work_orders || 0;
    const compWoEl = document.getElementById('stat-wo-completed');
    if (compWoEl) compWoEl.textContent = a.completed_work_orders || 0;

    // Preventive Schedule Compliance
    const totalSchedules = (a.upcoming_maintenance || 0) + (a.overdue_maintenance || 0);
    const compliancePct = totalSchedules > 0 ? Math.max(0, Math.round(((totalSchedules - a.overdue_maintenance) / totalSchedules) * 100)) : 100;
    const compPctEl = document.getElementById('stat-compliance-pct');
    if (compPctEl) compPctEl.textContent = `${compliancePct}%`;

    // Category Breakdown Bars
    const catContainer = document.getElementById('analytics-category-container');
    if (catContainer && a.categories) {
        const totalCatSpend = a.categories.reduce((acc, c) => acc + (c.total_amount || 0), 0) || 1;
        catContainer.innerHTML = a.categories.map(c => {
            const pct = Math.min(100, Math.round(((c.total_amount || 0) / totalCatSpend) * 100));
            return `
                <div class="category-bar-row">
                    <div class="category-bar-label">
                        <span>${escapeHTML(c.category)} (${c.record_count} services)</span>
                        <span class="font-bold">Rs. ${(c.total_amount || 0).toLocaleString()} (${pct}%)</span>
                    </div>
                    <div class="category-bar-track">
                        <div class="category-bar-fill" style="width: ${pct}%; background-color: var(--primary);"></div>
                    </div>
                </div>
            `;
        }).join('');
    }

    // Top 5 Vehicles
    const topVBody = document.getElementById('tbody-top-vehicles');
    if (topVBody && a.top_vehicles) {
        if (a.top_vehicles.length === 0) {
            topVBody.innerHTML = '<tr><td colspan="3" class="text-center text-muted py-3">No maintenance recorded yet.</td></tr>';
        } else {
            topVBody.innerHTML = a.top_vehicles.map(v => `
                <tr>
                    <td class="font-mono font-bold">${escapeHTML(v.registration_number)}</td>
                    <td>${escapeHTML(v.make)} ${escapeHTML(v.model)}</td>
                    <td class="font-mono font-bold text-right">Rs. ${(v.total_maintenance || 0).toLocaleString()}</td>
                </tr>
            `).join('');
        }
    }

    // Monthly Spend
    const monthBody = document.getElementById('tbody-monthly-spending');
    if (monthBody && a.monthly_spending) {
        if (a.monthly_spending.length === 0) {
            monthBody.innerHTML = '<tr><td colspan="2" class="text-center text-muted py-3">No expenditure recorded yet.</td></tr>';
        } else {
            monthBody.innerHTML = a.monthly_spending.map(m => `
                <tr>
                    <td class="font-bold">${escapeHTML(m.month)}</td>
                    <td class="font-mono font-bold text-right">Rs. ${(m.total_amount || 0).toLocaleString()}</td>
                </tr>
            `).join('');
        }
    }
}

function updateNavigationBadges() {
    const openWOs = (state.workOrders || []).filter(w => w.status !== 'Completed' && w.status !== 'Cancelled').length;
    const badgeWO = document.getElementById('nav-badge-work-orders');
    if (badgeWO) badgeWO.textContent = openWOs;

    const openDefects = (state.defects || []).length;
    const badgeInsp = document.getElementById('nav-badge-inspections');
    if (badgeInsp) {
        badgeInsp.textContent = openDefects;
        if (openDefects > 0) badgeInsp.classList.add('danger');
        else badgeInsp.classList.remove('danger');
    }
}

// ==========================================================================
// 9. Operations: Delete, Complete, Evaluate, Backup, DSA
// ==========================================================================
async function deleteVehicle(vehicleId) {
    if (!confirm('Are you sure you want to permanently delete this vehicle? (Vehicles with linked maintenance records must be archived instead)')) {
        return;
    }

    try {
        const res = await fetchAPI(`/api/vehicles?id=${vehicleId}`, { method: 'DELETE' });
        const json = await res.json();
        if (json.success) {
            showAlert('Vehicle deleted.', 'success');
            loadVehicles();
            loadDashboard();
            closeVehicleDrawer();
        } else {
            showAlert(json.message || 'Cannot delete vehicle.', 'danger');
        }
    } catch (e) {
        showAlert('Network error deleting vehicle.', 'danger');
    }
}

async function deleteDriver(driverId) {
    if (!confirm('Are you sure you want to delete this commercial driver?')) return;

    try {
        const res = await fetchAPI(`/api/drivers?id=${driverId}`, { method: 'DELETE' });
        const json = await res.json();
        if (json.success) {
            showAlert('Driver deleted.', 'success');
            loadDrivers();
        } else {
            showAlert(json.message || 'Cannot delete driver assigned to an active vehicle.', 'danger');
        }
    } catch (e) {
        showAlert('Network error deleting driver.', 'danger');
    }
}

async function completeSchedule(scheduleId) {
    try {
        const res = await fetchAPI('/api/schedules', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'complete', id: scheduleId })
        });
        const json = await res.json();
        if (json.success) {
            showAlert('Schedule milestone marked as completed.', 'success');
            loadSchedules();
            loadDashboard();
        }
    } catch (e) {
        showAlert('Failed to complete schedule.', 'danger');
    }
}

async function cancelSchedule(scheduleId) {
    if (!confirm('Cancel this maintenance schedule?')) return;
    try {
        const res = await fetchAPI('/api/schedules', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ action: 'cancel', id: scheduleId })
        });
        const json = await res.json();
        if (json.success) {
            showAlert('Schedule cancelled.', 'info');
            loadSchedules();
            loadDashboard();
        }
    } catch (e) {
        showAlert('Failed to cancel schedule.', 'danger');
    }
}

async function evaluateThresholds() {
    try {
        const res = await fetchAPI('/api/evaluate', { method: 'POST' });
        const json = await res.json();
        if (json.success) {
            showAlert(`Evaluated schedules: ${json.updated_count || 0} statuses updated, ${json.notification_count || 0} alerts active.`, 'success');
            loadAllData();
        }
    } catch (e) {
        showAlert('Failed to evaluate thresholds.', 'danger');
    }
}

async function triggerHotBackup() {
    try {
        showAlert('Executing hot online backup via SQLite Backup API...', 'info');
        const res = await fetchAPI('/api/backup', { method: 'POST' });
        const json = await res.json();
        if (json.success) {
            showAlert(`Hot backup created: ${json.filepath}`, 'success');
            const box = document.getElementById('backup-result-box');
            if (box) box.textContent = `[SUCCESS] Database snapshot generated at: ${json.filepath}\nVerified with PRAGMA integrity_check.`;
        } else {
            showAlert('Hot backup failed.', 'danger');
        }
    } catch (e) {
        showAlert('Network error during backup.', 'danger');
    }
}

async function runDsaBenchmarks() {
    try {
        showAlert('Executing pure C11 data structures and optimization algorithms...', 'info');
        const res = await fetchAPI('/api/dsa/run');
        const json = await res.json();
        if (json.success && json.algorithms) {
            const a = json.algorithms;
            if (a.hash_table) document.getElementById('dsa-ht-metric').textContent = `Indexed: ${a.hash_table.vehicles_indexed} vehicles (${a.hash_table.complexity})`;
            if (a.dijkstra) document.getElementById('dsa-dijkstra-metric').textContent = `${a.dijkstra.route} (${a.dijkstra.shortest_dist_km} km)`;
            if (a.dp_knapsack) document.getElementById('dsa-knapsack-metric').textContent = `Max Score: ${a.dp_knapsack.max_reliability_score} pts (${a.dp_knapsack.jobs_selected} jobs)`;
            if (a.tsp_branch_bound) document.getElementById('dsa-tsp-metric').textContent = `Tour: ${a.tsp_branch_bound.optimal_tour_distance_km} km`;
            if (a.merge_sort) document.getElementById('dsa-mergesort-metric').textContent = `Sorted: ${a.merge_sort.items_sorted} assets (${a.merge_sort.complexity})`;
            if (a.queue) document.getElementById('dsa-queue-metric').textContent = `Dispatched: ${a.queue.tasks_dispatched} tasks (${a.queue.complexity})`;
            showAlert('C11 DSA Algorithm Suite benchmarks executed in memory.', 'success');
        }
    } catch (e) {
        showAlert('Failed to execute DSA benchmarks.', 'danger');
    }
}

// ==========================================================================
// 10. CSV Export Utilities (Real SQLite Data to Compliant CSV)
// ==========================================================================
function downloadCSV(filename, csvContent) {
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const link = document.createElement('a');
    const url = URL.createObjectURL(blob);
    link.setAttribute('href', url);
    link.setAttribute('download', filename);
    link.style.visibility = 'hidden';
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
}

function escapeCSV(val) {
    if (val === null || val === undefined) return '""';
    let s = val.toString();
    // Formula Injection Protection
    if (s.startsWith('=') || s.startsWith('+') || s.startsWith('-') || s.startsWith('@')) {
        s = "'" + s;
    }
    return `"${s.replace(/"/g, '""')}"`;
}

function exportVehiclesCSV() {
    const headers = ['ID', 'Registration Number', 'Make', 'Model', 'Year', 'VIN', 'Fuel Type', 'Fuel Level (%)', 'Odometer (km)', 'Status', 'Driver', 'Insurance Expiry'];
    const rows = state.vehicles.map(v => [
        escapeCSV(v.id),
        escapeCSV(v.registration_number),
        escapeCSV(v.make),
        escapeCSV(v.model),
        escapeCSV(v.year),
        escapeCSV(v.vin),
        escapeCSV(v.fuel_type),
        escapeCSV(v.fuel_percentage),
        escapeCSV(v.odometer_km),
        escapeCSV(v.status),
        escapeCSV(v.driver_name),
        escapeCSV(v.insurance_expiry)
    ].join(','));

    const csv = [headers.join(','), ...rows].join('\r\n');
    downloadCSV(`fleet_vehicles_${new Date().toISOString().split('T')[0]}.csv`, csv);
    showAlert('Vehicles master CSV exported.', 'success');
}

function exportMaintenanceCSV() {
    const headers = ['ID', 'Vehicle Reg', 'Service Type', 'Date', 'Odometer (km)', 'Workshop', 'Work Description', 'Total Cost'];
    const rows = state.maintenance.map(m => [
        escapeCSV(m.id),
        escapeCSV(m.vehicle_reg),
        escapeCSV(m.service_type),
        escapeCSV(m.service_date),
        escapeCSV(m.odometer_km),
        escapeCSV(m.workshop_name),
        escapeCSV(m.description),
        escapeCSV(m.total_cost_str)
    ].join(','));

    const csv = [headers.join(','), ...rows].join('\r\n');
    downloadCSV(`maintenance_history_${new Date().toISOString().split('T')[0]}.csv`, csv);
    showAlert('Maintenance logs CSV exported.', 'success');
}

function exportExpensesCSV() {
    const headers = ['ID', 'Vehicle Reg', 'Category', 'Description', 'Amount', 'Date'];
    const rows = state.expenses.map(e => [
        escapeCSV(e.id),
        escapeCSV(e.vehicle_reg),
        escapeCSV(e.category),
        escapeCSV(e.description),
        escapeCSV(e.amount_str),
        escapeCSV(e.date)
    ].join(','));

    const csv = [headers.join(','), ...rows].join('\r\n');
    downloadCSV(`operating_expenses_${new Date().toISOString().split('T')[0]}.csv`, csv);
    showAlert('Operating expenses CSV exported.', 'success');
}

function exportSchedulesCSV() {
    const headers = ['ID', 'Vehicle Reg', 'Service Milestone', 'Due Date', 'Due Odometer (km)', 'Status', 'Days Remaining'];
    const rows = state.schedules.map(s => [
        escapeCSV(s.id),
        escapeCSV(s.vehicle_reg),
        escapeCSV(s.service_name),
        escapeCSV(s.due_date),
        escapeCSV(s.due_odometer_km),
        escapeCSV(s.status),
        escapeCSV(s.days_remaining)
    ].join(','));

    const csv = [headers.join(','), ...rows].join('\r\n');
    downloadCSV(`preventive_schedules_${new Date().toISOString().split('T')[0]}.csv`, csv);
    showAlert('Preventive schedules CSV exported.', 'success');
}

// ==========================================================================
// 11. Alerts & Notifications Banner
// ==========================================================================
function showAlert(message, type = 'info') {
    const banner = document.getElementById('alert-banner');
    if (!banner) return;
    banner.textContent = message;
    banner.className = `alert-banner ${type}`;
    banner.classList.remove('hidden');
    setTimeout(() => {
        banner.classList.add('hidden');
    }, 4000);
}

// ==========================================================================
// 12. Authentication & Session Controller
// ==========================================================================
async function handleLoginSubmit(e) {
    e.preventDefault();
    const errBox = document.getElementById('login-error-msg');
    if (errBox) errBox.classList.add('hidden');

    const username = document.getElementById('login-username').value.trim();
    const password = document.getElementById('login-password').value;

    try {
        const res = await fetch('/api/auth/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username, password })
        });
        const json = await res.json();
        if (json.success && json.user) {
            applyUserSession(json.user, json.token);
            closeModal('modal-auth-login');
            showAlert(`Authenticated successfully as ${json.user.username} (${json.user.role})`, 'success');
            await loadAllData();
        } else {
            if (errBox) {
                errBox.textContent = json.message || 'Invalid username or password.';
                errBox.classList.remove('hidden');
            }
        }
    } catch (err) {
        if (errBox) {
            errBox.textContent = 'Server connection error.';
            errBox.classList.remove('hidden');
        }
    }
}

// ==========================================================================
// LIVE FLEET MONITORING & TELEMETRY ENGINE
// ==========================================================================

function initLiveMonitoring() {
    const pollSelect = document.getElementById('poll-interval-select');
    if (pollSelect) {
        pollSelect.addEventListener('change', (e) => {
            const ms = parseInt(e.target.value, 10);
            state.pollIntervalMs = ms;
            restartPollingTimer();
            showAlert(`Polling interval set to ${ms > 0 ? (ms/1000) + 's' : 'Manual (Off)'}.`, 'info');
        });
    }

    document.getElementById('btn-retry-live-refresh')?.addEventListener('click', () => {
        loadLiveMonitoringStatus();
    });

    // Start background polling
    restartPollingTimer();
}

function restartPollingTimer() {
    if (state.pollTimerId) {
        clearInterval(state.pollTimerId);
        state.pollTimerId = null;
    }
    if (state.pollIntervalMs > 0) {
        state.pollTimerId = setInterval(() => {
            loadLiveMonitoringStatus();
        }, state.pollIntervalMs);
    }
}

async function loadLiveMonitoringStatus() {
    if (state.isLivePolling) return; // Prevent overlapping requests
    state.isLivePolling = true;

    const dot = document.getElementById('live-indicator-dot');
    const refreshText = document.getElementById('live-last-refresh');
    const staleBanner = document.getElementById('stale-data-banner');
    const refreshBtn = document.getElementById('btn-refresh-data');

    if (refreshBtn) refreshBtn.classList.add('btn-spin');

    try {
        const res = await fetchAPI('/api/live-status');
        if (!res.ok) throw new Error(`Server returned HTTP ${res.status}`);
        const json = await res.json();
        if (json.success) {
            state.lastSuccessfulSync = new Date();
            const timeStr = state.lastSuccessfulSync.toLocaleTimeString();

            if (dot) {
                dot.classList.add('pulse');
                dot.classList.remove('stale');
                dot.title = `Live C11 Engine Connected (Uptime: ${json.uptime_seconds || 0}s)`;
            }
            if (refreshText) refreshText.textContent = timeStr;
            if (staleBanner) staleBanner.classList.add('hidden');

            // Sync notifications in background
            if (json.notifications && Array.isArray(json.notifications)) {
                state.notifications = json.notifications;
                renderNotifications();
            }
        } else {
            throw new Error(json.message || 'Unknown response');
        }
    } catch (e) {
        console.warn('Live monitoring refresh failure:', e);
        if (dot) {
            dot.classList.remove('pulse');
            dot.classList.add('stale');
            dot.title = 'Telemetry Connection Offline / Stale';
        }
        if (staleBanner) staleBanner.classList.remove('hidden');
    } finally {
        state.isLivePolling = false;
        if (refreshBtn) refreshBtn.classList.remove('btn-spin');
    }
}

// ==========================================================================
// DEMONSTRATION MODE CONTROLS
// ==========================================================================

async function checkDemoStatus() {
    try {
        const res = await fetchAPI('/api/demo/status');
        const json = await res.json();
        if (json.success && json.is_demo_mode) {
            state.isDemoMode = true;
            state.demoDatabasePath = json.database_path;

            const banner = document.getElementById('demo-mode-banner');
            const pathEl = document.getElementById('demo-db-path');
            if (banner) banner.classList.remove('hidden');
            if (pathEl) pathEl.textContent = json.database_path || 'data/demo_fleet.db';

            document.getElementById('btn-reset-demo-data')?.addEventListener('click', handleResetDemoData);
        }
    } catch (e) {
        console.warn('Demo status check failed:', e);
    }
}

async function handleResetDemoData() {
    if (!confirm("Are you sure you want to reset the academic demonstration database?\n\nThis will re-seed data/demo_fleet.db with pristine vehicles, drivers, schedules, work orders, and inspections.\nThe production database 'data/fleet.db' will NOT be touched.")) {
        return;
    }
    try {
        showAlert('Resetting demo database with pristine records...', 'info');
        const res = await fetchAPI('/api/demo/reset', { method: 'POST' });
        const json = await res.json();
        if (json.success) {
            showAlert('Demonstration database reset successfully.', 'success');
            await loadAllData();
        } else {
            showAlert('Demo reset failed: ' + (json.message || ''), 'danger');
        }
    } catch (e) {
        console.error('Demo reset error:', e);
        showAlert('Error requesting demo reset.', 'danger');
    }
}

// ==========================================================================
// NOTIFICATION CENTER CONTROLS
// ==========================================================================

function initNotificationCenter() {
    // Filter buttons in flyout
    document.querySelectorAll('.notif-filter-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            document.querySelectorAll('.notif-filter-btn').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            state.notifFilter = btn.getAttribute('data-filter') || 'all';
            renderNotifications();
        });
    });

    document.getElementById('btn-ack-all-notifs')?.addEventListener('click', () => {
        ackAllNotifications();
    });
}

// ==========================================================================
// OPERATIONAL DSA INTEGRATION
// ==========================================================================

function initDsaOperations() {
    // 1. Vehicle Hash Table Lookup O(1)
    const btnLookup = document.getElementById('btn-hash-lookup');
    const inputLookup = document.getElementById('vehicle-hash-lookup-input');
    if (btnLookup && inputLookup) {
        const doLookup = async () => {
            const reg = inputLookup.value.trim();
            if (!reg) return;
            const resEl = document.getElementById('hash-lookup-result');
            if (resEl) resEl.innerHTML = '<span class="text-muted">Searching DJB2 hash index...</span>';

            try {
                const res = await fetchAPI(`/api/vehicles/lookup?reg=${encodeURIComponent(reg)}`);
                const json = await res.json();
                if (json.success && json.vehicle) {
                    const v = json.vehicle;
                    if (resEl) {
                        resEl.innerHTML = `<span class="text-success font-bold">&#10003; Found:</span> <strong>${escapeHTML(v.registration_number)}</strong> (${escapeHTML(v.make)} ${escapeHTML(v.model)}, ${v.odometer_km} km) &bull; Status: <span class="badge badge-info">${escapeHTML(v.status)}</span> <span class="badge badge-success">O(1)</span>`;
                    }
                    // Filter vehicles table to match
                    const vSearch = document.getElementById('vehicle-search-input');
                    if (vSearch) {
                        vSearch.value = v.registration_number;
                        state.searchTerm = v.registration_number.toLowerCase();
                        state.currentPage = 1;
                        renderVehiclesTable();
                    }
                } else {
                    if (resEl) resEl.innerHTML = `<span class="text-danger">&#10007; ${escapeHTML(json.message || 'No vehicle found')} (Checked all hash buckets)</span>`;
                }
            } catch (e) {
                if (resEl) resEl.innerHTML = `<span class="text-danger">Lookup error</span>`;
            }
        };

        btnLookup.addEventListener('click', doLookup);
        inputLookup.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                doLookup();
            }
        });
    }

    // 2. Work Order Undo (Stack O(1))
    document.getElementById('btn-undo-wo-status')?.addEventListener('click', async () => {
        if (!confirm('Revert the last work-order status change using the C11 Undo Stack?\n\nNote: Financial transitions on Completed orders cannot be reverted.')) {
            return;
        }
        try {
            const res = await fetchAPI('/api/work-orders/undo', { method: 'POST' });
            const json = await res.json();
            if (json.success) {
                showAlert(`Status Reverted: ${json.message}`, 'success');
                await loadWorkOrders();
            } else {
                showAlert(`Undo Rejected: ${json.message || 'No reversible operations in stack'}`, 'warning');
            }
        } catch (e) {
            showAlert('Failed to process undo request.', 'danger');
        }
    });

    // 3. Work Order FIFO Queue Dispatcher
    document.getElementById('btn-view-wo-queue')?.addEventListener('click', async () => {
        openModal('modal-wo-queue');
        const tbody = document.getElementById('tbody-wo-queue');
        if (tbody) tbody.innerHTML = '<tr><td colspan="6" class="text-center text-muted py-4">Fetching FIFO dispatch queue from C11 memory...</td></tr>';

        try {
            const res = await fetchAPI('/api/work-orders/dispatch-queue');
            const json = await res.json();
            if (json.success && json.queue && tbody) {
                if (json.queue.length === 0) {
                    tbody.innerHTML = '<tr><td colspan="6" class="text-center text-muted py-4">Workshop queue is empty.</td></tr>';
                } else {
                    tbody.innerHTML = json.queue.map(q => `
                        <tr>
                            <td class="font-mono text-center"><strong>#${q.queue_position}</strong></td>
                            <td class="font-mono font-bold">${escapeHTML(q.wo_number)}</td>
                            <td class="primary-cell">${escapeHTML(q.vehicle_reg)}</td>
                            <td>${escapeHTML(q.description)}</td>
                            <td><span class="badge ${q.priority === 'Critical' ? 'badge-danger' : (q.priority === 'High' ? 'badge-warning' : 'badge-info')}">${escapeHTML(q.priority)}</span></td>
                            <td><span class="badge badge-muted">${escapeHTML(q.status)}</span></td>
                        </tr>
                    `).join('');
                }
            }
        } catch (e) {
            if (tbody) tbody.innerHTML = '<tr><td colspan="6" class="text-center text-danger py-4">Error loading workshop queue.</td></tr>';
        }
    });

    // 4. Merge Sort Fleet Ranking Engine
    document.getElementById('btn-run-ranking-sort')?.addEventListener('click', async () => {
        const sortBy = document.getElementById('select-ranking-sort-by')?.value || 'odometer';
        const tbody = document.getElementById('tbody-ranking-sort');
        if (tbody) tbody.innerHTML = '<tr><td colspan="5" class="text-center text-muted py-4">Executing C11 Merge Sort (O(N log N))...</td></tr>';

        try {
            const res = await fetchAPI(`/api/analytics/rankings?sort_by=${encodeURIComponent(sortBy)}`);
            const json = await res.json();
            if (json.success && json.rankings && tbody) {
                tbody.innerHTML = json.rankings.map(r => `
                    <tr>
                        <td class="font-mono text-center"><strong>#${r.rank}</strong></td>
                        <td class="primary-cell">${escapeHTML(r.registration)}</td>
                        <td>${escapeHTML(r.make_model)}</td>
                        <td class="font-bold">${escapeHTML(r.metric_display)}</td>
                        <td><span class="badge badge-info">${escapeHTML(r.status)}</span></td>
                    </tr>
                `).join('');
            }
        } catch (e) {
            if (tbody) tbody.innerHTML = '<tr><td colspan="5" class="text-center text-danger py-4">Failed to run merge sort.</td></tr>';
        }
    });

    // 5. Interactive C11 DSA Lab Workbench
    const algoSelect = document.getElementById('dsa-lab-algo-select');
    if (algoSelect) {
        algoSelect.addEventListener('change', (e) => {
            const v = e.target.value;
            document.querySelectorAll('.dsa-algo-controls').forEach(el => el.classList.add('hidden'));
            const ctrl = document.getElementById(`dsa-ctrl-${v}`);
            if (ctrl) ctrl.classList.remove('hidden');
        });
    }

    // Run Dijkstra
    document.getElementById('btn-dsa-run-dijkstra')?.addEventListener('click', async () => {
        const from = document.getElementById('dsa-dijkstra-from')?.value || 'Central Depot';
        const to = document.getElementById('dsa-dijkstra-to')?.value || 'Port Facility';
        const out = document.getElementById('dsa-lab-output');
        const title = document.getElementById('dsa-output-title');
        const comp = document.getElementById('dsa-output-complexity');

        if (title) title.textContent = `Dijkstra Shortest Path: ${from} -> ${to}`;
        if (comp) comp.textContent = 'O((V + E) log V)';
        if (out) out.textContent = 'Running Dijkstra algorithm in pure C11 graph memory...';

        try {
            const res = await fetchAPI(`/api/routes/plan?from=${encodeURIComponent(from)}&to=${encodeURIComponent(to)}`);
            const json = await res.json();
            if (out) out.textContent = JSON.stringify(json, null, 2);
        } catch (e) {
            if (out) out.textContent = 'Error computing route: ' + e;
        }
    });

    // Run Knapsack
    document.getElementById('btn-dsa-run-knapsack')?.addEventListener('click', async () => {
        const budget = parseInt(document.getElementById('dsa-knapsack-budget')?.value || '18000', 10);
        const out = document.getElementById('dsa-lab-output');
        const title = document.getElementById('dsa-output-title');
        const comp = document.getElementById('dsa-output-complexity');

        if (title) title.textContent = `Dynamic Programming 0-1 Knapsack (Budget: Rs. ${budget})`;
        if (comp) comp.textContent = 'O(N * B) Pseudo-polynomial';
        if (out) out.textContent = 'Computing optimal maintenance job subset in C DP table...';

        try {
            const res = await fetchAPI('/api/optimization/knapsack', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ budget_ceiling: budget })
            });
            const json = await res.json();
            if (out) out.textContent = JSON.stringify(json, null, 2);
        } catch (e) {
            if (out) out.textContent = 'Error running knapsack optimization: ' + e;
        }
    });

    // Run TSP
    document.getElementById('btn-dsa-run-tsp')?.addEventListener('click', async () => {
        const stops = parseInt(document.getElementById('dsa-tsp-stops')?.value || '4', 10);
        const out = document.getElementById('dsa-lab-output');
        const title = document.getElementById('dsa-output-title');
        const comp = document.getElementById('dsa-output-complexity');

        if (title) title.textContent = `Branch & Bound TSP Depot Tour (${stops} Hubs)`;
        if (comp) comp.textContent = 'O(N!) Pruned State-Space';
        if (out) out.textContent = 'Executing Branch & Bound circuit optimizer in C memory...';

        try {
            const res = await fetchAPI('/api/optimization/tsp', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ num_stops: stops })
            });
            const json = await res.json();
            if (out) out.textContent = JSON.stringify(json, null, 2);
        } catch (e) {
            if (out) out.textContent = 'Error running TSP: ' + e;
        }
    });

    // Run Hash Lookup from Lab
    document.getElementById('btn-dsa-run-lookup')?.addEventListener('click', async () => {
        const reg = document.getElementById('dsa-lookup-reg')?.value || 'MH-01-AB-1001';
        const out = document.getElementById('dsa-lab-output');
        const title = document.getElementById('dsa-output-title');
        const comp = document.getElementById('dsa-output-complexity');

        if (title) title.textContent = `DJB2 Hash Table Lookup: "${reg}"`;
        if (comp) comp.textContent = 'O(1) Average';
        if (out) out.textContent = 'Querying normalized DJB2 hash buckets...';

        try {
            const res = await fetchAPI(`/api/vehicles/lookup?reg=${encodeURIComponent(reg)}`);
            const json = await res.json();
            if (out) out.textContent = JSON.stringify(json, null, 2);
        } catch (e) {
            if (out) out.textContent = 'Error executing lookup: ' + e;
        }
    });

    // Run Merge Sort from Lab
    document.getElementById('btn-dsa-run-mergesort')?.addEventListener('click', async () => {
        const key = document.getElementById('dsa-mergesort-key')?.value || 'odometer';
        const out = document.getElementById('dsa-lab-output');
        const title = document.getElementById('dsa-output-title');
        const comp = document.getElementById('dsa-output-complexity');

        if (title) title.textContent = `C11 Merge Sort by: ${key}`;
        if (comp) comp.textContent = 'O(N log N) Stable';
        if (out) out.textContent = 'Sorting vehicles in C memory...';

        try {
            const res = await fetchAPI(`/api/analytics/rankings?sort_by=${encodeURIComponent(key)}`);
            const json = await res.json();
            if (out) out.textContent = JSON.stringify(json, null, 2);
        } catch (e) {
            if (out) out.textContent = 'Error running merge sort: ' + e;
        }
    });
}
