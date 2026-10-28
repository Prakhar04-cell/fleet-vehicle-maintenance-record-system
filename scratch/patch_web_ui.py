with open('web/index.html', 'r', encoding='utf-8') as f:
    html = f.read()

# 1. Update user profile pill with ID and pointer
old_pill = """                        <!-- User Profile Pill -->
                        <div class="user-profile-pill">
                            <div class="user-avatar">AD</div>
                            <div class="user-details">
                                <span class="user-name">Administrator</span>
                                <span class="user-role">Role: System Admin</span>
                            </div>
                        </div>"""

new_pill = """                        <!-- User Profile Pill -->
                        <div class="user-profile-pill" id="user-profile-btn" style="cursor: pointer;" title="Click to Switch User / Login">
                            <div class="user-avatar" id="header-user-avatar">AD</div>
                            <div class="user-details">
                                <span class="user-name" id="header-user-name">admin</span>
                                <span class="user-role" id="header-user-role">Role: Administrator</span>
                            </div>
                        </div>"""

if old_pill in html:
    html = html.replace(old_pill, new_pill)
    print("Replaced user profile pill")
else:
    print("Could not find old user profile pill")

# 2. Update reports grid in tab-reports
old_reports_grid = """                                <div class="dsa-grid">
                                    <div class="dsa-card">
                                        <div class="dsa-header">
                                            <h4>Vehicles Master Register</h4>
                                            <span class="badge badge-info">CSV</span>
                                        </div>
                                        <p class="dsa-desc">Complete export of all commercial vehicles, odometers, driver allocations, VINs, and insurance dates.</p>
                                        <button class="btn btn-outline btn-sm" id="btn-rep-vehicles">Download Vehicles CSV</button>
                                    </div>

                                    <div class="dsa-card">
                                        <div class="dsa-header">
                                            <h4>Service & Maintenance Logs</h4>
                                            <span class="badge badge-info">CSV</span>
                                        </div>
                                        <p class="dsa-desc">Itemized audit of all maintenance services including parts cost, labour, taxes, and workshop providers.</p>
                                        <button class="btn btn-outline btn-sm" id="btn-rep-maint">Download Maintenance CSV</button>
                                    </div>

                                    <div class="dsa-card">
                                        <div class="dsa-header">
                                            <h4>Operating Expenditures</h4>
                                            <span class="badge badge-info">CSV</span>
                                        </div>
                                        <p class="dsa-desc">Ledger of all fuel, toll, parts, and maintenance expenses aggregated by vehicle registration.</p>
                                        <button class="btn btn-outline btn-sm" id="btn-rep-expenses">Download Expenses CSV</button>
                                    </div>

                                    <div class="dsa-card">
                                        <div class="dsa-header">
                                            <h4>Preventive Milestones</h4>
                                            <span class="badge badge-info">CSV</span>
                                        </div>
                                        <p class="dsa-desc">Threshold tracking report with overdue status, remaining distance, and scheduled maintenance dates.</p>
                                        <button class="btn btn-outline btn-sm" id="btn-rep-schedules">Download Schedules CSV</button>
                                    </div>
                                </div>"""

