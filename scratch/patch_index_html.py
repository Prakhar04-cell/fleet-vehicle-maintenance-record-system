import sys

with open('web/index.html', 'r', encoding='utf-8') as f:
    html = f.read()

# 1. Update header actions with polling controls & enhanced notification flyout
old_notif = '''                        <!-- Notification Bell & Flyout -->
                        <div class="notification-menu-wrapper">
                            <button class="btn btn-secondary btn-icon" id="btn-notifications-toggle" title="View System Alerts">
                                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"></path><path d="M13.73 21a2 2 0 0 1-3.46 0"></path></svg>
                            </button>
                            <span class="nav-badge danger hidden" id="header-notif-badge" style="position: absolute; top: -4px; right: -4px; font-size: 10px; padding: 1px 5px;">0</span>

                            <div class="notification-flyout" id="notification-flyout">
                                <div class="notification-flyout-header">
                                    <span class="notification-flyout-title">Active Fleet Notifications</span>
                                    <span class="badge badge-info" id="flyout-count">0 alerts</span>
                                </div>
                                <div class="notification-flyout-body" id="flyout-notification-list">
                                    <div class="text-center py-4 text-muted text-sm">No active alerts.</div>
                                </div>
                            </div>
                        </div>'''

new_notif = '''                        <!-- Polling Control & Live Telemetry Indicator -->
                        <div class="polling-control-wrap" style="display: flex; align-items: center; gap: 6px; font-size: 12px; color: var(--text-secondary); background: var(--bg-muted); padding: 2px 8px; border-radius: var(--radius-sm); border: 1px solid var(--border-color);">
                            <span class="live-dot" id="live-indicator-dot" title="Live Engine Connected" style="width: 8px; height: 8px; border-radius: 50%; background: #10b981; display: inline-block;"></span>
                            <select id="poll-interval-select" style="height: 26px; font-size: 11.5px; border: none; background: transparent; font-weight: 500; color: var(--text-primary); cursor: pointer;">
                                <option value="0">Polling: Off</option>
                                <option value="5000">Poll: 5s</option>
                                <option value="10000" selected>Poll: 10s</option>
                                <option value="30000">Poll: 30s</option>
                            </select>
                            <span id="live-last-refresh" style="font-size: 11px; color: var(--text-muted); min-width: 75px;">Just now</span>
                        </div>

                        <!-- Notification Bell & Enhanced Notification Center Flyout -->
                        <div class="notification-menu-wrapper">
                            <button class="btn btn-secondary btn-icon" id="btn-notifications-toggle" title="View Notification Center">
                                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9"></path><path d="M13.73 21a2 2 0 0 1-3.46 0"></path></svg>
                            </button>
                            <span class="nav-badge danger hidden" id="header-notif-badge" style="position: absolute; top: -4px; right: -4px; font-size: 10px; padding: 1px 5px;">0</span>

                            <div class="notification-flyout" id="notification-flyout">
                                <div class="notification-flyout-header" style="display: flex; justify-content: space-between; align-items: center;">
                                    <span class="notification-flyout-title">Notifications Center</span>
                                    <div style="display: flex; gap: 6px; align-items: center;">
                                        <span class="badge badge-info" id="flyout-count">0 alerts</span>
                                        <button class="btn btn-xs btn-outline" id="btn-ack-all-notifs" style="font-size: 10px; padding: 2px 6px;">Ack All</button>
                                    </div>
                                </div>
                                <div class="notification-filters" style="display: flex; gap: 4px; padding: 6px 12px; border-bottom: 1px solid var(--border-color); background: var(--bg-muted); overflow-x: auto;">
                                    <button class="notif-filter-btn active" data-filter="all">All</button>
                                    <button class="notif-filter-btn" data-filter="unread">Unread</button>
                                    <button class="notif-filter-btn" data-filter="maintenance">Maintenance</button>
                                    <button class="notif-filter-btn" data-filter="defects">Defects</button>
                                    <button class="notif-filter-btn" data-filter="work-orders">Work Orders</button>
                                </div>
                                <div class="notification-flyout-body" id="flyout-notification-list">
                                    <div class="text-center py-4 text-muted text-sm">No active alerts.</div>
                                </div>
                            </div>
                        </div>'''

if old_notif in html:
    html = html.replace(old_notif, new_notif)
    print('[OK] Header actions and notification flyout updated')
else:
    print('[FAIL] old_notif pattern not found')

