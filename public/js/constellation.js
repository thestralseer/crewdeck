// CREW/DECK Org Constellation Interactive Chart
const OrgConstellation = {
  canvas: null,
  ctx: null,
  nodes: [],
  links: [],
  scale: 1,
  panX: 0,
  panY: 0,
  isDragging: false,
  dragStartX: 0,
  dragStartY: 0,
  hoveredNode: null,
  animationId: null,
  searchQuery: '',

  async init(containerEl) {
    if (!containerEl) return;

    containerEl.innerHTML = `
      <div class="constellation-wrapper">
        <canvas id="constellation-canvas"></canvas>
        <div style="position: absolute; top: 16px; left: 16px; z-index: 10; display: flex; gap: 8px;">
          <input type="text" id="constellation-search" class="filter-input" placeholder="Search node in constellation..." style="width: 220px; background: rgba(22, 27, 34, 0.85); backdrop-filter: blur(4px);" />
          <button id="constellation-reset" class="btn btn-secondary btn-sm" style="background: rgba(22, 27, 34, 0.85);">Center View</button>
        </div>
        <div class="constellation-controls">
          <button id="ctrl-zoom-in" class="ctrl-btn" title="Zoom In">+</button>
          <button id="ctrl-zoom-out" class="ctrl-btn" title="Zoom Out">−</button>
          <button id="ctrl-zoom-reset" class="ctrl-btn" title="Reset View">⊙</button>
        </div>
        <div id="constellation-tooltip" style="position: absolute; display: none; pointer-events: none; background: rgba(22, 27, 34, 0.95); border: 1px solid var(--border-medium); border-radius: var(--radius-md); padding: 8px 12px; box-shadow: var(--shadow-lg); z-index: 20;"></div>
      </div>
    `;

    this.canvas = document.getElementById('constellation-canvas');
    this.ctx = this.canvas.getContext('2d');

    this.resizeCanvas();
    window.addEventListener('resize', () => this.resizeCanvas());

    this.setupEvents();

    try {
      const data = await API.get('/api/v1/org/constellation');
      if (data) {
        this.nodes = data.nodes || [];
        this.links = data.links || [];
        this.layoutNodes();
        this.centerView();
        this.startRenderLoop();
      }
    } catch (err) {
      console.error('Failed to load constellation data', err);
    }

    const searchInput = document.getElementById('constellation-search');
    searchInput.addEventListener('input', (e) => {
      this.searchQuery = e.target.value.toLowerCase().trim();
    });

    document.getElementById('constellation-reset').addEventListener('click', () => this.centerView());
    document.getElementById('ctrl-zoom-in').addEventListener('click', () => this.zoom(1.2));
    document.getElementById('ctrl-zoom-out').addEventListener('click', () => this.zoom(0.8));
    document.getElementById('ctrl-zoom-reset').addEventListener('click', () => this.centerView());
  },

  resizeCanvas() {
    if (!this.canvas) return;
    const rect = this.canvas.parentElement.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    this.canvas.width = rect.width * dpr;
    this.canvas.height = rect.height * dpr;
    this.ctx.scale(dpr, dpr);
    this.canvasWidth = rect.width;
    this.canvasHeight = rect.height;
  },

  layoutNodes() {
    const deptCenters = {
      1: { x: -320, y: -160, color: '#FFB020', label: 'Engineering' },
      2: { x: 320, y: -160, color: '#2DD4BF', label: 'Product & Design' },
      3: { x: -320, y: 220, color: '#EC4899', label: 'People Operations' },
      4: { x: 320, y: 220, color: '#3B82F6', label: 'Sales & Marketing' },
      5: { x: 0, y: 380, color: '#F59E0B', label: 'Finance & Operations' }
    };

    this.deptCenters = deptCenters;

    // Group nodes by department
    const deptBuckets = {};
    this.nodes.forEach(n => {
      const d = n.department_id || 1;
      if (!deptBuckets[d]) deptBuckets[d] = [];
      deptBuckets[d].push(n);
    });

    // Arrange nodes in spiral around department centers
    Object.keys(deptBuckets).forEach(dId => {
      const group = deptBuckets[dId];
      const center = deptCenters[dId] || { x: 0, y: 0 };

      group.forEach((node, idx) => {
        if (idx === 0) {
          // Department Head at center
          node.x = center.x;
          node.y = center.y;
          node.radius = 16;
        } else {
          const angle = (idx * 0.85);
          const dist = 55 + (Math.sqrt(idx) * 28);
          node.x = center.x + Math.cos(angle) * dist;
          node.y = center.y + Math.sin(angle) * dist;
          node.radius = 11;
        }
      });
    });
  },

  centerView() {
    this.scale = 0.9;
    this.panX = this.canvasWidth / 2;
    this.panY = this.canvasHeight / 2 - 40;
  },

  zoom(factor) {
    this.scale *= factor;
    this.scale = Math.max(0.3, Math.min(this.scale, 2.5));
  },

  setupEvents() {
    const c = this.canvas;

    c.addEventListener('mousedown', (e) => {
      this.isDragging = true;
      this.dragStartX = e.clientX - this.panX;
      this.dragStartY = e.clientY - this.panY;
    });

    window.addEventListener('mousemove', (e) => {
      if (this.isDragging) {
        this.panX = e.clientX - this.dragStartX;
        this.panY = e.clientY - this.dragStartY;
      } else {
        this.checkHover(e);
      }
    });

    window.addEventListener('mouseup', () => {
      this.isDragging = false;
    });

    c.addEventListener('wheel', (e) => {
      e.preventDefault();
      const zoomFactor = e.deltaY < 0 ? 1.08 : 0.92;
      this.zoom(zoomFactor);
    });

    c.addEventListener('click', (e) => {
      const node = this.getNodeAtScreen(e.clientX, e.clientY);
      if (node) {
        Drawer.open(node.id);
      }
    });
  },

  getNodeAtScreen(clientX, clientY) {
    const rect = this.canvas.getBoundingClientRect();
    const mouseX = clientX - rect.left;
    const mouseY = clientY - rect.top;

    // Convert to world coordinates
    const worldX = (mouseX - this.panX) / this.scale;
    const worldY = (mouseY - this.panY) / this.scale;

    for (let i = this.nodes.length - 1; i >= 0; i--) {
      const node = this.nodes[i];
      const dx = worldX - node.x;
      const dy = worldY - node.y;
      if (dx * dx + dy * dy <= (node.radius + 4) * (node.radius + 4)) {
        return node;
      }
    }
    return null;
  },

  checkHover(e) {
    const node = this.getNodeAtScreen(e.clientX, e.clientY);
    const tooltip = document.getElementById('constellation-tooltip');
    const c = this.canvas;

    if (node) {
      c.style.cursor = 'pointer';
      this.hoveredNode = node;
      if (tooltip) {
        const rect = c.getBoundingClientRect();
        tooltip.style.display = 'block';
        tooltip.style.left = `${e.clientX - rect.left + 15}px`;
        tooltip.style.top = `${e.clientY - rect.top - 10}px`;
        tooltip.innerHTML = `
          <div style="font-weight: 700; font-size: 13px; color: var(--text-primary);">${node.name}</div>
          <div style="font-size: 11px; color: ${node.avatar_color || '#FFB020'}; font-weight: 500;">${node.role}</div>
          <div style="font-size: 10px; color: var(--text-secondary); text-transform: uppercase; margin-top: 2px;">${node.department_name}</div>
        `;
      }
    } else {
      c.style.cursor = this.isDragging ? 'grabbing' : 'grab';
      this.hoveredNode = null;
      if (tooltip) tooltip.style.display = 'none';
    }
  },

  startRenderLoop() {
    const render = () => {
      this.draw();
      this.animationId = requestAnimationFrame(render);
    };
    if (this.animationId) cancelAnimationFrame(this.animationId);
    this.animationId = requestAnimationFrame(render);
  },

  draw() {
    const ctx = this.ctx;
    ctx.clearRect(0, 0, this.canvasWidth, this.canvasHeight);

    ctx.save();
    ctx.translate(this.panX, this.panY);
    ctx.scale(this.scale, this.scale);

    // Draw Department Halos
    if (this.deptCenters) {
      Object.keys(this.deptCenters).forEach(dId => {
        const c = this.deptCenters[dId];
        const grad = ctx.createRadialGradient(c.x, c.y, 10, c.x, c.y, 160);
        grad.addColorStop(0, `${c.color}22`);
        grad.addColorStop(1, 'transparent');
        ctx.fillStyle = grad;
        ctx.beginPath();
        ctx.arc(c.x, c.y, 160, 0, Math.PI * 2);
        ctx.fill();

        // Department Label
        ctx.fillStyle = `${c.color}99`;
        ctx.font = '600 11px "Space Grotesk", sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText(c.label.toUpperCase(), c.x, c.y - 120);
      });
    }

    // Node Lookup Map
    const nodeMap = {};
    this.nodes.forEach(n => nodeMap[n.id] = n);

    // Draw Links
    ctx.lineWidth = 1;
    this.links.forEach(l => {
      const src = nodeMap[l.source];
      const tgt = nodeMap[l.target];
      if (src && tgt) {
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.12)';
        ctx.beginPath();
        ctx.moveTo(src.x, src.y);
        ctx.lineTo(tgt.x, tgt.y);
        ctx.stroke();
      }
    });

    // Draw Nodes
    const q = this.searchQuery;
    this.nodes.forEach(n => {
      const isMatched = !q || n.name.toLowerCase().includes(q) || n.role.toLowerCase().includes(q);
      const isHovered = (this.hoveredNode && this.hoveredNode.id === n.id);

      ctx.save();
      ctx.globalAlpha = isMatched ? 1.0 : 0.25;

      // Glow halo on hovered or matched node
      if (isHovered || (q && isMatched)) {
        ctx.shadowColor = n.avatar_color || '#FFB020';
        ctx.shadowBlur = 15;
      }

      ctx.beginPath();
      ctx.arc(n.x, n.y, n.radius, 0, Math.PI * 2);
      ctx.fillStyle = n.avatar_color || '#FFB020';
      ctx.fill();

      // Border ring
      ctx.lineWidth = isHovered ? 2.5 : 1.5;
      ctx.strokeStyle = '#0E1116';
      ctx.stroke();

      // Label under node
      ctx.font = `${n.radius > 12 ? '600 11px' : '500 10px'} "Inter", sans-serif`;
      ctx.fillStyle = isHovered ? '#FFFFFF' : '#C9D1D9';
      ctx.textAlign = 'center';
      ctx.shadowBlur = 0;
      ctx.fillText(n.name, n.x, n.y + n.radius + 14);

      ctx.restore();
    });

    ctx.restore();
  }
};
