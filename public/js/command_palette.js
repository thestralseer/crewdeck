// CREW/DECK Command Palette (Ctrl+K / Cmd+K)
const CommandPalette = {
  isOpen: false,
  selectedIndex: 0,
  items: [],
  debounceTimer: null,

  init() {
    let backdrop = document.getElementById('cmd-backdrop');
    if (!backdrop) {
      backdrop = document.createElement('div');
      backdrop.id = 'cmd-backdrop';
      backdrop.className = 'cmd-backdrop';
      backdrop.innerHTML = `
        <div class="cmd-modal" role="dialog" aria-modal="true">
          <input type="text" id="cmd-input" class="cmd-search-input" placeholder="Type a command, page, or search employee name..." autofocus />
          <div id="cmd-list" class="cmd-list"></div>
          <div class="cmd-footer">
            <span>Navigation: <kbd class="kbd">↑</kbd> <kbd class="kbd">↓</kbd> &nbsp; Select: <kbd class="kbd">↵</kbd> &nbsp; Close: <kbd class="kbd">esc</kbd></span>
            <span style="font-family: var(--font-mono); color: var(--accent-amber);">CREW/DECK Command</span>
          </div>
        </div>
      `;
      document.body.appendChild(backdrop);

      backdrop.addEventListener('click', (e) => {
        if (e.target === backdrop) CommandPalette.close();
      });

      const input = document.getElementById('cmd-input');
      input.addEventListener('input', () => {
        clearTimeout(CommandPalette.debounceTimer);
        CommandPalette.debounceTimer = setTimeout(() => CommandPalette.handleInput(input.value), 120);
      });

      input.addEventListener('keydown', (e) => CommandPalette.handleKeydown(e));

      window.addEventListener('keydown', (e) => {
        if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') {
          e.preventDefault();
          CommandPalette.toggle();
        } else if (e.key === 'Escape' && CommandPalette.isOpen) {
          CommandPalette.close();
        }
      });
    }
  },

  toggle() {
    if (this.isOpen) this.close();
    else this.open();
  },

  open() {
    this.init();
    const backdrop = document.getElementById('cmd-backdrop');
    const input = document.getElementById('cmd-input');
    backdrop.classList.add('open');
    this.isOpen = true;
    input.value = '';
    this.selectedIndex = 0;
    this.renderDefaultActions();
    setTimeout(() => input.focus(), 50);
  },

  close() {
    const backdrop = document.getElementById('cmd-backdrop');
    if (backdrop) backdrop.classList.remove('open');
    this.isOpen = false;
  },

  getDefaultActions() {
    return [
      { type: 'NAV', title: 'Dashboard', subtitle: 'View system metrics and activity', action: () => App.navigate('dashboard') },
      { type: 'NAV', title: 'Employees', subtitle: 'Full employee roster and profiles', action: () => App.navigate('employees') },
      { type: 'NAV', title: 'Org Constellation', subtitle: 'Interactive organizational network graph', action: () => App.navigate('org') },
      { type: 'NAV', title: 'Departments', subtitle: 'Headcount and department structures', action: () => App.navigate('departments') },
      { type: 'NAV', title: 'Attendance', subtitle: 'Check-in roster and calendar heatmap', action: () => App.navigate('attendance') },
      { type: 'NAV', title: 'Leave Management', subtitle: 'Requests review and balance tracking', action: () => App.navigate('leave') },
      { type: 'NAV', title: 'Payroll Summary', subtitle: 'Department compensation and CSV export', action: () => App.navigate('payroll') },
      { type: 'NAV', title: 'Audit Log', subtitle: 'System event logs and change history', action: () => App.navigate('audit') },
      { type: 'ACTION', title: 'Check In Today', subtitle: 'Mark attendance check-in for current time', action: () => App.checkInToday() },
      { type: 'ACTION', title: 'Check Out Today', subtitle: 'Mark attendance check-out for current time', action: () => App.checkOutToday() },
      { type: 'ACTION', title: 'Export Payroll CSV', subtitle: 'Download complete salary roster as CSV', action: () => App.exportPayroll() },
      { type: 'ACTION', title: 'Toggle Light / Dark Theme', subtitle: 'Switch color theme', action: () => App.toggleTheme() },
      { type: 'ACTION', title: 'Sign Out', subtitle: 'Terminate active session', action: () => Auth.logout() },
    ];
  },

  renderDefaultActions() {
    this.items = this.getDefaultActions();
    this.renderItems();
  },

  async handleInput(query) {
    const q = query.trim().toLowerCase();
    if (!q) {
      this.renderDefaultActions();
      return;
    }

    const defaultMatches = this.getDefaultActions().filter(item =>
      item.title.toLowerCase().includes(q) || item.subtitle.toLowerCase().includes(q)
    );

    // Search employees via API
    let empMatches = [];
    try {
      const res = await API.get('/api/v1/employees', { search: q, limit: 5 });
      if (res && res.items) {
        empMatches = res.items.map(emp => ({
          type: 'EMPLOYEE',
          title: emp.name,
          subtitle: `${emp.role_title} • ${emp.department_name || 'Unassigned'}`,
          action: () => Drawer.open(emp.id)
        }));
      }
    } catch {}

    this.items = [...empMatches, ...defaultMatches];
    this.selectedIndex = 0;
    this.renderItems();
  },

  renderItems() {
    const list = document.getElementById('cmd-list');
    if (!list) return;

    if (this.items.length === 0) {
      list.innerHTML = `<div style="padding: 24px; text-align: center; color: var(--text-muted); font-size: 13px;">No matching actions or employees found</div>`;
      return;
    }

    list.innerHTML = this.items.map((item, idx) => {
      let icon = '⚡';
      if (item.type === 'NAV') icon = '🧭';
      else if (item.type === 'EMPLOYEE') icon = '👤';

      const isSelected = idx === this.selectedIndex;
      return `
        <div class="cmd-item ${isSelected ? 'selected' : ''}" data-index="${idx}">
          <div class="cmd-item-left">
            <span style="font-size: 14px;">${icon}</span>
            <div>
              <div style="font-weight: 600; color: var(--text-primary);">${item.title}</div>
              <div style="font-size: 11px; color: var(--text-secondary);">${item.subtitle}</div>
            </div>
          </div>
          <span class="cmd-badge">${item.type}</span>
        </div>
      `;
    }).join('');

    list.querySelectorAll('.cmd-item').forEach(el => {
      el.addEventListener('click', () => {
        const idx = parseInt(el.getAttribute('data-index'));
        CommandPalette.executeItem(idx);
      });
    });
  },

  handleKeydown(e) {
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      this.selectedIndex = (this.selectedIndex + 1) % this.items.length;
      this.renderItems();
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      this.selectedIndex = (this.selectedIndex - 1 + this.items.length) % this.items.length;
      this.renderItems();
    } else if (e.key === 'Enter') {
      e.preventDefault();
      this.executeItem(this.selectedIndex);
    }
  },

  executeItem(index) {
    const item = this.items[index];
    if (item && item.action) {
      this.close();
      item.action();
    }
  }
};
