const DEFAULT_SETTINGS = {
  idleThreshold: 10,
  electricityRate: 0.25,
  co2Factor: 0.195,
  updateInterval: 5000,
  liveIndicator: true
};

const DEFAULT_USERS = [
  {
    username: 'admin',
    fullName: 'System Administrator',
    email: 'admin@ecowattsense.com',
    password: 'admin12345',
    role: 'admin',
    pcName: null
  },
  {
    username: 'alex',
    fullName: 'Alex User',
    email: 'alex@example.com',
    password: 'demo123',
    role: 'user',
    pcName: 'PC-LAB-01'
  }
];

const DEFAULT_DEVICES = [
  {name:'PC-LAB-01',status:'Active',power:68,energy:.52,ownerUsername:'alex'},
  {name:'PC-LAB-02',status:'Active',power:72,energy:.61,ownerUsername:''},
  {name:'PC-LAB-03',status:'Idle',power:31,energy:.35,ownerUsername:''},
  {name:'PC-LAB-04',status:'Unused',power:18,energy:.21,ownerUsername:''},
  {name:'PC-LAB-05',status:'Active',power:64,energy:.55,ownerUsername:''},
  {name:'PC-LAB-06',status:'Idle',power:29,energy:.32,ownerUsername:''},
  {name:'PC-LAB-07',status:'Active',power:77,energy:.64,ownerUsername:''},
  {name:'PC-LAB-08',status:'Active',power:59,energy:.49,ownerUsername:''},
  {name:'PC-LAB-09',status:'Unused',power:17,energy:.18,ownerUsername:''},
  {name:'PC-LAB-10',status:'Idle',power:33,energy:.36,ownerUsername:''},
  {name:'PC-LAB-11',status:'Active',power:70,energy:.58,ownerUsername:''},
  {name:'PC-LAB-12',status:'Active',power:65,energy:.54,ownerUsername:''},
  {name:'PC-LAB-13',status:'Active',power:61,energy:.48,ownerUsername:''},
  {name:'PC-LAB-14',status:'Idle',power:30,energy:.31,ownerUsername:''},
  {name:'PC-LAB-15',status:'Active',power:74,energy:.63,ownerUsername:''},
  {name:'PC-LAB-16',status:'Active',power:67,energy:.55,ownerUsername:''},
  {name:'PC-LAB-17',status:'Unused',power:19,energy:.20,ownerUsername:''},
  {name:'PC-LAB-18',status:'Idle',power:28,energy:.29,ownerUsername:''},
  {name:'PC-LAB-19',status:'Active',power:71,energy:.60,ownerUsername:''},
  {name:'PC-LAB-20',status:'Active',power:66,energy:.56,ownerUsername:''}
];

function safeParse(key, fallback){
  try{
    const value = JSON.parse(localStorage.getItem(key));
    return value ?? fallback;
  }catch{
    return fallback;
  }
}

function loadSettings(){
  return {...DEFAULT_SETTINGS, ...safeParse('ecoSettings', {})};
}

function loadUsers(){
  const stored = safeParse('ecoUsers', null);
  if(Array.isArray(stored) && stored.length) return stored;
  localStorage.setItem('ecoUsers', JSON.stringify(DEFAULT_USERS));
  return JSON.parse(JSON.stringify(DEFAULT_USERS));
}

function saveUsers(users){
  localStorage.setItem('ecoUsers', JSON.stringify(users));
}

function loadDevices(){
  const stored = safeParse('ecoDevices', null);
  if(Array.isArray(stored) && stored.length) return stored;
  localStorage.setItem('ecoDevices', JSON.stringify(DEFAULT_DEVICES));
  return JSON.parse(JSON.stringify(DEFAULT_DEVICES));
}

function saveDevices(){
  localStorage.setItem('ecoDevices', JSON.stringify(devices));
}

let appSettings = loadSettings();
let users = loadUsers();
let devices = loadDevices();
let history=[690,720,735,755,748,790,805,798,822,816,840,824];

function currentSession(){
  try{
    const sessionValue = sessionStorage.getItem('ecoSession');
    if(sessionValue) return JSON.parse(sessionValue);
  }catch{}

  try{
    const localValue = localStorage.getItem('ecoSession');
    if(localValue) return JSON.parse(localValue);
  }catch{}

  return null;
}

