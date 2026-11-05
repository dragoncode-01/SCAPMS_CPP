const state = { patients: [], appointments: [], queue: [], dashboard: null, reports: null, page: 'overview', searchResults: null };
const $ = (selector, root = document) => root.querySelector(selector);
const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];

function escapeHtml(value = '') {
  return String(value).replace(/[&<>"']/g, char => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[char]));
}

function toast(message, kind = 'success') {
  const element = document.createElement('div');
  element.className = `toast ${kind}`;
  element.textContent = message;
  $('#toastRegion').append(element);
  setTimeout(() => element.remove(), 3400);
}

async function api(path, options = {}) {
  const response = await fetch(path, options);
  let result = {};
  const contentType = response.headers.get('content-type') || '';
  if (contentType.includes('application/json')) result = await response.json();
  else if (!response.ok) result = { error: await response.text() };
  if (!response.ok) throw new Error(result.error || `Request failed (${response.status}).`);
  return result;
}

function formBody(values) {
  return new URLSearchParams(values);
}

function postForm(path, values) {
  return api(path, { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' }, body: formBody(values) });
}

function pageTitle(page) {
  return ({ overview: 'Overview', patients: 'Patients', appointments: 'Appointments', queue: 'Waiting queue', search: 'Search & sort', reports: 'Reports' })[page] || 'Overview';
}

function navigate(page) {
  if (!$('#page-' + page)) return;
  state.page = page;
  if (location.hash !== `#${page}`) history.replaceState(null, '', `#${page}`);
  $$('.page').forEach(el => el.classList.toggle('active', el.id === 'page-' + page));
  $$('.nav-item').forEach(el => el.classList.toggle('active', el.dataset.page === page));
  $('#crumbTitle').textContent = pageTitle(page);
  window.scrollTo({ top: 0, behavior: 'smooth' });
  if (page === 'reports') refreshReports();
}

function categoryClass(category) {
  if (category === 'Emergency') return 'category-emergency';
  if (category === 'Follow-Up') return 'category-follow';
  return 'category-general';
}

function priorityClass(category) {
  if (category === 'Emergency') return 'priority-high';
  if (category === 'Follow-Up') return 'priority-follow';
  return 'priority-normal';
}

function initials(name) {
  return String(name || '?').trim().split(/\s+/).slice(0, 2).map(part => part[0] || '').join('').toUpperCase();
}

function patientRow(patient) {
  const avatarClass = patient.category === 'Emergency' ? 'emergency' : patient.category === 'Follow-Up' ? 'follow' : '';
  return `<tr>
    <td><div class="patient-cell"><span class="patient-avatar ${avatarClass}">${escapeHtml(initials(patient.name))}</span><span class="patient-name"><strong>${escapeHtml(patient.name)}</strong><small>${escapeHtml(patient.id)}</small></span></div></td>
    <td><span class="category-pill ${categoryClass(patient.category)}">${escapeHtml(patient.category)}</span></td>
    <td>${escapeHtml(patient.age)}</td><td>${escapeHtml(patient.contact)}</td><td>${escapeHtml(patient.appointments)}</td>
    <td><span class="priority-pill ${priorityClass(patient.category)}">${escapeHtml(patient.priority)}</span></td>
    <td><div class="row-actions"><button class="action-button" data-action="edit-patient" data-id="${escapeHtml(patient.id)}" title="Edit patient">Edit</button><button class="action-button danger" data-action="delete-patient" data-id="${escapeHtml(patient.id)}" title="Delete patient">Delete</button></div></td>
  </tr>`;
}

function renderPatients() {
  const filter = ($('#patientQuickFilter')?.value || '').toLowerCase().trim();
  const rows = state.patients.filter(patient => !filter || [patient.name, patient.id, patient.category, patient.contact].some(value => String(value).toLowerCase().includes(filter)));
  $('#patientsTable').innerHTML = rows.length ? rows.map(patientRow).join('') : `<tr><td colspan="7"><div class="empty-state">${state.patients.length ? 'No patients match this filter.' : 'No patients registered yet. Add the first patient to get started.'}</div></td></tr>`;
  $('#patientCountChip').textContent = String(state.patients.length);
  $('#patientNavCount').textContent = String(state.patients.length);
  $('#patientFooterCount').textContent = `Showing ${rows.length} of ${state.patients.length} patients`;
}

function formatDateTime(value) {
  if (!value) return 'Date not set';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return value.replace('T', ' ');
  return new Intl.DateTimeFormat('en-NG', { day: 'numeric', month: 'short', year: 'numeric', hour: 'numeric', minute: '2-digit' }).format(date);
}

function dateParts(value) {
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return { day: '--', month: '---' };
  return { day: new Intl.DateTimeFormat('en', { day: '2-digit' }).format(date), month: new Intl.DateTimeFormat('en', { month: 'short' }).format(date) };
}

function statusClass(status) {
  return `status-${String(status).toLowerCase()}`;
}

function appointmentActions(appointment) {
  if (appointment.status !== 'Scheduled') return '';
  return `<button class="action-button" data-action="complete-appointment" data-id="${escapeHtml(appointment.id)}" title="Mark completed">Done</button><button class="action-button danger" data-action="cancel-appointment" data-id="${escapeHtml(appointment.id)}" title="Cancel appointment">Cancel</button>`;
}

function renderAppointments() {
  const filter = ($('#appointmentFilter')?.value || '').toLowerCase().trim();
  const rows = state.appointments.filter(item => !filter || [item.id, item.patientName, item.patientId, item.reason, item.status].some(value => String(value).toLowerCase().includes(filter)));
  $('#appointmentsTable').innerHTML = rows.length ? rows.map(item => `<tr>
    <td><span class="appointment-id">${escapeHtml(item.id)}</span></td>
    <td><div class="appointment-cell"><strong>${escapeHtml(item.patientName)}</strong><small>${escapeHtml(item.patientId)} · ${escapeHtml(item.category)}</small></div></td>
    <td>${escapeHtml(formatDateTime(item.dateTime))}</td><td>${escapeHtml(item.reason)}</td>
    <td><span class="status-pill ${statusClass(item.status)}">${escapeHtml(item.status)}</span></td>
    <td><div class="row-actions">${appointmentActions(item)}</div></td></tr>`).join('') : `<tr><td colspan="6"><div class="empty-state">${state.appointments.length ? 'No appointments match this filter.' : 'No appointments scheduled. Use the form above to book one.'}</div></td></tr>`;
  $('#appointmentCountChip').textContent = String(state.appointments.length);
}

function fillPatientOptions() {
  const patientOptions = state.patients.map(patient => `<option value="${escapeHtml(patient.id)}">${escapeHtml(patient.name)} · ${escapeHtml(patient.id)} · ${escapeHtml(patient.category)}</option>`).join('');
  const appointmentSelect = $('#appointmentPatient');
  const queueSelect = $('#queuePatient');
  const appointmentValue = appointmentSelect.value;
  const queueValue = queueSelect.value;
  appointmentSelect.innerHTML = `<option value="">Select patient</option>${patientOptions}`;
  queueSelect.innerHTML = `<option value="">Select a patient</option>${patientOptions}`;
  if (state.patients.some(patient => patient.id === appointmentValue)) appointmentSelect.value = appointmentValue;
  if (state.patients.some(patient => patient.id === queueValue)) queueSelect.value = queueValue;
}

function renderQueue() {
  $('#queueCount').textContent = String(state.queue.length);
  const list = $('#queueList');
  list.innerHTML = state.queue.length ? state.queue.map((patient, index) => `<div class="queue-row"><span class="queue-number">${String(index + 1).padStart(2, '0')}</span><span><strong>${escapeHtml(patient.name)}</strong><small>${escapeHtml(patient.id)} · ${escapeHtml(patient.category)}</small></span><span class="queue-position">${index === 0 ? 'Next' : `#${index + 1}`}</span></div>`).join('') : `<div class="empty-state">No one is waiting. Add a patient to the queue when they arrive.</div>`;
  const next = state.queue[0];
  $('#nextPatientCard').innerHTML = next ? `<div class="next-card"><span class="patient-avatar ${next.category === 'Emergency' ? 'emergency' : next.category === 'Follow-Up' ? 'follow' : ''}">${escapeHtml(initials(next.name))}</span><span><strong>${escapeHtml(next.name)}</strong><small>${escapeHtml(next.id)} · ${escapeHtml(next.category)}</small></span></div>` : `<div class="empty-mini">No patients are waiting.</div>`;
  $('#callNextButton').disabled = !next;
  $('#callNextTop').disabled = !next;
}

function renderUpcoming() {
  const upcoming = (state.dashboard?.upcoming || []).slice(0, 5);
  $('#upcomingList').innerHTML = upcoming.length ? upcoming.map(item => {
    const parts = dateParts(item.dateTime);
    return `<div class="upcoming-row"><span class="date-block"><strong>${escapeHtml(parts.day)}</strong><small>${escapeHtml(parts.month)}</small></span><span class="upcoming-name"><strong>${escapeHtml(item.patientName)}</strong><small>${escapeHtml(item.reason)}</small></span><span class="appointment-time">${escapeHtml(formatDateTime(item.dateTime).split(', ').slice(-1)[0])}</span><span class="mini-status">Scheduled</span></div>`;
  }).join('') : `<div class="empty-state">No upcoming appointments yet.</div>`;
}

const categoryColors = { General: '#5592ed', Emergency: '#e47788', 'Follow-Up': '#9584e6' };
function renderCategorySummary() {
  const groups = ['General', 'Emergency', 'Follow-Up'];
  const total = state.patients.length || 1;
  $('#categorySummary').innerHTML = groups.map(category => {
    const count = state.patients.filter(patient => patient.category === category).length;
    const width = Math.round(count / total * 100);
    return `<div class="category-summary-item"><span class="category-dot" style="background:${categoryColors[category]}"></span><strong>${category} patients</strong><b>${count}</b><div class="category-track"><span style="width:${width}%;background:${categoryColors[category]}"></span></div></div>`;
  }).join('');
}

function renderDashboard() {
  const data = state.dashboard || {};
  $('#statPatients').textContent = String(data.patients ?? state.patients.length);
  $('#statAppointments').textContent = String(data.appointments ?? state.appointments.length);
  $('#statPending').textContent = String(data.pending ?? 0);
  $('#statWaiting').textContent = String(data.waiting ?? state.queue.length);
  $('#statEmergency').textContent = String(data.emergencies ?? 0);
  renderUpcoming();
  renderCategorySummary();
}

function renderSearchResults(items, title = 'Search results') {
  state.searchResults = items;
  $('#searchResultsTitle').textContent = title;
  $('#searchResultsSubtitle').textContent = `${items.length} matching ${items.length === 1 ? 'record' : 'records'}`;
  $('#searchResults').innerHTML = items.length ? items.map(patientRow).join('') : `<tr><td colspan="7"><div class="empty-state">No matching patient records were found.</div></td></tr>`;
}

function renderReports() {
  const report = state.reports;
  if (!report) return;
  const categories = ['General', 'Emergency', 'Follow-Up'];
  const total = categories.reduce((sum, category) => sum + report.categories[category], 0) || 1;
  $('#categoryReport').innerHTML = categories.map(category => {
    const value = report.categories[category];
    const width = Math.round(value / total * 100);
    return `<div class="report-category-row"><span>${category}</span><div class="report-track"><span style="width:${width}%;background:${categoryColors[category]}"></span></div><b>${value}</b></div>`;
  }).join('');
  $('#reportDate').textContent = new Intl.DateTimeFormat('en-NG', { weekday: 'long', day: 'numeric', month: 'long', year: 'numeric' }).format(new Date(`${report.daily.date}T12:00:00`));
  $('#dailyReport').innerHTML = [['total', 'Total'], ['completed', 'Completed'], ['pending', 'Pending'], ['cancelled', 'Cancelled']].map(([key, label]) => `<div class="daily-stat"><b>${report.daily[key]}</b><span>${label}</span></div>`).join('');
  $('#frequentReport').innerHTML = report.frequent.length ? report.frequent.map((patient, index) => `<div class="report-list-row"><span class="report-rank">${index + 1}</span><span><strong>${escapeHtml(patient.name)}</strong><small>${escapeHtml(patient.id)} · ${escapeHtml(patient.category)}</small></span><b>${patient.completedAppointments} completed</b></div>`).join('') : `<div class="empty-mini">Completed visits will appear here.</div>`;
  $('#pendingReport').innerHTML = report.pendingAppointments.length ? report.pendingAppointments.slice(0, 6).map((item, index) => `<div class="report-list-row"><span class="report-rank">◷</span><span><strong>${escapeHtml(item.patientName)}</strong><small>${escapeHtml(formatDateTime(item.dateTime))} · ${escapeHtml(item.reason)}</small></span><b>${escapeHtml(item.category)}</b></div>`).join('') : `<div class="empty-mini">No pending appointments.</div>`;
}

async function reloadAll({ report = state.page === 'reports' } = {}) {
  const [patients, appointments, queue, dashboard] = await Promise.all([
    api('/api/patients'), api('/api/appointments'), api('/api/queue'), api('/api/dashboard')
  ]);
  state.patients = patients;
  state.appointments = appointments;
  state.queue = queue;
  state.dashboard = dashboard;
  renderPatients();
  renderAppointments();
  fillPatientOptions();
  renderQueue();
  renderDashboard();
  if (report) await refreshReports();
}

async function refreshReports() {
  try {
    state.reports = await api('/api/reports');
    renderReports();
  } catch (error) { toast(error.message, 'error'); }
}

function setSavedStatus(message = 'All changes saved') {
  $('#saveStatus').innerHTML = `${escapeHtml(message)} <i class="save-check">✓</i>`;
}

async function afterChange(message) {
  await reloadAll();
  setSavedStatus();
  if (message) toast(message);
}

function openPatientModal(patient = null) {
  const modal = $('#patientModal');
  const form = $('#patientForm');
  form.reset();
  form.elements.id.value = patient?.id || '';
  form.elements.name.value = patient?.name || '';
  form.elements.type.value = patient?.category || 'General';
  form.elements.age.value = patient?.age ?? '';
  form.elements.contact.value = patient?.contact || '';
  form.elements.notes.value = patient?.notes || '';
  $('#patientModalTitle').textContent = patient ? 'Edit patient' : 'Register patient';
  $('#patientSubmit').textContent = patient ? 'Save changes' : 'Save patient';
  updatePatientTypeHint();
  modal.hidden = false;
  setTimeout(() => form.elements.name.focus(), 50);
}

function closePatientModal() { $('#patientModal').hidden = true; }

function updatePatientTypeHint() {
  const type = $('#patientType').value;
  const hints = {
    General: ['Clinical notes', 'Standard consultation · Normal priority'],
    Emergency: ['Urgent clinical notes', 'Immediate triage · High priority'],
    'Follow-Up': ['Follow-up notes', 'Review previous care plan · Follow-up priority']
  };
  $('#notesLabel').textContent = hints[type][0];
  $('#typeHint').textContent = hints[type][1];
}

function updateSearchOptions() {
  const isName = $('#searchField').value === 'name';
  const algorithm = $('#searchAlgorithm');
  [...algorithm.options].forEach(option => {
    if (option.value === 'binary' || option.value === 'recursive') option.disabled = !isName;
  });
  if (!isName && algorithm.value !== 'linear') algorithm.value = 'linear';
  const descriptions = {
    linear: 'Linear search checks each record and supports ID, name and category.',
    binary: 'Binary search uses a name-sorted copy and finds an exact, case-insensitive name.',
    recursive: 'Recursive search checks names one record at a time and supports partial text.'
  };
  $('#algorithmHint').textContent = descriptions[algorithm.value];
}

async function callNext() {
  try {
    const result = await postForm('/api/queue/next', {});
    if (result.id) toast(`Called ${result.name}.`);
    else toast('The waiting queue is empty.', 'error');
    await reloadAll();
  } catch (error) { toast(error.message, 'error'); }
}

async function saveData() {
  try { await postForm('/api/save', {}); setSavedStatus(); toast('Clinic data saved.'); }
  catch (error) { toast(error.message, 'error'); }
}

async function loadData() {
  if (!confirm('Load the saved clinic data? Unsaved changes will be replaced.')) return;
  try { await postForm('/api/load', {}); await reloadAll(); toast('Saved clinic data loaded.'); }
  catch (error) { toast(error.message, 'error'); }
}

function exportData() {
  const link = document.createElement('a');
  link.href = '/api/data/export';
  link.download = 'scapms-data.tsv';
  document.body.append(link);
  link.click();
  link.remove();
  toast('Your SCAPMS data file is ready to download.');
}

async function importData(file) {
  try {
    const contents = await file.text();
    await api('/api/data/import', { method: 'POST', headers: { 'Content-Type': 'text/plain;charset=UTF-8' }, body: contents });
    await reloadAll();
    toast('Clinic data imported successfully.');
  } catch (error) { toast(error.message, 'error'); }
  $('#importFile').value = '';
}

document.addEventListener('click', async event => {
  const nav = event.target.closest('[data-page]');
  if (nav) { navigate(nav.dataset.page); return; }
  const goto = event.target.closest('[data-goto]');
  if (goto) { navigate(goto.dataset.goto); return; }
  if (event.target.closest('[data-open-patient]')) { openPatientModal(); return; }
  if (event.target.closest('[data-close-modal]')) { closePatientModal(); return; }
  const menuButton = event.target.closest('#fileMenuButton');
  if (menuButton) { $('#fileMenu').hidden = !$('#fileMenu').hidden; return; }
  const fileAction = event.target.closest('[data-file-action]');
  if (fileAction) {
    $('#fileMenu').hidden = true;
    if (fileAction.dataset.fileAction === 'save') await saveData();
    else if (fileAction.dataset.fileAction === 'load') await loadData();
    else if (fileAction.dataset.fileAction === 'export') exportData();
    else if (fileAction.dataset.fileAction === 'import') $('#importFile').click();
    return;
  }
  const action = event.target.closest('[data-action]');
  if (action) {
    const id = action.dataset.id;
    try {
      if (action.dataset.action === 'edit-patient') {
        const patient = state.patients.find(record => record.id === id);
        if (patient) openPatientModal(patient);
      } else if (action.dataset.action === 'delete-patient') {
        const patient = state.patients.find(record => record.id === id);
        if (patient && confirm(`Delete ${patient.name}'s patient record? You can undo this action.`)) {
          await api(`/api/patients/${encodeURIComponent(id)}`, { method: 'DELETE' });
          await afterChange('Patient record deleted.');
        }
      } else if (action.dataset.action === 'cancel-appointment') {
        if (confirm('Cancel this appointment?')) { await postForm(`/api/appointments/${encodeURIComponent(id)}/cancel`, {}); await afterChange('Appointment cancelled.'); }
      } else if (action.dataset.action === 'complete-appointment') {
        await postForm(`/api/appointments/${encodeURIComponent(id)}/complete`, {}); await afterChange('Appointment marked completed.');
      }
    } catch (error) { toast(error.message, 'error'); }
    return;
  }
  if (event.target.closest('#callNextButton, #callNextTop')) { await callNext(); return; }
  if (!event.target.closest('.file-menu-wrap')) $('#fileMenu').hidden = true;
});

$('#patientType').addEventListener('change', updatePatientTypeHint);
$('#patientQuickFilter').addEventListener('input', renderPatients);
$('#appointmentFilter').addEventListener('input', renderAppointments);
$('#importFile').addEventListener('change', event => { const file = event.target.files?.[0]; if (file) importData(file); });
$('#refreshReports').addEventListener('click', refreshReports);
$('#clearSearch').addEventListener('click', () => renderSearchResults([], 'Patient records'));

$('#patientForm').addEventListener('submit', async event => {
  event.preventDefault();
  const form = event.currentTarget;
  const values = Object.fromEntries(new FormData(form).entries());
  try {
    if (values.id) { await postForm('/api/patients/edit', values); await afterChange('Patient record updated.'); }
    else { await postForm('/api/patients', values); await afterChange('Patient registered successfully.'); }
    closePatientModal();
  } catch (error) { toast(error.message, 'error'); }
});

$('#appointmentForm').addEventListener('submit', async event => {
  event.preventDefault();
  const values = Object.fromEntries(new FormData(event.currentTarget).entries());
  values.dateTime = `${values.date}T${values.time}`;
  delete values.date;
  delete values.time;
  try {
    await postForm('/api/appointments', values);
    event.currentTarget.reset();
    await afterChange('Appointment scheduled.');
  } catch (error) { toast(error.message, 'error'); }
});

$('#searchField').addEventListener('change', updateSearchOptions);
$('#searchAlgorithm').addEventListener('change', updateSearchOptions);
$('#searchForm').addEventListener('submit', async event => {
  event.preventDefault();
  const values = Object.fromEntries(new FormData(event.currentTarget).entries());
  try {
    const results = await postForm('/api/search', values);
    renderSearchResults(results, `${values.algorithm[0].toUpperCase()}${values.algorithm.slice(1)} search results`);
  } catch (error) { toast(error.message, 'error'); }
});

$('#sortForm').addEventListener('submit', async event => {
  event.preventDefault();
  const values = Object.fromEntries(new FormData(event.currentTarget).entries());
  try {
    const sorted = await postForm('/api/sort', values);
    state.patients = sorted;
    renderPatients();
    renderCategorySummary();
    if (state.searchResults) renderSearchResults(sorted, 'Sorted patient records');
    setSavedStatus('Sort order saved');
    toast(`${values.algorithm === 'selection' ? 'Selection' : 'Insertion'} sort completed by ${values.field}.`);
  } catch (error) { toast(error.message, 'error'); }
});

$('#addToQueue').addEventListener('click', async () => {
  const patientId = $('#queuePatient').value;
  if (!patientId) { toast('Select a patient to add to the queue.', 'error'); return; }
  try { await postForm('/api/queue/add', { patientId }); await afterChange('Patient added to the waiting queue.'); }
  catch (error) { toast(error.message, 'error'); }
});

$('#undoButton').addEventListener('click', async () => {
  try { await postForm('/api/undo', {}); await afterChange('Last administrative action undone.'); }
  catch (error) { toast(error.message, 'error'); }
});

$('#globalSearch').addEventListener('keydown', event => {
  if (event.key !== 'Enter') return;
  const query = event.currentTarget.value.trim();
  if (!query) return;
  navigate('search');
  $('#searchField').value = 'name';
  $('#searchAlgorithm').value = 'linear';
  updateSearchOptions();
  $('#searchQuery').value = query;
  $('#searchForm').requestSubmit();
});

document.addEventListener('keydown', event => {
  const key = event.key.toLowerCase();
  if ((event.ctrlKey || event.metaKey) && key === 's') { event.preventDefault(); saveData(); }
  if ((event.ctrlKey || event.metaKey) && key === 'f') { event.preventDefault(); navigate('search'); $('#searchQuery').focus(); }
  if ((event.ctrlKey || event.metaKey) && key === 'q') { event.preventDefault(); toast('Close this browser tab to exit SCAPMS.'); }
  if (event.key === 'Escape' && !$('#patientModal').hidden) closePatientModal();
});

function renderDate() {
  const now = new Date();
  const formatted = new Intl.DateTimeFormat('en-NG', { weekday: 'short', day: 'numeric', month: 'short', year: 'numeric' }).format(now);
  $('#topDate').textContent = formatted;
  $('#welcomeDate').textContent = new Intl.DateTimeFormat('en-NG', { weekday: 'long', day: 'numeric', month: 'long', year: 'numeric' }).format(now).toUpperCase();
}

async function init() {
  renderDate();
  updatePatientTypeHint();
  updateSearchOptions();
  const requestedPage = location.hash.slice(1);
  if ($('#page-' + requestedPage)) navigate(requestedPage);
  const tomorrow = new Date();
  tomorrow.setDate(tomorrow.getDate() + 1);
  $('#appointmentDate').min = new Date().toISOString().slice(0, 10);
  $('#appointmentDate').value = tomorrow.toISOString().slice(0, 10);
  $('#appointmentTime').value = '09:00';
  try { await reloadAll({ report: false }); }
  catch (error) { toast(`Could not load clinic data: ${error.message}`, 'error'); }
  // Refresh time and dashboard counters periodically without interrupting form entry.
  setInterval(async () => {
    renderDate();
    try {
      state.dashboard = await api('/api/dashboard');
      renderDashboard();
      $('#statWaiting').textContent = String(state.dashboard.waiting);
    } catch (_) { /* The next user action will show a request error if the server stopped. */ }
  }, 60000);
}

init();
