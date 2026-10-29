import sys

with open('web/app.js', 'r', encoding='utf-8') as f:
    js = f.read()

# 1. Update state object with new operational properties
old_state = '''    activeWorkOrder: null,
    activeInspection: null,'''

new_state = '''    activeWorkOrder: null,
    activeInspection: null,

    // Live Monitoring & Demonstration State
    isLivePolling: false,
    pollIntervalMs: 10000,
    pollTimerId: null,
    lastSuccessfulSync: null,
    isDemoMode: false,
    demoDatabasePath: '',
    notifFilter: 'all','''

if old_state in js:
    js = js.replace(old_state, new_state, 1)
    print('[OK] Updated state with live monitoring properties')
else:
    print('[FAIL] old_state pattern not found')

# 2. Update DOMContentLoaded to initialize live monitoring, demo check, notifications, and DSA
old_dom = '''    // Server-verified session initialization and RBAC policy application
    await initSessionAndApp();
});'''

new_dom = '''    // Server-verified session initialization and RBAC policy application
    await initSessionAndApp();

    // Live Telemetry, Notifications, Demo Mode, and DSA Integration
    initLiveMonitoring();
    checkDemoStatus();
    initNotificationCenter();
    initDsaOperations();
});'''

if old_dom in js:
    js = js.replace(old_dom, new_dom, 1)
    print('[OK] Updated DOMContentLoaded')
else:
    print('[FAIL] old_dom pattern not found')

# 3. Replace loadNotifications and renderNotifications
old_notif_funcs = '''async function loadNotifications() {
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
}'''

new_notif_funcs = '''async function loadNotifications() {
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
}'''

if old_notif_funcs in js:
    js = js.replace(old_notif_funcs, new_notif_funcs, 1)
    print('[OK] Replaced loadNotifications and added ack actions')
else:
    print('[FAIL] old_notif_funcs pattern not found')

# 4. Replace renderNotifications
old_render_notifs = '''function renderNotifications() {
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
}'''

new_render_notifs = '''function renderNotifications() {
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
}'''

if old_render_notifs in js:
    js = js.replace(old_render_notifs, new_render_notifs, 1)
    print('[OK] Replaced renderNotifications with rich filtering and ack UI')
else:
    print('[FAIL] old_render_notifs pattern not found')

# 5. Append new controller modules for Live Monitoring, Demo Mode, Notification Center, and DSA Operations
new_modules = '''
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
    if (!confirm("Are you sure you want to reset the academic demonstration database?\\n\\nThis will re-seed data/demo_fleet.db with pristine vehicles, drivers, schedules, work orders, and inspections.\\nThe production database 'data/fleet.db' will NOT be touched.")) {
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
        if (!confirm('Revert the last work-order status change using the C11 Undo Stack?\\n\\nNote: Financial transitions on Completed orders cannot be reverted.')) {
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
'''

js += new_modules

with open('web/app.js', 'w', encoding='utf-8') as f:
    f.write(js)
print('[SUCCESS] web/app.js successfully updated')
