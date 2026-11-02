// CREW/DECK Core Application Logic
const App = {
  currentView: 'dashboard',
  currentParams: {},

  async init() {
    // Theme initialization
    const savedTheme = localStorage.getItem('crew_theme') || 'dark';
    document.documentElement.setAttribute('data-theme', savedTheme);
    this.updateThemeIcon(savedTheme);

    // Verify session
    const user = await Auth.checkSession();
    if (!user) {
      window.location.href = '/login.html';
      return;
    }

    Drawer.init();
    CommandPalette.init();

    this.setupNavigation();
    this.setupTopBar();

    // Default to dashboard
    this.navigate('dashboard');
  },

  setupNavigation() {
    document.querySelectorAll('.nav-item').forEach(btn => {
      btn.addEventListener('click', () => {
        const view = btn.getAttribute('data-view');
        if (view) this.navigate(view);
      });
    });

    // Theme toggle button
    const themeBtn = document.getElementById('theme-toggle-btn');
    if (themeBtn) {
      themeBtn.addEventListener('click', () => this.toggleTheme());
    }

    // Search trigger
    const searchTrigger = document.getElementById('search-trigger');
    if (searchTrigger) {
      searchTrigger.addEventListener('click', () => CommandPalette.open());
    }
  },

  setupTopBar() {
    const brand = document.getElementById('brand-icon');
    if (brand) {
      brand.addEventListener('click', () => this.navigate('dashboard'));
    }
  },

  toggleTheme() {
    const current = document.documentElement.getAttribute('data-theme') || 'dark';
    const next = current === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', next);
    localStorage.setItem('crew_theme', next);
    this.updateThemeIcon(next);
  },

  updateThemeIcon(theme) {
    const btn = document.getElementById('theme-toggle-btn');
    if (btn) {
      btn.innerHTML = theme === 'dark' ? '☀️' : '🌙';
      btn.title = `Switch to ${theme === 'dark' ? 'Light' : 'Dark'} theme`;
    }
  },

  navigate(viewName, params = {}) {
    this.currentView = viewName;
    this.currentParams = params;

    // Update active nav rail item
    document.querySelectorAll('.nav-item').forEach(el => {
      if (el.getAttribute('data-view') === viewName) {
        el.classList.add('active');
      } else {
        el.classList.remove('active');
      }
    });

    // Update top bar title
    const titleEl = document.getElementById('page-title-text');
    const badgeEl = document.getElementById('page-title-badge');
    const viewTitles = {
      dashboard: { title: 'Mission Deck', badge: 'OVERVIEW' },
      employees: { title: 'Employee Directory', badge: 'ROSTER' },
      org: { title: 'Org Constellation', badge: 'GRAPH' },
      departments: { title: 'Departments', badge: 'STRUCTURE' },
      attendance: { title: 'Attendance Deck', badge: 'HEATMAP' },
      leave: { title: 'Leave Operations', badge: 'TIME OFF' },
      payroll: { title: 'Payroll Summaries', badge: 'FINANCE' },
      audit: { title: 'Audit Trail', badge: 'SECURITY' },
    };

    const info = viewTitles[viewName] || { title: viewName, badge: 'VIEW' };
    if (titleEl) titleEl.innerText = info.title;
    if (badgeEl) badgeEl.innerText = info.badge;

    const viewport = document.getElementById('viewport-content');
    if (!viewport) return;
    viewport.innerHTML = `<div style="padding: 40px; text-align: center; color: var(--text-muted); font-size: 14px;">Loading view...</div>`;

    if (this.views[viewName]) {
      this.views[viewName](params);
    } else {
      viewport.innerHTML = `<div style="padding: 40px; color: var(--accent-rose);">View '${viewName}' not found.</div>`;
    }
  },

  async checkInToday() {
    try {
      const res = await API.post('/api/v1/attendance/check-in', {});
      API.showToast(`Checked in at ${res.check_in} (${res.status})`, 'success');
      if (this.currentView === 'attendance') this.views.attendance();
      else if (this.currentView === 'dashboard') this.views.dashboard();
    } catch {}
  },

  async checkOutToday() {
    try {
      const res = await API.post('/api/v1/attendance/check-out', {});
      API.showToast(`Checked out at ${res.check_out}`, 'success');
      if (this.currentView === 'attendance') this.views.attendance();
    } catch {}
  },

  async exportPayroll() {
    try {
      API.showToast('Generating payroll export CSV...', 'info');
      const blob = await API.get('/api/v1/payroll/export');
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `crewdeck_payroll_export_${new Date().toISOString().slice(0, 10)}.csv`;
      document.body.appendChild(a);
      a.click();
      a.remove();
      window.URL.revokeObjectURL(url);
      API.showToast('Payroll export downloaded successfully', 'success');
    } catch {}
  },

  /* ================== VIEWS ================== */
  views: {
    /* 1. DASHBOARD */
    async dashboard() {
      const viewport = document.getElementById('viewport-content');
      try {
        const m = await API.get('/api/v1/dashboard/metrics');
        if (!m) return;

        const maxDeptCount = Math.max(...m.department_distribution.map(d => d.count), 1);

        viewport.innerHTML = `
          <!-- Bento Grid Metrics -->
          <div class="bento-grid">
            <!-- Metric 1: Headcount -->
            <div class="bento-card card-span-3 metric-card">
              <div class="metric-header">
                <span class="metric-label">Total Headcount</span>
                <div class="metric-icon-wrap">👥</div>
              </div>
              <div class="metric-value">${m.headcount}</div>
              <div class="metric-footer">
                <span class="metric-trend-up">● Active</span>
                <span>across 5 departments</span>
              </div>
            </div>

            <!-- Metric 2: New Joiners -->
            <div class="bento-card card-span-3 metric-card">
              <div class="metric-header">
                <span class="metric-label">New Joiners</span>
                <div class="metric-icon-wrap" style="color: var(--accent-teal);">✨</div>
              </div>
              <div class="metric-value">${m.new_joiners}</div>
              <div class="metric-footer">
                <span>Joined this month</span>
              </div>
            </div>

            <!-- Metric 3: On Leave Today -->
            <div class="bento-card card-span-3 metric-card">
              <div class="metric-header">
                <span class="metric-label">On Leave Today</span>
                <div class="metric-icon-wrap" style="color: var(--accent-amber);">🏖️</div>
              </div>
              <div class="metric-value">${m.on_leave_today}</div>
              <div class="metric-footer">
                <span>Approved & active time-off</span>
              </div>
            </div>

            <!-- Metric 4: Pending Leaves -->
            <div class="bento-card card-span-3 metric-card">
              <div class="metric-header">
                <span class="metric-label">Pending Reviews</span>
                <div class="metric-icon-wrap" style="color: var(--accent-rose);">⏱️</div>
              </div>
              <div class="metric-value">${m.pending_leave_requests}</div>
              <div class="metric-footer">
                <a href="javascript:void(0)" onclick="App.navigate('leave')" style="color: var(--accent-amber); font-weight: 600; text-decoration: none;">Review queue →</a>
              </div>
            </div>

            <!-- Quick Action Command Banner -->
            <div class="card-span-12 action-banner">
              <div>
                <h3 style="font-family: var(--font-display); font-size: 16px; font-weight: 700; margin-bottom: 2px;">Crew Deck Fast Actions</h3>
                <p style="font-size: 12px; color: var(--text-secondary);">Direct control shortcuts for daily HR and attendance operations</p>
              </div>
              <div style="display: flex; gap: 8px; flex-wrap: wrap;">
                <button class="btn btn-teal btn-sm" onclick="App.checkInToday()">⚡ Check In Now</button>
                <button class="btn btn-secondary btn-sm" onclick="App.checkOutToday()">Check Out</button>
                <button class="btn btn-primary btn-sm" onclick="App.navigate('employees', { action: 'create' })">+ Onboard Employee</button>
                <button class="btn btn-secondary btn-sm" onclick="App.exportPayroll()">📥 Export Payroll</button>
              </div>
            </div>

            <!-- 14-day Attendance Trend -->
            <div class="bento-card card-span-8">
              <div class="card-title-bar">
                <h3 class="card-title">14-Day Attendance Stability Trend</h3>
                <span class="page-badge">Daily Check-in Ratio</span>
              </div>
              <div class="sparkline-container">
                ${m.attendance_trend.map(d => `
                  <div class="spark-bar-col" title="${d.date}: ${d.rate}% present">
                    <div class="spark-bar ${d.is_weekend ? 'weekend' : ''}" style="height: ${Math.max(d.rate, 6)}%;"></div>
                    <span class="spark-date">${d.date.slice(8)}</span>
                  </div>
                `).join('')}
              </div>
            </div>

            <!-- Department Distribution -->
            <div class="bento-card card-span-4">
              <div class="card-title-bar">
                <h3 class="card-title">Department Distribution</h3>
                <span class="page-badge">Headcount</span>
              </div>
              <div class="dept-progress-list">
                ${m.department_distribution.map(d => {
                  const pct = Math.round((d.count / maxDeptCount) * 100);
                  return `
                    <div class="dept-progress-row">
                      <div class="dept-progress-meta">
                        <span class="dept-name-label">${d.name}</span>
                        <span class="dept-count-label">${d.count} crew</span>
                      </div>
                      <div class="progress-track">
                        <div class="progress-fill" style="width: ${pct}%;"></div>
                      </div>
                    </div>
                  `;
                }).join('')}
              </div>
            </div>

            <!-- Recent Activity Timeline -->
            <div class="bento-card card-span-12">
              <div class="card-title-bar">
                <h3 class="card-title">Audit Activity Feed</h3>
                <a href="javascript:void(0)" onclick="App.navigate('audit')" style="font-size: 12px; color: var(--accent-amber); text-decoration: none;">View full log →</a>
              </div>
              <div style="display: flex; flex-direction: column; gap: 8px;">
                ${m.recent_activity.map(a => `
                  <div style="display: flex; align-items: center; justify-content: space-between; padding: 10px 14px; background: var(--bg-surface-elevated); border-radius: var(--radius-md); font-size: 13px;">
                    <div style="display: flex; align-items: center; gap: 12px;">
                      <span class="user-role-badge" style="font-size: 10px;">${a.action}</span>
                      <span style="color: var(--text-primary);">${a.details}</span>
                    </div>
                    <div style="display: flex; align-items: center; gap: 12px; font-size: 11px; color: var(--text-muted);">
                      <span>by ${a.user_email}</span>
                      <span class="font-mono">${a.created_at.replace('T', ' ').slice(0, 19)}</span>
                    </div>
                  </div>
                `).join('')}
              </div>
            </div>
          </div>
        `;
      } catch (err) {
        viewport.innerHTML = `<div style="color: var(--accent-rose); padding: 20px;">Failed to load dashboard metrics.</div>`;
      }
    },

    /* 2. EMPLOYEES DIRECTORY */
    async employees(params = {}) {
      const viewport = document.getElementById('viewport-content');

      let currentSearch = params.search || '';
      let currentDept = params.dept || '';
      let currentStatus = params.status || '';
      let currentSort = params.sort || 'id';
      let currentOrder = params.order || 'asc';
      let currentPage = params.page || 1;

      const renderRoster = async () => {
        try {
          const [empRes, deptRes] = await Promise.all([
            API.get('/api/v1/employees', {
              search: currentSearch,
              dept: currentDept,
              status: currentStatus,
              sort: currentSort,
              order: currentOrder,
              page: currentPage,
              limit: 12
            }),
            API.get('/api/v1/departments')
          ]);

          const depts = deptRes?.departments || [];
          const items = empRes?.items || [];
          const pag = empRes?.pagination || { page: 1, limit: 12, total: 0, pages: 1 };

          viewport.innerHTML = `
            <div class="table-card">
              <!-- Toolbar -->
              <div class="table-toolbar">
                <div class="toolbar-left">
                  <input type="text" id="emp-search-input" class="filter-input" placeholder="Search name, role, email..." value="${currentSearch}" />
                  <select id="emp-dept-filter" class="filter-select">
                    <option value="">All Departments</option>
                    ${depts.map(d => `<option value="${d.id}" ${d.id == currentDept ? 'selected' : ''}>${d.name}</option>`).join('')}
                  </select>
                  <select id="emp-status-filter" class="filter-select">
                    <option value="">All Statuses</option>
                    <option value="active" ${currentStatus === 'active' ? 'selected' : ''}>Active</option>
                    <option value="on-leave" ${currentStatus === 'on-leave' ? 'selected' : ''}>On-Leave</option>
                    <option value="exited" ${currentStatus === 'exited' ? 'selected' : ''}>Exited</option>
                  </select>
                </div>
                <div class="toolbar-right">
                  <select id="emp-sort-select" class="filter-select">
                    <option value="id-asc" ${currentSort === 'id' && currentOrder === 'asc' ? 'selected' : ''}>ID (Asc)</option>
                    <option value="name-asc" ${currentSort === 'name' && currentOrder === 'asc' ? 'selected' : ''}>Name (A-Z)</option>
                    <option value="salary-desc" ${currentSort === 'salary' && currentOrder === 'desc' ? 'selected' : ''}>Salary (Highest)</option>
                    <option value="join_date-desc" ${currentSort === 'join_date' && currentOrder === 'desc' ? 'selected' : ''}>Join Date (Newest)</option>
                  </select>
                  <button id="add-emp-btn" class="btn btn-primary btn-sm">+ Onboard Employee</button>
                </div>
              </div>

              <!-- Table -->
              <div class="table-responsive">
                <table class="data-table">
                  <thead>
                    <tr>
                      <th>Employee</th>
                      <th>Role & Dept</th>
                      <th>Contact</th>
                      <th>Join Date</th>
                      <th>Compensation</th>
                      <th>Status</th>
                      <th>Actions</th>
                    </tr>
                  </thead>
                  <tbody>
                    ${items.length === 0 ? `<tr><td colspan="7" style="text-align: center; padding: 36px; color: var(--text-muted);">No employees found matching criteria.</td></tr>` :
                      items.map(e => {
                        const inits = e.name.split(' ').map(p => p[0]).slice(0, 2).join('');
                        const salaryINR = '₹' + Number(e.salary).toLocaleString('en-IN');
                        return `
                          <tr onclick="Drawer.open(${e.id})">
                            <td>
                              <div style="display: flex; align-items: center; gap: 10px;">
                                <div class="avatar-circle" style="background-color: ${e.avatar_color};">
                                  ${inits}
                                </div>
                                <div>
                                  <div style="font-weight: 600; color: var(--text-primary);">${e.name}</div>
                                  <div class="id-badge" style="font-size: 11px; color: var(--text-muted);">EMP-${String(e.id).padStart(4, '0')}</div>
                                </div>
                              </div>
                            </td>
                            <td>
                              <div style="font-weight: 500;">${e.role_title}</div>
                              <div style="font-size: 11px; color: var(--accent-amber);">${e.department_name}</div>
                            </td>
                            <td>
                              <div style="font-size: 12px;">${e.email}</div>
                              <div class="font-mono" style="font-size: 11px; color: var(--text-muted);">${e.phone}</div>
                            </td>
                            <td class="font-mono" style="font-size: 12px;">${e.join_date}</td>
                            <td class="font-mono salary" style="font-weight: 600; color: var(--accent-teal);">${salaryINR}</td>
                            <td>
                              <span class="status-badge status-${e.status}">${e.status}</span>
                            </td>
                            <td onclick="event.stopPropagation()">
                              <button class="btn btn-secondary btn-sm" onclick="Drawer.open(${e.id})">View</button>
                              ${Auth.user && Auth.user.role === 'admin' ? `
                                <button class="btn btn-danger btn-sm" style="margin-left: 4px;" onclick="App.deleteEmployee(${e.id}, '${e.name}')">Delete</button>
                              ` : ''}
                            </td>
                          </tr>
                        `;
                      }).join('')
                    }
                  </tbody>
                </table>
              </div>

              <!-- Pagination Footer -->
              <div class="table-pagination">
                <span>Showing ${items.length} of ${pag.total} crew members</span>
                <div style="display: flex; gap: 6px; align-items: center;">
                  <button class="btn btn-secondary btn-sm" ${currentPage <= 1 ? 'disabled' : ''} id="prev-page-btn">← Prev</button>
                  <span class="font-mono">Page ${pag.page} of ${pag.pages}</span>
                  <button class="btn btn-secondary btn-sm" ${currentPage >= pag.pages ? 'disabled' : ''} id="next-page-btn">Next →</button>
                </div>
              </div>
            </div>
          `;

          // Event attachments
          let timer = null;
          document.getElementById('emp-search-input').addEventListener('input', (e) => {
            clearTimeout(timer);
            timer = setTimeout(() => {
              currentSearch = e.target.value.trim();
              currentPage = 1;
              renderRoster();
            }, 250);
          });

          document.getElementById('emp-dept-filter').addEventListener('change', (e) => {
            currentDept = e.target.value;
            currentPage = 1;
            renderRoster();
          });

          document.getElementById('emp-status-filter').addEventListener('change', (e) => {
            currentStatus = e.target.value;
            currentPage = 1;
            renderRoster();
          });

          document.getElementById('emp-sort-select').addEventListener('change', (e) => {
            const [s, o] = e.target.value.split('-');
            currentSort = s;
            currentOrder = o;
            renderRoster();
          });

          document.getElementById('prev-page-btn').onclick = () => {
            if (currentPage > 1) { currentPage--; renderRoster(); }
          };

          document.getElementById('next-page-btn').onclick = () => {
            if (currentPage < pag.pages) { currentPage++; renderRoster(); }
          };

          document.getElementById('add-emp-btn').onclick = () => App.showAddEmployeeModal(depts);

          if (params.action === 'create') {
            App.showAddEmployeeModal(depts);
          }
        } catch (err) {
          viewport.innerHTML = `<div style="color: var(--accent-rose); padding: 20px;">Failed to load employee list.</div>`;
        }
      };

      renderRoster();
    },

    /* 3. ORG CONSTELLATION */
    org() {
      const viewport = document.getElementById('viewport-content');
      OrgConstellation.init(viewport);
    },

    /* 4. DEPARTMENTS */
    async departments() {
      const viewport = document.getElementById('viewport-content');
      try {
        const res = await API.get('/api/v1/departments');
        const depts = res?.departments || [];

        viewport.innerHTML = `
          <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px;">
            <div>
              <h2 style="font-family: var(--font-display); font-size: 18px; font-weight: 700;">Organizational Units</h2>
              <p style="font-size: 12px; color: var(--text-secondary);">Core functional teams, headcount, and budget allocations</p>
            </div>
            ${Auth.user && Auth.user.role === 'admin' ? `
              <button id="add-dept-btn" class="btn btn-primary btn-sm">+ Create Department</button>
            ` : ''}
          </div>

          <div class="bento-grid">
            ${depts.map(d => {
              const annualPayrollINR = '₹' + Math.round(d.total_payroll).toLocaleString('en-IN');
              const monthlyPayrollINR = '₹' + Math.round(d.total_payroll / 12).toLocaleString('en-IN');
              return `
                <div class="bento-card card-span-4" style="gap: 14px;">
                  <div style="display: flex; justify-content: space-between; align-items: flex-start;">
                    <div>
                      <h3 style="font-family: var(--font-display); font-size: 17px; font-weight: 700; color: var(--text-primary);">${d.name}</h3>
                      <span class="font-mono" style="font-size: 11px; color: var(--text-muted);">DEPT-${String(d.id).padStart(3, '0')}</span>
                    </div>
                    <span class="page-badge" style="color: var(--accent-amber); font-weight: 600;">${d.headcount} Members</span>
                  </div>

                  <p style="font-size: 13px; color: var(--text-secondary); line-height: 1.4; min-height: 38px;">
                    ${d.description || 'No description provided'}
                  </p>

                  <div style="background: var(--bg-surface-elevated); border: 1px solid var(--border-subtle); border-radius: var(--radius-md); padding: 12px; display: flex; flex-direction: column; gap: 8px;">
                    <div style="display: flex; justify-content: space-between; align-items: center; font-size: 12px;">
                      <span style="color: var(--text-muted);">Department Lead</span>
                      <span style="font-weight: 600; color: var(--text-primary);">${d.head_name}</span>
                    </div>
                    <div style="display: flex; justify-content: space-between; align-items: center; font-size: 12px;">
                      <span style="color: var(--text-muted);">Monthly Payout</span>
                      <span class="font-mono" style="font-weight: 600; color: var(--accent-teal);">${monthlyPayrollINR}</span>
                    </div>
                    <div style="display: flex; justify-content: space-between; align-items: center; font-size: 12px;">
                      <span style="color: var(--text-muted);">Annual Payout</span>
                      <span class="font-mono" style="font-size: 11px; color: var(--text-secondary);">${annualPayrollINR}</span>
                    </div>
                  </div>

                  <div style="display: flex; justify-content: flex-end; gap: 8px; margin-top: auto;">
                    <button class="btn btn-secondary btn-sm" onclick="App.navigate('employees', { dept: ${d.id} })">View Crew</button>
                    ${Auth.user && Auth.user.role === 'admin' ? `
                      <button class="btn btn-danger btn-sm" onclick="App.deleteDepartment(${d.id}, '${d.name}')">Delete</button>
                    ` : ''}
                  </div>
                </div>
              `;
            }).join('')}
          </div>
        `;

        const addDeptBtn = document.getElementById('add-dept-btn');
        if (addDeptBtn) {
          addDeptBtn.onclick = () => App.showAddDepartmentModal();
        }
      } catch (err) {
        viewport.innerHTML = `<div style="color: var(--accent-rose); padding: 20px;">Failed to load departments.</div>`;
      }
    },

    /* 5. ATTENDANCE & 365-DAY HEATMAP */
    async attendance(params = {}) {
      const viewport = document.getElementById('viewport-content');
      let currentDate = params.date || new Date().toISOString().slice(0, 10);
      let selectedEmpForHeatmap = params.employeeId || null;

      const renderView = async () => {
        try {
          const res = await API.get('/api/v1/attendance', { date: currentDate, limit: 50 });
          const items = res?.items || [];

          // Summary counts
          const presentCount = items.filter(i => i.status === 'present' || i.status === 'late').length;
          const onLeaveCount = items.filter(i => i.status === 'on-leave').length;
          const absentCount = items.filter(i => i.status === 'absent').length;

          viewport.innerHTML = `
            <!-- Top Punch Banner -->
            <div class="action-banner">
              <div>
                <h3 style="font-family: var(--font-display); font-size: 16px; font-weight: 700;">Web Attendance Punch Deck</h3>
                <p style="font-size: 12px; color: var(--text-secondary);">Logged in as <b>${Auth.user.name}</b> (${Auth.user.role})</p>
              </div>
              <div style="display: flex; gap: 8px; align-items: center;">
                <button class="btn btn-teal btn-sm" onclick="App.checkInToday()">⚡ Check In (Now)</button>
                <button class="btn btn-secondary btn-sm" onclick="App.checkOutToday()">Check Out (Now)</button>
              </div>
            </div>

            <!-- Heatmap View Section (if employee selected or default to first) -->
            <div id="heatmap-section" class="bento-card" style="padding: 16px;">
              <div class="card-title-bar">
                <div>
                  <h3 class="card-title" id="heatmap-title">365-Day Attendance Calendar Heatmap</h3>
                  <p style="font-size: 11px; color: var(--text-secondary);" id="heatmap-subtitle">Select an employee below to view their annual consistency</p>
                </div>
                <div class="heatmap-legend">
                  <span>Less</span>
                  <div class="heatmap-cell level-0" style="display: inline-block;"></div>
                  <div class="heatmap-cell level-1" style="display: inline-block;"></div>
                  <div class="heatmap-cell level-2" style="display: inline-block;"></div>
                  <div class="heatmap-cell level-3" style="display: inline-block;"></div>
                  <div class="heatmap-cell level-4" style="display: inline-block;"></div>
                  <span>More</span>
                </div>
              </div>
              <div id="heatmap-grid-container" class="heatmap-container">
                <div style="padding: 20px; text-align: center; color: var(--text-muted);">Loading calendar heatmap...</div>
              </div>
            </div>

            <!-- Daily Attendance Roster Table -->
            <div class="table-card">
              <div class="table-toolbar">
                <div class="toolbar-left">
                  <span style="font-weight: 600; font-size: 13px;">Date:</span>
                  <input type="date" id="att-date-picker" class="filter-input" value="${currentDate}" style="width: 160px;" />
                  <span class="status-badge status-present" style="margin-left: 8px;">${presentCount} Present</span>
                  <span class="status-badge status-on-leave">${onLeaveCount} On Leave</span>
                  <span class="status-badge status-absent">${absentCount} Absent</span>
                </div>
              </div>

              <div class="table-responsive">
                <table class="data-table">
                  <thead>
                    <tr>
                      <th>Employee</th>
                      <th>Department</th>
                      <th>Check-In</th>
                      <th>Check-Out</th>
                      <th>Status</th>
                      <th>Notes</th>
                      <th>Actions</th>
                    </tr>
                  </thead>
                  <tbody>
                    ${items.map(row => `
                      <tr onclick="App.loadEmployeeHeatmap(${row.employee_id}, '${row.employee_name}')">
                        <td>
                          <div style="display: flex; align-items: center; gap: 8px;">
                            <div class="avatar-circle" style="background-color: ${row.avatar_color}; width: 26px; height: 26px; font-size: 10px;">
                              ${row.employee_name[0]}
                            </div>
                            <span style="font-weight: 600;">${row.employee_name}</span>
                          </div>
                        </td>
                        <td style="color: var(--text-secondary);">${row.department_name}</td>
                        <td class="font-mono" style="font-size: 12px;">${row.check_in}</td>
                        <td class="font-mono" style="font-size: 12px;">${row.check_out}</td>
                        <td><span class="status-badge status-${row.status}">${row.status}</span></td>
                        <td style="font-size: 11px; color: var(--text-muted);">${row.notes || '--'}</td>
                        <td onclick="event.stopPropagation()">
                          <button class="btn btn-secondary btn-sm" onclick="App.loadEmployeeHeatmap(${row.employee_id}, '${row.employee_name}')">Heatmap</button>
                          <button class="btn btn-secondary btn-sm" style="margin-left: 4px;" onclick="Drawer.open(${row.employee_id})">Profile</button>
                        </td>
                      </tr>
                    `).join('')}
                  </tbody>
                </table>
              </div>
            </div>
          `;

          document.getElementById('att-date-picker').addEventListener('change', (e) => {
            currentDate = e.target.value;
            renderView();
          });

          // Load heatmap for default employee or selected
          const targetEmpId = selectedEmpForHeatmap || items[0]?.employee_id || 1;
          const targetEmpName = items.find(i => i.employee_id === targetEmpId)?.employee_name || 'Aarav Sharma';
          App.loadEmployeeHeatmap(targetEmpId, targetEmpName);
        } catch (err) {
          viewport.innerHTML = `<div style="color: var(--accent-rose); padding: 20px;">Failed to load attendance roster.</div>`;
        }
      };

      renderView();
    },

    /* 6. LEAVE MANAGEMENT */
    async leave() {
      const viewport = document.getElementById('viewport-content');
      try {
        const [reqRes, meBal] = await Promise.all([
          API.get('/api/v1/leave/requests', { limit: 50 }),
          API.get(`/api/v1/leave/balances/${Auth.user.employee_id || 1}`)
        ]);

        const requests = reqRes?.items || [];
        const bal = meBal || { annual_total: 18, annual_used: 0, sick_total: 12, sick_used: 0, casual_total: 10, casual_used: 0 };

        viewport.innerHTML = `
          <!-- Balance Summary Bento Row -->
          <div class="bento-grid" style="margin-bottom: 20px;">
            <div class="bento-card card-span-4" style="background: linear-gradient(135deg, rgba(255, 176, 32, 0.08), var(--bg-surface));">
              <span class="metric-label">Annual Leave Balance</span>
              <div class="metric-value" style="color: var(--accent-amber); margin: 8px 0;">
                ${bal.annual_total - bal.annual_used} <span style="font-size: 14px; color: var(--text-muted);">/ ${bal.annual_total} Days</span>
              </div>
              <div class="metric-footer">Used: ${bal.annual_used} days in 2026</div>
            </div>

            <div class="bento-card card-span-4" style="background: linear-gradient(135deg, rgba(45, 212, 191, 0.08), var(--bg-surface));">
              <span class="metric-label">Sick Leave Balance</span>
              <div class="metric-value" style="color: var(--accent-teal); margin: 8px 0;">
                ${bal.sick_total - bal.sick_used} <span style="font-size: 14px; color: var(--text-muted);">/ ${bal.sick_total} Days</span>
              </div>
              <div class="metric-footer">Used: ${bal.sick_used} days in 2026</div>
            </div>

            <div class="bento-card card-span-4" style="background: linear-gradient(135deg, rgba(236, 72, 153, 0.08), var(--bg-surface));">
              <span class="metric-label">Casual Leave Balance</span>
              <div class="metric-value" style="color: var(--accent-rose); margin: 8px 0;">
                ${bal.casual_total - bal.casual_used} <span style="font-size: 14px; color: var(--text-muted);">/ ${bal.casual_total} Days</span>
              </div>
              <div class="metric-footer">Used: ${bal.casual_used} days in 2026</div>
            </div>
          </div>

          <!-- Leave Requests Roster Card -->
          <div class="table-card">
            <div class="table-toolbar">
              <div class="toolbar-left">
                <h3 style="font-family: var(--font-display); font-size: 15px; font-weight: 700;">Leave Requests Queue</h3>
              </div>
              <div class="toolbar-right">
                <button id="req-leave-btn" class="btn btn-primary btn-sm">+ Request Time Off</button>
              </div>
            </div>

            <div class="table-responsive">
              <table class="data-table">
                <thead>
                  <tr>
                    <th>Crew Member</th>
                    <th>Leave Type</th>
                    <th>Dates</th>
                    <th>Days</th>
                    <th>Reason</th>
                    <th>Status</th>
                    <th>Reviewed By</th>
                    <th>Actions</th>
                  </tr>
                </thead>
                <tbody>
                  ${requests.map(r => `
                    <tr>
                      <td>
                        <div style="display: flex; align-items: center; gap: 8px;">
                          <div class="avatar-circle" style="background-color: ${r.avatar_color}; width: 26px; height: 26px; font-size: 10px;">
                            ${r.employee_name[0]}
                          </div>
                          <div>
                            <div style="font-weight: 600;">${r.employee_name}</div>
                            <div style="font-size: 10px; color: var(--text-muted);">${r.department_name}</div>
                          </div>
                        </div>
                      </td>
                      <td style="text-transform: capitalize; font-weight: 500;">${r.leave_type}</td>
                      <td class="font-mono" style="font-size: 12px;">${r.start_date} → ${r.end_date}</td>
                      <td class="font-mono" style="font-weight: 600;">${r.days}d</td>
                      <td style="font-size: 12px; color: var(--text-secondary); max-width: 240px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;">
                        ${r.reason}
                      </td>
                      <td><span class="status-badge status-${r.status}">${r.status}</span></td>
                      <td style="font-size: 11px; color: var(--text-muted);">
                        ${r.reviewer_name ? `${r.reviewer_name}` : '--'}
                        ${r.review_comment ? `<div style="font-style: italic; color: var(--text-secondary);">${r.review_comment}</div>` : ''}
                      </td>
                      <td>
                        ${r.status === 'pending' && (Auth.user.role === 'admin' || Auth.user.role === 'manager') ? `
                          <button class="btn btn-teal btn-sm" onclick="App.reviewLeave(${r.id}, 'approved')">Approve</button>
                          <button class="btn btn-danger btn-sm" style="margin-left: 4px;" onclick="App.reviewLeave(${r.id}, 'rejected')">Reject</button>
                        ` : `
                          <span style="font-size: 11px; color: var(--text-muted);">Completed</span>
                        `}
                      </td>
                    </tr>
                  `).join('')}
                </tbody>
              </table>
            </div>
          </div>
        `;

        document.getElementById('req-leave-btn').onclick = () => App.showRequestLeaveModal(bal);
      } catch (err) {
        viewport.innerHTML = `<div style="color: var(--accent-rose); padding: 20px;">Failed to load leave requests.</div>`;
      }
    },

    /* 7. PAYROLL */
    async payroll() {
      const viewport = document.getElementById('viewport-content');
      try {
        const p = await API.get('/api/v1/payroll/summary');
        if (!p) return;

        const totalAnnualINR = '₹' + Math.round(p.total_annual_payroll).toLocaleString('en-IN');
        const totalMonthlyINR = '₹' + Math.round(p.total_monthly_payroll).toLocaleString('en-IN');
        const avgAnnualINR = '₹' + Math.round(p.average_annual_salary).toLocaleString('en-IN');
        const avgMonthlyINR = '₹' + Math.round(p.average_monthly_salary).toLocaleString('en-IN');

        viewport.innerHTML = `
          <!-- Payroll Metrics -->
          <div class="bento-grid" style="margin-bottom: 20px;">
            <div class="bento-card card-span-3">
              <span class="metric-label">Monthly Payroll Outflow</span>
              <div class="metric-value font-mono" style="color: var(--accent-teal); margin: 6px 0;">${totalMonthlyINR}</div>
              <div class="metric-footer">Direct monthly disbursement</div>
            </div>

            <div class="bento-card card-span-3">
              <span class="metric-label">Annual Projected CTC</span>
              <div class="metric-value font-mono" style="margin: 6px 0;">${totalAnnualINR}</div>
              <div class="metric-footer">${p.total_headcount} active employees</div>
            </div>

            <div class="bento-card card-span-3">
              <span class="metric-label">Average Annual Salary</span>
              <div class="metric-value font-mono" style="color: var(--accent-amber); margin: 6px 0;">${avgAnnualINR}</div>
              <div class="metric-footer">${avgMonthlyINR}/month average</div>
            </div>

            <div class="bento-card card-span-3" style="justify-content: center; align-items: center; text-align: center;">
              <button class="btn btn-primary" onclick="App.exportPayroll()" style="padding: 12px 20px; width: 100%;">
                📥 Export Payroll CSV
              </button>
              <span style="font-size: 11px; color: var(--text-muted); margin-top: 6px;">Includes full breakdown for all crew</span>
            </div>
          </div>

          <!-- Department Payroll Table -->
          <div class="table-card">
            <div class="table-toolbar">
              <h3 style="font-family: var(--font-display); font-size: 15px; font-weight: 700;">Department Compensation Breakdown</h3>
            </div>
            <div class="table-responsive">
              <table class="data-table">
                <thead>
                  <tr>
                    <th>Department</th>
                    <th>Headcount</th>
                    <th>Monthly Outflow</th>
                    <th>Annual Budget</th>
                    <th>Average Salary</th>
                    <th>% of Total Budget</th>
                  </tr>
                </thead>
                <tbody>
                  ${p.departments.map(d => {
                    const pct = ((d.annual_payroll / (p.total_annual_payroll || 1)) * 100).toFixed(1);
                    return `
                      <tr>
                        <td style="font-weight: 600;">${d.department_name}</td>
                        <td class="font-mono">${d.headcount}</td>
                        <td class="font-mono" style="font-weight: 600; color: var(--accent-teal);">₹${Math.round(d.monthly_payroll).toLocaleString('en-IN')}</td>
                        <td class="font-mono">₹${Math.round(d.annual_payroll).toLocaleString('en-IN')}</td>
                        <td class="font-mono">₹${Math.round(d.average_salary).toLocaleString('en-IN')}</td>
                        <td>
                          <div style="display: flex; align-items: center; gap: 8px;">
                            <div class="progress-track" style="width: 80px;">
                              <div class="progress-fill" style="width: ${pct}%;"></div>
                            </div>
                            <span class="font-mono" style="font-size: 11px;">${pct}%</span>
                          </div>
                        </td>
                      </tr>
                    `;
                  }).join('')}
                </tbody>
              </table>
            </div>
          </div>
        `;
      } catch (err) {
        viewport.innerHTML = `<div style="color: var(--accent-rose); padding: 20px;">Failed to load payroll summary.</div>`;
      }
    },

    /* 8. AUDIT LOG */
    async audit() {
      const viewport = document.getElementById('viewport-content');
      try {
        const res = await API.get('/api/v1/audit', { limit: 40 });
        const items = res?.items || [];

        viewport.innerHTML = `
          <div class="table-card">
            <div class="table-toolbar">
              <div class="toolbar-left">
                <h3 style="font-family: var(--font-display); font-size: 15px; font-weight: 700;">System Event Audit Trail</h3>
                <span class="page-badge">Immutable Record</span>
              </div>
            </div>

            <div class="table-responsive">
              <table class="data-table">
                <thead>
                  <tr>
                    <th>Timestamp (UTC)</th>
                    <th>Action</th>
                    <th>User</th>
                    <th>Entity</th>
                    <th>Details</th>
                    <th>IP Address</th>
                  </tr>
                </thead>
                <tbody>
                  ${items.map(log => `
                    <tr>
                      <td class="font-mono" style="font-size: 12px; color: var(--text-secondary);">${log.created_at.replace('T', ' ').slice(0, 19)}</td>
                      <td><span class="user-role-badge" style="font-size: 10px;">${log.action}</span></td>
                      <td style="font-size: 12px; font-weight: 500;">${log.user_email}</td>
                      <td style="font-size: 11px; text-transform: uppercase; color: var(--accent-amber);">${log.entity_type} #${log.entity_id}</td>
                      <td style="font-size: 13px;">${log.details}</td>
                      <td class="font-mono" style="font-size: 11px; color: var(--text-muted);">${log.ip_address}</td>
                    </tr>
                  `).join('')}
                </tbody>
              </table>
            </div>
          </div>
        `;
      } catch (err) {
        viewport.innerHTML = `<div style="color: var(--accent-rose); padding: 20px;">Failed to load audit logs.</div>`;
      }
    }
  },

  /* Load employee 365-day calendar heatmap */
  async loadEmployeeHeatmap(empId, empName) {
    const titleEl = document.getElementById('heatmap-title');
    const subtitleEl = document.getElementById('heatmap-subtitle');
    const container = document.getElementById('heatmap-grid-container');
    if (!container) return;

    if (titleEl) titleEl.innerText = `365-Day Attendance Heatmap: ${empName}`;
    if (subtitleEl) subtitleEl.innerText = `Full year calendar showing daily check-in intensity`;

    try {
      const res = await API.get(`/api/v1/attendance/heatmap/${empId}`);
      const records = res?.records || [];

      // Map date -> record
      const recordMap = {};
      records.forEach(r => recordMap[r.date] = r);

      // Generate 52 weeks x 7 days
      const days = [];
      const startDate = new Date(2026, 0, 1);
      for (let i = 0; i < 365; i++) {
        const d = new Date(startDate.getTime() + (i * 86400000));
        const dateStr = d.toISOString().slice(0, 10);
        const rec = recordMap[dateStr];
        days.push({
          date: dateStr,
          level: rec ? rec.level : (d.getDay() === 0 || d.getDay() === 6 ? 0 : 0),
          status: rec ? rec.status : (d.getDay() === 0 || d.getDay() === 6 ? 'Weekend' : 'Not recorded'),
          checkIn: rec?.check_in || '--',
          checkOut: rec?.check_out || '--'
        });
      }

      container.innerHTML = `
        <div class="heatmap-grid">
          ${days.map(d => `
            <div class="heatmap-cell level-${d.level}"
                 title="${d.date}: ${d.status} (${d.checkIn} - ${d.checkOut})"></div>
          `).join('')}
        </div>
      `;
    } catch {
      container.innerHTML = `<div style="padding: 20px; color: var(--accent-rose);">Failed to load heatmap data.</div>`;
    }
  },

  /* Modals */
  showAddEmployeeModal(depts) {
    let modalBackdrop = document.getElementById('modal-backdrop');
    if (!modalBackdrop) {
      modalBackdrop = document.createElement('div');
      modalBackdrop.id = 'modal-backdrop';
      modalBackdrop.className = 'drawer-backdrop';
      document.body.appendChild(modalBackdrop);
    }

    modalBackdrop.innerHTML = `
      <div style="background: var(--bg-surface); border: 1px solid var(--border-medium); border-radius: var(--radius-lg); width: 480px; max-width: 90vw; padding: 24px; box-shadow: var(--shadow-lg); margin: 60px auto;">
        <h3 style="font-family: var(--font-display); font-size: 18px; font-weight: 700; margin-bottom: 4px;">Onboard New Employee</h3>
        <p style="font-size: 12px; color: var(--text-secondary); margin-bottom: 16px;">Create employee profile and initialize leave balances</p>
        <form id="add-emp-form" style="display: flex; flex-direction: column; gap: 12px;">
          <div>
            <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Full Name *</label>
            <input type="text" id="new-name" class="filter-input" style="width: 100%;" placeholder="e.g. Rahul Sharma" required />
          </div>
          <div>
            <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Work Email *</label>
            <input type="email" id="new-email" class="filter-input" style="width: 100%;" placeholder="rahul.sharma@crewdeck.io" required />
          </div>
          <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
            <div>
              <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Phone *</label>
              <input type="text" id="new-phone" class="filter-input" style="width: 100%;" placeholder="+91 98200 12345" required />
            </div>
            <div>
              <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Join Date *</label>
              <input type="date" id="new-join" class="filter-input" style="width: 100%;" value="${new Date().toISOString().slice(0, 10)}" required />
            </div>
          </div>
          <div>
            <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Job Title *</label>
            <input type="text" id="new-role" class="filter-input" style="width: 100%;" placeholder="e.g. Senior Software Engineer" required />
          </div>
          <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
            <div>
              <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Department</label>
              <select id="new-dept" class="filter-select" style="width: 100%;">
                ${depts.map(d => `<option value="${d.id}">${d.name}</option>`).join('')}
              </select>
            </div>
            <div>
              <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Annual Salary (INR)</label>
              <input type="number" id="new-salary" class="filter-input" style="width: 100%;" value="1800000" step="50000" required />
            </div>
          </div>
          <div style="display: flex; justify-content: flex-end; gap: 8px; margin-top: 12px;">
            <button type="button" class="btn btn-secondary btn-sm" onclick="document.getElementById('modal-backdrop').classList.remove('open')">Cancel</button>
            <button type="submit" class="btn btn-primary btn-sm">Onboard Employee</button>
          </div>
        </form>
      </div>
    `;

    modalBackdrop.classList.add('open');

    document.getElementById('add-emp-form').onsubmit = async (e) => {
      e.preventDefault();
      try {
        const body = {
          name: document.getElementById('new-name').value.trim(),
          email: document.getElementById('new-email').value.trim(),
          phone: document.getElementById('new-phone').value.trim(),
          role_title: document.getElementById('new-role').value.trim(),
          join_date: document.getElementById('new-join').value,
          department_id: parseInt(document.getElementById('new-dept').value),
          salary: parseFloat(document.getElementById('new-salary').value),
          status: 'active'
        };

        const res = await API.post('/api/v1/employees', body);
        API.showToast(`Onboarded ${body.name} successfully`, 'success');
        modalBackdrop.classList.remove('open');
        App.views.employees();
        Drawer.open(res.id);
      } catch {}
    };
  },

  showAddDepartmentModal() {
    let modalBackdrop = document.getElementById('modal-backdrop');
    if (!modalBackdrop) {
      modalBackdrop = document.createElement('div');
      modalBackdrop.id = 'modal-backdrop';
      modalBackdrop.className = 'drawer-backdrop';
      document.body.appendChild(modalBackdrop);
    }

    modalBackdrop.innerHTML = `
      <div style="background: var(--bg-surface); border: 1px solid var(--border-medium); border-radius: var(--radius-lg); width: 420px; max-width: 90vw; padding: 24px; box-shadow: var(--shadow-lg); margin: 80px auto;">
        <h3 style="font-family: var(--font-display); font-size: 18px; font-weight: 700; margin-bottom: 4px;">Create New Department</h3>
        <p style="font-size: 12px; color: var(--text-secondary); margin-bottom: 16px;">Define a new organizational unit</p>
        <form id="add-dept-form" style="display: flex; flex-direction: column; gap: 12px;">
          <div>
            <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Department Name *</label>
            <input type="text" id="new-dept-name" class="filter-input" style="width: 100%;" placeholder="e.g. Data & Analytics" required />
          </div>
          <div>
            <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Description</label>
            <textarea id="new-dept-desc" class="filter-input" style="width: 100%; height: 70px; resize: vertical;" placeholder="Mission and scope of this department..."></textarea>
          </div>
          <div style="display: flex; justify-content: flex-end; gap: 8px; margin-top: 10px;">
            <button type="button" class="btn btn-secondary btn-sm" onclick="document.getElementById('modal-backdrop').classList.remove('open')">Cancel</button>
            <button type="submit" class="btn btn-primary btn-sm">Create Department</button>
          </div>
        </form>
      </div>
    `;

    modalBackdrop.classList.add('open');

    document.getElementById('add-dept-form').onsubmit = async (e) => {
      e.preventDefault();
      try {
        const body = {
          name: document.getElementById('new-dept-name').value.trim(),
          description: document.getElementById('new-dept-desc').value.trim()
        };
        await API.post('/api/v1/departments', body);
        API.showToast(`Department '${body.name}' created`, 'success');
        modalBackdrop.classList.remove('open');
        App.views.departments();
      } catch {}
    };
  },

  showRequestLeaveModal(bal) {
    let modalBackdrop = document.getElementById('modal-backdrop');
    if (!modalBackdrop) {
      modalBackdrop = document.createElement('div');
      modalBackdrop.id = 'modal-backdrop';
      modalBackdrop.className = 'drawer-backdrop';
      document.body.appendChild(modalBackdrop);
    }

    const todayStr = new Date().toISOString().slice(0, 10);

    modalBackdrop.innerHTML = `
      <div style="background: var(--bg-surface); border: 1px solid var(--border-medium); border-radius: var(--radius-lg); width: 440px; max-width: 90vw; padding: 24px; box-shadow: var(--shadow-lg); margin: 70px auto;">
        <h3 style="font-family: var(--font-display); font-size: 18px; font-weight: 700; margin-bottom: 4px;">Request Time Off</h3>
        <p style="font-size: 12px; color: var(--text-secondary); margin-bottom: 14px;">Available: ${bal.annual_total - bal.annual_used} Annual, ${bal.sick_total - bal.sick_used} Sick, ${bal.casual_total - bal.casual_used} Casual</p>
        <form id="req-leave-form" style="display: flex; flex-direction: column; gap: 12px;">
          <div>
            <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Leave Type *</label>
            <select id="leave-type-select" class="filter-select" style="width: 100%;">
              <option value="annual">Annual Leave (${bal.annual_total - bal.annual_used} days remaining)</option>
              <option value="sick">Sick Leave (${bal.sick_total - bal.sick_used} days remaining)</option>
              <option value="casual">Casual Leave (${bal.casual_total - bal.casual_used} days remaining)</option>
              <option value="unpaid">Unpaid Leave</option>
            </select>
          </div>
          <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
            <div>
              <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Start Date *</label>
              <input type="date" id="leave-start" class="filter-input" style="width: 100%;" value="${todayStr}" required />
            </div>
            <div>
              <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">End Date *</label>
              <input type="date" id="leave-end" class="filter-input" style="width: 100%;" value="${todayStr}" required />
            </div>
          </div>
          <div>
            <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Total Working Days *</label>
            <input type="number" id="leave-days" class="filter-input" style="width: 100%;" value="1" min="1" max="60" required />
          </div>
          <div>
            <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Reason / Notes *</label>
            <textarea id="leave-reason" class="filter-input" style="width: 100%; height: 60px; resize: vertical;" placeholder="Reason for time off..." required></textarea>
          </div>
          <div style="display: flex; justify-content: flex-end; gap: 8px; margin-top: 10px;">
            <button type="button" class="btn btn-secondary btn-sm" onclick="document.getElementById('modal-backdrop').classList.remove('open')">Cancel</button>
            <button type="submit" class="btn btn-primary btn-sm">Submit Request</button>
          </div>
        </form>
      </div>
    `;

    modalBackdrop.classList.add('open');

    document.getElementById('req-leave-form').onsubmit = async (e) => {
      e.preventDefault();
      try {
        const body = {
          leave_type: document.getElementById('leave-type-select').value,
          start_date: document.getElementById('leave-start').value,
          end_date: document.getElementById('leave-end').value,
          days: parseInt(document.getElementById('leave-days').value),
          reason: document.getElementById('leave-reason').value.trim()
        };
        await API.post('/api/v1/leave/requests', body);
        API.showToast('Leave request submitted for review', 'success');
        modalBackdrop.classList.remove('open');
        App.views.leave();
      } catch {}
    };
  },

  async reviewLeave(reqId, status) {
    const comment = prompt(`Provide a review note for ${status.toUpperCase()} decision (optional):`) || '';
    try {
      await API.put(`/api/v1/leave/requests/${reqId}/review`, {
        status,
        review_comment: comment
      });
      API.showToast(`Leave request ${status}`, 'success');
      App.views.leave();
    } catch {}
  },

  async deleteEmployee(empId, empName) {
    if (!confirm(`Are you sure you want to delete employee '${empName}' (ID: ${empId})? This action will be recorded in the audit trail.`)) return;
    try {
      await API.delete(`/api/v1/employees/${empId}`);
      API.showToast(`Employee '${empName}' removed`, 'info');
      App.views.employees();
    } catch {}
  },

  async deleteDepartment(deptId, deptName) {
    if (!confirm(`Are you sure you want to delete department '${deptName}'? Crew members will become unassigned.`)) return;
    try {
      await API.delete(`/api/v1/departments/${deptId}`);
      API.showToast(`Department '${deptName}' removed`, 'info');
      App.views.departments();
    } catch {}
  }
};

document.addEventListener('DOMContentLoaded', () => App.init());
