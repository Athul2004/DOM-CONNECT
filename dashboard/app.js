document.addEventListener('DOMContentLoaded', () => {
    // UI Elements
    const navItems = document.querySelectorAll('.nav-item');
    const viewSections = document.querySelectorAll('.view-section');
    const viewTitle = document.getElementById('current-view-title');
    const mqttStatus = document.getElementById('mqtt-status');
    const statusDot = document.querySelector('.dot');
    
    // Stats
    const totalDevicesEl = document.getElementById('total-devices');
    const totalMessagesEl = document.getElementById('total-messages');
    const activityFeed = document.getElementById('activity-feed');
    const devicesGrid = document.getElementById('devices-grid');
    
    // Modal
    const btnPermitJoin = document.getElementById('btn-permit-join');
    const btnCancelJoin = document.getElementById('btn-cancel-join');
    const joinModal = document.getElementById('join-modal');
    const countdownEl = document.getElementById('countdown');
    
    // State
    let devices = {};
    let messageCount = 0;
    
    // Navigation Logic
    navItems.forEach(item => {
        item.addEventListener('click', (e) => {
            e.preventDefault();
            const targetId = item.getAttribute('data-target');
            
            navItems.forEach(nav => nav.classList.remove('active'));
            item.classList.add('active');
            
            viewSections.forEach(section => {
                section.classList.remove('active');
                section.classList.add('hidden');
            });
            
            document.getElementById(targetId).classList.remove('hidden');
            document.getElementById(targetId).classList.add('active');
            
            viewTitle.textContent = item.textContent.trim();
        });
    });

    // Permit Join Modal Logic
    let joinTimer;
    btnPermitJoin.addEventListener('click', () => {
        // Here we would ideally publish a message to the MQTT broker to permit join
        // mqttClient.publish('zigbee2mqtt/bridge/request/permit_join', JSON.stringify({value: true}));
        
        joinModal.classList.remove('hidden');
        let count = 60;
        countdownEl.textContent = count;
        
        joinTimer = setInterval(() => {
            count--;
            countdownEl.textContent = count;
            if (count <= 0) {
                clearInterval(joinTimer);
                joinModal.classList.add('hidden');
            }
        }, 1000);
    });

    btnCancelJoin.addEventListener('click', () => {
        clearInterval(joinTimer);
        joinModal.classList.add('hidden');
    });

    // MQTT Logic
    const mqttUrl = 'ws://192.168.0.180:8080';
    let client = null;
    
    function connectMqtt() {
        try {
            client = mqtt.connect(mqttUrl);
            
            client.on('connect', () => {
                mqttStatus.textContent = 'Broker Connected';
                statusDot.classList.remove('offline');
                statusDot.classList.add('pulse');
                
                client.subscribe('zigbee2mqtt/#', (err) => {
                    if (!err) {
                        logActivity('System', 'Subscribed to DOM Connect Gateway topic');
                    }
                });
            });
            
            client.on('message', (topic, message) => {
                messageCount++;
                totalMessagesEl.textContent = messageCount;
                
                try {
                    const payload = message.toString();
                    // Basic parsing heuristics
                    if (topic.includes('/set')) return; // Ignore outgoing commands
                    
                    if (topic === 'zigbee2mqtt/bridge/log') {
                        const logData = JSON.parse(payload);
                        logActivity(logData.component || 'System', logData.message || payload);
                        return;
                    }
                    
                    if (topic === 'zigbee2mqtt/bridge/devices') {
                        const dbDevices = JSON.parse(payload);
                        for (const [node_id, devInfo] of Object.entries(dbDevices)) {
                            let storedName = localStorage.getItem('name_' + node_id) || devInfo.model || `Device ${node_id}`;
                            let storedPhoto = localStorage.getItem('photo_' + node_id) || null;
                            devices[node_id] = { 
                                id: node_id, 
                                name: storedName, 
                                photo: storedPhoto, 
                                state: 'OFF', 
                                last_seen: new Date() 
                            };
                        }
                        updateDevicesGrid();
                        totalDevicesEl.textContent = Object.keys(devices).length;
                        return;
                    }
                    
                    const topicParts = topic.split('/');
                    const node_id = topicParts[1];
                    
                    if (!devices[node_id] && node_id !== 'bridge' && node_id.startsWith('0x')) {
                        let storedName = localStorage.getItem('name_' + node_id) || `Device ${node_id}`;
                        let storedPhoto = localStorage.getItem('photo_' + node_id) || null;
                        devices[node_id] = { id: node_id, name: storedName, photo: storedPhoto, state: 'OFF', last_seen: new Date() };
                        updateDevicesGrid();
                        totalDevicesEl.textContent = Object.keys(devices).length;
                        logActivity('Device Joined', `Node ${node_id} discovered`);
                    }
                    
                    // Parse JSON if possible
                    if (payload.startsWith('{')) {
                        const json = JSON.parse(payload);
                        if (json.zcl && json.zcl.attributes) {
                            if (json.zcl.attributes.attr_0x0000 !== undefined) {
                                devices[node_id].state = json.zcl.attributes.attr_0x0000 === 1 ? 'ON' : 'OFF';
                                updateDeviceCard(node_id);
                                logActivity('Device State', `${node_id} turned ${devices[node_id].state}`);
                            }
                            
                            // Check for Model ID during Interview
                            if (json.zcl.attributes.attr_0x0005) {
                                const modelId = json.zcl.attributes.attr_0x0005;
                                devices[node_id].name = modelId;
                                localStorage.setItem('name_' + node_id, modelId);
                                
                                // Mini-Database mappings
                                if (modelId.toLowerCase().includes("lumi") || modelId.toLowerCase().includes("sensor")) {
                                    devices[node_id].photo = "https://cdn.shopify.com/s/files/1/0066/8149/3559/products/Sonoff-SNZB-03-Zigbee-Motion-Sensor.png";
                                    localStorage.setItem('photo_' + node_id, devices[node_id].photo);
                                } else {
                                    devices[node_id].photo = "https://m.media-amazon.com/images/I/51r26kYp7nL._AC_SL1500_.jpg";
                                    localStorage.setItem('photo_' + node_id, devices[node_id].photo);
                                }
                                
                                updateDeviceCard(node_id);
                                logActivity('Device Interview', `Identified as ${modelId}`);
                            }
                        }
                    }
                } catch (e) {
                    console.error("Failed to parse message", e);
                }
            });
            
            client.on('error', (err) => {
                mqttStatus.textContent = 'Connection Error';
                statusDot.classList.remove('pulse');
                statusDot.classList.add('offline');
                console.error('MQTT Error:', err);
            });
            
            client.on('close', () => {
                mqttStatus.textContent = 'Offline';
                statusDot.classList.remove('pulse');
                statusDot.classList.add('offline');
            });
            
        } catch(e) {
            console.error("MQTT initialization failed", e);
        }
    }
    
    function logActivity(type, text) {
        const item = document.createElement('div');
        item.className = 'activity-item';
        
        const time = new Date().toLocaleTimeString([], {hour: '2-digit', minute:'2-digit', second:'2-digit'});
        
        item.innerHTML = `
            <span><strong>${type}:</strong> ${text}</span>
            <span style="color: var(--text-muted); font-size: 0.8rem;">${time}</span>
        `;
        
        activityFeed.prepend(item);
        if (activityFeed.children.length > 20) {
            activityFeed.removeChild(activityFeed.lastChild);
        }
    }
    
    const devicesTableBody = document.getElementById('devices-table-body');
    
    function updateDevicesGrid() {
        const count = Object.keys(devices).length;
        if (count > 0) {
            const emptyState = devicesTableBody.querySelector('.empty-state');
            if (emptyState) emptyState.remove();
        }
        
        Object.values(devices).forEach(dev => {
            let row = document.getElementById(`row-${dev.id}`);
            if (!row) {
                row = document.createElement('tr');
                row.id = `row-${dev.id}`;
                row.style.borderBottom = '1px solid rgba(255,255,255,0.05)';
                devicesTableBody.appendChild(row);
            }
            
            const isOn = dev.state === 'ON';
            const modelStr = dev.name !== `Device ${dev.id}` ? dev.name : "Unknown Model";
            
            row.innerHTML = `
                <td style="padding: 12px; cursor: pointer;" onclick="changeDevicePhoto('${dev.id}')" title="Change Photo">
                    ${dev.photo ? `<img src="${dev.photo}" style="width: 40px; height: 40px; object-fit: contain;">` : `<i class="fa-solid fa-lightbulb" style="font-size: 1.5em; color: var(--text-muted);"></i>`}
                </td>
                <td style="padding: 12px; cursor: pointer; color: var(--primary);" onclick="renameDevice('${dev.id}')" title="Rename">
                    ${dev.name} <i class="fa-solid fa-pen" style="font-size: 0.7em; opacity: 0.5;"></i>
                </td>
                <td style="padding: 12px; font-family: monospace; color: var(--text-muted);">${dev.id}</td>
                <td style="padding: 12px;">DOM Connect Generic</td>
                <td style="padding: 12px; font-weight: bold;">${modelStr}</td>
                <td style="padding: 12px;">
                    <span style="padding: 4px 8px; border-radius: 4px; font-size: 0.85em; font-weight: bold; background: ${isOn ? 'var(--success)' : 'rgba(255,255,255,0.1)'}; color: ${isOn ? '#fff' : 'var(--text-muted)'};">
                        ${dev.state}
                    </span>
                </td>
                <td style="padding: 12px; text-align: right;">
                    <button class="btn btn-primary" onclick="toggleDevice('${dev.id}')" style="padding: 6px 12px; font-size: 0.9em;">
                        Toggle
                    </button>
                </td>
            `;
        });
    }
    
    function updateDeviceCard(node_id) {
        updateDevicesGrid(); // Re-render grid to update state
    }
    
    window.renameDevice = function(node_id) {
        const newName = prompt("Enter a new name for this device:", devices[node_id].name);
        if (newName && newName.trim() !== '') {
            devices[node_id].name = newName.trim();
            localStorage.setItem('name_' + node_id, devices[node_id].name);
            updateDeviceCard(node_id);
        }
    };

    window.changeDevicePhoto = function(node_id) {
        const newPhotoUrl = prompt("Paste an image URL for this device (or leave blank to remove):", devices[node_id].photo || "");
        if (newPhotoUrl !== null) {
            devices[node_id].photo = newPhotoUrl.trim() === "" ? null : newPhotoUrl.trim();
            if (devices[node_id].photo) {
                localStorage.setItem('photo_' + node_id, devices[node_id].photo);
            } else {
                localStorage.removeItem('photo_' + node_id);
            }
            updateDeviceCard(node_id);
        }
    };

    // Global function for onclick
    window.toggleDevice = function(node_id) {
        if (!client || !client.connected) {
            alert('Cannot send command. MQTT is disconnected.');
            return;
        }
        const topic = `zigbee2mqtt/${node_id}/set`;
        const payload = JSON.stringify({ state: "TOGGLE" });
        client.publish(topic, payload);
        logActivity('Command Sent', `Toggling ${node_id}`);
    };

    // Initialize
    connectMqtt();
    logActivity('System', 'Dashboard Initialized');
});
