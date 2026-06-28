#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LiquidCrystal.h>
#include "secrets.h"   // create this from secrets.h.example — keep it out of git

// ==============================
// WIFI CREDENTIALS (from secrets.h)
// ==============================
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
bool intrusionLatched = false;

// ==============================
// SERVER + WEBSOCKET
// ==============================
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ==============================
// LCD PIN CONNECTIONS
// ==============================
LiquidCrystal lcd(21, 22, 19, 18, 5, 17);

// ==============================
// ADC CONFIGURATION
// ==============================
#define ADC_PIN 34

#define NO_LIGHT_MAX     500
#define DIGGING_SPIKE_THRESHOLD 300
#define LOW_LIGHT_MAX    1500
#define NORMAL_LIGHT_MAX 3000

// ==============================
// VIBRATION CONFIGURATION
// ==============================
#define MIN_DELTA_ADC        15
#define TIME_WINDOW_MS       700
#define INTRUSION_SUM_THRESHOLD 2000
#define WALKING_SUM_THRESHOLD   420

// ==============================
// BUZZER CONFIG
// ==============================
#define BUZZER_PIN 23

int prevADC = 0;
long vibrationSum = 0;
unsigned long windowStart = 0;
bool intrusionDetected = false;
bool walkingDetected = false;
bool diggingDetected = false;
unsigned long diggingTimer = 0;
unsigned long walkingTimer = 0;

// ==============================
// HTML PAGE
// ==============================
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Intrusion Monitor - TACTICAL</title>

<style>
*{
    margin:0;
    padding:0;
    box-sizing:border-box;
}

body{
    font-family:'Courier New',monospace;
    display:flex;
    justify-content:center;
    align-items:center;
    min-height:100vh;
    background:#0d1117;
    background-image:
        repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(20,40,30,0.03) 2px,rgba(20,40,30,0.03) 4px),
        repeating-linear-gradient(90deg,transparent,transparent 2px,rgba(20,40,30,0.03) 2px,rgba(20,40,30,0.03) 4px);
    padding:20px;
    color:#5ff55f;
}

.container{
    width:100%;
    max-width:600px;
    padding:30px;
    border-radius:4px;
    background:#161b22;
    border:2px solid #2d4a2e;
    box-shadow:
        0 0 20px rgba(47,79,79,0.3),
        inset 0 0 50px rgba(0,0,0,0.5);
    position:relative;
}

