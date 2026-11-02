// CREW/DECK Auth Manager
const Auth = {
  user: null,

  async checkSession() {
    try {
      const res = await API.get('/api/v1/auth/me');
      if (res && res.user) {
        this.user = res.user;
        this.renderUserBadge();
        return this.user;
      }
    } catch {
      this.user = null;
    }
    return null;
  },

  async login(email, password) {
    const res = await API.post('/api/v1/auth/login', { email, password });
    if (res && res.user) {
      this.user = res.user;
      return res.user;
    }
    throw new Error('Login failed');
  },

  async logout() {
    try {
      await API.post('/api/v1/auth/logout', {});
    } finally {
      this.user = null;
      window.location.href = '/login.html';
    }
  },

  renderUserBadge() {
    const badge = document.getElementById('user-badge');
    if (!badge || !this.user) return;

    const initials = (this.user.name || 'User')
      .split(' ')
      .map(p => p[0])
      .slice(0, 2)
      .join('');

    badge.innerHTML = `
      <div class="avatar-circle" style="background-color: ${this.user.avatar_color || '#FFB020'}">
        ${initials}
      </div>
      <span style="font-weight: 600; font-size: 12px; color: var(--text-primary);">${this.user.name || this.user.email}</span>
      <span class="user-role-badge">${this.user.role}</span>
    `;

    badge.onclick = () => {
      if (confirm(`Logged in as ${this.user.email} (${this.user.role}). Sign out?`)) {
        this.logout();
      }
    };
  }
};