function setSession(user){
  const session = {
    username:user.username,
    fullName:user.fullName,
    role:user.role,
    pcName:user.pcName || null
  };

  const sessionJson = JSON.stringify(session);

  try{
    sessionStorage.setItem('ecoSession', sessionJson);
    sessionStorage.setItem('loggedIn','true');
  }catch{}

  try{
    localStorage.setItem('ecoSession', sessionJson);
  }catch{}

  return session;
}

function clearSession(){
  try{
    sessionStorage.removeItem('ecoSession');
    sessionStorage.removeItem('loggedIn');
  }catch{}

  try{
    localStorage.removeItem('ecoSession');
  }catch{}
}

function findUser(username){
  return users.find(u=>u.username.toLowerCase()===String(username).trim().toLowerCase());
}

function statusClass(status){
  return status==='Active'?'active-status':status==='Idle'?'idle-status':'unused-status';
}

function money(value){
  return `£${Number(value).toFixed(2)}`;
}

function totalEnergy(list=devices){
  return list.reduce((sum,d)=>sum+Number(d.energy||0),0);
}

function totalPower(list=devices){
  return list.reduce((sum,d)=>sum+Number(d.power||0),0);
}

function statusCounts(list=devices){
  return {
    Active:list.filter(d=>d.status==='Active').length,
    Idle:list.filter(d=>d.status==='Idle').length,
    Unused:list.filter(d=>d.status==='Unused').length
  };
}

// ---------------- Landing mobile menu ----------------
const mobileMenuBtn=document.getElementById('mobileMenuBtn');
const mainNav=document.getElementById('mainNav');
if(mobileMenuBtn && mainNav){
  mobileMenuBtn.addEventListener('click',()=>mainNav.classList.toggle('mobile-open'));
  mainNav.querySelectorAll('a').forEach(a=>a.addEventListener('click',()=>mainNav.classList.remove('mobile-open')));
}

// ---------------- Login with role detection ----------------
const loginForm=document.getElementById('loginForm');
if(loginForm){
  loginForm.addEventListener('submit',e=>{
    e.preventDefault();
    const username=document.getElementById('username').value.trim();
    const password=document.getElementById('password').value;
    const error=document.getElementById('loginError');
    const user=users.find(u =>
      u.username.toLowerCase()===username.toLowerCase() ||
      u.email.toLowerCase()===username.toLowerCase()
    );

    if(!user || user.password!==password){
      error.textContent='Incorrect username or password.';
      error.style.display='block';
      return;
    }

    error.style.display='none';
    setSession(user);
    window.location.href=user.role==='admin'?'dashboard.html':'user-dashboard.html';
  });
}

// ---------------- Registration ----------------
const registerForm=document.getElementById('registerForm');
if(registerForm){
  registerForm.addEventListener('submit',e=>{
    e.preventDefault();

    const fullName=document.getElementById('regFullName').value.trim();
    const username=document.getElementById('regUsername').value.trim();
    const email=document.getElementById('regEmail').value.trim().toLowerCase();
    const pcName=document.getElementById('regPcName').value.trim().toUpperCase();
    const password=document.getElementById('regPassword').value;
    const confirm=document.getElementById('regConfirmPassword').value;
    const error=document.getElementById('registerError');
    const success=document.getElementById('registerSuccess');

    error.style.display='none';
    success.style.display='none';

    if(username.length<3){
      error.textContent='Username must be at least 3 characters.';
      error.style.display='block';
      return;
    }
    if(findUser(username)){
      error.textContent='That username is already registered.';
      error.style.display='block';
      return;
    }
    if(users.some(u=>u.email.toLowerCase()===email)){
      error.textContent='That email address is already registered.';
      error.style.display='block';
      return;
    }
    if(password.length<6){
      error.textContent='Password must be at least 6 characters.';
      error.style.display='block';
      return;
    }
    if(password!==confirm){
      error.textContent='Passwords do not match.';
      error.style.display='block';
      return;
    }

    const pc=devices.find(d=>d.name.toUpperCase()===pcName);
    if(!pc){
      error.textContent='Computer ID not found. Ask an administrator to add the computer first.';
      error.style.display='block';
      return;
    }
    if(pc.ownerUsername){
      error.textContent='That computer is already assigned to another user.';
      error.style.display='block';
      return;
    }

    const newUser={
      username,
      fullName,
      email,
      password,
      role:'user',
      pcName:pc.name
    };
    users.push(newUser);
    pc.ownerUsername=username;
    saveUsers(users);
    saveDevices();

    success.textContent='Registration successful. You can now log in.';
    success.style.display='block';
    registerForm.reset();
    setTimeout(()=>window.location.href='login.html',1400);
  });
}

