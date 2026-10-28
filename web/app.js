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
    document.getElementById('btn-mgr-quick-vehicle')?.addEventListener('click', () => openModal('modal-vehicle'));
    document.getElementById('btn-mgr-quick-service')?.addEventListener('click', () => openLogMaintenanceModal());
    document.getElementById('btn-mgr-quick-schedule')?.addEventListener('click', () => openCreateScheduleModal());
    document.getElementById('btn-mgr-quick-assign')?.addEventListener('click', () => openAssignDriverModal(state.vehicles[0]?.id || 1));

    // Maintenance Staff Dashboard Quick Action Buttons
    document.getElementById('btn-staff-quick-service')?.addEventListener('click', () => openLogMaintenanceModal());
    document.getElementById('btn-staff-quick-schedule')?.addEventListener('click', () => openCreateScheduleModal());

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
        reminders: 'Maintenance Reminders',
        expenses: 'Operating Expenses',
        reports: 'Reports & Analytics',
        dsa: 'DSA Optimization Suite',
        audit: 'System Audit Trail',
        backup: 'Disaster Recovery'
    };

    const breadcrumb = document.getElementById('breadcrumb-current');
    if (breadcrumb && breadcrumbTitles[tabId]) {
        breadcrumb.textContent = breadcrumbTitles[tabId];
    }

    // Lazy load specific tab data if needed
    if (tabId === 'audit') loadAudit();
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
        loadMaintenance()
    ];
    if (role !== 'staff') {
        tasks.push(loadDrivers());
        tasks.push(loadExpenses());
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
        const res = await fetchAPI('/api/notifications');
        const json = await res.json();
        if (json.success && json.data) {
            state.notifications = json.data;
            renderNotifications();
        }
    } catch (e) {
        console.error('Failed to load notifications:', e);
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

    const active = state.notifications || [];
    if (countBadge) {
        if (active.length > 0) {
            countBadge.textContent = active.length;
            countBadge.classList.remove('hidden');
        } else {
            countBadge.classList.add('hidden');
        }
    }

    if (flyoutCount) flyoutCount.textContent = `${active.length} alerts`;

    if (flyoutList) {
        if (active.length === 0) {
            flyoutList.innerHTML = `<div class="text-center py-4 text-muted text-sm">No active alerts.</div>`;
        } else {
            flyoutList.innerHTML = active.map(n => `
                <div class="notification-item-compact">
                    <span class="status-indicator-dot" style="margin-top: 4px;"></span>
                    <div>
                        <div style="font-weight: 600; font-size: 12.5px; color: var(--text-primary);">${n.vehicle_reg}: ${n.service_name}</div>
                        <div style="font-size: 11px; color: var(--text-muted);">${n.type} &bull; ${n.scheduled_at || ''}</div>
                    </div>
                </div>
            `).join('');
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
    const selects = ['m-vehicle-id', 's-vehicle-id', 'e-vehicle-id'];
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
        showAlert('Executing C11 core data structures and optimization algorithms...', 'info');
        const res = await fetchAPI('/api/dsa/run');
        const json = await res.json();
        if (json.success && json.algorithms) {
            const a = json.algorithms;
            document.getElementById('dsa-ht-metric').textContent = `Indexed: ${a.hash_table.vehicles_indexed} vehicles (${a.hash_table.complexity})`;
            document.getElementById('dsa-dijkstra-metric').textContent = `${a.dijkstra.route} (${a.dijkstra.shortest_dist_km} km)`;
            document.getElementById('dsa-knapsack-metric').textContent = `Max: ${a.dp_knapsack.max_reliability_score} pts (${a.dp_knapsack.jobs_selected} jobs)`;
            document.getElementById('dsa-tsp-metric').textContent = `Tour: ${a.tsp_branch_bound.optimal_tour_distance_km} km`;
            showAlert('C11 DSA Algorithm Benchmarks completed successfully.', 'success');
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
