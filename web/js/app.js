/**
 * ZENON SMART ROOM LIGHTING CONTROLLER - JAVASCRIPT APP CONTROLLER
 * Full offline-first, WebSocket real-time state synchronization, 
 * interactive SVG schematic bindings, RGB Studio, and Settings Manager.
 */

class RoomLightsApp {
  constructor() {
    this.ws = null;
    this.wsReconnectTimer = null;
    this.isWsConnected = false;
    this.currentTab = 'dashboard';
    
    // Application State
    this.state = {
      relays: [
        { id: 1, name: "Left Wall", state: false, gpio: 4, activeLow: true, enabled: true, type: "warm" },
        { id: 2, name: "Back Wall Main", state: false, gpio: 5, activeLow: true, enabled: true, type: "warm" },
        { id: 3, name: "Spare Ch 3", state: false, gpio: 6, activeLow: true, enabled: false, type: "spare" },
        { id: 4, name: "Behind Monitor", state: false, gpio: 7, activeLow: true, enabled: true, type: "warm" },
        { id: 5, name: "Under Table", state: false, gpio: 15, activeLow: true, enabled: true, type: "warm" },
        { id: 6, name: "Right Wall / Bed", state: false, gpio: 16, activeLow: true, enabled: true, type: "warm" },
        { id: 7, name: "Spare Ch 7", state: false, gpio: 17, activeLow: true, enabled: false, type: "spare" },
        { id: 8, name: "Spare Ch 8", state: false, gpio: 18, activeLow: true, enabled: false, type: "spare" }
      ],
      rgb: {
        power: false,
        brightness: 210, // ~82%
        color: { r: 0, g: 240, b: 255 }, // Cyan default
        effect: "aurora",
        speed: 128,
        intensity: 160,
        chipset: "WS2812B",
        ledCount: 144,
        gpio: 48,
        colorOrder: "GRB",
        maxCurrent_mA: 5000
      },
      system: {
        device: "roomlights",
        ip: "192.168.1.50",
        hostname: "roomlights.local",
        rssi: -54,
        uptime: 0,
        heap: 0,
        version: "v1.0.0"
      }
    };

    // Color Wheel Dragging State
    this.isDraggingWheel = false;
    this.lastWsSendTime = 0;
    this.wsSendThrottleMs = 40; // 25 fps max throttle for smooth slider responsiveness

    // Initialize UI
    this.initElements();
    this.initEventListeners();
    this.initColorWheel();
    this.initWebSocket();
    this.renderAll();
  }

  initElements() {
    this.statusDot = document.getElementById('statusDot');
    this.statusText = document.getElementById('statusText');
    this.wifiRssi = document.getElementById('wifiRssi');

    // Master Controls
    this.btnAllLights = document.getElementById('btnAllLights');
    this.btnAllWarm = document.getElementById('btnAllWarm');
    this.btnAllOff = document.getElementById('btnAllOff');
    this.btnRgbToggle = document.getElementById('btnRgbToggle');

    // Cards Grid
    this.relayCardsGrid = document.getElementById('relayCardsGrid');
    this.allRelaysList = document.getElementById('allRelaysList');

    // Modals
    this.rgbModal = document.getElementById('rgbModal');
    this.relayModal = document.getElementById('relayModal');
    this.relayModalTitle = document.getElementById('relayModalTitle');
    this.relayNameInput = document.getElementById('relayNameInput');
    this.relayGpioSelect = document.getElementById('relayGpioSelect');
    this.relayLogicSelect = document.getElementById('relayLogicSelect');
    this.relayEnabledCheck = document.getElementById('relayEnabledCheck');
    this.currentEditingRelayId = null;

    // RGB Controls
    this.rgbPowerSwitch = document.getElementById('rgbPowerSwitch');
    this.rgbBrightnessSlider = document.getElementById('rgbBrightnessSlider');
    this.rgbBrightnessVal = document.getElementById('rgbBrightnessVal');
    this.rgbSpeedSlider = document.getElementById('rgbSpeedSlider');
    this.rgbSpeedVal = document.getElementById('rgbSpeedVal');
    this.rgbIntensitySlider = document.getElementById('rgbIntensitySlider');
    this.rgbIntensityVal = document.getElementById('rgbIntensityVal');
    this.colorWheelCanvas = document.getElementById('colorWheelCanvas');
    this.colorCenterPreview = document.getElementById('colorCenterPreview');
    this.rgbHexInput = document.getElementById('rgbHexInput');
    this.effectsContainer = document.getElementById('effectsContainer');
  }

