with open('web/app.js', 'r', encoding='utf-8') as f:
    js = f.read()

# 1. Update initEventListeners in web/app.js
old_reports_events = """    // Report Download Buttons
    document.getElementById('btn-rep-vehicles')?.addEventListener('click', exportVehiclesCSV);
    document.getElementById('btn-rep-maint')?.addEventListener('click', exportMaintenanceCSV);
    document.getElementById('btn-rep-expenses')?.addEventListener('click', exportExpensesCSV);
    document.getElementById('btn-rep-schedules')?.addEventListener('click', exportSchedulesCSV);
    document.getElementById('btn-export-vehicles-csv')?.addEventListener('click', exportVehiclesCSV);
    document.getElementById('btn-export-maint-csv')?.addEventListener('click', exportMaintenanceCSV);
    document.getElementById('btn-export-expenses-csv')?.addEventListener('click', exportExpensesCSV);"""

new_reports_events = """    // Direct Backend C11 Reports & Exports (CSV and PDF)
    document.getElementById('btn-rep-fleet-pdf')?.addEventListener('click', () => {
        showAlert('Generating C11 PDF Fleet Executive Summary...', 'info');
        window.open('/api/reports/pdf?type=fleet', '_blank');
    });
    document.getElementById('btn-rep-expenses-pdf')?.addEventListener('click', () => {
        showAlert('Generating C11 PDF Operating Expenses Ledger...', 'info');
        window.open('/api/reports/pdf?type=expenses', '_blank');
    });
    document.getElementById('btn-rep-maint-pdf')?.addEventListener('click', () => {
        const vid = state.vehicles[0]?.id || 1;
        showAlert(`Generating C11 PDF Maintenance Dossier for Vehicle #${vid}...`, 'info');
        window.open(`/api/reports/pdf?type=maintenance&vehicle_id=${vid}`, '_blank');
    });
    document.getElementById('btn-rep-vehicles')?.addEventListener('click', () => {
        showAlert('Downloading Vehicles Register CSV...', 'info');
        window.location.href = '/api/reports/csv?type=vehicles';
    });
    document.getElementById('btn-rep-maint')?.addEventListener('click', () => {
        showAlert('Downloading Service & Maintenance CSV...', 'info');
        window.location.href = '/api/reports/csv?type=maintenance';
    });
    document.getElementById('btn-rep-expenses')?.addEventListener('click', () => {
        showAlert('Downloading Operating Expenditures CSV...', 'info');
        window.location.href = '/api/reports/csv?type=expenses';
    });
    document.getElementById('btn-rep-schedules')?.addEventListener('click', () => {
        showAlert('Downloading Preventive Milestones CSV...', 'info');
        window.location.href = '/api/reports/csv?type=schedules';
    });
    document.getElementById('btn-rep-spending-csv')?.addEventListener('click', () => {
        showAlert('Downloading Monthly Spending CSV...', 'info');
        window.location.href = '/api/reports/csv?type=spending';
    });
    document.getElementById('btn-rep-cost-csv')?.addEventListener('click', () => {
        showAlert('Downloading Lifetime Vehicle Costs CSV...', 'info');
        window.location.href = '/api/reports/csv?type=vehicle_costs';
    });
    document.getElementById('btn-export-vehicles-csv')?.addEventListener('click', exportVehiclesCSV);
    document.getElementById('btn-export-maint-csv')?.addEventListener('click', exportMaintenanceCSV);
    document.getElementById('btn-export-expenses-csv')?.addEventListener('click', exportExpensesCSV);

    // Authentication & Role Switcher
    document.getElementById('user-profile-btn')?.addEventListener('click', () => openModal('modal-auth-login'));
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
    document.getElementById('form-auth-login')?.addEventListener('submit', handleLoginSubmit);"""

if old_reports_events in js:
    js = js.replace(old_reports_events, new_reports_events)
    print("Replaced report events and added auth events")
else:
    print("Could not find old_reports_events!")

# 2. Add handleLoginSubmit at the bottom of app.js
login_func = """
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
            const u = json.user;
            document.getElementById('header-user-name').textContent = u.username;
            document.getElementById('header-user-role').textContent = `Role: ${u.role}`;
            document.getElementById('header-user-avatar').textContent = u.username.slice(0, 2).toUpperCase();
            closeModal('modal-auth-login');
            showAlert(`Authenticated successfully as ${u.username} (${u.role})`, 'success');
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
"""

js += login_func

with open('web/app.js', 'w', encoding='utf-8') as f:
    f.write(js)

print("Saved web/app.js")
