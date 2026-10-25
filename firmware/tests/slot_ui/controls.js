const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

function input(value = '') { return {value, checked: false}; }
const elements = {
  deviceName: input(), reportingBackend: input(), channelKey: input(),
  customUrl: input(), pingInterval: input(), pingPaused: input(),
  pingSlotCount: input('2'), pingSlotIndex: input('1'), pingSlotEnabled: input()
};
elements.pingSlotIndex.options = [1, 2, 3, 4].map(value => ({value: String(value)}));
// Match native type=number constraints (min=30, max=3600, default step=1).
// The field is not required, so browsers consider an empty value valid.
Object.defineProperty(elements.pingInterval, 'validity', {get() {
  const value = Number(this.value);
  return {valid: this.value === '' || Number.isInteger(value) && value >= 30 && value <= 3600};
}});

const classes = {};
const nodes = new Map();
const form = {elements, querySelector(selector) {
  if (selector === '.custom-fields') return {
    classList: {toggle: (name, hidden) => { classes.customFields = hidden; }},
    querySelector: () => elements.customUrl
  };
  if (selector === '.svitlobot-fields') return {
    classList: {toggle: (name, hidden) => { classes.svitlobotFields = hidden; }}
  };
  return elements[selector.match(/name=([^\]]+)/)[1]];
}};
nodes.set('#settings', form);
function q(selector) {
  if (!nodes.has(selector)) nodes.set(selector, {
    classList: {toggle: (name, enabled) => { classes[selector] = enabled; }},
    getAttribute: () => 'already-loaded'
  });
  return nodes.get(selector);
}
let config;
let generation = 0;
const context = vm.createContext({
  q, document: {activeElement: null}, handoff: false, loading: false,
  busyOperation: false, cfg: null, features: {}, renderedGeneration: -1,
  scanned: true, renderEvents() {}, renderWifi() {},
  api: async url => url === '/api/config' ? config : {
    features: {}, api: {backend: 'svitlobot', result: 'waiting_slot'}, wifi: {},
    configGeneration: generation, commit: 'test', version: 'test'
  }
});
// toggleDestination, updatePingSlots, and load are extracted unchanged from WebUi.h.
vm.runInContext(fs.readFileSync(process.argv[2], 'utf8'), context);

async function restore(count, index, interval = 120, backend = 'svitlobot') {
  config = {
    setupComplete: true, wifi: [], reportingBackend: backend, channelKey: '',
    customUrl: '', pingPaused: false, pingInterval: interval
  };
  if (count !== undefined) config.pingSlotCount = count;
  if (index !== undefined) config.pingSlotIndex = index;
  generation++;
  await context.load();
  assert.equal(context.renderedGeneration, generation, 'load must complete successfully');
}