.container::before{
    content:'';
    position:absolute;
    top:0;
    left:0;
    right:0;
    height:3px;
    background:linear-gradient(90deg,transparent,#2ea043,transparent);
    animation:scanline 3s linear infinite;
}

@keyframes scanline{
    0%{ transform:translateX(-100%); }
    100%{ transform:translateX(100%); }
}

.header{
    border-bottom:2px solid #2d4a2e;
    padding-bottom:20px;
    margin-bottom:25px;
    position:relative;
}

h1{
    font-size:24px;
    letter-spacing:3px;
    color:#5ff55f;
    text-transform:uppercase;
    font-weight:700;
    text-shadow:0 0 10px rgba(95,245,95,0.5);
    margin-bottom:5px;
}

.subtitle{
    font-size:11px;
    color:#6e7681;
    letter-spacing:2px;
    text-transform:uppercase;
}

.timestamp{
    position:absolute;
    top:0;
    right:0;
    font-size:11px;
    color:#6e7681;
    font-family:monospace;
}

.connection-status{
    padding:8px 16px;
    border:1px solid;
    margin-bottom:20px;
    font-weight:600;
    font-size:12px;
    text-align:center;
    letter-spacing:1px;
    text-transform:uppercase;
    position:relative;
    font-family:'Courier New',monospace;
}

.connection-status::before{
    content:'●';
    margin-right:8px;
    font-size:14px;
    animation:blink 1.5s infinite;
}

.connected{
    background:rgba(46,160,67,0.1);
    border-color:#2ea043;
    color:#3fb950;
}

.disconnected{
    background:rgba(248,81,73,0.1);
    border-color:#f85149;
    color:#ff7b72;
}

.connecting{
    background:rgba(187,128,9,0.1);
    border-color:#bb8009;
    color:#d29922;
}

@keyframes blink{
    0%,100%{ opacity:1; }
    50%{ opacity:0.3; }
}

.grid{
    display:grid;
    grid-template-columns:1fr 1fr;
    gap:15px;
    margin-bottom:20px;
}

.data-section{
    padding:20px;
    border:1px solid #2d4a2e;
    background:rgba(13,17,23,0.6);
    position:relative;
    transition:all 0.3s ease;
}

.data-section::before{
    content:'';
    position:absolute;
    top:0;
    left:0;
    width:4px;
    height:100%;
    background:#2ea043;
    opacity:0;
    transition:opacity 0.3s;
}

.data-section:hover{
    border-color:#3fb950;
    background:rgba(13,17,23,0.8);
}

.data-section:hover::before{
    opacity:1;
}

.label{
    font-size:10px;
    color:#6e7681;
    margin-bottom:8px;
    text-transform:uppercase;
    letter-spacing:1.5px;
}

.value{
    font-size:36px;
    font-weight:700;
    color:#5ff55f;
    font-family:'Courier New',monospace;
    text-shadow:0 0 10px rgba(95,245,95,0.3);
    line-height:1;
}

.status-value{
    font-size:18px;
    padding:8px 16px;
    background:rgba(46,160,67,0.15);
    border:1px solid #2ea043;
    color:#3fb950;
    font-weight:600;
    display:inline-block;
    font-family:'Courier New',monospace;
}

.intrusion-section{
    margin:20px 0;
    padding:25px;
    border:2px solid #2d4a2e;
    background:rgba(13,17,23,0.6);
    position:relative;
}

.intrusion-section .label{
    margin-bottom:15px;
}

.intrusion-indicator{
    font-size:42px;
    font-weight:800;
    padding:20px;
    text-align:center;
    letter-spacing:8px;
    position:relative;
    font-family:'Courier New',monospace;
}

.intrusion-safe{
    background:rgba(46,160,67,0.15);
    border:2px solid #2ea043;
    color:#3fb950;
    text-shadow:0 0 15px rgba(63,185,80,0.6);
}

.intrusion-alert{
    background:rgba(248,81,73,0.2);
    border:2px solid #f85149;
    color:#ff7b72;
    text-shadow:0 0 20px rgba(255,123,114,0.8);
    animation:alertFlash 1s infinite,shake 0.5s infinite;
}

.intrusion-alert::before{
    content:'⚠ WARNING ⚠';
    position:absolute;
    top:-12px;
    left:50%;
    transform:translateX(-50%);
    background:#f85149;
    color:#0d1117;
    padding:4px 12px;
    font-size:10px;
    letter-spacing:2px;
    font-weight:700;
}

@keyframes alertFlash{
    0%,100%{ 
        border-color:#f85149;
        box-shadow:0 0 20px rgba(248,81,73,0.6);
    }
    50%{ 
        border-color:#da3633;
        box-shadow:0 0 30px rgba(248,81,73,0.8);
    }
}

@keyframes shake{
    0%,100%{ transform:translateX(0); }
    25%{ transform:translateX(-3px); }
    75%{ transform:translateX(3px); }
}

.chart-container{
    margin:20px 0;
    padding:20px;
    border:1px solid #2d4a2e;
    background:rgba(13,17,23,0.6);
    position:relative;
}

.chart-header{
    display:flex;
    justify-content:space-between;
    align-items:center;
    margin-bottom:15px;
    padding-bottom:10px;
    border-bottom:1px solid #2d4a2e;
}

.chart-title{
    font-size:11px;
    color:#6e7681;
    letter-spacing:1.5px;
    text-transform:uppercase;
}

.chart-info{
    display:flex;
    gap:15px;
    align-items:center;
}

.chart-live{
    font-size:10px;
    color:#3fb950;
    display:flex;
    align-items:center;
}

.chart-live::before{
    content:'●';
    margin-right:5px;
    animation:pulse 1.5s infinite;
}

.chart-live.paused{
    color:#d29922;
}

.chart-live.paused::before{
    animation:none;
}

.data-points{
    font-size:10px;
    color:#6e7681;
}

@keyframes pulse{
    0%,100%{ opacity:1; }
    50%{ opacity:0.4; }
}

.chart-wrapper{
    position:relative;
    padding-left:45px;
    padding-bottom:30px;
}

.y-axis{
    position:absolute;
    left:0;
    top:0;
    bottom:30px;
    width:40px;
    display:flex;
    flex-direction:column;
    justify-content:space-between;
    font-size:9px;
    color:#6e7681;
    text-align:right;
    padding-right:8px;
}

.y-axis-label{
    position:absolute;
    left:-5px;
    top:50%;
    transform:rotate(-90deg) translateX(-50%);
    transform-origin:left center;
    font-size:10px;
    color:#3fb950;
    letter-spacing:1px;
    text-transform:uppercase;
    white-space:nowrap;
}

.x-axis{
    position:absolute;
    bottom:0;
    left:45px;
    right:0;
    height:25px;
    display:flex;
    justify-content:space-between;
    align-items:flex-start;
    font-size:9px;
    color:#6e7681;
    padding-top:5px;
}

.x-axis-label{
    position:absolute;
    bottom:-20px;
    left:50%;
    transform:translateX(-50%);
    font-size:10px;
    color:#3fb950;
    letter-spacing:1px;
    text-transform:uppercase;
}

canvas{
    width:100%;
    display:block;
    background:#0d1117;
    border:1px solid #21262d;
}

.chart-controls{
    margin-top:15px;
    display:flex;
    gap:10px;
    align-items:center;
    justify-content:center;
}

.nav-button{
    padding:8px 16px;
    font-size:11px;
    border:1px solid #2d4a2e;
    background:rgba(46,160,67,0.1);
    color:#3fb950;
    cursor:pointer;
    font-weight:600;
    letter-spacing:1px;
    text-transform:uppercase;
    transition:all 0.2s ease;
    font-family:'Courier New',monospace;
}

.nav-button:hover{
    background:rgba(46,160,67,0.2);
    border-color:#3fb950;
}

.nav-button:active{
    transform:scale(0.95);
}

.nav-button:disabled{
    opacity:0.3;
    cursor:not-allowed;
}

.view-mode{
    font-size:10px;
    color:#3fb950;
    padding:8px 12px;
    background:rgba(46,160,67,0.15);
    border:1px solid #2ea043;
    border-radius:2px;
}

.controls{
    margin-top:20px;
    display:grid;
    gap:10px;
}

.alarm-button{
    padding:14px 24px;
    font-size:13px;
    border:2px solid #2ea043;
    background:rgba(46,160,67,0.1);
    color:#3fb950;
    cursor:pointer;
    font-weight:700;
    letter-spacing:2px;
    text-transform:uppercase;
    transition:all 0.3s ease;
    position:relative;
    overflow:hidden;
    font-family:'Courier New',monospace;
}

.alarm-button::before{
    content:'';
    position:absolute;
    top:0;
    left:-100%;
    width:100%;
    height:100%;
    background:linear-gradient(90deg,transparent,rgba(95,245,95,0.2),transparent);
    transition:left 0.5s;
}

.alarm-button:hover{
    background:rgba(46,160,67,0.2);
    border-color:#3fb950;
    box-shadow:0 0 15px rgba(63,185,80,0.3);
}

.alarm-button:hover::before{
    left:100%;
}

.alarm-button:active{
    transform:scale(0.98);
}

.alarm-button.enabled{
    background:rgba(46,160,67,0.25);
    border-color:#3fb950;
    box-shadow:0 0 20px rgba(63,185,80,0.4);
}

.alarm-button.enabled::after{
    content:'✓ ACTIVE';
    position:absolute;
    top:50%;
    right:15px;
    transform:translateY(-50%);
    font-size:10px;
    color:#5ff55f;
}

.footer{
    margin-top:20px;
    padding-top:15px;
    border-top:1px solid #2d4a2e;
    display:flex;
    justify-content:space-between;
    font-size:10px;
    color:#6e7681;
    text-transform:uppercase;
    letter-spacing:1px;
}

@media (max-width:600px){
    .grid{
        grid-template-columns:1fr;
    }
    .container{
        padding:20px;
    }
    h1{
        font-size:20px;
    }
    .value{
        font-size:28px;
    }
    .intrusion-indicator{
        font-size:32px;
        letter-spacing:4px;
    }
    .chart-wrapper{
        padding-left:35px;
    }
    .y-axis{
        width:30px;
    }
    .chart-controls{
        flex-wrap:wrap;
    }
}
</style>
</head>

<body>

<div class="container">

<div class="header">
<h1>█ INTRUSION MONITOR</h1>
<div class="subtitle">TACTICAL SECURITY SYSTEM</div>
<div class="timestamp" id="timestamp">--:--:--</div>
</div>

<div class="connection-status connecting" id="connectionStatus">
CONNECTING TO SERVER
</div>

<div class="grid">
<div class="data-section">
<div class="label">ADC READING</div>
<div class="value" id="adcValue">----</div>
</div>

<div class="data-section">
<div class="label">SYSTEM STATUS</div>
<div class="status-value" id="statusValue">----</div>
</div>
</div>

<div class="intrusion-section">
<div class="label">THREAT LEVEL</div>
<div class="intrusion-indicator intrusion-safe" id="intrusionValue">
SECURE
</div>
</div>

<div class="chart-container">
<div class="chart-header">
<div class="chart-title">SENSOR DATA FEED</div>
<div class="chart-info">
<div class="data-points" id="dataPoints">SAMPLES: 0</div>
<div class="chart-live" id="liveIndicator">LIVE</div>
</div>
</div>
<div class="chart-wrapper">
<div class="y-axis">
<div class="y-axis-label">ADC VALUE</div>
<span>4095</span>
<span>3072</span>
<span>2048</span>
<span>1024</span>
<span>0</span>
</div>
<canvas id="adcChart" width="495" height="180"></canvas>
<div class="x-axis" id="xAxis">
<span>-100s</span>
<span>-75s</span>
<span>-50s</span>
<span>-25s</span>
<span>NOW</span>
<div class="x-axis-label">TIME (SECONDS)</div>
</div>
</div>
<div class="chart-controls">
<button class="nav-button" id="btnStart" onclick="jumpToStart()">⏮ START</button>
<button class="nav-button" id="btnPrev" onclick="scrollPrevious()">◀ PREV</button>
<div class="view-mode" id="viewMode">VIEWING: LIVE</div>
<button class="nav-button" id="btnNext" onclick="scrollNext()">NEXT ▶</button>
<button class="nav-button" id="btnLive" onclick="jumpToLive()">⏭ LIVE</button>
</div>
</div>

<div class="controls">
<button class="alarm-button" id="alarmButton" onclick="enableAlarm()">
🔊 ENABLE ALARM SYSTEM
</button>
</div>

<div class="footer">
<span>SYSTEM V2.3</span>
<span id="uptime">UPTIME: 00:00:00</span>
</div>

</div>

<script>

let ws;
let alarmEnabled=false;
let audioContext;
let startTime=Date.now();
let isAlarmPlaying=false;

const WS_URL='ws://'+window.location.hostname+':81/';

const connectionStatus=document.getElementById('connectionStatus');
const adcValue=document.getElementById('adcValue');
const statusValue=document.getElementById('statusValue');
const intrusionValue=document.getElementById('intrusionValue');
const alarmButton=document.getElementById('alarmButton');
const timestamp=document.getElementById('timestamp');
const uptime=document.getElementById('uptime');
const dataPoints=document.getElementById('dataPoints');
const liveIndicator=document.getElementById('liveIndicator');
const viewMode=document.getElementById('viewMode');

const canvas=document.getElementById("adcChart");
const ctx=canvas.getContext("2d");

// PERSISTENT DATA STORAGE WITH SCROLLING
let adcHistory=[];  // Stores ALL historical data
const maxHistory=1000;  // Keep last 1000 data points
const displayPoints=100;  // Display 100 points at a time
let viewOffset=0;  // 0 = viewing latest (live), positive = viewing older data
let isLiveView=true;  // Track if we're in live view mode

// Update timestamp every second
setInterval(()=>{
    const now=new Date();
    timestamp.textContent=now.toLocaleTimeString('en-US',{hour12:false});
    
    const elapsed=Math.floor((Date.now()-startTime)/1000);
    const hours=Math.floor(elapsed/3600);
    const minutes=Math.floor((elapsed%3600)/60);
    const seconds=elapsed%60;
    uptime.textContent=`UPTIME: ${String(hours).padStart(2,'0')}:${String(minutes).padStart(2,'0')}:${String(seconds).padStart(2,'0')}`;
},1000);

function drawGraph(){
    ctx.clearRect(0,0,canvas.width,canvas.height);
    
    // Draw grid
    ctx.strokeStyle='rgba(45,74,46,0.3)';
    ctx.lineWidth=1;
    for(let i=0;i<=4;i++){
        let y=(canvas.height/4)*i;
        ctx.beginPath();
        ctx.moveTo(0,y);
        ctx.lineTo(canvas.width,y);
        ctx.stroke();
    }
    for(let i=0;i<=10;i++){
        let x=(canvas.width/10)*i;
        ctx.beginPath();
        ctx.moveTo(x,0);
        ctx.lineTo(x,canvas.height);
        ctx.stroke();
    }
    
    // Calculate which data to display based on viewOffset
    const endIndex=adcHistory.length-viewOffset;
    const startIndex=Math.max(0,endIndex-displayPoints);
    const displayData=adcHistory.slice(startIndex,endIndex);
    
    // Draw line
    if(displayData.length>1){
        ctx.beginPath();
        ctx.moveTo(0,canvas.height-displayData[0]);

        for(let i=1;i<displayData.length;i++){
            let x=i*(canvas.width/displayPoints);
            let y=canvas.height-displayData[i];
            ctx.lineTo(x,y);
        }

        ctx.strokeStyle='#2ea043';
        ctx.lineWidth=2;
        ctx.shadowBlur=8;
        ctx.shadowColor='#2ea043';
        ctx.stroke();
        ctx.shadowBlur=0;
        
        // Draw dots on line
        for(let i=0;i<displayData.length;i+=5){
            let x=i*(canvas.width/displayPoints);
            let y=canvas.height-displayData[i];
            ctx.beginPath();
            ctx.arc(x,y,2,0,Math.PI*2);
            ctx.fillStyle='#3fb950';
            ctx.fill();
        }
    }
    
    // Update UI
    dataPoints.textContent=`SAMPLES: ${adcHistory.length}`;
    updateViewModeDisplay();
    updateNavigationButtons();
}

function updateViewModeDisplay(){
    if(isLiveView){
        viewMode.textContent='VIEWING: LIVE';
        liveIndicator.classList.remove('paused');
    }else{
        const samplesFromEnd=viewOffset;
        const timeAgo=Math.floor(samplesFromEnd*1);  // Assuming ~1 sample per second
        viewMode.textContent=`VIEWING: -${timeAgo}s AGO`;
        liveIndicator.textContent='PAUSED';
        liveIndicator.classList.add('paused');
    }
}

function updateNavigationButtons(){
    const btnStart=document.getElementById('btnStart');
    const btnPrev=document.getElementById('btnPrev');
    const btnNext=document.getElementById('btnNext');
    const btnLive=document.getElementById('btnLive');
    
    // Disable start/prev if at the beginning
    const canGoBack=viewOffset<adcHistory.length-displayPoints;
    btnStart.disabled=!canGoBack;
    btnPrev.disabled=!canGoBack;
    
    // Disable next/live if already at live view
    btnNext.disabled=viewOffset===0;
    btnLive.disabled=viewOffset===0;
}

function jumpToStart(){
    if(adcHistory.length>displayPoints){
        viewOffset=adcHistory.length-displayPoints;
        isLiveView=false;
        drawGraph();
    }
}

function scrollPrevious(){
    if(viewOffset<adcHistory.length-displayPoints){
        viewOffset+=10;  // Jump back 10 samples
        if(viewOffset>adcHistory.length-displayPoints){
            viewOffset=adcHistory.length-displayPoints;
        }
        isLiveView=false;
        drawGraph();
    }
}

function scrollNext(){
    if(viewOffset>0){
        viewOffset-=10;  // Jump forward 10 samples
        if(viewOffset<0){
            viewOffset=0;
        }
        if(viewOffset===0){
            isLiveView=true;
            liveIndicator.textContent='LIVE';
        }
        drawGraph();
    }
}

function jumpToLive(){
    viewOffset=0;
    isLiveView=true;
    liveIndicator.textContent='LIVE';
    drawGraph();
}

function connectWebSocket(){
    ws=new WebSocket(WS_URL);

    ws.onopen=()=>{
        connectionStatus.textContent="CONNECTION ESTABLISHED";
        connectionStatus.className="connection-status connected";
    };

    ws.onmessage=(event)=>{
        const data=JSON.parse(event.data);

        adcValue.textContent=data.adc;

        // Scale ADC value to fit canvas height (0-4095 -> 0-180px)
        let scaled=(data.adc/4095)*canvas.height;
        
        // Add to persistent history
        adcHistory.push(scaled);
        
        // Keep only maxHistory points to prevent memory issues
        if(adcHistory.length>maxHistory){
            adcHistory.shift();
            // Adjust viewOffset if needed
            if(viewOffset>0){
                viewOffset--;
            }
        }
        
        // Only update graph if in live view
        if(isLiveView){
            drawGraph();
        }

        statusValue.textContent=data.status;

        if(data.intrusion===1){
            intrusionValue.textContent="BREACH";
            intrusionValue.className="intrusion-indicator intrusion-alert";
            if(alarmEnabled && !isAlarmPlaying) playJamesBondAlarm();
        }else{
            intrusionValue.textContent="SECURE";
            intrusionValue.className="intrusion-indicator intrusion-safe";
            stopJamesBondAlarm();
        }
    };

    ws.onclose=()=>{
        connectionStatus.textContent="CONNECTION LOST";
        connectionStatus.className="connection-status disconnected";
        setTimeout(connectWebSocket,3000);
    };

    ws.onerror=()=>{
        connectionStatus.textContent="CONNECTION ERROR";
        connectionStatus.className="connection-status disconnected";
    };
}

function enableAlarm(){
    if(!alarmEnabled){
        audioContext=new(window.AudioContext||window.webkitAudioContext)();
        alarmEnabled=true;
        alarmButton.classList.add("enabled");
    }
}

// Police Siren-style alarm (wee-ooo-wee-ooo)
let sirenOscillator=null;
let sirenGain=null;

function playJamesBondAlarm(){
    if(!audioContext||isAlarmPlaying) return;
    isAlarmPlaying=true;
    
    sirenOscillator=audioContext.createOscillator();
    sirenGain=audioContext.createGain();
    
    sirenOscillator.connect(sirenGain);
    sirenGain.connect(audioContext.destination);
    
    sirenOscillator.type='sine';  // Sine wave for classic siren sound
    sirenGain.gain.value=0.5;  // 50% volume
    
    sirenOscillator.start();
    
    // Police siren oscillates between 600Hz and 800Hz
    let isHigh=false;
    sirenOscillator.frequency.value=600;
    
    function oscillateSiren(){
        if(!isAlarmPlaying) return;
        
        if(isHigh){
            // Sweep down from 800Hz to 600Hz
            sirenOscillator.frequency.exponentialRampToValueAtTime(600,audioContext.currentTime+0.5);
        }else{
            // Sweep up from 600Hz to 800Hz
            sirenOscillator.frequency.exponentialRampToValueAtTime(800,audioContext.currentTime+0.5);
        }
        
        isHigh=!isHigh;
        
        if(isAlarmPlaying){
            setTimeout(oscillateSiren,500);  // Change every 500ms
        }
    }
    
    oscillateSiren();
}

function stopJamesBondAlarm(){
    isAlarmPlaying=false;
    if(sirenOscillator){
        sirenOscillator.stop();
        sirenOscillator=null;
        sirenGain=null;
    }
}

connectWebSocket();

</script>

</body>
</html>
)rawliteral";