# 2. Add Demo Mode Banner and Stale Data Banner
old_main = '''            <!-- Main Content Container -->
            <main class="main-content">
                <!-- Live In-App Alerts Banner -->
                <div id="alert-banner" class="alert-banner hidden"></div>'''

new_main = '''            <!-- Main Content Container -->
            <main class="main-content">
                <!-- Demo Mode Banner (shown when running on data/demo_fleet.db) -->
                <div id="demo-mode-banner" class="demo-banner hidden" style="background: #eff6ff; border: 1px solid #bfdbfe; color: #1e40af; padding: 10px 16px; border-radius: 8px; margin-bottom: 14px; display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 8px;">
                    <div>
                        <strong>🎓 Demonstration Mode Active:</strong> Operating on isolated database <code id="demo-db-path" style="background: #dbeafe; padding: 2px 6px; border-radius: 4px; font-weight: 600;">data/demo_fleet.db</code>. Production database <code>data/fleet.db</code> is protected.
                    </div>
                    <button class="btn btn-sm btn-outline danger" id="btn-reset-demo-data" title="Re-seed demo database with fresh sample records">Reset Demo Data</button>
                </div>

                <!-- Stale Data Warning Banner -->
                <div id="stale-data-banner" class="alert-banner warning hidden" style="background: #fffbeb; border: 1px solid #fde68a; color: #b45309; padding: 8px 16px; border-radius: 6px; margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center;">
                    <span><strong>Warning: Stale Operational Telemetry</strong> — Server refresh encountered an error. Displaying last valid SQLite cached snapshot.</span>
                    <button class="btn btn-sm btn-secondary" id="btn-retry-live-refresh">Retry Refresh</button>
                </div>

                <!-- Live In-App Alerts Banner -->
                <div id="alert-banner" class="alert-banner hidden"></div>'''

if old_main in html:
    html = html.replace(old_main, new_main)
    print('[OK] Banners added to main-content')
else:
    print('[FAIL] old_main pattern not found')

# 3. Add C11 Hash Table lookup widget to tab-vehicles
old_veh_toolbar = '''                <section id="tab-vehicles" class="tab-pane">
                    <div class="card">
                        <!-- Toolbar with Search, Status Filter, Fuel Filter, Column Picker, Add Button -->
                        <div class="table-toolbar">'''

new_veh_toolbar = '''                <section id="tab-vehicles" class="tab-pane">
                    <!-- Instant C11 Hash Table Lookup O(1) -->
                    <div class="hash-lookup-bar" style="display: flex; align-items: center; gap: 10px; background: var(--bg-surface); padding: 10px 16px; border-radius: var(--radius-md); border: 1px solid var(--border-color); margin-bottom: 14px; flex-wrap: wrap;">
                        <span class="badge badge-info" style="font-size: 11px; font-weight: 600;">C11 Hash Table DJB2 O(1)</span>
                        <input type="text" id="vehicle-hash-lookup-input" class="form-control" placeholder="Instant Reg Lookup (e.g. MH-01-AB-1001 or mh01ab1001)..." style="max-width: 320px; height: 32px; font-size: 12px; padding: 4px 10px; border: 1px solid var(--border-color); border-radius: var(--radius-sm);" />
                        <button class="btn btn-secondary btn-sm" id="btn-hash-lookup">Find in O(1)</button>
                        <span id="hash-lookup-result" style="font-size: 12px; color: var(--text-secondary); margin-left: 4px;"></span>
                    </div>

                    <div class="card">
                        <!-- Toolbar with Search, Status Filter, Fuel Filter, Column Picker, Add Button -->
                        <div class="table-toolbar">'''

if old_veh_toolbar in html:
    html = html.replace(old_veh_toolbar, new_veh_toolbar)
    print('[OK] Hash table lookup widget added to tab-vehicles')
else:
    print('[FAIL] old_veh_toolbar pattern not found')

# 4. Add Undo Status (Stack) and FIFO Queue buttons to tab-work-orders
old_wo_buttons = '''                                <button class="btn btn-secondary btn-sm" id="btn-refresh-work-orders">
                                    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="23 4 23 10 17 10"></polyline><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"></path></svg>
                                    <span>Refresh</span>
                                </button>
                                <button class="btn btn-primary btn-sm" id="btn-add-wo-modal">'''