  initEventListeners() {
    // Navigation Tabs
    document.querySelectorAll('.nav-item').forEach(btn => {
      btn.addEventListener('click', (e) => {
        e.preventDefault();
        const tab = btn.getAttribute('data-tab');
        this.switchTab(tab);
      });
    });

    // Master Buttons
    this.btnAllLights.addEventListener('click', () => this.handleMasterAllLights());
    this.btnAllWarm.addEventListener('click', () => this.handleMasterAllWarm());
    this.btnAllOff.addEventListener('click', () => this.handleMasterAllOff());
    this.btnRgbToggle.addEventListener('click', () => this.toggleRgbPower());

    // SVG Schematic Segment Clicks
    document.querySelectorAll('.segment-group').forEach(seg => {
      seg.addEventListener('click', (e) => {
        const segId = parseInt(seg.getAttribute('data-segment'));
        const segType = seg.getAttribute('data-type');
        this.handleSegmentClick(segId, segType);
      });
    });

    // RGB Studio Controls
    if (this.rgbPowerSwitch) {
      this.rgbPowerSwitch.addEventListener('click', () => this.toggleRgbPower());
    }

    if (this.rgbBrightnessSlider) {
      this.rgbBrightnessSlider.addEventListener('input', (e) => {
        const val = parseInt(e.target.value);
        this.state.rgb.brightness = val;
        this.rgbBrightnessVal.textContent = `${Math.round((val / 255) * 100)}%`;
        this.sendRgbUpdate({ brightness: val });
        this.updateSvgRgb();
      });
    }

    if (this.rgbSpeedSlider) {
      this.rgbSpeedSlider.addEventListener('input', (e) => {
        const val = parseInt(e.target.value);
        this.state.rgb.speed = val;
        this.rgbSpeedVal.textContent = `${val}`;
        this.sendRgbUpdate({ speed: val });
      });
    }

    if (this.rgbIntensitySlider) {
      this.rgbIntensitySlider.addEventListener('input', (e) => {
        const val = parseInt(e.target.value);
        this.state.rgb.intensity = val;
        this.rgbIntensityVal.textContent = `${val}`;
        this.sendRgbUpdate({ intensity: val });
      });
    }

    // Color Swatches
    document.querySelectorAll('.color-swatch').forEach(swatch => {
      swatch.addEventListener('click', () => {
        const hex = swatch.getAttribute('data-color');
        this.setColorFromHex(hex);
      });
    });

    // Effect Chips
    document.querySelectorAll('.effect-chip').forEach(chip => {
      chip.addEventListener('click', () => {
        const effectName = chip.getAttribute('data-effect');
        this.setEffect(effectName);
      });
    });

    // Modals Close
    document.querySelectorAll('.sheet-close-btn, .modal-overlay').forEach(el => {
      el.addEventListener('click', (e) => {
        if (e.target === el || el.classList.contains('sheet-close-btn') || el.closest('.sheet-close-btn')) {
          this.closeModals();
        }
      });
    });

    // Stop modal body click from closing
    document.querySelectorAll('.bottom-sheet').forEach(sheet => {
      sheet.addEventListener('click', (e) => e.stopPropagation());
    });

    // Relay Edit Form Save
    const saveRelayBtn = document.getElementById('saveRelayBtn');
    if (saveRelayBtn) {
      saveRelayBtn.addEventListener('click', () => this.saveRelaySettings());
    }

    // Settings Form Save (RGB & Hardware)
    const saveRgbConfigBtn = document.getElementById('saveRgbConfigBtn');
    if (saveRgbConfigBtn) {
      saveRgbConfigBtn.addEventListener('click', () => this.saveRgbHardwareConfig());
    }

    // Restart ESP32 Button
    const restartEspBtn = document.getElementById('restartEspBtn');
    if (restartEspBtn) {
      restartEspBtn.addEventListener('click', () => this.restartEsp32());
    }
  }

  /* ==========================================================================
     WEBSOCKET CLIENT & STATE SYNC
     ========================================================================== */
  initWebSocket() {
    if (this.wsReconnectTimer) clearTimeout(this.wsReconnectTimer);

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.host || 'roomlights.local';
    const wsUrl = `${protocol}//${host}/ws`;

    try {
      this.ws = new WebSocket(wsUrl);

      this.ws.onopen = () => {
        this.isWsConnected = true;
        this.updateConnectionStatus(true);
        this.showToast('ESP32-S3 Connected');
        // Request initial state sync
        this.sendWsMessage({ action: 'getState' });
      };

      this.ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          this.handleWsMessage(msg);
        } catch (err) {
          console.warn('WS Message parse error:', err);
        }
      };