// ---------------- Access control ----------------
const adminPage=document.body.classList.contains('dashboard-page');
const userPage=document.body.classList.contains('user-dashboard-page');

if(adminPage || userPage){
  const session=currentSession();

  if(!session){
    window.location.replace('login.html');
  }else if(adminPage && session.role!=='admin'){
    window.location.replace('user-dashboard.html');
  }else if(userPage && session.role!=='user'){
    window.location.replace('dashboard.html');
  }

  if(adminPage){
    const page=document.body.dataset.page;
    document.querySelectorAll('.sidebar-nav a').forEach(link=>{
      link.classList.toggle('active',link.dataset.page===page);
    });
    if(!appSettings.liveIndicator){
      document.querySelectorAll('.live').forEach(el=>el.style.display='none');
    }

    // Replace the hard-coded Administrator text with the actual logged-in admin.
    if(session){
      document.querySelectorAll('.user-pill strong').forEach(el=>el.textContent=session.fullName || session.username);
      document.querySelectorAll('.user-avatar').forEach(el=>el.textContent=(session.username||'A').charAt(0).toUpperCase());
    }
  }
}

const logout=document.getElementById('logoutBtn');
if(logout){
  logout.addEventListener('click',()=>{
    clearSession();
    window.location.href='login.html';
  });
}

// ---------------- Admin dashboard ----------------
function renderTable(list=devices){
  const body=document.getElementById('deviceTable');
  if(!body)return;
  body.innerHTML=list.slice(0,10).map(d=>`<tr>
    <td><strong>${d.name}</strong></td>
    <td><span class="status ${statusClass(d.status)}">${d.status}</span></td>
    <td>${d.power} W</td>
    <td>${Number(d.energy).toFixed(2)} kWh</td>
  </tr>`).join('');
}

function updateStats(){
  if(!document.getElementById('activeCount'))return;
  const counts=statusCounts();
  const energy=totalEnergy();
  const power=totalPower();

  document.getElementById('activeCount').textContent=counts.Active;
  document.getElementById('idleCount').textContent=counts.Idle;
  document.getElementById('unusedCount').textContent=counts.Unused;
  document.getElementById('powerNow').textContent=power;

  const energyEl=document.getElementById('dashboardEnergyToday');
  const costEl=document.getElementById('dashboardCost');
  const co2El=document.getElementById('dashboardCo2');
  const lastEl=document.getElementById('lastUpdated');

  if(energyEl) energyEl.textContent=`${energy.toFixed(2)} kWh`;
  if(costEl) costEl.textContent=money(energy*Number(appSettings.electricityRate));
  if(co2El) co2El.textContent=`${(energy*Number(appSettings.co2Factor)).toFixed(2)} kg`;
  if(lastEl) lastEl.textContent=new Date().toLocaleTimeString([], {hour:'2-digit',minute:'2-digit',second:'2-digit'});
}

function drawLine(){
  const c=document.getElementById('energyChart'); if(!c)return;
  const ctx=c.getContext('2d'),w=c.width,h=c.height,p=30;
  ctx.clearRect(0,0,w,h);
  ctx.strokeStyle='#e8eeea';ctx.lineWidth=1;
  for(let i=0;i<5;i++){
    const y=p+i*(h-2*p)/4;
    ctx.beginPath();ctx.moveTo(p,y);ctx.lineTo(w-p,y);ctx.stroke();
  }
  const min=Math.min(...history)-20,max=Math.max(...history)+20;
  ctx.beginPath();
  history.forEach((v,i)=>{
    const x=p+i*(w-2*p)/(history.length-1);
    const y=h-p-(v-min)/(max-min)*(h-2*p);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  });
  ctx.strokeStyle='#20a65d';ctx.lineWidth=3;ctx.stroke();
}

