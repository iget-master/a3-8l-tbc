#pragma once
#include <Arduino.h>

// Página web única (pt-BR, tema escuro, self-contained) servida em GET /.
// Contrato HTTP: ver webui.h — os nomes dos campos do form são os membros de
// Settings e os campos do JSON de status são os do /api/status.
const char WEBUI_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>A3-8L TBC</title>
<style>
:root{--bg:#0e1217;--card:#161c24;--edge:#26303d;--tx:#dde5ee;--dim:#8595a8;--acc:#4da3ff;--ok:#3ecf7a;--warn:#ffb54d;--err:#ff5d5d}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--tx);font:14px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;padding:12px;max-width:900px;margin:0 auto}
h1{font-size:18px;display:flex;align-items:center;gap:10px;flex-wrap:wrap}
h2{font-size:14px;color:var(--acc);margin:0 0 8px}
.badge{font-size:11px;padding:2px 8px;border-radius:10px;background:var(--err);color:#fff}
.badge.on{background:var(--ok);color:#08301a}
.card{background:var(--card);border:1px solid var(--edge);border-radius:8px;padding:12px;margin:12px 0}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:8px}
.stat{background:var(--bg);border:1px solid var(--edge);border-radius:6px;padding:6px 8px;min-width:0}
.stat .l{font-size:11px;color:var(--dim)}
.stat .v{font-size:15px;font-variant-numeric:tabular-nums;overflow-wrap:anywhere}
.stat.wide{grid-column:span 2}
.v.err{color:var(--err)}.v.warn{color:var(--warn)}
.bar{position:relative;height:22px;background:var(--bg);border:1px solid var(--edge);border-radius:6px;overflow:hidden;margin-top:10px}
#barPos{position:absolute;top:0;bottom:0;left:50%;width:0;background:linear-gradient(90deg,#2b6cb0,#4da3ff);transition:left .12s linear,width .12s linear}
#barSp{position:absolute;top:0;bottom:0;width:2px;background:var(--warn);left:50%}
.bar .zero{position:absolute;top:0;bottom:0;left:50%;width:1px;background:var(--dim);opacity:.6}
.leg{display:flex;gap:16px;font-size:12px;color:var(--dim);margin-top:4px}
.leg .p{color:var(--acc)}.leg .s{color:var(--warn)}
.warnbox{background:#3a2a10;border:1px solid #6b4d17;color:var(--warn);padding:8px 10px;border-radius:6px;font-size:13px;margin-bottom:10px}
.hint{color:var(--dim);font-size:12px;margin-top:6px}
.chk{display:inline-flex;align-items:center;gap:6px;cursor:pointer}
.manrow{display:flex;align-items:center;gap:10px;margin-top:8px}
.manrow input[type=range]{flex:1;accent-color:var(--acc)}
.manrow b{min-width:52px;text-align:right;font-variant-numeric:tabular-nums}
.pgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(250px,1fr));gap:12px}
fieldset{border:1px solid var(--edge);border-radius:6px;padding:8px 10px;min-width:0}
legend{color:var(--acc);font-size:12px;padding:0 4px}
.f{display:flex;justify-content:space-between;align-items:center;gap:8px;margin:6px 0}
.f span{color:var(--dim);font-size:12px}
input[type=number],input[type=text]{width:110px;background:var(--bg);border:1px solid var(--edge);border-radius:4px;color:var(--tx);padding:4px 6px;font:inherit}
input[type=text]{width:150px}
input[type=checkbox]{accent-color:var(--acc);width:16px;height:16px}
button{background:var(--acc);border:0;border-radius:6px;color:#04121f;padding:8px 14px;font:inherit;font-weight:600;cursor:pointer}
button.danger{background:var(--err);color:#fff}
.btnrow{display:flex;align-items:center;gap:10px;margin-top:10px;flex-wrap:wrap}
.msg{color:var(--ok);font-size:13px}
</style>
</head>
<body>
<h1>A3-8L TBC <span id="conn" class="badge">sem conexão</span></h1>

<div class="card">
<h2>Monitor</h2>
<div class="grid">
<div class="stat"><div class="l">Modo</div><div class="v" id="sMode">–</div></div>
<div class="stat"><div class="l">Falha</div><div class="v" id="sFault">–</div></div>
<div class="stat"><div class="l">Idle (pedal solto)</div><div class="v" id="sIdle">–</div></div>
<div class="stat"><div class="l">Setpoint</div><div class="v" id="sSp">–</div></div>
<div class="stat"><div class="l">Posição</div><div class="v" id="sPos">–</div></div>
<div class="stat"><div class="l">Duty aplicado</div><div class="v" id="sDuty">–</div></div>
<div class="stat"><div class="l">Saída analógica</div><div class="v" id="sAout">–</div></div>
<div class="stat"><div class="l">TPS (raw)</div><div class="v" id="sRaw">–</div></div>
<div class="stat"><div class="l">Comando PWM</div><div class="v" id="sCmd">–</div></div>
<div class="stat"><div class="l">Manual</div><div class="v" id="sManual">–</div></div>
<div class="stat wide"><div class="l">Termos PID</div><div class="v" id="sPid">–</div></div>
<div class="stat"><div class="l">Calibração</div><div class="v" id="sCal">–</div></div>
<div class="stat wide"><div class="l">Calibração (raw)</div><div class="v" id="sCalV">–</div></div>
<div class="stat"><div class="l">Versão</div><div class="v" id="sVer">–</div></div>
<div class="stat"><div class="l">Uptime</div><div class="v" id="sUptime">–</div></div>
</div>
<div class="bar"><div class="zero"></div><div id="barPos"></div><div id="barSp"></div></div>
<div class="leg"><span>−100%</span><span class="p">■ posição</span><span class="s">▌ setpoint</span><span>0 = repouso (centro)</span><span style="margin-left:auto">+100%</span></div>
</div>

<div class="card">
<h2>Modo manual (bancada)</h2>
<p class="warnbox">⚠ Uso exclusivo em bancada, com o veículo desligado. O duty vai
direto à ponte H, sem PID e sem a trava do pedal. Sem keepalive (aba fechada ou
travada) o firmware desliga o motor em 3 s.</p>
<label class="chk"><input type="checkbox" id="manOn"> ligar modo manual</label>
<div class="manrow">
<input type="range" id="manDuty" min="-100" max="100" step="1" value="0" disabled>
<b id="manVal">0 %</b>
</div>
<p class="hint">−100% = fechar todo · 0 = coast (mola leva ao repouso) · +100% = abrir todo</p>
</div>

<div class="card">
<h2>Parâmetros</h2>
<form id="pform" onsubmit="return false">
<div class="pgrid">
<fieldset><legend>PID</legend>
<label class="f"><span>Kp (%duty/%erro)</span><input id="kp" type="number" step="any"></label>
<label class="f"><span>Ki (/s)</span><input id="ki" type="number" step="any"></label>
<label class="f"><span>Kd (s)</span><input id="kd" type="number" step="any"></label>
<label class="f"><span>Zona morta (%)</span><input id="deadbandPct" type="number" step="any"></label>
<label class="f"><span>Duty máx (%)</span><input id="maxDutyPct" type="number" step="any"></label>
<label class="f"><span>Malha (Hz)</span><input id="loopHz" type="number" step="1"></label>
</fieldset>
<fieldset><legend>Comando</legend>
<label class="f"><span>Timeout (ms)</span><input id="cmdTimeoutMs" type="number" step="1"></label>
<label class="f"><span>Nível alto fixo = 100%</span><input id="cmdStuckHighIs100" type="checkbox"></label>
</fieldset>
<fieldset><legend>Ponte H</legend>
<label class="f"><span>Freq. PWM (Hz)</span><input id="pwmFreqHz" type="number" step="1"></label>
</fieldset>
<fieldset><legend>TPS</legend>
<label class="f"><span>Falha abaixo de (raw)</span><input id="tpsFaultLowRaw" type="number" step="1"></label>
<label class="f"><span>Falha acima de (raw)</span><input id="tpsFaultHighRaw" type="number" step="1"></label>
</fieldset>
<fieldset><legend>Calibração</legend>
<label class="f"><span>Estabilização (ms)</span><input id="calSettleMs" type="number" step="1"></label>
<label class="f"><span>Banda estável (counts)</span><input id="calStabilityCounts" type="number" step="1"></label>
<label class="f"><span>Timeout por fase (ms)</span><input id="calTimeoutMs" type="number" step="1"></label>
<label class="f"><span>Faixa mínima (counts)</span><input id="calMinRangeCounts" type="number" step="1"></label>
<label class="f"><span>Duty da calibração (%)</span><input id="calDrivePct" type="number" step="any"></label>
</fieldset>
<fieldset><legend>Saída analógica</legend>
<label class="f"><span>Mín (raw, 0 = auto)</span><input id="outMinRaw" type="number" step="1"></label>
<label class="f"><span>Máx (raw, 0 = auto)</span><input id="outMaxRaw" type="number" step="1"></label>
<label class="f"><span>Aprender máx fora de idle</span><input id="outAutoLearnMax" type="checkbox"></label>
</fieldset>
<fieldset><legend>Switch de idle</legend>
<label class="f"><span>Ativo em nível baixo</span><input id="idleActiveLow" type="checkbox"></label>
<label class="f"><span>Debounce (ms)</span><input id="idleDebounceMs" type="number" step="1"></label>
</fieldset>
<fieldset><legend>WiFi (AP)</legend>
<label class="f"><span>SSID</span><input id="apSsid" type="text" maxlength="32"></label>
<label class="f"><span>Senha (≥ 8)</span><input id="apPass" type="text" maxlength="64"></label>
<p class="hint">SSID/senha valem após reiniciar o ESP32.</p>
</fieldset>
</div>
<div class="btnrow">
<button type="button" id="btnSave">Salvar parâmetros</button>
<span id="msg" class="msg"></span>
</div>
</form>
</div>

<div class="card">
<h2>Ações</h2>
<div class="btnrow">
<button type="button" id="btnCal">Calibrar</button>
<button type="button" id="btnDef" class="danger">Restaurar padrões</button>
</div>
<p class="hint">A calibração só executa com o pedal solto (idle) e movimenta o
atuador até os fins de curso.</p>
</div>

<script>
var el=function(id){return document.getElementById(id);};
var t=function(id,v){el(id).textContent=v;};
var MODES={Boot:'Inicializando',Calibrating:'Calibrando',Run:'Regulando (idle)',DriverActive:'Pedal acionado',Fault:'FALHA',Manual:'Manual'};
var FLOATS=['kp','ki','kd','deadbandPct','maxDutyPct','calDrivePct'];
var INTS=['loopHz','cmdTimeoutMs','pwmFreqHz','tpsFaultLowRaw','tpsFaultHighRaw','calSettleMs','calStabilityCounts','calTimeoutMs','calMinRangeCounts','outMinRaw','outMaxRaw','idleDebounceMs'];
var BOOLS=['cmdStuckHighIs100','outAutoLearnMax','idleActiveLow'];
var TEXTS=['apSsid','apPass'];

function setOnline(on){var b=el('conn');b.textContent=on?'conectado':'sem conexão';b.className='badge'+(on?' on':'');}

function post(url,body){
  return fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body||''})
    .then(function(r){setOnline(true);return r;})
    .catch(function(){setOnline(false);});
}

function up(ms){var s=Math.floor(ms/1000);return Math.floor(s/3600)+'h '+(Math.floor(s/60)%60)+'m '+(s%60)+'s';}

function render(s){
  t('sMode',MODES[s.mode]||s.mode);
  el('sMode').className='v'+(s.mode==='Fault'?' err':(s.mode==='Manual'||s.mode==='Calibrating')?' warn':'');
  t('sFault',s.fault||'—');el('sFault').className='v'+(s.fault?' err':'');
  t('sIdle',s.idle?'sim':'não');
  t('sSp',s.setpoint.toFixed(1)+' %');
  t('sPos',s.pos.toFixed(1)+' %');
  t('sDuty',s.duty.toFixed(1)+' %');
  t('sAout',s.analogOut.toFixed(1)+' %');
  t('sRaw',s.rawTps);
  t('sCmd',s.cmdPresent?s.cmdDuty.toFixed(1)+' % @ '+s.cmdFreq.toFixed(0)+' Hz':'ausente');
  t('sManual',s.manual?'ATIVO':'—');
  el('sManual').className='v'+(s.manual?' warn':'');
  t('sPid','P '+s.pidP.toFixed(1)+' · I '+s.pidI.toFixed(1)+' · D '+s.pidD.toFixed(1));
  t('sCal',s.cal.state+(s.cal.valid?'':' (inválida)'));
  el('sCal').className='v'+(s.cal.valid?'':' err');
  t('sCalV','rep '+s.cal.rest+' · mín '+s.cal.min+' · máx '+s.cal.max+' · apr '+s.cal.learnedMax);
  t('sVer','v'+s.ver);
  t('sUptime',up(s.uptimeMs));
  // Escala −100..+100 com o repouso (0) no centro da barra
  var p=Math.max(-100,Math.min(100,s.pos)),q=Math.max(-100,Math.min(100,s.setpoint));
  var bp=el('barPos');
  bp.style.left=(p<0?50+p/2:50)+'%';
  bp.style.width=Math.abs(p)/2+'%';
  el('barSp').style.left='calc('+(50+q/2)+'% - 1px)';
}

var busy=false;
setInterval(function(){
  if(busy)return;
  busy=true;
  fetch('/api/status')
    .then(function(r){return r.json();})
    .then(function(s){setOnline(true);render(s);})
    .catch(function(){setOnline(false);})
    .finally(function(){busy=false;});
},300);

function loadParams(){
  fetch('/api/params').then(function(r){return r.json();}).then(function(p){
    FLOATS.concat(INTS,TEXTS).forEach(function(k){el(k).value=p[k];});
    BOOLS.forEach(function(k){el(k).checked=!!p[k];});
  }).catch(function(){setOnline(false);});
}

function saveParams(){
  var b=new URLSearchParams();
  FLOATS.concat(INTS,TEXTS).forEach(function(k){b.append(k,el(k).value);});
  BOOLS.forEach(function(k){b.append(k,el(k).checked?'1':'0');});
  post('/api/params',b.toString()).then(function(r){
    if(r&&r.ok){flash('parâmetros salvos');loadParams();}
    else flash('erro ao salvar');
  });
}

var flashT=null;
function flash(m){t('msg',m);clearTimeout(flashT);flashT=setTimeout(function(){t('msg','');},3000);}

function sendManual(){
  var on=el('manOn').checked;
  post('/api/manual','on='+(on?1:0)+'&duty='+(on?el('manDuty').value:0));
}
var manT=null;
el('manDuty').addEventListener('input',function(){
  t('manVal',this.value+' %');
  if(!manT)manT=setTimeout(function(){manT=null;sendManual();},150);
});
el('manDuty').addEventListener('change',sendManual);
el('manOn').addEventListener('change',function(){
  el('manDuty').disabled=!this.checked;
  if(!this.checked){el('manDuty').value=0;t('manVal','0 %');}
  sendManual();
});
setInterval(function(){if(el('manOn').checked)sendManual();},1000);

el('btnSave').addEventListener('click',saveParams);
el('btnCal').addEventListener('click',function(){
  if(confirm('Iniciar auto calibração? O atuador vai até os fins de curso. Só executa com o pedal solto (idle).'))post('/api/cal');
});
el('btnDef').addEventListener('click',function(){
  if(confirm('Restaurar TODOS os parâmetros para os padrões de fábrica?'))
    post('/api/defaults').then(function(){flash('padrões restaurados');loadParams();});
});
loadParams();
</script>
</body>
</html>
)rawliteral";