      this.ws.onclose = () => {
        this.isWsConnected = false;
        this.updateConnectionStatus(false);
        this.wsReconnectTimer = setTimeout(() => this.initWebSocket(), 3000);
      };

      this.ws.onerror = (err) => {
        this.isWsConnected = false;
        this.updateConnectionStatus(false);
      };
    } catch (e) {
      this.isWsConnected = false;
      this.updateConnectionStatus(false);
      this.wsReconnectTimer = setTimeout(() => this.initWebSocket(), 3000);
    }
  }

  sendWsMessage(payload) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(payload));
    } else {
      // Fallback to REST API if WS not ready
      this.sendRestFallback(payload);
    }
  }

  sendRestFallback(payload) {
    if (payload.action === 'setRelay') {
      fetch(`/api/relay/${payload.channel}/${payload.state ? 'on' : 'off'}`, { method: 'POST' }).catch(() => {});
    } else if (payload.action === 'setRgb') {
      fetch('/api/rgb/state', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      }).catch(() => {});
    }
  }

  handleWsMessage(msg) {
    if (msg.type === 'state' || msg.type === 'fullState') {
      if (msg.relays) {
        msg.relays.forEach(r => {
          const idx = this.state.relays.findIndex(item => item.id === r.id);
          if (idx !== -1) {
            this.state.relays[idx] = { ...this.state.relays[idx], ...r };
          }
        });
      }
      if (msg.rgb) {
        this.state.rgb = { ...this.state.rgb, ...msg.rgb };
      }
      if (msg.system) {
        this.state.system = { ...this.state.system, ...msg.system };
      }
      this.renderAll();
    } else if (msg.type === 'relayUpdate') {
      const idx = this.state.relays.findIndex(item => item.id === msg.channel);
      if (idx !== -1) {
        this.state.relays[idx].state = msg.state;
        this.renderRelay(this.state.relays[idx]);
        this.updateSvgRelays();
        this.updateMasterButtons();
      }
    } else if (msg.type === 'rgbUpdate') {
      this.state.rgb = { ...this.state.rgb, ...msg };
      this.renderRgb();
      this.updateSvgRgb();
      this.updateMasterButtons();
    }
  }

  updateConnectionStatus(connected) {
    if (connected) {
      this.statusDot.classList.add('connected');
      this.statusText.textContent = 'CONNECTED';
    } else {
      this.statusDot.classList.remove('connected');
      this.statusText.textContent = 'RECONNECTING...';
    }
  }

  /* ==========================================================================
     TAB NAVIGATION & VIEW SWITCHING
     ========================================================================== */
  switchTab(tabName) {
    this.currentTab = tabName;
    document.querySelectorAll('.tab-view').forEach(view => view.classList.remove('active'));
    document.querySelectorAll('.nav-item').forEach(btn => btn.classList.remove('active'));

    const activeView = document.getElementById(`view-${tabName}`);
    const activeNav = document.querySelector(`.nav-item[data-tab="${tabName}"]`);
    if (activeView) activeView.classList.add('active');
    if (activeNav) activeNav.classList.add('active');

    if (tabName === 'settings') {
      this.renderSettingsView();
    }
  }

  /* ==========================================================================
     SEGMENT & RELAY INTERACTIONS
     ========================================================================== */
  handleSegmentClick(segId, segType) {
    if (segType === 'rgb') {
      this.openRgbModal();
    } else {
      // Warm white relay segment
      const relay = this.state.relays.find(r => r.id === segId);
      if (relay) {
        this.toggleRelay(relay.id);
      }
    }
  }

  toggleRelay(chId) {
    const relay = this.state.relays.find(r => r.id === chId);
    if (!relay) return;

    const newState = !relay.state;
    relay.state = newState;

    // Send command
    this.sendWsMessage({
      action: 'setRelay',
      channel: chId,
      state: newState
    });

    this.renderRelay(relay);
    this.updateSvgRelays();
    this.updateMasterButtons();
  }

  openRelayModal(chId) {
    const relay = this.state.relays.find(r => r.id === chId);
    if (!relay) return;

    this.currentEditingRelayId = chId;
    this.relayModalTitle.textContent = `Configure Channel 0${chId}`;
    this.relayNameInput.value = relay.name;
    this.relayGpioSelect.value = relay.gpio;
    this.relayLogicSelect.value = relay.activeLow ? 'low' : 'high';
    this.relayEnabledCheck.checked = relay.enabled;

    this.relayModal.classList.add('open');
  }

  saveRelaySettings() {
    if (!this.currentEditingRelayId) return;
    const relay = this.state.relays.find(r => r.id === this.currentEditingRelayId);
    if (!relay) return;

    relay.name = this.relayNameInput.value.trim() || `Channel ${relay.id}`;
    relay.gpio = parseInt(this.relayGpioSelect.value);
    relay.activeLow = this.relayLogicSelect.value === 'low';
    relay.enabled = this.relayEnabledCheck.checked;

    this.sendWsMessage({
      action: 'updateRelayConfig',
      channel: relay.id,
      name: relay.name,
      gpio: relay.gpio,
      activeLow: relay.activeLow,
      enabled: relay.enabled
    });

    this.closeModals();
    this.renderAll();
    this.showToast(`Relay ${relay.id} updated`);
  }

  /* ==========================================================================
     RGB STUDIO CONTROLS & COLOR PICKER
     ========================================================================== */
  openRgbModal() {
    this.renderRgbModalControls();
    this.rgbModal.classList.add('open');
  }

  toggleRgbPower() {
    const newState = !this.state.rgb.power;
    this.state.rgb.power = newState;
    this.sendRgbUpdate({ power: newState });
    this.renderRgb();
    this.updateSvgRgb();
    this.updateMasterButtons();
  }

  setColorFromHex(hex) {
    const rgb = this.hexToRgb(hex);
    if (!rgb) return;
    this.state.rgb.color = rgb;
    this.state.rgb.power = true; // Auto-turn on when picking color
    this.sendRgbUpdate({ color: rgb, power: true });
    this.renderRgb();
    this.updateSvgRgb();
    this.updateMasterButtons();
  }

  setEffect(effectName) {
    this.state.rgb.effect = effectName;
    this.state.rgb.power = true;
    this.sendRgbUpdate({ effect: effectName, power: true });
    this.renderRgb();
    this.updateSvgRgb();
    this.updateMasterButtons();
  }

  sendRgbUpdate(params) {
    const now = Date.now();
    if (now - this.lastWsSendTime < this.wsSendThrottleMs) {
      if (this.wsThrottleTimer) clearTimeout(this.wsThrottleTimer);
      this.wsThrottleTimer = setTimeout(() => {
        this.sendWsMessage({ action: 'setRgb', ...params });
        this.lastWsSendTime = Date.now();
      }, this.wsSendThrottleMs);
      return;
    }

    this.lastWsSendTime = now;
    this.sendWsMessage({ action: 'setRgb', ...params });
  }

  initColorWheel() {
    const canvas = this.colorWheelCanvas;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const size = canvas.width;
    const radius = size / 2;

    // Draw HSL Color Disc
    for (let angle = 0; angle < 360; angle++) {
      const startAngle = ((angle - 1) * Math.PI) / 180;
      const endAngle = ((angle + 1) * Math.PI) / 180;
      ctx.beginPath();
      ctx.moveTo(radius, radius);
      ctx.arc(radius, radius, radius, startAngle, endAngle);
      ctx.closePath();

      const gradient = ctx.createRadialGradient(radius, radius, 0, radius, radius, radius);
      gradient.addColorStop(0, '#ffffff');
      gradient.addColorStop(1, `hsl(${angle}, 100%, 50%)`);
      ctx.fillStyle = gradient;
      ctx.fill();
    }

    // Touch & Mouse Event Handlers
    const handlePick = (e) => {
      const rect = canvas.getBoundingClientRect();
      const clientX = e.touches ? e.touches[0].clientX : e.clientX;
      const clientY = e.touches ? e.touches[0].clientY : e.clientY;
      const x = (clientX - rect.left) * (canvas.width / rect.width);
      const y = (clientY - rect.top) * (canvas.height / rect.height);

      const dx = x - radius;
      const dy = y - radius;
      const dist = Math.sqrt(dx * dx + dy * dy);

      if (dist <= radius) {
        const pixel = ctx.getImageData(Math.floor(x), Math.floor(y), 1, 1).data;
        const color = { r: pixel[0], g: pixel[1], b: pixel[2] };
        this.state.rgb.color = color;
        this.state.rgb.power = true;
        this.colorCenterPreview.style.backgroundColor = `rgb(${color.r}, ${color.g}, ${color.b})`;
        this.sendRgbUpdate({ color: color, power: true });
        this.updateSvgRgb();
        this.updateMasterButtons();
      }
    };

    canvas.addEventListener('mousedown', (e) => {
      this.isDraggingWheel = true;
      handlePick(e);
    });
    window.addEventListener('mousemove', (e) => {
      if (this.isDraggingWheel) handlePick(e);
    });
    window.addEventListener('mouseup', () => {
      this.isDraggingWheel = false;
    });

    canvas.addEventListener('touchstart', (e) => {
      this.isDraggingWheel = true;
      handlePick(e);
    }, { passive: false });
    window.addEventListener('touchmove', (e) => {
      if (this.isDraggingWheel) {
        e.preventDefault();
        handlePick(e);
      }
    }, { passive: false });
    window.addEventListener('touchend', () => {
      this.isDraggingWheel = false;
    });
  }

  /* ==========================================================================
     MASTER CONTROLS (ALL LIGHTS, ALL WARM, ALL OFF)
     ========================================================================== */
  handleMasterAllLights() {
    const anyOn = this.state.relays.some(r => r.enabled && r.state) || this.state.rgb.power;
    const targetState = !anyOn;

    this.state.relays.forEach(r => {
      if (r.enabled) r.state = targetState;
    });
    this.state.rgb.power = targetState;

    this.sendWsMessage({
      action: 'masterAll',
      state: targetState
    });

    this.renderAll();
  }

  handleMasterAllWarm() {
    this.state.relays.forEach(r => {
      if (r.enabled && r.type === 'warm') r.state = true;
    });

    this.sendWsMessage({ action: 'masterAllWarm', state: true });
    this.renderAll();
  }

  handleMasterAllOff() {
    this.state.relays.forEach(r => {
      if (r.enabled) r.state = false;
    });
    this.state.rgb.power = false;

    this.sendWsMessage({ action: 'masterAllOff' });
    this.renderAll();
  }

  updateMasterButtons() {
    const warmOn = this.state.relays.some(r => r.enabled && r.type === 'warm' && r.state);
    const allWarmOn = this.state.relays.filter(r => r.enabled && r.type === 'warm').every(r => r.state);
    const allLightsOn = this.state.relays.filter(r => r.enabled).every(r => r.state) && this.state.rgb.power;

    if (this.btnAllWarm) {
      if (allWarmOn) this.btnAllWarm.classList.add('active');
      else this.btnAllWarm.classList.remove('active');
    }

    if (this.btnAllLights) {
      if (allLightsOn) this.btnAllLights.classList.add('active');
      else this.btnAllLights.classList.remove('active');
    }

    if (this.btnRgbToggle) {
      if (this.state.rgb.power) this.btnRgbToggle.classList.add('active');
      else this.btnRgbToggle.classList.remove('active');
    }
  }

  /* ==========================================================================
     RENDERERS & SVG REALTIME UPDATES
     ========================================================================== */
  renderAll() {
    this.renderRelayCards();
    this.renderRgb();
    this.updateSvgRelays();
    this.updateSvgRgb();
    this.updateMasterButtons();
    if (this.currentTab === 'settings') {
      this.renderSettingsView();
    }
  }

  renderRelayCards() {
    if (!this.relayCardsGrid) return;
    this.relayCardsGrid.innerHTML = '';

    // Render quick cards for the 6 primary room segments
    this.state.relays.slice(0, 6).forEach(relay => {
      if (relay.id === 3) {
        // Render RGB Hero Card in place of Segment 3
        this.renderRgbHeroCard();
      } else {
        const card = document.createElement('div');
        card.className = `relay-card ${relay.state ? 'is-on' : ''}`;
        card.id = `card-relay-${relay.id}`;
        card.innerHTML = `
          <div class="card-top">
            <span class="channel-tag">0${relay.id}</span>
            <button class="card-settings-btn" onclick="app.openRelayModal(${relay.id})" title="Configure Channel">
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>
            </button>
          </div>
          <div class="card-body" onclick="app.toggleRelay(${relay.id})">
            <h3 class="card-name">${relay.name}</h3>
            <span class="card-type">WARM WHITE RELAY</span>
          </div>
          <div class="card-footer">
            <span class="card-status-label">${relay.state ? '● ON' : '○ OFF'}</span>
            <div class="tactile-switch" onclick="app.toggleRelay(${relay.id})">
              <div class="switch-thumb"></div>
            </div>
          </div>
        `;
        this.relayCardsGrid.appendChild(card);
      }
    });

    // Render full list in Relays Tab
    if (this.allRelaysList) {
      this.allRelaysList.innerHTML = '';
      this.state.relays.forEach(relay => {
        const item = document.createElement('div');
        item.className = `relay-card ${relay.state ? 'is-on' : ''}`;
        item.id = `card-relay-tab-${relay.id}`;
        item.innerHTML = `
          <div class="card-top">
            <span class="channel-tag">CHANNEL 0${relay.id} &bull; GPIO ${relay.gpio}</span>
            <button class="card-settings-btn" onclick="app.openRelayModal(${relay.id})">
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>
            </button>
          </div>
          <div class="card-body" onclick="app.toggleRelay(${relay.id})">
            <h3 class="card-name">${relay.name}</h3>
            <span class="card-type">${relay.enabled ? 'ACTIVE CHANNEL' : 'DISABLED'} &bull; ${relay.activeLow ? 'ACTIVE LOW' : 'ACTIVE HIGH'}</span>
          </div>
          <div class="card-footer">
            <span class="card-status-label">${relay.state ? '● ON' : '○ OFF'}</span>
            <div class="tactile-switch" onclick="app.toggleRelay(${relay.id})">
              <div class="switch-thumb"></div>
            </div>
          </div>
        `;
        this.allRelaysList.appendChild(item);
      });
    }
  }

  renderRgbHeroCard() {
    const card = document.createElement('div');
    const rgb = this.state.rgb;
    card.className = `rgb-hero-card ${rgb.power ? 'is-on' : ''}`;
    const hexColor = this.rgbToHex(rgb.color.r, rgb.color.g, rgb.color.b);
    const briPct = Math.round((rgb.brightness / 255) * 100);

    card.innerHTML = `
      <div class="card-top">
        <span class="channel-tag" style="color: var(--rgb-cyan);">03 &bull; RGB</span>
        <button class="card-settings-btn" onclick="app.openRgbModal()" title="Open RGB Studio">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><path d="M12 2a7 7 0 0 0 0 14v6"/></svg>
        </button>
      </div>
      <div class="card-body" onclick="app.openRgbModal()">
        <h3 class="card-name">Corner RGB Strip</h3>
        <span class="card-type" style="color: var(--rgb-cyan);">ADDRESSABLE RGB &bull; 144 LED/M</span>
        <div class="rgb-preview-pill" style="background: ${hexColor}; box-shadow: 0 0 14px ${hexColor}88;"></div>
      </div>
      <div class="rgb-quick-stats" onclick="app.openRgbModal()">
        <div class="stat-box"><span class="stat-label">Effect</span><span class="stat-val" style="text-transform: capitalize;">${rgb.effect}</span></div>
        <div class="stat-box"><span class="stat-label">Brightness</span><span class="stat-val">${briPct}%</span></div>
        <div class="stat-box"><span class="stat-label">Power</span><span class="stat-val">${rgb.power ? 'ON' : 'OFF'}</span></div>
      </div>
      <div class="card-footer">
        <span class="card-status-label" style="color: ${rgb.power ? 'var(--rgb-cyan)' : 'var(--text-secondary)'};">${rgb.power ? '● ACTIVE' : '○ OFF'}</span>
        <div class="tactile-switch" onclick="app.toggleRgbPower()">
          <div class="switch-thumb" style="${rgb.power ? 'left:25px; background:var(--rgb-cyan); box-shadow:0 0 10px var(--rgb-cyan);' : ''}"></div>
        </div>
      </div>
    `;
    this.relayCardsGrid.appendChild(card);
  }

  renderRelay(relay) {
    const card = document.getElementById(`card-relay-${relay.id}`);
    if (card) {
      if (relay.state) card.classList.add('is-on');
      else card.classList.remove('is-on');
      const label = card.querySelector('.card-status-label');
      if (label) label.textContent = relay.state ? '● ON' : '○ OFF';
    }

    const tabCard = document.getElementById(`card-relay-tab-${relay.id}`);
    if (tabCard) {
      if (relay.state) tabCard.classList.add('is-on');
      else tabCard.classList.remove('is-on');
      const label = tabCard.querySelector('.card-status-label');
      if (label) label.textContent = relay.state ? '● ON' : '○ OFF';
    }
  }

  renderRgb() {
    const rgb = this.state.rgb;
    if (this.rgbBrightnessSlider) {
      this.rgbBrightnessSlider.value = rgb.brightness;
      this.rgbBrightnessVal.textContent = `${Math.round((rgb.brightness / 255) * 100)}%`;
    }
    if (this.rgbSpeedSlider) {
      this.rgbSpeedSlider.value = rgb.speed;
      this.rgbSpeedVal.textContent = `${rgb.speed}`;
    }
    if (this.rgbIntensitySlider) {
      this.rgbIntensitySlider.value = rgb.intensity;
      this.rgbIntensityVal.textContent = `${rgb.intensity}`;
    }
    if (this.colorCenterPreview) {
      this.colorCenterPreview.style.backgroundColor = `rgb(${rgb.color.r}, ${rgb.color.g}, ${rgb.color.b})`;
    }

    // Update Hero Card on Dashboard
    const heroCard = document.querySelector('.rgb-hero-card');
    if (heroCard) {
      if (rgb.power) heroCard.classList.add('is-on');
      else heroCard.classList.remove('is-on');
      const hex = this.rgbToHex(rgb.color.r, rgb.color.g, rgb.color.b);
      const pill = heroCard.querySelector('.rgb-preview-pill');
      if (pill) {
        pill.style.background = hex;
        pill.style.boxShadow = `0 0 14px ${hex}88`;
      }
      const statVals = heroCard.querySelectorAll('.stat-val');
      if (statVals.length >= 3) {
        statVals[0].textContent = rgb.effect;
        statVals[1].textContent = `${Math.round((rgb.brightness / 255) * 100)}%`;
        statVals[2].textContent = rgb.power ? 'ON' : 'OFF';
      }
      const statusLabel = heroCard.querySelector('.card-status-label');
      if (statusLabel) {
        statusLabel.textContent = rgb.power ? '● ACTIVE' : '○ OFF';
        statusLabel.style.color = rgb.power ? 'var(--rgb-cyan)' : 'var(--text-secondary)';
      }
      const thumb = heroCard.querySelector('.switch-thumb');
      if (thumb) {
        if (rgb.power) {
          thumb.style.left = '25px';
          thumb.style.background = 'var(--rgb-cyan)';
          thumb.style.boxShadow = '0 0 10px var(--rgb-cyan)';
        } else {
          thumb.style.left = '3px';
          thumb.style.background = '#2a2a30';
          thumb.style.boxShadow = 'var(--shadow-extruded-sm)';
        }
      }
    }

    // Effect chips selection
    document.querySelectorAll('.effect-chip').forEach(chip => {
      if (chip.getAttribute('data-effect') === rgb.effect) {
        chip.classList.add('active');
      } else {
        chip.classList.remove('active');
      }
    });
  }

  renderRgbModalControls() {
    this.renderRgb();
  }

  updateSvgRelays() {
    this.state.relays.forEach(relay => {
      const segGroup = document.getElementById(`group-segment-${relay.id}`);
      if (segGroup) {
        if (relay.state) {
          segGroup.classList.add('active');
        } else {
          segGroup.classList.remove('active');
        }
      }
    });
  }

  updateSvgRgb() {
    const segGroup = document.getElementById('group-segment-3');
    const rgb = this.state.rgb;
    if (!segGroup) return;

    const rgbPath = document.getElementById('segment-3');
    const rgbGlow = segGroup.querySelector('.segment-glow-3');
    const hexColor = this.rgbToHex(rgb.color.r, rgb.color.g, rgb.color.b);

    if (rgb.power) {
      segGroup.classList.add('active');
      if (rgb.effect === 'static') {
        if (rgbPath) rgbPath.style.stroke = hexColor;
        if (rgbGlow) {
          rgbGlow.style.stroke = hexColor;
          rgbGlow.style.opacity = (rgb.brightness / 255).toString();
        }
      } else {
        // Animated gradient / rainbow / aurora effect representation
        if (rgbPath) rgbPath.style.stroke = 'url(#dynamicRgbGradient)';
        if (rgbGlow) {
          rgbGlow.style.stroke = 'url(#dynamicRgbGradient)';
          rgbGlow.style.opacity = (rgb.brightness / 255).toString();
        }
      }
    } else {
      segGroup.classList.remove('active');
      if (rgbGlow) rgbGlow.style.opacity = '0';
      if (rgbPath) rgbPath.style.stroke = '#333338';
    }
  }

  renderSettingsView() {
    const sys = this.state.system;
    const rgb = this.state.rgb;

    const wifiRssiVal = document.getElementById('settingWifiRssi');
    const deviceIpVal = document.getElementById('settingDeviceIp');
    const uptimeVal = document.getElementById('settingUptime');
    const heapVal = document.getElementById('settingHeap');
    const versionVal = document.getElementById('settingVersion');

    if (wifiRssiVal) wifiRssiVal.textContent = `${sys.rssi} dBm`;
    if (deviceIpVal) deviceIpVal.textContent = sys.ip;
    if (uptimeVal) {
      const mins = Math.floor(sys.uptime / 60);
      const hrs = Math.floor(mins / 60);
      uptimeVal.textContent = `${hrs}h ${mins % 60}m`;
    }
    if (heapVal) heapVal.textContent = `${Math.round(sys.heap / 1024)} KB free`;
    if (versionVal) versionVal.textContent = sys.version;

    // RGB Hardware Config Form
    const chipSelect = document.getElementById('settingRgbChipset');
    const ledCountInput = document.getElementById('settingLedCount');
    const rgbGpioSelect = document.getElementById('settingRgbGpio');
    const colorOrderSelect = document.getElementById('settingColorOrder');
    const maxCurrentInput = document.getElementById('settingMaxCurrent');

    if (chipSelect) chipSelect.value = rgb.chipset;
    if (ledCountInput) ledCountInput.value = rgb.ledCount;
    if (rgbGpioSelect) rgbGpioSelect.value = rgb.gpio;
    if (colorOrderSelect) colorOrderSelect.value = rgb.colorOrder;
    if (maxCurrentInput) maxCurrentInput.value = rgb.maxCurrent_mA;
  }

  saveRgbHardwareConfig() {
    const chipSelect = document.getElementById('settingRgbChipset');
    const ledCountInput = document.getElementById('settingLedCount');
    const rgbGpioSelect = document.getElementById('settingRgbGpio');
    const colorOrderSelect = document.getElementById('settingColorOrder');
    const maxCurrentInput = document.getElementById('settingMaxCurrent');

    this.state.rgb.chipset = chipSelect ? chipSelect.value : 'WS2812B';
    this.state.rgb.ledCount = ledCountInput ? parseInt(ledCountInput.value) : 144;
    this.state.rgb.gpio = rgbGpioSelect ? parseInt(rgbGpioSelect.value) : 48;
    this.state.rgb.colorOrder = colorOrderSelect ? colorOrderSelect.value : 'GRB';
    this.state.rgb.maxCurrent_mA = maxCurrentInput ? parseInt(maxCurrentInput.value) : 5000;

    this.sendWsMessage({
      action: 'updateRgbHardwareConfig',
      chipset: this.state.rgb.chipset,
      ledCount: this.state.rgb.ledCount,
      gpio: this.state.rgb.gpio,
      colorOrder: this.state.rgb.colorOrder,
      maxCurrent_mA: this.state.rgb.maxCurrent_mA
    });

    this.showToast('RGB Hardware Configuration Saved');
  }

  restartEsp32() {
    if (confirm('Reboot ESP32-S3 lighting controller?')) {
      this.sendWsMessage({ action: 'restartEsp' });
      this.showToast('Restarting ESP32-S3...');
      setTimeout(() => {
        window.location.reload();
      }, 5000);
    }
  }

  closeModals() {
    document.querySelectorAll('.modal-overlay').forEach(modal => {
      modal.classList.remove('open');
    });
    this.currentEditingRelayId = null;
  }

  showToast(message) {
    let container = document.querySelector('.toast-container');
    if (!container) {
      container = document.createElement('div');
      container.className = 'toast-container';
      document.body.appendChild(container);
    }

    const toast = document.createElement('div');
    toast.className = 'toast';
    toast.textContent = message;
    container.appendChild(toast);

    setTimeout(() => {
      if (toast.parentNode) toast.parentNode.removeChild(toast);
    }, 3000);
  }

  /* ==========================================================================
     COLOR UTILITY FUNCTIONS
     ========================================================================== */
  rgbToHex(r, g, b) {
    return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
  }

  hexToRgb(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : null;
  }
}

// Global instance initialization on DOM loaded
document.addEventListener('DOMContentLoaded', () => {
  window.app = new RoomLightsApp();
});