function drawDonut(){
  const c=document.getElementById('statusChart'); if(!c)return;
  const ctx=c.getContext('2d'),cx=c.width/2,cy=c.height/2,r=80,lw=34;
  ctx.clearRect(0,0,c.width,c.height);
  const counts=statusCounts();
  const vals=[[counts.Active,'#31c968'],[counts.Idle,'#f1b93b'],[counts.Unused,'#e85d5d']];
  let start=-Math.PI/2;
  vals.forEach(([n,color])=>{
    const end=start+(devices.length?n/devices.length:0)*Math.PI*2;
    ctx.beginPath();ctx.arc(cx,cy,r,start,end);ctx.strokeStyle=color;ctx.lineWidth=lw;ctx.stroke();start=end;
  });
  ctx.fillStyle='#17211d';ctx.textAlign='center';ctx.font='bold 28px Arial';ctx.fillText(devices.length,cx,cy+5);
  ctx.font='12px Arial';ctx.fillStyle='#6b756f';ctx.fillText('Devices',cx,cy+24);
}

const search=document.getElementById('searchBox');
if(search){
  search.addEventListener('input',()=>{
    const q=search.value.toLowerCase();
    renderTable(devices.filter(d=>d.name.toLowerCase().includes(q)||d.status.toLowerCase().includes(q)));
  });
}

// ---------------- Admin computer management ----------------
let currentComputerFilter='All';

function renderComputerCards(){
  const grid=document.getElementById('computerGrid');
  if(!grid)return;

  const q=(document.getElementById('computerSearch')?.value||'').toLowerCase();
  const filtered=devices.filter(d=>{
    const matchesFilter=currentComputerFilter==='All'||d.status===currentComputerFilter;
    const matchesSearch=d.name.toLowerCase().includes(q)||d.status.toLowerCase().includes(q)||(d.ownerUsername||'').toLowerCase().includes(q);
    return matchesFilter&&matchesSearch;
  });

  grid.innerHTML=filtered.map(d=>`
    <article class="computer-card">
      <div class="computer-card-top">
        <div>
          <h3>${d.name}</h3>
          <small>Monitored device</small>
        </div>
        <span class="status ${statusClass(d.status)}">${d.status}</span>
      </div>
      <div class="computer-card-metrics">
        <div><span>CURRENT POWER</span><strong>${d.power} W</strong></div>
        <div><span>ENERGY TODAY</span><strong>${Number(d.energy).toFixed(2)} kWh</strong></div>
      </div>
      <div class="computer-card-actions">
        <span class="pc-owner">Owner: <strong>${d.ownerUsername||'Unassigned'}</strong></span>
        <button class="remove-pc-btn" data-remove-pc="${d.name}">Remove</button>
      </div>
    </article>
  `).join('');

  grid.querySelectorAll('[data-remove-pc]').forEach(btn=>{
    btn.addEventListener('click',()=>removeComputer(btn.dataset.removePc));
  });
}

function removeComputer(name){
  const device=devices.find(d=>d.name===name);
  if(!device) return;
  if(!confirm(`Remove ${name} from EcoWattSense?`)) return;

  devices=devices.filter(d=>d.name!==name);

  if(device.ownerUsername){
    const owner=findUser(device.ownerUsername);
    if(owner) owner.pcName=null;
    saveUsers(users);
  }

  saveDevices();
  renderComputerCards();
  renderTable();
  updateStats();
  drawDonut();

  const msg=document.getElementById('pcManagerMessage');
  if(msg) msg.textContent=`${name} removed from the system.`;
}