new_wo_buttons = '''                                <button class="btn btn-outline btn-sm" id="btn-undo-wo-status" title="Revert last non-completed status change via C11 Stack (O(1))">
                                    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 7v6h6"/><path d="M21 17a9 9 0 0 0-9-9 9 9 0 0 0-6 2.3L3 13"/></svg>
                                    <span>Undo Status (Stack)</span>
                                </button>
                                <button class="btn btn-secondary btn-sm" id="btn-view-wo-queue" title="View Workshop FIFO Dispatch Queue (O(1))">
                                    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="8" y1="6" x2="21" y2="6"></line><line x1="8" y1="12" x2="21" y2="12"></line><line x1="8" y1="18" x2="21" y2="18"></line><line x1="3" y1="6" x2="3.01" y2="6"></line><line x1="3" y1="12" x2="3.01" y2="12"></line><line x1="3" y1="18" x2="3.01" y2="18"></line></svg>
                                    <span>FIFO Queue</span>
                                </button>
                                <button class="btn btn-secondary btn-sm" id="btn-refresh-work-orders">
                                    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="23 4 23 10 17 10"></polyline><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"></path></svg>
                                    <span>Refresh</span>
                                </button>
                                <button class="btn btn-primary btn-sm" id="btn-add-wo-modal">'''

if old_wo_buttons in html:
    html = html.replace(old_wo_buttons, new_wo_buttons)
    print('[OK] Work order stack undo and FIFO queue buttons added')
else:
    print('[FAIL] old_wo_buttons pattern not found')

# 5. Add C11 Merge Sort Asset Ranking Card to tab-analytics
old_analytics_bottom = '''                        </div>
                    </div>
                </section>

                <!-- ==================================================== -->
                <!-- TAB 8: SCHEDULES / PREVENTIVE -->'''

# Let's find end of tab-analytics
import re
analytics_match = re.search(r'(</section>\s+<!-- [=\s]+ -->\s+<!-- TAB: REPORTS & STATS -->\s+<section id="tab-reports")', html)
if not analytics_match:
    analytics_match = re.search(r'(</section>\s+<!-- [=\s]+ -->\s+<section id="tab-reports")', html)

merge_sort_card = '''
                    <!-- C11 Merge Sort Asset Ranking Engine (O(N log N)) -->
                    <div class="card mt-4 mb-4">
                        <div class="card-header flex-between">
                            <div>
                                <h3>C11 Merge Sort Asset Ranking Engine</h3>
                                <p>Divide-and-Conquer stable sorting running on native C11 server memory</p>
                            </div>
                            <div style="display: flex; gap: 8px; align-items: center;">
                                <span class="badge badge-success">O(N log N) Stable</span>
                                <select id="select-ranking-sort-by" class="form-select" style="height: 32px; font-size: 12px; padding: 2px 8px;">
                                    <option value="odometer">Rank by Odometer (Highest Usage)</option>
                                    <option value="registration">Rank by Registration Number</option>
                                    <option value="make">Rank by Make / Model</option>
                                    <option value="fuel">Rank by Fuel Tank Level</option>
                                </select>
                                <button class="btn btn-primary btn-sm" id="btn-run-ranking-sort">Sort with Merge Sort</button>
                            </div>
                        </div>
                        <div class="card-body-flush">
                            <div class="table-responsive">
                                <table class="data-table" id="table-ranking-sort">
                                    <thead>
                                        <tr>
                                            <th style="width: 60px;">Rank</th>
                                            <th>Vehicle Registration</th>
                                            <th>Make & Model</th>
                                            <th>Sorted Metric Value</th>
                                            <th>Status</th>
                                        </tr>
                                    </thead>
                                    <tbody id="tbody-ranking-sort">
                                        <tr><td colspan="5" class="text-center text-muted py-4">Click "Sort with Merge Sort" to execute C11 merge sort.</td></tr>
                                    </tbody>
                                </table>
                            </div>
                        </div>
                    </div>
'''

if analytics_match:
    idx = analytics_match.start()
    html = html[:idx] + merge_sort_card + html[idx:]
    print('[OK] Merge sort ranking card added to tab-analytics')
else:
    print('[FAIL] Could not locate end of tab-analytics')

