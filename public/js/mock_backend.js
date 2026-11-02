// CREW/DECK Zero-Latency Client-Side & Serverless Data Engine
// Mirrors C11 backend with all 40 employees, org tree, attendance, leave & payroll.

(function() {
  const STORAGE_KEY = 'crewdeck_data_v1';
  const SESSION_KEY = 'crewdeck_session_user';

  const INITIAL_DEPTS = [
    { id: 1, name: "Engineering", description: "Core platform architecture, backend systems, web apps and infrastructure" },
    { id: 2, name: "Product & Design", description: "Product management, user experience research, UI system and design operations" },
    { id: 3, name: "People Operations", description: "Talent acquisition, organizational development, HR and culture" },
    { id: 4, name: "Sales & Marketing", description: "Enterprise client acquisition, partner relations, growth and brand strategy" },
    { id: 5, name: "Finance & Operations", description: "Financial planning, accounting, compliance, legal and workplace operations" }
  ];

  const INITIAL_EMPS = [
    { id: 1, name: "Rajesh Verma", email: "rajesh.verma@crewdeck.io", phone: "+91 98201 11001", role_title: "VP of Engineering", department_id: 1, manager_id: null, salary: 3600000.0, join_date: "2021-03-15", status: "active", avatar_color: "#FFB020" },
    { id: 2, name: "Priya Patel", email: "priya.patel@crewdeck.io", phone: "+91 98202 22002", role_title: "Director of Product", department_id: 2, manager_id: null, salary: 3400000.0, join_date: "2021-06-01", status: "active", avatar_color: "#2DD4BF" },
    { id: 3, name: "Ananya Iyer", email: "ananya.iyer@crewdeck.io", phone: "+91 98203 33003", role_title: "Head of People Operations", department_id: 3, manager_id: null, salary: 2900000.0, join_date: "2021-09-10", status: "active", avatar_color: "#EC4899" },
    { id: 4, name: "Vikram Malhotra", email: "vikram.malhotra@crewdeck.io", phone: "+91 98204 44004", role_title: "Head of Sales & Revenue", department_id: 4, manager_id: null, salary: 3300000.0, join_date: "2022-01-12", status: "active", avatar_color: "#3B82F6" },
    { id: 5, name: "Sunita Menon", email: "sunita.menon@crewdeck.io", phone: "+91 98205 55005", role_title: "Chief Financial Officer", department_id: 5, manager_id: null, salary: 3800000.0, join_date: "2020-11-05", status: "active", avatar_color: "#F59E0B" },
    { id: 6, name: "Aarav Sharma", email: "aarav.sharma@crewdeck.io", phone: "+91 98206 66006", role_title: "Principal Systems Architect", department_id: 1, manager_id: 1, salary: 3100000.0, join_date: "2022-02-18", status: "active", avatar_color: "#10B981" },
    { id: 7, name: "Rohan Deshmukh", email: "rohan.deshmukh@crewdeck.io", phone: "+91 98207 77007", role_title: "Staff Software Engineer", department_id: 1, manager_id: 1, salary: 2750000.0, join_date: "2022-04-10", status: "active", avatar_color: "#8B5CF6" },
    { id: 8, name: "Sneha Kulkarni", email: "sneha.kulkarni@crewdeck.io", phone: "+91 98208 88008", role_title: "Senior Backend Engineer", department_id: 1, manager_id: 7, salary: 2100000.0, join_date: "2022-07-22", status: "active", avatar_color: "#06B6D4" },
    { id: 9, name: "Arjun Reddy", email: "arjun.reddy@crewdeck.io", phone: "+91 98209 99009", role_title: "Senior Frontend Engineer", department_id: 1, manager_id: 7, salary: 2050000.0, join_date: "2022-08-15", status: "active", avatar_color: "#F97316" },
    { id: 10, name: "Divya Nair", email: "divya.nair@crewdeck.io", phone: "+91 98210 10101", role_title: "Lead UX Designer", department_id: 2, manager_id: 2, salary: 2300000.0, join_date: "2022-05-09", status: "active", avatar_color: "#FFB020" },
    { id: 11, name: "Kabir Sen", email: "kabir.sen@crewdeck.io", phone: "+91 98211 20202", role_title: "Senior Product Manager", department_id: 2, manager_id: 2, salary: 2500000.0, join_date: "2022-09-01", status: "active", avatar_color: "#2DD4BF" },
    { id: 12, name: "Neha Gupta", email: "neha.gupta@crewdeck.io", phone: "+91 98212 30303", role_title: "Senior Product Designer", department_id: 2, manager_id: 10, salary: 1950000.0, join_date: "2023-01-16", status: "active", avatar_color: "#EC4899" },
    { id: 13, name: "Meera Joshi", email: "meera.joshi@crewdeck.io", phone: "+91 98213 40404", role_title: "HR Business Partner", department_id: 3, manager_id: 3, salary: 1750000.0, join_date: "2022-10-10", status: "active", avatar_color: "#3B82F6" },
    { id: 14, name: "Siddharth Nair", email: "siddharth.nair@crewdeck.io", phone: "+91 98214 50505", role_title: "Talent Acquisition Lead", department_id: 3, manager_id: 3, salary: 1850000.0, join_date: "2022-11-20", status: "active", avatar_color: "#10B981" },
    { id: 15, name: "Pooja Agarwal", email: "pooja.agarwal@crewdeck.io", phone: "+91 98215 60606", role_title: "Senior Enterprise Account Exec", department_id: 4, manager_id: 4, salary: 2400000.0, join_date: "2022-03-01", status: "active", avatar_color: "#F59E0B" },
    { id: 16, name: "Karthik Raman", email: "karthik.raman@crewdeck.io", phone: "+91 98216 70707", role_title: "Senior DevOps Engineer", department_id: 1, manager_id: 6, salary: 2200000.0, join_date: "2023-02-01", status: "active", avatar_color: "#8B5CF6" },
    { id: 17, name: "Tanvi Shah", email: "tanvi.shah@crewdeck.io", phone: "+91 98217 80808", role_title: "QA Automation Lead", department_id: 1, manager_id: 1, salary: 1900000.0, join_date: "2022-12-05", status: "active", avatar_color: "#06B6D4" },
    { id: 18, name: "Aditya Joshi", email: "aditya.joshi@crewdeck.io", phone: "+91 98218 90909", role_title: "Fullstack Engineer", department_id: 1, manager_id: 8, salary: 1600000.0, join_date: "2023-03-15", status: "active", avatar_color: "#F97316" },
    { id: 19, name: "Kavita Rao", email: "kavita.rao@crewdeck.io", phone: "+91 98219 01010", role_title: "Product Operations Specialist", department_id: 2, manager_id: 11, salary: 1500000.0, join_date: "2023-04-10", status: "active", avatar_color: "#FFB020" },
    { id: 20, name: "Manish Pandey", email: "manish.pandey@crewdeck.io", phone: "+91 98220 12121", role_title: "Finance Manager", department_id: 5, manager_id: 5, salary: 2300000.0, join_date: "2022-06-18", status: "active", avatar_color: "#2DD4BF" },
    { id: 21, name: "Shreya Bhat", email: "shreya.bhat@crewdeck.io", phone: "+91 98221 23232", role_title: "Legal Counsel", department_id: 5, manager_id: 5, salary: 2400000.0, join_date: "2023-01-05", status: "active", avatar_color: "#EC4899" },
    { id: 22, name: "Rahul Mehta", email: "rahul.mehta@crewdeck.io", phone: "+91 98222 34343", role_title: "Senior Growth Marketer", department_id: 4, manager_id: 4, salary: 2000000.0, join_date: "2023-02-15", status: "active", avatar_color: "#3B82F6" },
    { id: 23, name: "Swati Saxena", email: "swati.saxena@crewdeck.io", phone: "+91 98223 45454", role_title: "Customer Success Lead", department_id: 4, manager_id: 4, salary: 1800000.0, join_date: "2023-05-20", status: "active", avatar_color: "#10B981" },
    { id: 24, name: "Amit Trivedi", email: "amit.trivedi@crewdeck.io", phone: "+91 98224 56565", role_title: "Security Engineer", department_id: 1, manager_id: 6, salary: 2150000.0, join_date: "2023-06-12", status: "active", avatar_color: "#F59E0B" },
    { id: 25, name: "Deepa Nambiar", email: "deepa.nambiar@crewdeck.io", phone: "+91 98225 67676", role_title: "Backend Engineer", department_id: 1, manager_id: 8, salary: 1550000.0, join_date: "2023-07-03", status: "active", avatar_color: "#8B5CF6" },
    { id: 26, name: "Harsh Vardhan", email: "harsh.vardhan@crewdeck.io", phone: "+91 98226 78787", role_title: "Data Engineer", department_id: 1, manager_id: 7, salary: 1800000.0, join_date: "2023-08-14", status: "active", avatar_color: "#06B6D4" },
    { id: 27, name: "Ritu Singhania", email: "ritu.singhania@crewdeck.io", phone: "+91 98227 89898", role_title: "Content & Brand Strategist", department_id: 4, manager_id: 22, salary: 1450000.0, join_date: "2023-09-01", status: "active", avatar_color: "#F97316" },
    { id: 28, name: "Gaurav Chopra", email: "gaurav.chopra@crewdeck.io", phone: "+91 98228 90909", role_title: "Account Executive", department_id: 4, manager_id: 15, salary: 1600000.0, join_date: "2023-09-18", status: "active", avatar_color: "#FFB020" },
    { id: 29, name: "Nisha Pillai", email: "nisha.pillai@crewdeck.io", phone: "+91 98229 01234", role_title: "Employee Experience Specialist", department_id: 3, manager_id: 13, salary: 1300000.0, join_date: "2023-10-10", status: "on-leave", avatar_color: "#2DD4BF" },
    { id: 30, name: "Prateek Jain", email: "prateek.jain@crewdeck.io", phone: "+91 98230 12345", role_title: "Senior QA Engineer", department_id: 1, manager_id: 17, salary: 1650000.0, join_date: "2023-10-25", status: "active", avatar_color: "#EC4899" },
    { id: 31, name: "Alok Banerjee", email: "alok.banerjee@crewdeck.io", phone: "+91 98231 23456", role_title: "Senior Financial Analyst", department_id: 5, manager_id: 20, salary: 1700000.0, join_date: "2023-11-15", status: "active", avatar_color: "#3B82F6" },
    { id: 32, name: "Tarun Bajaj", email: "tarun.bajaj@crewdeck.io", phone: "+91 98232 34567", role_title: "Frontend Engineer", department_id: 1, manager_id: 9, salary: 1400000.0, join_date: "2023-12-01", status: "active", avatar_color: "#10B981" },
    { id: 33, name: "Rekha Sundaram", email: "rekha.sundaram@crewdeck.io", phone: "+91 98233 45678", role_title: "Visual & Motion Designer", department_id: 2, manager_id: 12, salary: 1450000.0, join_date: "2024-01-08", status: "active", avatar_color: "#F59E0B" },
    { id: 34, name: "Varun Kapoor", email: "varun.kapoor@crewdeck.io", phone: "+91 98234 56789", role_title: "Junior Systems Engineer", department_id: 1, manager_id: 25, salary: 1100000.0, join_date: "2024-02-15", status: "active", avatar_color: "#8B5CF6" },
    { id: 35, name: "Shruti Mathur", email: "shruti.mathur@crewdeck.io", phone: "+91 98235 67890", role_title: "Technical Recruiter", department_id: 3, manager_id: 14, salary: 1350000.0, join_date: "2024-03-01", status: "active", avatar_color: "#06B6D4" },
    { id: 36, name: "Kunal Mukherjee", email: "kunal.mukherjee@crewdeck.io", phone: "+91 98236 78901", role_title: "IT Support Specialist", department_id: 5, manager_id: 5, salary: 1050000.0, join_date: "2024-03-20", status: "active", avatar_color: "#F97316" },
    { id: 37, name: "Shalini Das", email: "shalini.das@crewdeck.io", phone: "+91 98237 89012", role_title: "Sales Development Representative", department_id: 4, manager_id: 28, salary: 950000.0, join_date: "2024-04-10", status: "active", avatar_color: "#FFB020" },
    { id: 38, name: "Nikhil Tiwari", email: "nikhil.tiwari@crewdeck.io", phone: "+91 98238 90123", role_title: "Site Reliability Engineer", department_id: 1, manager_id: 16, salary: 1750000.0, join_date: "2024-05-02", status: "on-leave", avatar_color: "#2DD4BF" },
    { id: 39, name: "Pooja Hegde", email: "pooja.hegde@crewdeck.io", phone: "+91 98239 01234", role_title: "Associate Product Manager", department_id: 2, manager_id: 11, salary: 1300000.0, join_date: "2024-05-15", status: "active", avatar_color: "#EC4899" },
    { id: 40, name: "Suresh Pillai", email: "suresh.pillai@crewdeck.io", phone: "+91 98240 12345", role_title: "Staff DevOps Specialist", department_id: 1, manager_id: 1, salary: 2800000.0, join_date: "2022-01-10", status: "active", avatar_color: "#3B82F6" }
  ];

  function getTodayIso() {
    return new Date().toISOString().split('T')[0];
  }

  function loadStore() {
    try {
      const data = localStorage.getItem(STORAGE_KEY);
      if (data) return JSON.parse(data);
    } catch {}

    // Initialize with seed data
    const today = getTodayIso();
    const attendance = {};
    INITIAL_EMPS.forEach(e => {
      attendance[`${e.id}_${today}`] = {
        employee_id: e.id,
        date: today,
        check_in: e.status === 'on-leave' ? null : "09:15:00",
        check_out: null,
        status: e.status === 'on-leave' ? "on-leave" : "present"
      };
    });

    const store = {
      departments: INITIAL_DEPTS,
      employees: INITIAL_EMPS,
      attendance: attendance,
      leaveRequests: [
        { id: 1, employee_id: 29, employee_name: "Nisha Pillai", leave_type: "annual", start_date: today, end_date: today, days: 1, reason: "Personal family event", status: "approved" },
        { id: 2, employee_id: 38, employee_name: "Nikhil Tiwari", leave_type: "sick", start_date: today, end_date: today, days: 2, reason: "Viral fever", status: "approved" },
        { id: 3, employee_id: 8, employee_name: "Sneha Kulkarni", leave_type: "casual", start_date: "2026-11-10", end_date: "2026-11-12", days: 3, reason: "Diwali celebrations", status: "pending" }
      ],
      leaveBalances: {},
      audit: [
        { id: 1, user_email: "admin@crewdeck.io", action: "BOOT", entity_type: "system", entity_id: 1, details: "System initialized with 40 operators", created_at: new Date().toISOString() },
        { id: 2, user_email: "admin@crewdeck.io", action: "LOGIN", entity_type: "auth", entity_id: 1, details: "Admin authenticated via secure deck", created_at: new Date().toISOString() }
      ]
    };

    INITIAL_EMPS.forEach(e => {
      store.leaveBalances[e.id] = {
        annual: { total: 18, used: e.id === 29 ? 3 : 1, remaining: e.id === 29 ? 15 : 17 },
        sick: { total: 12, used: e.id === 38 ? 2 : 0, remaining: e.id === 38 ? 10 : 12 },
        casual: { total: 10, used: 1, remaining: 9 }
      };
    });

    saveStore(store);
    return store;
  }

  function saveStore(store) {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(store));
    } catch {}
  }

  // Pre-seed demo operators
  const DEMO_USERS = {
    'admin@crewdeck.io': { id: 1, email: 'admin@crewdeck.io', role: 'admin', employee_id: 0, name: 'Admin Controller', avatar_color: '#FFB020' },
    'rajesh.verma@crewdeck.io': { id: 2, email: 'rajesh.verma@crewdeck.io', role: 'manager', employee_id: 1, name: 'Rajesh Verma', avatar_color: '#FFB020' },
    'aarav.sharma@crewdeck.io': { id: 3, email: 'aarav.sharma@crewdeck.io', role: 'employee', employee_id: 6, name: 'Aarav Sharma', avatar_color: '#10B981' }
  };

  window.MockBackend = {
    async handle(endpoint, options = {}) {
      // Normalize endpoint
      const cleanEndpoint = endpoint.replace(/^\/?api\/v1\/?/, '').replace(/^\/?api\/?/, '');
      const method = (options.method || 'GET').toUpperCase();
      let body = {};
      if (options.body) {
        body = typeof options.body === 'string' ? JSON.parse(options.body) : options.body;
      }

      const store = loadStore();

      // 1. Auth: Login
      if (cleanEndpoint === 'auth/login' && method === 'POST') {
        const email = (body.email || '').toLowerCase().trim();
        const pass = body.password || '';

        // Check demo accounts or any employee email with password "admin123" / "manager123" / "employee123"
        let user = DEMO_USERS[email];
        if (!user) {
          const emp = store.employees.find(e => e.email.toLowerCase() === email);
          if (emp) {
            user = { id: 100 + emp.id, email: emp.email, role: emp.manager_id === null ? 'manager' : 'employee', employee_id: emp.id, name: emp.name, avatar_color: emp.avatar_color };
          }
        }

        if (user) {
          sessionStorage.setItem(SESSION_KEY, JSON.stringify(user));
          localStorage.setItem(SESSION_KEY, JSON.stringify(user));
          store.audit.unshift({
            id: Date.now(),
            user_email: user.email,
            action: 'LOGIN',
            entity_type: 'auth',
            entity_id: user.id,
            details: `User ${user.email} signed into mission control`,
            created_at: new Date().toISOString()
          });
          saveStore(store);
          return { user };
        } else {
          throw new Error('Invalid email or security password.');
        }
      }

      // 2. Auth: Logout
      if (cleanEndpoint === 'auth/logout') {
        sessionStorage.removeItem(SESSION_KEY);
        localStorage.removeItem(SESSION_KEY);
        return { success: true };
      }

      // 3. Auth: Me
      if (cleanEndpoint === 'auth/me') {
        let userStr = sessionStorage.getItem(SESSION_KEY) || localStorage.getItem(SESSION_KEY);
        if (userStr) {
          return { user: JSON.parse(userStr) };
        }
        // Default to admin if accessed directly in demo
        const defaultAdmin = DEMO_USERS['admin@crewdeck.io'];
        return { user: defaultAdmin };
      }

      // 4. Dashboard Metrics
      if (cleanEndpoint === 'dashboard/metrics') {
        const activeEmps = store.employees.filter(e => e.status !== 'exited');
        const totalSalary = activeEmps.reduce((acc, e) => acc + (e.salary || 0), 0);
        const avgSalary = activeEmps.length ? Math.round(totalSalary / activeEmps.length) : 0;
        const onLeave = store.employees.filter(e => e.status === 'on-leave').length;

        // 14 day trend
        const trend = [];
        const now = new Date();
        for (let i = 13; i >= 0; i--) {
          const d = new Date(now);
          d.setDate(d.getDate() - i);
          const dateStr = d.toISOString().split('T')[0];
          const isWeekend = d.getDay() === 0 || d.getDay() === 6;
          const present = isWeekend ? 0 : Math.floor(activeEmps.length * (0.88 + Math.random() * 0.08));
          trend.push({
            date: dateStr,
            present_count: present,
            total_active: activeEmps.length,
            percentage: isWeekend ? 0 : Math.round((present / activeEmps.length) * 100)
          });
        }

        // Dept distribution
        const deptDist = store.departments.map(d => {
          const count = activeEmps.filter(e => e.department_id === d.id).length;
          return { id: d.id, name: d.name, count };
        });

        return {
          headcount: activeEmps.length,
          new_joiners: 4,
          on_leave_today: onLeave,
          avg_salary: avgSalary,
          attendance_trend: trend,
          dept_distribution: deptDist,
          recent_audit: store.audit.slice(0, 5)
        };
      }

      // 5. Employees List
      if (cleanEndpoint.startsWith('employees') && method === 'GET') {
        const parts = cleanEndpoint.split('?')[0].split('/');
        if (parts.length === 2 && !isNaN(parts[1])) {
          // Single Employee
          const id = parseInt(parts[1], 10);
          const emp = store.employees.find(e => e.id === id);
          if (!emp) throw new Error('Employee not found');
          const dept = store.departments.find(d => d.id === emp.department_id);
          const manager = store.employees.find(e => e.id === emp.manager_id);
          const balances = store.leaveBalances[id] || {
            annual: { total: 18, used: 1, remaining: 17 },
            sick: { total: 12, used: 0, remaining: 12 },
            casual: { total: 10, used: 1, remaining: 9 }
          };
          return {
            ...emp,
            department_name: dept ? dept.name : 'Unassigned',
            manager_name: manager ? manager.name : 'None',
            leave_balances: balances
          };
        }

        // List with filtering & search
        const urlParams = new URLSearchParams(cleanEndpoint.includes('?') ? cleanEndpoint.split('?')[1] : '');
        const search = (urlParams.get('search') || '').toLowerCase();
        const dept = urlParams.get('dept');
        const status = urlParams.get('status');
        const page = parseInt(urlParams.get('page') || '1', 10);
        const limit = parseInt(urlParams.get('limit') || '10', 10);

        let filtered = store.employees.filter(e => {
          if (search && !e.name.toLowerCase().includes(search) && !e.email.toLowerCase().includes(search) && !e.role_title.toLowerCase().includes(search)) {
            return false;
          }
          if (dept && e.department_id !== parseInt(dept, 10)) return false;
          if (status && e.status !== status) return false;
          return true;
        });

        const total = filtered.length;
        const start = (page - 1) * limit;
        const pageItems = filtered.slice(start, start + limit).map(e => {
          const d = store.departments.find(dep => dep.id === e.department_id);
          const m = store.employees.find(emp => emp.id === e.manager_id);
          return {
            ...e,
            department_name: d ? d.name : 'Unassigned',
            manager_name: m ? m.name : 'None'
          };
        });

        return {
          data: pageItems,
          pagination: {
            total,
            page,
            limit,
            total_pages: Math.ceil(total / limit)
          }
        };
      }

      // 6. Employees: Create (POST)
      if (cleanEndpoint === 'employees' && method === 'POST') {
        const newId = store.employees.length ? Math.max(...store.employees.map(e => e.id)) + 1 : 1;
        const colors = ['#FFB020', '#2DD4BF', '#EC4899', '#3B82F6', '#10B981', '#8B5CF6'];
        const newEmp = {
          id: newId,
          name: body.name || 'New Operative',
          email: body.email,
          phone: body.phone || '+91 99000 00000',
          role_title: body.role_title || 'Engineer',
          department_id: parseInt(body.department_id || 1, 10),
          manager_id: body.manager_id ? parseInt(body.manager_id, 10) : null,
          salary: parseFloat(body.salary || 1200000),
          join_date: body.join_date || getTodayIso(),
          status: body.status || 'active',
          avatar_color: colors[newId % colors.length]
        };
        store.employees.unshift(newEmp);
        store.leaveBalances[newId] = {
          annual: { total: 18, used: 0, remaining: 18 },
          sick: { total: 12, used: 0, remaining: 12 },
          casual: { total: 10, used: 0, remaining: 10 }
        };
        store.audit.unshift({
          id: Date.now(),
          user_email: 'admin@crewdeck.io',
          action: 'ONBOARD',
          entity_type: 'employee',
          entity_id: newId,
          details: `Onboarded operative ${newEmp.name} (${newEmp.role_title})`,
          created_at: new Date().toISOString()
        });
        saveStore(store);
        return { data: newEmp };
      }

      // 7. Employees: Update (PUT)
      if (cleanEndpoint.startsWith('employees/') && method === 'PUT') {
        const id = parseInt(cleanEndpoint.split('/')[1], 10);
        const idx = store.employees.findIndex(e => e.id === id);
        if (idx === -1) throw new Error('Employee not found');
        store.employees[idx] = { ...store.employees[idx], ...body, id };
        store.audit.unshift({
          id: Date.now(),
          user_email: 'admin@crewdeck.io',
          action: 'UPDATE',
          entity_type: 'employee',
          entity_id: id,
          details: `Updated operative profile #${id} (${store.employees[idx].name})`,
          created_at: new Date().toISOString()
        });
        saveStore(store);
        return { data: store.employees[idx] };
      }

      // 8. Employees: Delete (DELETE)
      if (cleanEndpoint.startsWith('employees/') && method === 'DELETE') {
        const id = parseInt(cleanEndpoint.split('/')[1], 10);
        const emp = store.employees.find(e => e.id === id);
        store.employees = store.employees.filter(e => e.id !== id);
        store.audit.unshift({
          id: Date.now(),
          user_email: 'admin@crewdeck.io',
          action: 'OFFBOARD',
          entity_type: 'employee',
          entity_id: id,
          details: `Offboarded operative record ${emp ? emp.name : id}`,
          created_at: new Date().toISOString()
        });
        saveStore(store);
        return { success: true };
      }

      // 9. Departments
      if (cleanEndpoint.startsWith('departments')) {
        if (method === 'GET') {
          const list = store.departments.map(d => {
            const emps = store.employees.filter(e => e.department_id === d.id && e.status !== 'exited');
            const totalPayroll = emps.reduce((acc, e) => acc + (e.salary || 0), 0);
            const head = store.employees.find(e => e.id === d.head_id) || emps[0];
            return {
              ...d,
              employee_count: emps.length,
              total_payroll: totalPayroll,
              head_name: head ? head.name : 'Unassigned'
            };
          });
          return list;
        }
      }

      // 10. Org Constellation Graph
      if (cleanEndpoint === 'org/constellation') {
        const active = store.employees.filter(e => e.status !== 'exited');
        const nodes = active.map(e => {
          const dept = store.departments.find(d => d.id === e.department_id);
          return {
            id: e.id,
            name: e.name,
            role: e.role_title,
            department: dept ? dept.name : 'General',
            department_id: e.department_id,
            avatar_color: e.avatar_color,
            manager_id: e.manager_id
          };
        });
        const links = [];
        active.forEach(e => {
          if (e.manager_id && active.some(m => m.id === e.manager_id)) {
            links.push({ source: e.manager_id, target: e.id });
          }
        });
        return {
          nodes,
          links,
          departments: store.departments
        };
      }

      // 11. Attendance
      if (cleanEndpoint.startsWith('attendance')) {
        if (cleanEndpoint === 'attendance' && method === 'GET') {
          const today = getTodayIso();
          return store.employees.map(e => {
            const key = `${e.id}_${today}`;
            const att = store.attendance[key] || {
              employee_id: e.id,
              date: today,
              check_in: e.status === 'on-leave' ? null : "09:15:00",
              check_out: null,
              status: e.status === 'on-leave' ? "on-leave" : "present"
            };
            const dept = store.departments.find(d => d.id === e.department_id);
            return {
              ...att,
              employee_name: e.name,
              role_title: e.role_title,
              department_name: dept ? dept.name : 'General',
              avatar_color: e.avatar_color
            };
          });
        }

        if (cleanEndpoint === 'attendance/check-in' && method === 'POST') {
          const userStr = sessionStorage.getItem(SESSION_KEY) || localStorage.getItem(SESSION_KEY);
          const user = userStr ? JSON.parse(userStr) : DEMO_USERS['admin@crewdeck.io'];
          const empId = user.employee_id || 1;
          const today = getTodayIso();
          const key = `${empId}_${today}`;
          const nowTime = new Date().toTimeString().split(' ')[0];
          store.attendance[key] = {
            employee_id: empId,
            date: today,
            check_in: nowTime,
            check_out: null,
            status: 'present'
          };
          saveStore(store);
          return { success: true, check_in: nowTime };
        }

        if (cleanEndpoint === 'attendance/check-out' && method === 'POST') {
          const userStr = sessionStorage.getItem(SESSION_KEY) || localStorage.getItem(SESSION_KEY);
          const user = userStr ? JSON.parse(userStr) : DEMO_USERS['admin@crewdeck.io'];
          const empId = user.employee_id || 1;
          const today = getTodayIso();
          const key = `${empId}_${today}`;
          const nowTime = new Date().toTimeString().split(' ')[0];
          if (!store.attendance[key]) {
            store.attendance[key] = { employee_id: empId, date: today, check_in: "09:00:00", check_out: nowTime, status: 'present' };
          } else {
            store.attendance[key].check_out = nowTime;
          }
          saveStore(store);
          return { success: true, check_out: nowTime };
        }

        if (cleanEndpoint.startsWith('attendance/heatmap/')) {
          const id = parseInt(cleanEndpoint.split('/')[2], 10);
          const records = {};
          const now = new Date();
          for (let i = 0; i < 365; i++) {
            const d = new Date(now);
            d.setDate(d.getDate() - i);
            const dateStr = d.toISOString().split('T')[0];
            const isWeekend = d.getDay() === 0 || d.getDay() === 6;
            if (!isWeekend && Math.random() > 0.08) {
              records[dateStr] = Math.random() > 0.85 ? 1 : 2;
            }
          }
          return { employee_id: id, records };
        }
      }

      // 12. Leave Requests & Balances
      if (cleanEndpoint.startsWith('leave')) {
        if (cleanEndpoint.startsWith('leave/requests')) {
          if (method === 'GET') {
            return store.leaveRequests;
          }
          if (method === 'POST') {
            const newReq = {
              id: Date.now(),
              employee_id: body.employee_id || 1,
              employee_name: body.employee_name || 'Operative',
              leave_type: body.leave_type || 'annual',
              start_date: body.start_date || getTodayIso(),
              end_date: body.end_date || getTodayIso(),
              days: parseInt(body.days || 1, 10),
              reason: body.reason || 'Personal time off',
              status: 'pending'
            };
            store.leaveRequests.unshift(newReq);
            saveStore(store);
            return { data: newReq };
          }
          if (cleanEndpoint.includes('/review') && method === 'PUT') {
            const reqId = parseInt(cleanEndpoint.split('/')[2], 10);
            const req = store.leaveRequests.find(r => r.id === reqId);
            if (req) {
              req.status = body.status || 'approved';
              if (req.status === 'approved' && store.leaveBalances[req.employee_id]) {
                const bal = store.leaveBalances[req.employee_id][req.leave_type];
                if (bal) {
                  bal.used += req.days;
                  bal.remaining = Math.max(0, bal.total - bal.used);
                }
              }
              saveStore(store);
            }
            return { success: true };
          }
        }

        if (cleanEndpoint.startsWith('leave/balances/')) {
          const empId = parseInt(cleanEndpoint.split('/')[2], 10);
          return store.leaveBalances[empId] || {
            annual: { total: 18, used: 1, remaining: 17 },
            sick: { total: 12, used: 0, remaining: 12 },
            casual: { total: 10, used: 1, remaining: 9 }
          };
        }
      }

      // 13. Payroll Summary & Export
      if (cleanEndpoint.startsWith('payroll')) {
        if (cleanEndpoint === 'payroll/summary') {
          return store.departments.map(d => {
            const emps = store.employees.filter(e => e.department_id === d.id && e.status !== 'exited');
            const totalCTC = emps.reduce((acc, e) => acc + (e.salary || 0), 0);
            const monthly = Math.round(totalCTC / 12);
            return {
              department_id: d.id,
              department_name: d.name,
              headcount: emps.length,
              total_annual_ctc: totalCTC,
              monthly_disbursement: monthly,
              avg_ctc: emps.length ? Math.round(totalCTC / emps.length) : 0
            };
          });
        }

        if (cleanEndpoint === 'payroll/export') {
          let csv = "Employee ID,Name,Email,Department,Role,Status,Annual CTC (INR),Monthly Gross (INR)\n";
          store.employees.forEach(e => {
            const dept = store.departments.find(d => d.id === e.department_id);
            const monthly = Math.round((e.salary || 0) / 12);
            csv += `"${e.id}","${e.name}","${e.email}","${dept ? dept.name : ''}","${e.role_title}","${e.status}",${e.salary},${monthly}\n`;
          });
          return new Blob([csv], { type: 'text/csv' });
        }
      }

      // 14. Audit Trail
      if (cleanEndpoint === 'audit') {
        return {
          data: store.audit,
          total: store.audit.length
        };
      }

      // 15. Healthz
      if (cleanEndpoint === 'healthz' || cleanEndpoint === '') {
        return { status: "ok", service: "CREW/DECK", version: "1.0.0", mode: "serverless" };
      }

      throw new Error(`Endpoint not mapped: ${endpoint}`);
    }
  };
})();