const addComputerForm=document.getElementById('addComputerForm');
if(addComputerForm){
  addComputerForm.addEventListener('submit',e=>{
    e.preventDefault();

    const name=document.getElementById('newPcName').value.trim().toUpperCase();
    const status=document.getElementById('newPcStatus').value;
    const ownerUsername=document.getElementById('newPcOwner').value.trim();
    const msg=document.getElementById('pcManagerMessage');

    if(devices.some(d=>d.name.toUpperCase()===name)){
      msg.style.color='#bd4242';
      msg.textContent='A computer with that ID already exists.';
      return;
    }

    let owner='';
    if(ownerUsername){
      const user=findUser(ownerUsername);
      if(!user || user.role!=='user'){
        msg.style.color='#bd4242';
        msg.textContent='Owner username must belong to a registered normal user.';
        return;
      }
      if(user.pcName){
        msg.style.color='#bd4242';
        msg.textContent=`${user.username} is already assigned to ${user.pcName}.`;
        return;
      }
      owner=user.username;
      user.pcName=name;
      saveUsers(users);
    }

    const ranges={
      Active:[45,90],
      Idle:[24,46],
      Unused:[12,24]
    };
    const [min,max]=ranges[status];
    const power=Math.floor(Math.random()*(max-min+1))+min;

    devices.push({name,status,power,energy:0,ownerUsername:owner});
    saveDevices();

    msg.style.color='#1a8f56';
    msg.textContent=`${name} added successfully${owner?` and assigned to ${owner}`:''}.`;
    addComputerForm.reset();
    renderComputerCards();
    updateStats();
    drawDonut();
  });
}

document.querySelectorAll('.filter-btn').forEach(btn=>{
  btn.addEventListener('click',()=>{
    currentComputerFilter=btn.dataset.filter;
    document.querySelectorAll('.filter-btn').forEach(b=>b.classList.remove('active-filter'));
    btn.classList.add('active-filter');
    renderComputerCards();
  });
});
const computerSearch=document.getElementById('computerSearch');
if(computerSearch) computerSearch.addEventListener('input',renderComputerCards);

// ---------------- Energy page ----------------
const energyPeriods={
  day:[6.2,6.6,7.1,7.5,7.2,7.9,8.4,8.1,8.7,9.0,9.4,9.8],
  week:[52,58,61,57,64,68,66],
  month:[210,226,219,242,238,251,247,265,258,274,269,284]
};

function drawEnergyUsageChart(period='day'){
  const c=document.getElementById('energyUsageChart'); if(!c)return;
  const values=energyPeriods[period];
  const ctx=c.getContext('2d'),w=c.width,h=c.height,p=42;
  ctx.clearRect(0,0,w,h);
  ctx.strokeStyle='#e7ece9';ctx.lineWidth=1;
  for(let i=0;i<5;i++){
    const y=p+i*(h-2*p)/4;
    ctx.beginPath();ctx.moveTo(p,y);ctx.lineTo(w-p,y);ctx.stroke();
  }
  const min=Math.min(...values)*.88;
  const max=Math.max(...values)*1.08;
  ctx.beginPath();
  values.forEach((v,i)=>{
    const x=p+i*(w-2*p)/(values.length-1);
    const y=h-p-(v-min)/(max-min)*(h-2*p);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  });
  ctx.strokeStyle='#1a8f56';ctx.lineWidth=3;ctx.stroke();
  ctx.fillStyle='#1a8f56';
  values.forEach((v,i)=>{
    const x=p+i*(w-2*p)/(values.length-1);
    const y=h-p-(v-min)/(max-min)*(h-2*p);
    ctx.beginPath();ctx.arc(x,y,4,0,Math.PI*2);ctx.fill();
  });
}

document.querySelectorAll('.period-tab').forEach(btn=>{
  btn.addEventListener('click',()=>{
    document.querySelectorAll('.period-tab').forEach(b=>b.classList.remove('active-filter'));
    btn.classList.add('active-filter');
    drawEnergyUsageChart(btn.dataset.period);
  });
});

function renderEnergyPage(){
  if(!document.getElementById('energyUsageChart'))return;
  const energy=totalEnergy();
  const power=totalPower();

  document.getElementById('energyTodayValue').textContent=`${energy.toFixed(2)} kWh`;
  document.getElementById('energyWeekValue').textContent=`${(energy*6.93).toFixed(1)} kWh`;
  document.getElementById('energyCostValue').textContent=money(energy*6.93*Number(appSettings.electricityRate));
  document.getElementById('averagePowerValue').textContent=`${power} W`;
  document.getElementById('impactCost').textContent=money(energy*Number(appSettings.electricityRate));
  document.getElementById('impactCo2').textContent=`${(energy*Number(appSettings.co2Factor)).toFixed(2)} kg`;

  const top=[...devices].sort((a,b)=>b.energy-a.energy).slice(0,5);
  document.getElementById('topConsumers').innerHTML=top.map((d,i)=>`
    <div class="rank-item">
      <span class="rank-number">0${i+1}</span>
      <div><strong>${d.name}</strong><small>${d.status}</small></div>
      <strong>${Number(d.energy).toFixed(2)} kWh</strong>
    </div>
  `).join('');
  drawEnergyUsageChart('day');
}

