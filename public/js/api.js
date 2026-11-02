// CREW/DECK API Client
const API = {
  baseUrl: '/api/v1',

  async request(endpoint, options = {}) {
    const url = endpoint.startsWith('/') ? endpoint : `${this.baseUrl}/${endpoint}`;
    const defaultHeaders = {
      'Accept': 'application/json',
    };

    if (options.body && typeof options.body === 'object' && !(options.body instanceof FormData)) {
      defaultHeaders['Content-Type'] = 'application/json';
      options.body = JSON.stringify(options.body);
    }

    const config = {
      credentials: 'same-origin',
      headers: { ...defaultHeaders, ...options.headers },
      ...options,
    };

    try {
      const response = await fetch(url, config);

      // Handle CSV file downloads
      const contentType = response.headers.get('content-type') || '';
      if (contentType.includes('text/csv')) {
        return await response.blob();
      }

      const data = await response.json().catch(() => ({}));

      if (!response.ok) {
        if (response.status === 401 && !endpoint.includes('auth/login') && !endpoint.includes('auth/me')) {
          window.location.href = '/login.html';
          return null;
        }
        const errMessage = data?.error?.message || `Request failed with status ${response.status}`;
        throw new Error(errMessage);
      }

      return data;
    } catch (err) {
      console.error(`[API Error] ${endpoint}:`, err);
      API.showToast(err.message || 'Network error occurred', 'error');
      throw err;
    }
  },

  get(endpoint, params = {}) {
    const query = new URLSearchParams(params).toString();
    const url = query ? `${endpoint}?${query}` : endpoint;
    return this.request(url, { method: 'GET' });
  },

  post(endpoint, body = {}) {
    return this.request(endpoint, { method: 'POST', body });
  },

  put(endpoint, body = {}) {
    return this.request(endpoint, { method: 'PUT', body });
  },

  delete(endpoint) {
    return this.request(endpoint, { method: 'DELETE' });
  },

  showToast(message, type = 'info', onUndo = null) {
    let container = document.getElementById('toast-container');
    if (!container) {
      container = document.createElement('div');
      container.id = 'toast-container';
      container.className = 'toast-container';
      document.body.appendChild(container);
    }

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    
    let icon = 'ℹ️';
    if (type === 'success') icon = '✓';
    else if (type === 'error') icon = '⚠️';

    toast.innerHTML = `
      <span style="font-size: 14px; font-weight: bold;">${icon}</span>
      <span style="flex: 1; font-size: 13px;">${message}</span>
      ${onUndo ? `<button class="toast-btn-undo">UNDO</button>` : ''}
    `;

    if (onUndo) {
      toast.querySelector('.toast-btn-undo').addEventListener('click', () => {
        onUndo();
        toast.remove();
      });
    }

    container.appendChild(toast);
    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateY(10px)';
      toast.style.transition = 'all 200ms ease';
      setTimeout(() => toast.remove(), 200);
    }, 4000);
  }
};