# 6. Add Interactive C11 DSA Lab Workbench to tab-dsa
dsa_lab_workbench = '''
                    <!-- Interactive C11 DSA Lab Workbench -->
                    <div class="card mt-4">
                        <div class="card-header flex-between">
                            <div>
                                <h3>Interactive C11 DSA Laboratory</h3>
                                <p>Directly test and configure backend C algorithms with custom parameters and live telemetry</p>
                            </div>
                            <span class="badge badge-primary">Native Winsock2 C11 Execution</span>
                        </div>
                        <div class="card-body">
                            <div class="form-row" style="margin-bottom: 16px;">
                                <div class="form-group" style="flex: 1;">
                                    <label>Select Algorithm to Test</label>
                                    <select id="dsa-lab-algo-select" class="form-control" style="font-weight: 600;">
                                        <option value="dijkstra">Dijkstra Shortest Path Routing (Graph O((V+E) log V))</option>
                                        <option value="knapsack">Dynamic Programming 0-1 Knapsack (Job Budget Allocation O(N * B))</option>
                                        <option value="tsp">Branch & Bound TSP Inspection Circuit (Optimal Tour O(N!))</option>
                                        <option value="lookup">DJB2 Hash Table Vehicle Lookup (O(1))</option>
                                        <option value="mergesort">Merge Sort Fleet Ranking (O(N log N))</option>
                                    </select>
                                </div>
                            </div>

                            <!-- 1. Dijkstra Controls -->
                            <div id="dsa-ctrl-dijkstra" class="dsa-algo-controls">
                                <div class="form-row">
                                    <div class="form-group">
                                        <label>Origin Depot / Hub</label>
                                        <select id="dsa-dijkstra-from" class="form-control">
                                            <option value="Central Depot">Central Depot (Hub 0)</option>
                                            <option value="North Hub">North Hub (Hub 1)</option>
                                            <option value="South Terminal">South Terminal (Hub 2)</option>
                                            <option value="East Station">East Station (Hub 3)</option>
                                            <option value="West Hub">West Hub (Hub 4)</option>
                                            <option value="Port Facility">Port Facility (Hub 5)</option>
                                        </select>
                                    </div>
                                    <div class="form-group">
                                        <label>Destination Depot / Hub</label>
                                        <select id="dsa-dijkstra-to" class="form-control">
                                            <option value="Port Facility">Port Facility (Hub 5)</option>
                                            <option value="Central Depot">Central Depot (Hub 0)</option>
                                            <option value="North Hub">North Hub (Hub 1)</option>
                                            <option value="South Terminal">South Terminal (Hub 2)</option>
                                            <option value="East Station">East Station (Hub 3)</option>
                                            <option value="West Hub">West Hub (Hub 4)</option>
                                        </select>
                                    </div>
                                </div>
                                <button class="btn btn-primary btn-sm" id="btn-dsa-run-dijkstra">Compute Shortest Route</button>
                            </div>

                            <!-- 2. Knapsack Controls -->
                            <div id="dsa-ctrl-knapsack" class="dsa-algo-controls hidden">
                                <div class="form-row">
                                    <div class="form-group">
                                        <label>Quarterly Maintenance Budget Ceiling (Rs.)</label>
                                        <input type="number" id="dsa-knapsack-budget" class="form-control" value="18000" min="1000" max="100000" step="1000" />
                                        <span class="text-xs text-muted">DP 0-1 Knapsack table computes optimal job selection maximizing safety points</span>
                                    </div>
                                </div>
                                <button class="btn btn-primary btn-sm" id="btn-dsa-run-knapsack">Optimize Job Selection (DP)</button>
                            </div>

                            <!-- 3. TSP Controls -->
                            <div id="dsa-ctrl-tsp" class="dsa-algo-controls hidden">
                                <div class="form-row">
                                    <div class="form-group">
                                        <label>Number of Depot Stops in Inspection Tour</label>
                                        <select id="dsa-tsp-stops" class="form-control">
                                            <option value="4" selected>4 Hubs (Central, North, South, East)</option>
                                            <option value="5">5 Hubs (Central, North, South, East, West)</option>
                                            <option value="6">6 Hubs (All Depot Network Hubs)</option>
                                        </select>
                                        <span class="text-xs text-muted">Branch and Bound state-space search with cost-matrix reduction pruning</span>
                                    </div>
                                </div>
                                <button class="btn btn-primary btn-sm" id="btn-dsa-run-tsp">Compute Optimal TSP Circuit (B&B)</button>
                            </div>

                            <!-- 4. Hash Table Lookup Controls -->
                            <div id="dsa-ctrl-lookup" class="dsa-algo-controls hidden">
                                <div class="form-row">
                                    <div class="form-group">
                                        <label>Vehicle Registration to Lookup (O(1))</label>
                                        <input type="text" id="dsa-lookup-reg" class="form-control" placeholder="e.g. MH-01-AB-1001 or mh01ab1001" value="MH-01-AB-1001" />
                                        <span class="text-xs text-muted">Case-normalized & hyphen-insensitive DJB2 hash lookup</span>
                                    </div>
                                </div>
                                <button class="btn btn-primary btn-sm" id="btn-dsa-run-lookup">Query DJB2 Hash Table</button>
                            </div>

                            <!-- 5. Merge Sort Controls -->
                            <div id="dsa-ctrl-mergesort" class="dsa-algo-controls hidden">
                                <div class="form-row">
                                    <div class="form-group">
                                        <label>Sort Metric Key</label>
                                        <select id="dsa-mergesort-key" class="form-control">
                                            <option value="odometer">Odometer (KM)</option>
                                            <option value="registration">Registration Number</option>
                                            <option value="make">Manufacturer / Make</option>
                                            <option value="fuel">Fuel Tank Percentage</option>
                                        </select>
                                    </div>
                                </div>
                                <button class="btn btn-primary btn-sm" id="btn-dsa-run-mergesort">Execute C Merge Sort</button>
                            </div>

                            <!-- Results / Telemetry Output Box -->
                            <div class="mt-4" style="background: #0f172a; color: #f8fafc; border-radius: 8px; padding: 16px; font-family: monospace; font-size: 13px;">
                                <div style="display: flex; justify-content: space-between; align-items: center; border-bottom: 1px solid #334155; padding-bottom: 8px; margin-bottom: 10px;">
                                    <span style="color: #38bdf8; font-weight: 600;" id="dsa-output-title">C11 Algorithm Telemetry Output</span>
                                    <span id="dsa-output-complexity" class="badge" style="background: #1e293b; color: #4ade80;">Ready</span>
                                </div>
                                <pre id="dsa-lab-output" style="margin: 0; white-space: pre-wrap; max-height: 280px; overflow-y: auto; color: #e2e8f0;">Select an algorithm above and click execute to view real C11 computational results.</pre>
                            </div>
                        </div>
                    </div>
'''

