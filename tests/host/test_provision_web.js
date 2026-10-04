/* No browser dependencies: execute the embedded page script against its UI/API contract. */
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');
const {TextEncoder} = require('util');
const html = fs.readFileSync(path.join(__dirname, '../../main/network/provision.html'), 'utf8');
const script = html.split('<script>')[1].split('</script>')[0];
const elements = new Map();
const element = id => {
    if (!elements.has(id)) elements.set(id, {
        value: '', checked: false, disabled: false, hidden: false, textContent: '', options: [],
        replaceChildren(...children) { this.options = children; },
        add(option) { this.options.push(option); }
    });
    return elements.get(id);
};
let config = {ssid: '', api_url: 'http://host/api/account', api_username: '',
    has_wifi_password: false, has_api_password: false, token: 'session-token'};
let status = {phase: 1, saved: false, connected: false, api_ok: false, message: 'Waiting'};
let failSubmit = false, payload, posts = 0, pendingStatus, abortScan = false;
const timers = [];
const context = vm.createContext({
    document: {getElementById: element}, TextEncoder,
    Option: function(text, value) {this.text = text; this.value = value;},
    AbortController,
    setTimeout: (fn, ms) => { const timer={fn, ms, active:true};timers.push(timer);return timer; },
    clearTimeout: timer => {timer.active=false;},
    fetch: async (url, options) => {
        let data, ok = true;
        if (url === '/api/config' && options.method === 'GET') data = config;
        else if (url === '/api/status') {
            if(pendingStatus){const older=status;await pendingStatus;data=older;}else data=status;
        }
        else {
            assert.strictEqual(options.headers['X-Codeck-Token'], 'session-token');
            if (url === '/api/scan' && abortScan) {
                await new Promise((resolve, reject) => options.signal.addEventListener('abort', () => {
                    const error=new Error('Aborted');error.name='AbortError';reject(error);
                }));
            }
            if (url === '/api/scan') data = [
                {ssid: '<Wi-Fi>&', rssi: -55, secure: true},
                {ssid: '<Wi-Fi>&', rssi: -60, secure: true}
            ];
            else if (url === '/api/config') {
                posts++;
                payload = JSON.parse(options.body);
                if (failSubmit) {ok = false; data = {error: 'Busy'};}
                else {status = {phase: 2, saved: false, message: 'Connecting'}; data = {};}
            } else if (url === '/api/finish') data = {};
            else throw Error('Unexpected endpoint');
        }
        return {ok, json: async () => data};
    }
});
(async () => {
    await vm.runInContext(script, context);
    assert.strictEqual(element('api_url').value, config.api_url);
    assert(element('keep_wifi_row').hidden && element('keep_api_row').hidden);
    await element('scan').onclick();
    assert.strictEqual(element('networks').options.length, 2);
    assert(element('networks').options[1].text.startsWith('<Wi-Fi>&'));
    element('networks').value = '<Wi-Fi>&'; element('networks').onchange();
    assert.strictEqual(element('ssid').value, '<Wi-Fi>&');
    assert(!element('keep_wifi_password').checked);
    await vm.runInContext('poll()', context);
    element('wifi_password').value = 'wifi-secret';
    element('api_username').value = 'user'; element('api_password').value = 'api-secret';
    let releaseStatus;
    pendingStatus = new Promise(resolve => {releaseStatus=resolve;});
    const olderPoll=vm.runInContext('poll()', context);
    await element('form').onsubmit({preventDefault() {}});
    releaseStatus();pendingStatus=undefined;await olderPoll;
    assert(element('ssid').disabled && element('api_password').disabled);
    assert.strictEqual(payload.ssid, '<Wi-Fi>&');
    assert.strictEqual(payload.wifi_password, 'wifi-secret');
    assert(!payload.keep_wifi_password && !payload.keep_api_password);
    await vm.runInContext('poll()', context);
    assert(element('submit').disabled && element('finish').hidden);
    assert(element('ssid').disabled && element('api_url').disabled && element('keep_api_password').disabled);
    config = {...config, ssid: '<Wi-Fi>&', api_username: 'user', has_wifi_password: true, has_api_password: true};
    status = {phase: 4, saved: true, connected: true, api_ok: false, message: 'API HTTP 401'};
    await vm.runInContext('poll()', context);
    assert(!element('submit').disabled && !element('finish').hidden);
    assert(!element('ssid').disabled && !element('api_url').disabled);
    assert(element('status').textContent.includes('401'));
    assert.strictEqual(element('wifi_password').value, '');
    assert.strictEqual(element('api_password').value, '');
    assert(element('keep_wifi_password').checked && element('keep_api_password').checked);
    failSubmit = true;
    await element('form').onsubmit({preventDefault() {}});
    assert(payload.keep_wifi_password && payload.keep_api_password);
    assert.strictEqual(element('error').textContent, 'Busy');
    assert(!element('submit').disabled);
    element('ssid').value = 'another'; element('ssid').oninput();
    assert(!element('keep_wifi_password').checked && !element('wifi_password').disabled);
    const before = posts;
    element('ssid').value = '网络'.repeat(6);
    await element('form').onsubmit({preventDefault() {}});
    assert.strictEqual(posts, before); assert(element('error').textContent.length > 0);
    element('ssid').value = 'another';
    abortScan=true;
    const scanTimeout=element('scan').onclick();
    assert(element('ssid').disabled);
    const timeout=timers.filter(timer => timer.active && timer.ms===10000).pop();
    assert(timeout);timeout.fn();await scanTimeout;abortScan=false;
    assert(!element('submit').disabled && !element('ssid').disabled);
    assert(element('error').textContent.includes('超时'));
    await element('finish').onclick();
    assert(element('finish').hidden && element('status').textContent.length > 0);
    assert(timers.length > 0);
    console.log('Embedded webpage scan, form submission, password retention, busy/error states and completion passed');
})().catch(error => {console.error(error); process.exitCode = 1;});