(async () => {
  for (let count = 2; count <= 4; count++) {
    for (let index = 1; index <= count; index++) {
      await restore(count, index);
      assert.equal(elements.pingSlotEnabled.checked, true);
      assert.equal(+elements.pingSlotCount.value, count);
      assert.equal(+elements.pingSlotIndex.value, index);
      assert.equal(classes['#pingIntervalField'], true);
      assert.equal(classes['#pingSlots'], false);
      assert.equal(q('#pingSlotPreview').textContent,
        `Кожні ${count * 45} с · зсув ${(index - 1) * 45} с`);
      for (const option of elements.pingSlotIndex.options) {
        assert.equal(option.disabled, +option.value > count);
        assert.equal(option.hidden, +option.value > count);
      }
      assert.equal(elements.pingInterval.value, 120);
    }
  }

  // Reducing group size clamps a now-invalid board number before submission.
  await restore(4, 4, 95);
  elements.pingSlotCount.value = '2';
  context.updatePingSlots();
  assert.equal(+elements.pingSlotIndex.value, 2);
  assert.equal(q('#pingSlotPreview').textContent, 'Кожні 90 с · зсув 45 с');

  // Switching mode does not overwrite or disable the stored normal interval.
  elements.pingSlotEnabled.checked = false;
  context.updatePingSlots();
  assert.equal(classes['#pingSlots'], true);
  assert.equal(classes['#pingIntervalField'], false);
  assert.equal(elements.pingInterval.value, 95);
  assert.notEqual(elements.pingInterval.disabled, true);
  assert.notEqual(elements.pingSlotCount.disabled, true);
  assert.notEqual(elements.pingSlotIndex.disabled, true);
  elements.pingSlotEnabled.checked = true;
  context.updatePingSlots();
  assert.equal(classes['#pingSlots'], false);
  assert.equal(+elements.pingSlotCount.value, 2);
  assert.equal(+elements.pingSlotIndex.value, 2);
  assert.equal(elements.pingInterval.value, 95);

  // Reload a saved disabled state and an older response with no slot fields.
  for (const [count, index] of [[0, 1], [undefined, undefined]]) {
    await restore(count, index, 77);
    assert.equal(elements.pingSlotEnabled.checked, false);
    assert.equal(+elements.pingSlotCount.value, 2);
    assert.equal(+elements.pingSlotIndex.value, 1);
    assert.equal(classes['#pingSlots'], true);
    assert.equal(classes['#pingIntervalField'], false);
    assert.equal(elements.pingInterval.value, 77);
  }
  // A hidden invalid number would block native form submission. Switching
  // to slots restores the saved value, while a valid unsaved edit survives.
  for (const invalid of ['20', '', '30.5', '4000']) {
    await restore(0, 1, 95);
    elements.pingInterval.value = invalid;
    elements.pingSlotEnabled.checked = true;
    context.updatePingSlots();
    assert.equal(+elements.pingInterval.value, 95);
    assert.equal(elements.pingInterval.validity.valid, true);
  }
  await restore(0, 1, 95);
  elements.pingInterval.value = '123';
  for (const enabled of [true, false, true]) {
    elements.pingSlotEnabled.checked = enabled;
    context.updatePingSlots();
    assert.equal(elements.pingInterval.value, '123');
  }
  context.cfg = null;
  elements.pingInterval.value = '';
  context.updatePingSlots();
  assert.equal(+elements.pingInterval.value, 60);

  // Slot controls belong only to Svitlobot. Backend toggles preserve the
  // checkbox and slot selection while restoring Custom URL's normal interval.
  await restore(4, 3, 100);
  assert.equal(classes['#pingSlotControls'], false);
  for (const backend of ['custom', 'future-backend']) {
    elements.reportingBackend.value = backend;
    context.toggleDestination(form);
    assert.equal(classes['#pingSlotControls'], true);
    assert.equal(classes['#pingSlots'], true);
    assert.equal(classes['#pingIntervalField'], false);
    assert.equal(elements.pingInterval.value, 100);
    assert.equal(elements.pingSlotEnabled.checked, true);
    assert.equal(+elements.pingSlotCount.value, 4);
    assert.equal(+elements.pingSlotIndex.value, 3);
    elements.reportingBackend.value = 'svitlobot';
    context.toggleDestination(form);
    assert.equal(classes['#pingSlotControls'], false);
    assert.equal(classes['#pingSlots'], false);
    assert.equal(classes['#pingIntervalField'], true);
    assert.equal(elements.pingSlotEnabled.checked, true);
    assert.equal(+elements.pingSlotCount.value, 4);
    assert.equal(+elements.pingSlotIndex.value, 3);
  }

  // A saved Custom URL configuration may still retain an enabled Sv schedule.
  // Loading it must show the editable normal interval, with all slots hidden.
  await restore(3, 2, 80, 'custom');
  assert.equal(classes['#pingSlotControls'], true);
  assert.equal(classes['#pingSlots'], true);
  assert.equal(classes['#pingIntervalField'], false);
  assert.equal(elements.pingSlotEnabled.checked, true);
  assert.equal(+elements.pingSlotCount.value, 3);
  assert.equal(+elements.pingSlotIndex.value, 2);
  elements.pingInterval.value = '20';
  context.updatePingSlots();
  assert.equal(elements.pingInterval.value, '20', 'Custom interval validation stays visible');
  elements.reportingBackend.value = 'svitlobot';
  context.toggleDestination(form);
  assert.equal(+elements.pingInterval.value, 80);
  assert.equal(classes['#pingSlotControls'], false);
  assert.equal(+elements.pingSlotCount.value, 3);
  assert.equal(+elements.pingSlotIndex.value, 2);

  // Setup has destination fields but no scheduling controls. Its toggle must
  // remain safe and must not mutate the settings form's current slot state.
  const setupElements = {reportingBackend: input('custom'), customUrl: input()};
  const setupVisibility = {};
  const setupForm = {elements: setupElements, querySelector(selector) {
    if (selector === '[name=reportingBackend]') return setupElements.reportingBackend;
    return {
      classList: {toggle: (name, hidden) => { setupVisibility[selector] = hidden; }},
      querySelector: () => setupElements.customUrl
    };
  }};
  context.toggleDestination(setupForm);
  assert.equal(setupVisibility['.custom-fields'], false);
  assert.equal(setupVisibility['.svitlobot-fields'], true);
  assert.equal(setupElements.customUrl.required, true);
  assert.equal(classes['#pingSlotControls'], false);
  setupElements.reportingBackend.value = 'svitlobot';
  context.toggleDestination(setupForm);
  assert.equal(setupVisibility['.custom-fields'], true);
  assert.equal(setupVisibility['.svitlobot-fields'], false);
  assert.equal(setupElements.customUrl.required, false);
  console.log('Slot UI: restore, preview, validation, backend isolation, and preserved preferences passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