dsa_match = re.search(r'(</section>\s+<!-- [=\s]+ -->\s+<!-- TAB 10: AUDIT HISTORY -->\s+<section id="tab-audit")', html)
if not dsa_match:
    dsa_match = re.search(r'(</section>\s+<!-- [=\s]+ -->\s+<section id="tab-audit")', html)

if dsa_match:
    idx = dsa_match.start()
    html = html[:idx] + dsa_lab_workbench + html[idx:]
    print('[OK] Interactive DSA Lab Workbench added to tab-dsa')
else:
    print('[FAIL] Could not locate end of tab-dsa')

# 7. Add Modal for FIFO Workshop Queue
modal_wo_queue = '''
    <!-- Modal: FIFO Workshop Dispatch Queue -->
    <div id="modal-wo-queue" class="modal-backdrop hidden">
        <div class="modal-box modal-box-lg">
            <div class="modal-header">
                <div>
                    <h3>Workshop FIFO Dispatch Queue</h3>
                    <div class="text-sm text-muted">First-In First-Out sequential processing queue maintained in C memory (O(1))</div>
                </div>
                <button class="modal-close" data-close="modal-wo-queue">&times;</button>
            </div>
            <div class="modal-body">
                <div class="table-responsive">
                    <table class="data-table">
                        <thead>
                            <tr>
                                <th style="width: 70px;">Queue Pos</th>
                                <th>WO Number</th>
                                <th>Vehicle</th>
                                <th>Work Description</th>
                                <th>Priority</th>
                                <th>Status</th>
                            </tr>
                        </thead>
                        <tbody id="tbody-wo-queue">
                            <tr><td colspan="6" class="text-center text-muted py-4">Loading workshop dispatch queue...</td></tr>
                        </tbody>
                    </table>
                </div>
            </div>
            <div class="modal-footer">
                <button type="button" class="btn btn-secondary" data-close="modal-wo-queue">Close</button>
            </div>
        </div>
    </div>
'''

if 'id="modal-wo-queue"' not in html:
    html = html.replace('<!-- Client Controller Logic -->', modal_wo_queue + '\n    <!-- Client Controller Logic -->')
    print('[OK] Modal for FIFO workshop queue added')

with open('web/index.html', 'w', encoding='utf-8') as f:
    f.write(html)
print('[SUCCESS] web/index.html successfully updated')