new_reports_grid = """                                <div class="dsa-grid">
                                    <!-- PDF Reports -->
                                    <div class="dsa-card" style="border-top: 3px solid #2563EB;">
                                        <div class="dsa-header">
                                            <h4>Fleet Executive Summary</h4>
                                            <span class="badge badge-primary">PDF %PDF-1.4</span>
                                        </div>
                                        <p class="dsa-desc">Formal executive report with active fleet count, financial expenditure totals, overdue services, and vehicle readiness breakdown.</p>
                                        <button class="btn btn-primary btn-sm" id="btn-rep-fleet-pdf">
                                            <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path><polyline points="14 2 14 8 20 8"></polyline></svg>
                                            <span>Generate Fleet Summary PDF</span>
                                        </button>
                                    </div>

                                    <div class="dsa-card" style="border-top: 3px solid #2563EB;">
                                        <div class="dsa-header">
                                            <h4>Operating Expenses Ledger</h4>
                                            <span class="badge badge-primary">PDF %PDF-1.4</span>
                                        </div>
                                        <p class="dsa-desc">Financial audit statement itemizing fuel, tolls, insurance, and maintenance expenses per registration with cost centers.</p>
                                        <button class="btn btn-primary btn-sm" id="btn-rep-expenses-pdf">
                                            <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path><polyline points="14 2 14 8 20 8"></polyline></svg>
                                            <span>Generate Expenses PDF</span>
                                        </button>
                                    </div>

                                    <div class="dsa-card" style="border-top: 3px solid #2563EB;">
                                        <div class="dsa-header">
                                            <h4>Vehicle Maintenance Dossier</h4>
                                            <span class="badge badge-primary">PDF %PDF-1.4</span>
                                        </div>
                                        <p class="dsa-desc">Complete vehicle maintenance passport showing odometer history, part replacements, and technician sign-offs.</p>
                                        <button class="btn btn-primary btn-sm" id="btn-rep-maint-pdf">
                                            <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path><polyline points="14 2 14 8 20 8"></polyline></svg>
                                            <span>Generate Maintenance PDF</span>
                                        </button>
                                    </div>

                                    <!-- CSV Reports -->
                                    <div class="dsa-card">
                                        <div class="dsa-header">
                                            <h4>Vehicles Master Register</h4>
                                            <span class="badge badge-info">CSV RFC 4180</span>
                                        </div>
                                        <p class="dsa-desc">Complete export of all commercial vehicles, odometers, driver allocations, VINs, and insurance dates.</p>
                                        <button class="btn btn-outline btn-sm" id="btn-rep-vehicles">Download Vehicles CSV</button>
                                    </div>

                                    <div class="dsa-card">
                                        <div class="dsa-header">
                                            <h4>Service & Maintenance Logs</h4>
                                            <span class="badge badge-info">CSV RFC 4180</span>
                                        </div>
                                        <p class="dsa-desc">Itemized audit of all maintenance services including parts cost, labour, taxes, and workshop providers.</p>
                                        <button class="btn btn-outline btn-sm" id="btn-rep-maint">Download Maintenance CSV</button>
                                    </div>

                                    <div class="dsa-card">
                                        <div class="dsa-header">
                                            <h4>Operating Expenditures</h4>
                                            <span class="badge badge-info">CSV RFC 4180</span>
                                        </div>
                                        <p class="dsa-desc">Ledger of all fuel, toll, parts, and maintenance expenses aggregated by vehicle registration.</p>
                                        <button class="btn btn-outline btn-sm" id="btn-rep-expenses">Download Expenses CSV</button>
                                    </div>

                                    <div class="dsa-card">
                                        <div class="dsa-header">
                                            <h4>Preventive Milestones</h4>
                                            <span class="badge badge-info">CSV RFC 4180</span>
                                        </div>
                                        <p class="dsa-desc">Threshold tracking report with overdue status, remaining distance, and scheduled maintenance dates.</p>
                                        <button class="btn btn-outline btn-sm" id="btn-rep-schedules">Download Schedules CSV</button>
                                    </div>

                                    <div class="dsa-card">
                                        <div class="dsa-header">
                                            <h4>Monthly Spending Trends</h4>
                                            <span class="badge badge-info">CSV RFC 4180</span>
                                        </div>
                                        <p class="dsa-desc">Chronological monthly financial summaries grouped by calendar year and operating cost center.</p>
                                        <button class="btn btn-outline btn-sm" id="btn-rep-spending-csv">Download Spending CSV</button>
                                    </div>

                                    <div class="dsa-card">
                                        <div class="dsa-header">
                                            <h4>Vehicle Lifetime Costs</h4>
                                            <span class="badge badge-info">CSV RFC 4180</span>
                                        </div>
                                        <p class="dsa-desc">Aggregate TCO metrics comparing maintenance expenditure against operational fuel and toll expenditures.</p>
                                        <button class="btn btn-outline btn-sm" id="btn-rep-cost-csv">Download Lifetime Costs CSV</button>
                                    </div>
                                </div>"""

if old_reports_grid in html:
    html = html.replace(old_reports_grid, new_reports_grid)
    print("Replaced reports grid with PDF + CSV cards")
else:
    print("Could not find old reports grid")

# 3. Add Modal 8 (Auth / Login) before </body>
modal_auth_html = """    <!-- Modal 8: Authentication & Role Switcher -->
    <div id="modal-auth-login" class="modal-backdrop hidden">
        <div class="modal-box" style="max-width: 440px;">
            <div class="modal-header">
                <h3>Account Authentication & Role Switching</h3>
                <button class="modal-close" data-close="modal-auth-login">&times;</button>
            </div>
            <form id="form-auth-login" class="modal-body">
                <p class="text-sm text-muted" style="margin-bottom: 12px;">Authenticate against the C11 SQLite backend with PBKDF2-HMAC-SHA256 password verification and role-based access control.</p>
                <div class="form-group">
                    <label>Username *</label>
                    <input type="text" id="login-username" required placeholder="admin / manager / staff" />
                </div>
                <div class="form-group">
                    <label>Password *</label>
                    <input type="password" id="login-password" required placeholder="Enter password" />
                </div>
                <div style="background: #F1F5F9; border: 1px solid #E2E8F0; border-radius: 6px; padding: 10px; margin-bottom: 14px; font-size: 12px;">
                    <span style="font-weight: 600; color: #475569; display: block; margin-bottom: 6px;">Quick Fill Demo Roles:</span>
                    <div style="display: flex; gap: 6px; flex-wrap: wrap;">
                        <button type="button" class="btn btn-outline btn-sm" id="btn-fill-admin" style="padding: 2px 8px; font-size: 11px;">Admin (Full Control)</button>
                        <button type="button" class="btn btn-outline btn-sm" id="btn-fill-manager" style="padding: 2px 8px; font-size: 11px;">Manager</button>
                        <button type="button" class="btn btn-outline btn-sm" id="btn-fill-staff" style="padding: 2px 8px; font-size: 11px;">Staff</button>
                    </div>
                </div>
                <div id="login-error-msg" class="text-danger text-sm hidden" style="margin-bottom: 10px; color: #DC2626; font-weight: 500;"></div>
                <div class="modal-footer">
                    <button type="button" class="btn btn-secondary" data-close="modal-auth-login">Cancel</button>
                    <button type="submit" class="btn btn-primary">Authenticate</button>
                </div>
            </form>
        </div>
    </div>
"""

if '<!-- Modal 7: Assign Driver -->' in html:
    html = html.replace('<!-- Modal 7: Assign Driver -->', modal_auth_html + '\n    <!-- Modal 7: Assign Driver -->')
    print("Added Modal 8 for authentication")
else:
    print("Could not find Modal 7 marker")

with open('web/index.html', 'w', encoding='utf-8') as f:
    f.write(html)

print("Saved web/index.html")