// ==============================
// SEND DATA TO PHONE
// ==============================
void sendWS(int adc, const char* status, bool intrusion) {
  String msg = "{";
  msg += "\"adc\":" + String(adc) + ",";
  msg += "\"status\":\"" + String(status) + "\",";
  msg += "\"intrusion\":" + String(intrusion ? 1 : 0);
  msg += "}";
  webSocket.broadcastTXT(msg);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.println("WebSocket client connected");
  }
}

// ==============================
// SETUP
// ==============================
void setup() {
  Serial.begin(115200);

  lcd.begin(16, 2);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  prevADC = analogRead(ADC_PIN);
  windowStart = millis();

  pinMode(BUZZER_PIN, OUTPUT);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.setCursor(0,1);
    lcd.print("Please wait...   ");
  }

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi Connected");
  delay(1500);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("IP Address:");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());

  delay(5000);
  lcd.clear();

  Serial.println("\nWiFi Connected");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send(200, "text/html", webpage);
  });
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

// ==============================
// LOOP
// ==============================
void loop() {
  server.handleClient();
  webSocket.loop();

  int adcValue = 4095 - analogRead(ADC_PIN);

  int deltaADC = abs(adcValue - prevADC);
  prevADC = adcValue;

  // DIGGING = very large sudden spike
  if (deltaADC >= DIGGING_SPIKE_THRESHOLD) {
    diggingDetected = true;
    diggingTimer = millis();
  }

  // Walking resets if vibration stops for 2 seconds
  if (walkingDetected && (millis() - walkingTimer > 2000)) {
    walkingDetected = false;
  }

  // Reset digging after 2 seconds
  if (diggingDetected && (millis() - diggingTimer > 2000)) {
    diggingDetected = false;
  }

  unsigned long now = millis();
  const char* status = "NORMAL";

  lcd.setCursor(0, 0);
  lcd.print("ADC: ");
  lcd.print(adcValue);
  lcd.print("    ");

  lcd.setCursor(0, 1);

  if (adcValue <= NO_LIGHT_MAX) {
    status = "NO LIGHT";
    lcd.print("NO LIGHT / FAULT ");
    intrusionDetected = false;
  }
  else if (adcValue <= LOW_LIGHT_MAX) {
    status = "LOW LIGHT";
    lcd.print("LOW LIGHT        ");
    intrusionDetected = false;
  }
  else if (adcValue > NORMAL_LIGHT_MAX) {
    status = "HIGH SATURATION";
    lcd.print("HIGH / SATURATED ");
    intrusionDetected = false;
  }
  else {
    status = "NORMAL";
    lcd.print("NORMAL (OK)      ");

    if (deltaADC >= MIN_DELTA_ADC) {
      vibrationSum += deltaADC;
    }

    if (now - windowStart >= TIME_WINDOW_MS) {

      // STRONG RAPID VIBRATION = INTRUSION
      if (vibrationSum >= INTRUSION_SUM_THRESHOLD) {
        intrusionDetected = true;
        intrusionLatched = true;
      }
      // SMALL CONTINUOUS VIBRATION = WALKING
      else if (vibrationSum >= WALKING_SUM_THRESHOLD) {
        walkingDetected = true;
        walkingTimer = now;
      }

      vibrationSum = 0;
      windowStart = now;
    }
  }

  if (intrusionLatched) {
    digitalWrite(BUZZER_PIN, HIGH);
    lcd.setCursor(0,1);
    lcd.print("INTRUSION ALERT ");
    sendWS(adcValue, "INTRUSION", true);
  }
  else if (diggingDetected) {
    digitalWrite(BUZZER_PIN, HIGH);
    lcd.setCursor(0,1);
    lcd.print("DIGGING DETECT  ");
    sendWS(adcValue, "DIGGING", true);
  }
  else if (walkingDetected) {
    digitalWrite(BUZZER_PIN, LOW);
    lcd.setCursor(0,1);
    lcd.print("WALKING DETECT  ");
    sendWS(adcValue, "WALKING", false);
  }
  else {
    digitalWrite(BUZZER_PIN, LOW);
    sendWS(adcValue, status, false);
  }

  delay(20);
}
