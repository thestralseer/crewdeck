// CREW/DECK Slide-over Profile Drawer Manager
const Drawer = {
  currentEmployee: null,
  previousSnapshot: null,

  init() {
    let backdrop = document.getElementById('drawer-backdrop');
    if (!backdrop) {
      backdrop = document.createElement('div');
      backdrop.id = 'drawer-backdrop';
      backdrop.className = 'drawer-backdrop';
      backdrop.innerHTML = `
        <div id="slide-drawer" class="slide-drawer" role="dialog" aria-modal="true">
          <div class="drawer-header">
            <div id="drawer-title-area" style="display: flex; align-items: center; gap: 12px;"></div>
            <button id="drawer-close-btn" class="ctrl-btn" aria-label="Close Drawer" style="font-size: 20px;">✕</button>
          </div>
          <div id="drawer-body" class="drawer-body"></div>
          <div id="drawer-footer" class="drawer-footer"></div>
        </div>
      `;
      document.body.appendChild(backdrop);

      backdrop.addEventListener('click', (e) => {
        if (e.target === backdrop) Drawer.close();
      });

      document.getElementById('drawer-close-btn').addEventListener('click', () => Drawer.close());

      window.addEventListener('keydown', (e) => {
        if (e.key === 'Escape' && backdrop.classList.contains('open')) {
          Drawer.close();
        }
      });
    }
  },

  async open(employeeId) {
    this.init();
    const backdrop = document.getElementById('drawer-backdrop');
    const drawer = document.getElementById('slide-drawer');
    const body = document.getElementById('drawer-body');
    const titleArea = document.getElementById('drawer-title-area');
    const footer = document.getElementById('drawer-footer');

    backdrop.classList.add('open');
    drawer.classList.add('open');

    // Skeleton loader while fetching
    titleArea.innerHTML = `<span style="font-weight: 600; color: var(--text-secondary);">Loading employee profile...</span>`;
    body.innerHTML = `
      <div style="display: flex; gap: 16px; align-items: center; padding: 20px 0;">
        <div style="width: 60px; height: 60px; border-radius: 50%; background: var(--bg-surface-elevated);"></div>
        <div style="display: flex; flex-direction: column; gap: 8px; flex: 1;">
          <div style="height: 18px; width: 60%; background: var(--bg-surface-elevated); border-radius: 4px;"></div>
          <div style="height: 12px; width: 40%; background: var(--bg-surface-elevated); border-radius: 4px;"></div>
        </div>
      </div>
    `;
    footer.innerHTML = '';

    try {
      const emp = await API.get(`/api/v1/employees/${employeeId}`);
      if (!emp) return;
      this.currentEmployee = emp;
      this.renderViewMode();
    } catch (err) {
      body.innerHTML = `<div style="color: var(--accent-rose); padding: 20px;">Failed to load employee details.</div>`;
    }
  },

  close() {
    const backdrop = document.getElementById('drawer-backdrop');
    const drawer = document.getElementById('slide-drawer');
    if (backdrop && drawer) {
      backdrop.classList.remove('open');
      drawer.classList.remove('open');
    }
  },

  renderViewMode() {
    const emp = this.currentEmployee;
    if (!emp) return;

    const titleArea = document.getElementById('drawer-title-area');
    const body = document.getElementById('drawer-body');
    const footer = document.getElementById('drawer-footer');

    const initials = (emp.name || 'Emp').split(' ').map(p => p[0]).slice(0, 2).join('');
    const formattedSalary = '₹' + Number(emp.salary || 0).toLocaleString('en-IN');
    const monthlySalary = '₹' + Math.round(Number(emp.salary || 0) / 12).toLocaleString('en-IN');

    titleArea.innerHTML = `
      <div class="avatar-circle" style="width: 38px; height: 38px; font-size: 14px; background-color: ${emp.avatar_color || '#FFB020'};">
        ${initials}
      </div>
      <div>
        <h3 style="font-family: var(--font-display); font-size: 16px; font-weight: 700;">${emp.name}</h3>
        <span style="font-size: 12px; color: var(--text-secondary); font-family: var(--font-mono);">EMP-${String(emp.id).padStart(4, '0')}</span>
      </div>
    `;

    const bal = emp.leave_balance || { annual_total: 18, annual_used: 0, sick_total: 12, sick_used: 0, casual_total: 10, casual_used: 0 };
    const recentAtt = emp.recent_attendance || [];

    body.innerHTML = `
      <!-- Core Info Card -->
      <div style="background-color: var(--bg-surface-elevated); border: 1px solid var(--border-subtle); border-radius: var(--radius-md); padding: 16px; display: flex; flex-direction: column; gap: 12px;">
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <span style="font-size: 12px; color: var(--text-muted); text-transform: uppercase; font-weight: 600;">Status</span>
          <span class="status-badge status-${emp.status}">${emp.status}</span>
        </div>
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <span style="font-size: 12px; color: var(--text-muted); text-transform: uppercase; font-weight: 600;">Role / Title</span>
          <span style="font-weight: 600; color: var(--text-primary);">${emp.role_title}</span>
        </div>
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <span style="font-size: 12px; color: var(--text-muted); text-transform: uppercase; font-weight: 600;">Department</span>
          <span style="font-weight: 500; color: var(--accent-amber);">${emp.department_name || 'Unassigned'}</span>
        </div>
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <span style="font-size: 12px; color: var(--text-muted); text-transform: uppercase; font-weight: 600;">Reporting Manager</span>
          <span style="color: var(--text-secondary);">${emp.manager_name || 'None (Direct Head)'}</span>
        </div>
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <span style="font-size: 12px; color: var(--text-muted); text-transform: uppercase; font-weight: 600;">Join Date</span>
          <span class="font-mono" style="color: var(--text-primary);">${emp.join_date}</span>
        </div>
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <span style="font-size: 12px; color: var(--text-muted); text-transform: uppercase; font-weight: 600;">Compensation</span>
          <div style="text-align: right;">
            <div class="font-mono" style="font-weight: 700; color: var(--accent-teal);">${formattedSalary}/yr</div>
            <div style="font-size: 11px; color: var(--text-muted);">${monthlySalary}/mo</div>
          </div>
        </div>
      </div>

      <!-- Contact Info -->
      <div style="display: flex; flex-direction: column; gap: 8px;">
        <span style="font-size: 12px; font-weight: 600; text-transform: uppercase; color: var(--text-secondary);">Contact Details</span>
        <div style="display: flex; align-items: center; gap: 10px; font-size: 13px; color: var(--text-primary);">
          <span>✉️</span> <span>${emp.email}</span>
        </div>
        <div style="display: flex; align-items: center; gap: 10px; font-size: 13px; color: var(--text-primary);">
          <span>📞</span> <span class="font-mono">${emp.phone}</span>
        </div>
      </div>

      <!-- 2026 Leave Balance Widget -->
      <div style="display: flex; flex-direction: column; gap: 10px;">
        <span style="font-size: 12px; font-weight: 600; text-transform: uppercase; color: var(--text-secondary);">2026 Leave Balances</span>
        <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px;">
          <div style="background-color: var(--bg-surface-elevated); border: 1px solid var(--border-subtle); border-radius: var(--radius-sm); padding: 10px; text-align: center;">
            <div style="font-size: 11px; color: var(--text-muted); margin-bottom: 2px;">Annual</div>
            <div class="font-mono" style="font-size: 18px; font-weight: 700; color: var(--text-primary);">
              ${bal.annual_total - bal.annual_used}<span style="font-size: 11px; color: var(--text-muted);">/${bal.annual_total}</span>
            </div>
          </div>
          <div style="background-color: var(--bg-surface-elevated); border: 1px solid var(--border-subtle); border-radius: var(--radius-sm); padding: 10px; text-align: center;">
            <div style="font-size: 11px; color: var(--text-muted); margin-bottom: 2px;">Sick</div>
            <div class="font-mono" style="font-size: 18px; font-weight: 700; color: var(--accent-teal);">
              ${bal.sick_total - bal.sick_used}<span style="font-size: 11px; color: var(--text-muted);">/${bal.sick_total}</span>
            </div>
          </div>
          <div style="background-color: var(--bg-surface-elevated); border: 1px solid var(--border-subtle); border-radius: var(--radius-sm); padding: 10px; text-align: center;">
            <div style="font-size: 11px; color: var(--text-muted); margin-bottom: 2px;">Casual</div>
            <div class="font-mono" style="font-size: 18px; font-weight: 700; color: var(--accent-amber);">
              ${bal.casual_total - bal.casual_used}<span style="font-size: 11px; color: var(--text-muted);">/${bal.casual_total}</span>
            </div>
          </div>
        </div>
      </div>

      <!-- Recent Attendance Snapshot -->
      <div style="display: flex; flex-direction: column; gap: 8px;">
        <span style="font-size: 12px; font-weight: 600; text-transform: uppercase; color: var(--text-secondary);">Recent Activity Roster</span>
        <div style="background-color: var(--bg-surface-elevated); border: 1px solid var(--border-subtle); border-radius: var(--radius-md); overflow: hidden;">
          ${recentAtt.length === 0 ? '<div style="padding: 12px; color: var(--text-muted); font-size: 12px;">No recent attendance recorded</div>' :
            recentAtt.map(r => `
              <div style="display: flex; justify-content: space-between; align-items: center; padding: 8px 12px; border-bottom: 1px solid var(--border-subtle); font-size: 12px;">
                <span class="font-mono" style="color: var(--text-secondary);">${r.date}</span>
                <span style="color: var(--text-primary); font-family: var(--font-mono);">${r.check_in} - ${r.check_out}</span>
                <span class="status-badge status-${r.status}" style="font-size: 10px;">${r.status}</span>
              </div>
            `).join('')
          }
        </div>
      </div>
    `;

    footer.innerHTML = `
      <button id="drawer-view-heatmap-btn" class="btn btn-secondary btn-sm">View 365d Heatmap</button>
      <button id="drawer-edit-btn" class="btn btn-primary btn-sm">Edit Details</button>
    `;

    document.getElementById('drawer-edit-btn').onclick = () => this.renderEditMode();
    document.getElementById('drawer-view-heatmap-btn').onclick = () => {
      Drawer.close();
      App.navigate('attendance', { employeeId: emp.id });
    };
  },

  async renderEditMode() {
    const emp = this.currentEmployee;
    if (!emp) return;

    const body = document.getElementById('drawer-body');
    const footer = document.getElementById('drawer-footer');

    // Fetch department options
    let depts = [];
    try {
      const res = await API.get('/api/v1/departments');
      if (res && res.departments) depts = res.departments;
    } catch {}

    body.innerHTML = `
      <form id="drawer-edit-form" style="display: flex; flex-direction: column; gap: 14px;">
        <div>
          <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Full Name</label>
          <input type="text" id="edit-name" class="filter-input" style="width: 100%;" value="${emp.name}" required />
        </div>
        <div>
          <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Email</label>
          <input type="email" id="edit-email" class="filter-input" style="width: 100%;" value="${emp.email}" required />
        </div>
        <div>
          <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Phone</label>
          <input type="text" id="edit-phone" class="filter-input" style="width: 100%;" value="${emp.phone}" required />
        </div>
        <div>
          <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Role / Title</label>
          <input type="text" id="edit-role" class="filter-input" style="width: 100%;" value="${emp.role_title}" required />
        </div>
        <div>
          <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Department</label>
          <select id="edit-dept" class="filter-select" style="width: 100%;">
            <option value="">Unassigned</option>
            ${depts.map(d => `<option value="${d.id}" ${d.id === emp.department_id ? 'selected' : ''}>${d.name}</option>`).join('')}
          </select>
        </div>
        <div>
          <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Annual Salary (INR)</label>
          <input type="number" id="edit-salary" class="filter-input" style="width: 100%;" value="${emp.salary}" step="10000" min="0" required />
        </div>
        <div>
          <label style="font-size: 11px; text-transform: uppercase; font-weight: 600; color: var(--text-secondary); display: block; margin-bottom: 4px;">Status</label>
          <select id="edit-status" class="filter-select" style="width: 100%;">
            <option value="active" ${emp.status === 'active' ? 'selected' : ''}>Active</option>
            <option value="on-leave" ${emp.status === 'on-leave' ? 'selected' : ''}>On-Leave</option>
            <option value="exited" ${emp.status === 'exited' ? 'selected' : ''}>Exited</option>
          </select>
        </div>
      </form>
    `;

    footer.innerHTML = `
      <button id="drawer-cancel-btn" class="btn btn-secondary btn-sm">Cancel</button>
      <button id="drawer-save-btn" class="btn btn-primary btn-sm">Save Changes</button>
    `;

    document.getElementById('drawer-cancel-btn').onclick = () => this.renderViewMode();
    document.getElementById('drawer-save-btn').onclick = () => this.saveChanges();
  },

  async saveChanges() {
    const emp = this.currentEmployee;
    if (!emp) return;

    const previousSnapshot = { ...emp };

    const updatedData = {
      name: document.getElementById('edit-name').value.trim(),
      email: document.getElementById('edit-email').value.trim(),
      phone: document.getElementById('edit-phone').value.trim(),
      role_title: document.getElementById('edit-role').value.trim(),
      department_id: parseInt(document.getElementById('edit-dept').value) || null,
      salary: parseFloat(document.getElementById('edit-salary').value) || 0,
      status: document.getElementById('edit-status').value,
    };

    // Optimistic local update
    Object.assign(this.currentEmployee, updatedData);
    this.renderViewMode();

    try {
      await API.put(`/api/v1/employees/${emp.id}`, updatedData);
      
      // Toast with undo capability
      API.showToast(`Updated profile for ${updatedData.name}`, 'success', async () => {
        // Undo callback
        try {
          await API.put(`/api/v1/employees/${emp.id}`, previousSnapshot);
          Object.assign(this.currentEmployee, previousSnapshot);
          this.renderViewMode();
          API.showToast(`Reverted changes for ${previousSnapshot.name}`, 'info');
          if (App.currentView === 'employees') App.views.employees();
        } catch {
          API.showToast('Failed to revert changes', 'error');
        }
      });

      if (App.currentView === 'employees') App.views.employees();
    } catch (err) {
      // Rollback on error
      this.currentEmployee = previousSnapshot;
      this.renderViewMode();
    }
  }
};