// ---------------- Reports ----------------
function renderReportPage(){
  const body=document.getElementById('reportTable');
  if(!body)return;
  const energy=totalEnergy();
  document.getElementById('reportDeviceCount').textContent=devices.length;
  document.getElementById('reportEnergy').textContent=`${energy.toFixed(2)} kWh`;
  document.getElementById('reportCost').textContent=money(energy*Number(appSettings.electricityRate));
  body.innerHTML=devices.map(d=>`<tr>
    <td><strong>${d.name}</strong></td>
    <td><span class="status ${statusClass(d.status)}">${d.status}</span></td>
    <td>${d.power} W</td>
    <td>${Number(d.energy).toFixed(2)} kWh</td>
  </tr>`).join('');
}

const downloadCsv=document.getElementById('downloadCsv');
if(downloadCsv){
  downloadCsv.addEventListener('click',()=>{
    const period=document.getElementById('reportPeriod').value;
    const rows=[
      ['EcoWattSense Report',period],[],
      ['Computer','Owner','Status','Power (W)','Energy Today (kWh)'],
      ...devices.map(d=>[d.name,d.ownerUsername||'',d.status,d.power,Number(d.energy).toFixed(2)])
    ];
    const csv=rows.map(row=>row.map(v=>`"${String(v??'').replaceAll('"','""')}"`).join(',')).join('\n');
    const blob=new Blob([csv],{type:'text/csv;charset=utf-8'});
    const url=URL.createObjectURL(blob);
    const a=document.createElement('a');
    a.href=url;
    a.download='ecowattsense-report.csv';
    a.click();
    URL.revokeObjectURL(url);
  });
}

// ---------------- Settings ----------------
const settingsForm=document.getElementById('settingsForm');
if(settingsForm){
  document.getElementById('idleThreshold').value=appSettings.idleThreshold;
  document.getElementById('electricityRate').value=appSettings.electricityRate;
  document.getElementById('co2Factor').value=appSettings.co2Factor;
  document.getElementById('updateInterval').value=appSettings.updateInterval;
  document.getElementById('liveIndicator').checked=Boolean(appSettings.liveIndicator);

  settingsForm.addEventListener('submit',e=>{
    e.preventDefault();
    appSettings={
      idleThreshold:Number(document.getElementById('idleThreshold').value),
      electricityRate:Number(document.getElementById('electricityRate').value),
      co2Factor:Number(document.getElementById('co2Factor').value),
      updateInterval:Number(document.getElementById('updateInterval').value),
      liveIndicator:document.getElementById('liveIndicator').checked
    };
    localStorage.setItem('ecoSettings',JSON.stringify(appSettings));
    const message=document.getElementById('settingsMessage');
    message.textContent='Settings saved';
    setTimeout(()=>message.textContent='',1800);
  });
}

// ---------------- Normal user dashboard ----------------
let userHistory=[];

function drawUserLine(device){
  const c=document.getElementById('userEnergyChart');
  if(!c || !device) return;

  if(!userHistory.length){
    userHistory=[device.power*0.83,device.power*0.91,device.power*0.88,device.power*1.03,device.power*.97,device.power];
  }

  const ctx=c.getContext('2d'),w=c.width,h=c.height,p=30;
  ctx.clearRect(0,0,w,h);
  ctx.strokeStyle='#e8eeea';ctx.lineWidth=1;
  for(let i=0;i<5;i++){
    const y=p+i*(h-2*p)/4;
    ctx.beginPath();ctx.moveTo(p,y);ctx.lineTo(w-p,y);ctx.stroke();
  }

  const min=Math.max(0,Math.min(...userHistory)-12);
  const max=Math.max(...userHistory)+12;
  ctx.beginPath();
  userHistory.forEach((v,i)=>{
    const x=p+i*(w-2*p)/(userHistory.length-1);
    const y=h-p-(v-min)/(max-min||1)*(h-2*p);
    if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
  });
  ctx.strokeStyle='#20a65d';ctx.lineWidth=3;ctx.stroke();
}

function renderUserDashboard(){
  if(!document.body.classList.contains('user-dashboard-page')) return;

  const session=currentSession();
  if(!session) return;

  const user=findUser(session.username);
  const device=devices.find(d=>d.name===user?.pcName);

  const displayName=document.getElementById('userDisplayName');
  const avatar=document.getElementById('userAvatar');
  if(displayName) displayName.textContent=user?.fullName||user?.username||'User';
  if(avatar) avatar.textContent=(user?.username||'U').charAt(0).toUpperCase();

  const noPc=document.getElementById('userNoPc');
  const dashboard=document.getElementById('userPcDashboard');

  if(!device){
    noPc.hidden=false;
    dashboard.hidden=true;
    return;
  }

  noPc.hidden=true;
  dashboard.hidden=false;

  document.getElementById('userPcName').textContent=device.name;
  document.getElementById('summaryPcName').textContent=device.name;
  document.getElementById('userStatusText').textContent=device.status;
  document.getElementById('summaryPcStatus').textContent=device.status;
  document.getElementById('userPowerNow').textContent=device.power;
  document.getElementById('userEnergyToday').textContent=`${Number(device.energy).toFixed(2)} kWh`;
  document.getElementById('userEstimatedCost').textContent=money(Number(device.energy)*Number(appSettings.electricityRate));
  document.getElementById('userCo2').textContent=`${(Number(device.energy)*Number(appSettings.co2Factor)).toFixed(2)} kg`;

  const status=document.getElementById('userPcStatus');
  status.textContent=device.status;
  status.className=`status ${statusClass(device.status)}`;

  drawUserLine(device);
}

function simulateUserDevice(){
  if(!document.body.classList.contains('user-dashboard-page')) return;
  const session=currentSession();
  const user=findUser(session?.username);
  const device=devices.find(d=>d.name===user?.pcName);
  if(!device) return;

  const min=device.status==='Active'?45:device.status==='Idle'?24:12;
  const max=device.status==='Active'?95:device.status==='Idle'?48:25;
  device.power=Math.max(min,Math.min(max,device.power+Math.round(Math.random()*8-4)));
  device.energy+=device.power/3600000*(Number(appSettings.updateInterval)/1000);

  userHistory.push(device.power);
  if(userHistory.length>14) userHistory.shift();

  saveDevices();
  renderUserDashboard();
}

// ---------------- Live simulation for admin pages ----------------
function simulateAdmin(){
  if(!document.body.classList.contains('dashboard-page')) return;

  devices.forEach(d=>{
    const min=d.status==='Active'?45:d.status==='Idle'?24:12;
    const max=d.status==='Active'?95:d.status==='Idle'?48:25;
    d.power=Math.max(min,Math.min(max,d.power+Math.round(Math.random()*8-4)));
    d.energy+=d.power/3600000*(Number(appSettings.updateInterval)/1000);
    if(Math.random()<.02){
      d.status=['Active','Idle','Unused'][Math.floor(Math.random()*3)];
    }
  });

  history.push(totalPower());
  if(history.length>14)history.shift();
  saveDevices();

  renderTable();
  updateStats();
  drawLine();
  drawDonut();
  renderComputerCards();
  renderEnergyPage();
  renderReportPage();
}

// ---------------- Initialise ----------------
if(document.body.classList.contains('dashboard-page')){
  renderTable();
  updateStats();
  drawLine();
  drawDonut();
  renderComputerCards();
  renderEnergyPage();
  renderReportPage();
  setInterval(simulateAdmin,Number(appSettings.updateInterval)||5000);
}

if(document.body.classList.contains('user-dashboard-page')){
  renderUserDashboard();
  setInterval(simulateUserDevice,Number(appSettings.updateInterval)||5000);
}


// EcoWattSense login compatibility note:
// Session is mirrored to localStorage so role-based page changes remain reliable
// when using VS Code Live Server / Edge.
